#include "ailo_model_internal.hpp"
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <curl/curl.h>
#include <limits>
#include <memory>
#include <mutex>
#include <thread>
#include <unistd.h>

namespace ailo {
namespace model {
namespace {
std::mutex runtime_mutex;
unsigned runtime_users = 0;
class runtime {
public:
  runtime() {
    std::lock_guard<std::mutex> lock(runtime_mutex);
    if (!runtime_users) {
      CURLcode code = curl_global_init(CURL_GLOBAL_DEFAULT);
      if (code == CURLE_OUT_OF_MEMORY)
        throw std::bad_alloc();
      if (code != CURLE_OK)
        throw failure(AILO_DOWNLOAD_FAILED,
                      "Cannot initialize HTTPS transport");
    }
    ++runtime_users;
  }
  ~runtime() {
    std::lock_guard<std::mutex> lock(runtime_mutex);
    if (!--runtime_users)
      curl_global_cleanup();
  }
  runtime(const runtime &) = delete;
  runtime &operator=(const runtime &) = delete;
};

std::string lower(std::string s) {
  for (char &c : s)
    if (c >= 'A' && c <= 'Z')
      c = static_cast<char>(c - 'A' + 'a');
  return s;
}

std::string url_part(CURLU *u, CURLUPart part, unsigned flags = 0) {
  char *value = nullptr;
  CURLUcode code = curl_url_get(u, part, &value, flags);
  if (code == CURLUE_OUT_OF_MEMORY)
    throw std::bad_alloc();
  if (code != CURLUE_OK)
    return {};
  auto free_string = [](char *p) { curl_free(p); };
  std::unique_ptr<char, decltype(free_string)> owned(value, free_string);
  return value;
}

static bool has_url_part(CURLU *url, CURLUPart part) {
  char *text = nullptr;
  CURLUcode code = curl_url_get(url, part, &text, 0);
  if (code == CURLUE_OUT_OF_MEMORY)
    throw std::bad_alloc();
  if (code == CURLUE_OK)
    curl_free(text);
  return code == CURLUE_OK;
}

std::string https_origin(const std::string &url) {
  std::unique_ptr<CURLU, decltype(&curl_url_cleanup)> u(curl_url(),
                                                        curl_url_cleanup);
  if (!u)
    throw std::bad_alloc();
  if (url.size() > 16384)
    throw failure(AILO_DOWNLOAD_FAILED,
                  "Model redirect URL exceeds its length limit");
  CURLUcode code = curl_url_set(u.get(), CURLUPART_URL, url.c_str(), 0);
  if (code == CURLUE_OUT_OF_MEMORY)
    throw std::bad_alloc();
  if (code != CURLUE_OK ||
      lower(url_part(u.get(), CURLUPART_SCHEME)) != "https" ||
      url_part(u.get(), CURLUPART_HOST).empty() ||
      has_url_part(u.get(), CURLUPART_USER) ||
      has_url_part(u.get(), CURLUPART_PASSWORD))
    throw failure(
        AILO_DOWNLOAD_FAILED,
        "Model origins and redirects must use HTTPS without URL credentials");
  return lower(url_part(u.get(), CURLUPART_HOST)) + ":" +
         url_part(u.get(), CURLUPART_PORT, CURLU_DEFAULT_PORT);
}

template <typename Value>
void option(CURL *curl, CURLoption name, Value value) {
  CURLcode code = curl_easy_setopt(curl, name, value);
  if (code == CURLE_OUT_OF_MEMORY)
    throw std::bad_alloc();
  if (code != CURLE_OK)
    throw failure(AILO_DOWNLOAD_FAILED,
                  "Cannot configure verified HTTPS transport");
}

struct transfer {
  CURL *curl;
  int file;
  const plan &settings;
  unsigned shard;
  uint64_t received = 0;
  ailo_status status = AILO_OK;
  const char *message = "";
  void fail(ailo_status code, const char *text) noexcept {
    status = code;
    message = text;
  }
};

size_t write_bytes(char *data, size_t size, size_t count,
                   void *opaque) noexcept {
  auto &t = *static_cast<transfer *>(opaque);
  if (size && count > std::numeric_limits<size_t>::max() / size) {
    t.fail(AILO_DOWNLOAD_FAILED, "Model chunk exceeds its size limit");
    return 0;
  }
  const size_t bytes = size * count;
  long http = 0;
  if (curl_easy_getinfo(t.curl, CURLINFO_RESPONSE_CODE, &http) != CURLE_OK) {
    t.fail(AILO_DOWNLOAD_FAILED, "Cannot inspect model HTTP response");
    return 0;
  }
  if (http != 200)
    return bytes; // Redirect/error bodies never become model data.
  if (t.received > t.settings.max_bytes ||
      bytes > t.settings.max_bytes - t.received) {
    t.fail(AILO_DOWNLOAD_FAILED, "Downloaded model exceeds its size limit");
    return 0;
  }
  size_t offset = 0;
  while (offset < bytes) {
    ssize_t written = write(t.file, data + offset, bytes - offset);
    if (written < 0 && errno == EINTR)
      continue;
    if (written <= 0) {
      t.fail(AILO_IO_FAILED, "Cannot write downloaded model bytes");
      return 0;
    }
    offset += static_cast<size_t>(written);
  }
  t.received += bytes;
  return bytes;
}

int progress(void *opaque, curl_off_t total, curl_off_t received, curl_off_t,
             curl_off_t) noexcept {
  auto &t = *static_cast<transfer *>(opaque);
  if (t.status != AILO_OK)
    return 1;
  if (total > 0 && static_cast<uint64_t>(total) > t.settings.max_bytes) {
    t.fail(AILO_DOWNLOAD_FAILED,
           "Advertised model length exceeds its size limit");
    return 1;
  }
  try {
    notify(t.settings, t.shard,
           received < 0 ? 0 : static_cast<uint64_t>(received),
           total < 0 ? 0 : static_cast<uint64_t>(total));
    return 0;
  } catch (const failure &e) {
    t.fail(e.status, "Model acquisition cancelled");
  } catch (...) {
    t.fail(AILO_INTERNAL_ERROR, "Model progress callback raised an exception");
  }
  return 1;
}

bool retryable(CURLcode code, long http) {
  if (code == CURLE_OPERATION_TIMEDOUT || code == CURLE_COULDNT_CONNECT ||
      code == CURLE_COULDNT_RESOLVE_HOST || code == CURLE_RECV_ERROR ||
      code == CURLE_SEND_ERROR || code == CURLE_PARTIAL_FILE ||
      code == CURLE_GOT_NOTHING)
    return true;
  return code == CURLE_OK &&
         (http == 408 || http == 429 || (http >= 500 && http <= 599));
}

void retry_delay(const plan &p, unsigned shard, unsigned attempt) {
  const unsigned duration = std::min(100U << attempt, 1000U);
  for (unsigned waited = 0; waited < duration; waited += 20) {
    notify(p, shard, 0, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
}
} // namespace

void download(int destination, const file &f, const plan &p, unsigned shard,
              const transport_policy &policy) {
  if (!policy.origin)
    throw failure(AILO_INTERNAL_ERROR, "Model transport origin is required");
  runtime lifetime;
  std::string base = policy.origin;
  while (!base.empty() && base.back() == '/')
    base.pop_back();
  const std::string origin = https_origin(base);
  const std::string initial = base + "/" + encode_path(p.repo, true) +
                              "/resolve/" + encode_path(p.revision, false) +
                              "/" + encode_path(f.name, true) +
                              "?download=true";
  CURLcode last_code = CURLE_OK;
  long last_http = 0;
  for (unsigned attempt = 0; attempt < p.attempts; ++attempt) {
    if (ftruncate(destination, 0) || lseek(destination, 0, SEEK_SET) < 0)
      io_failure("Cannot reset model partial file before an attempt");
    const auto started = std::chrono::steady_clock::now();
    std::string url = initial;
    for (unsigned redirect = 0;; ++redirect) {
      const std::string current_origin = https_origin(url);
      const auto elapsed =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() - started)
              .count();
      if (elapsed >= p.transfer_ms) {
        last_code = CURLE_OPERATION_TIMEDOUT;
        break;
      }
      const long remaining = p.transfer_ms - static_cast<long>(elapsed);
      std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(
          curl_easy_init(), curl_easy_cleanup);
      if (!curl)
        throw std::bad_alloc();
      transfer t = {curl.get(), destination, p, shard};
      option(curl.get(), CURLOPT_URL, url.c_str());
      option(curl.get(), CURLOPT_PROTOCOLS_STR, "https");
      option(curl.get(), CURLOPT_REDIR_PROTOCOLS_STR, "https");
      option(curl.get(), CURLOPT_FOLLOWLOCATION, 0L);
      option(curl.get(), CURLOPT_SSL_VERIFYPEER, 1L);
      option(curl.get(), CURLOPT_SSL_VERIFYHOST, 2L);
      option(curl.get(), CURLOPT_NETRC, static_cast<long>(CURL_NETRC_IGNORED));
      option(curl.get(), CURLOPT_CONNECTTIMEOUT_MS,
             std::min(p.connect_ms, remaining));
      option(curl.get(), CURLOPT_TIMEOUT_MS, remaining);
      option(curl.get(), CURLOPT_NOSIGNAL, 1L);
      option(curl.get(), CURLOPT_BUFFERSIZE, 65536L);
      option(curl.get(), CURLOPT_HTTP_CONTENT_DECODING, 0L);
      option(curl.get(), CURLOPT_MAXFILESIZE_LARGE,
             static_cast<curl_off_t>(std::min(
                 p.max_bytes, static_cast<uint64_t>(
                                  std::numeric_limits<curl_off_t>::max()))));
      option(curl.get(), CURLOPT_WRITEFUNCTION, write_bytes);
      option(curl.get(), CURLOPT_WRITEDATA, &t);
      option(curl.get(), CURLOPT_NOPROGRESS, 0L);
      option(curl.get(), CURLOPT_XFERINFOFUNCTION, progress);
      option(curl.get(), CURLOPT_XFERINFODATA, &t);
      if (policy.ca_file && *policy.ca_file)
        option(curl.get(), CURLOPT_CAINFO, policy.ca_file);
      std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> headers(
          nullptr, curl_slist_free_all);
      if (!p.token.empty() && current_origin == origin) {
        std::string bearer = "Authorization: Bearer " + p.token;
        headers.reset(curl_slist_append(nullptr, bearer.c_str()));
        if (!headers)
          throw std::bad_alloc();
        option(curl.get(), CURLOPT_HTTPHEADER, headers.get());
      }
      last_code = curl_easy_perform(curl.get());
      if (t.status != AILO_OK)
        throw failure(t.status, t.message);
      if (curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &last_http) !=
          CURLE_OK)
        throw failure(AILO_DOWNLOAD_FAILED, "Cannot inspect final HTTP status");
      if (last_code != CURLE_OK)
        break;
      if (last_http >= 300 && last_http <= 399) {
        char *next = nullptr;
        if (redirect == 5 ||
            curl_easy_getinfo(curl.get(), CURLINFO_REDIRECT_URL, &next) !=
                CURLE_OK ||
            !next)
          throw failure(AILO_DOWNLOAD_FAILED,
                        "Model redirect limit exceeded or destination missing");
        url = next; // Resolve relative redirects through libcurl's URL API.
        continue;
      }
      if (last_http == 200) {
        curl_off_t size = -1, advertised = -1;
        if (curl_easy_getinfo(curl.get(), CURLINFO_SIZE_DOWNLOAD_T, &size) !=
                CURLE_OK ||
            curl_easy_getinfo(curl.get(), CURLINFO_CONTENT_LENGTH_DOWNLOAD_T,
                              &advertised) != CURLE_OK ||
            size < 0 || static_cast<uint64_t>(size) != t.received ||
            (advertised >= 0 &&
             static_cast<uint64_t>(advertised) != t.received))
          throw failure(
              AILO_DOWNLOAD_FAILED,
              "Model transfer byte count does not match its HTTP response");
        return;
      }
      break;
    }
    if (!retryable(last_code, last_http) || attempt + 1 == p.attempts)
      break;
    retry_delay(p, shard, attempt);
  }
  if (last_code == CURLE_OUT_OF_MEMORY)
    throw std::bad_alloc();
  if (last_code != CURLE_OK)
    throw failure(AILO_DOWNLOAD_FAILED,
                  std::string("Verified HTTPS model transfer failed: ") +
                      curl_easy_strerror(last_code));
  throw failure(AILO_DOWNLOAD_FAILED, "Model origin returned HTTP status " +
                                          std::to_string(last_http));
}
} // namespace model
} // namespace ailo
