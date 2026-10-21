// Test-only loopback transport shared by simulated and trained fixtures.
#include "ailo/compaction.h"
#include "ailo/handler.h"
#include "serving_fixture_internal.hpp"
#include "serving_repository.hpp"
#include <arpa/inet.h>
#include <charconv>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <map>
#include <netinet/tcp.h>
#include <poll.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

namespace serving_test {
volatile std::sig_atomic_t stopping = 0;
unsigned generations = 0, tool_calls = 0, tool_results = 0, answers = 0;
unsigned summaries = 0, current_request = 0;
std::string directory;
void stop(int) { stopping = 1; }
void require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
void notify(const char *name, unsigned value) {
  const auto path = directory + "/" + name;
  struct stat metadata{};
  if (stat(path.c_str(), &metadata) == 0 && S_ISFIFO(metadata.st_mode)) {
    std::ofstream pipe(path);
    pipe << value << '\n';
    require(bool(pipe), "Fixture notification failed");
  }
}
struct socket_owner {
  int fd;
  ~socket_owner() {
    if (fd >= 0)
      close(fd);
  }
};
struct transport {
  int fd;
  bool gate = false, held = false;
  std::string frame;
  int observe(const void *p, size_t n) {
    if (!gate || held)
      return 1;
    // A bounded test observation AFTER forwarding bytes, never an output spool.
    require(n <= 65536 - frame.size(), "SSE observation exceeds 64 KiB");
    frame.append(static_cast<const char *>(p), n);
    bool delta = false;
    size_t end;
    while ((end = frame.find("\n\n")) != std::string::npos) {
      delta = frame.substr(0, end).find(
                  "event: response.output_text.delta\n") != std::string::npos;
      if (!delta && frame.compare(0, 6, "data: ") == 0 &&
          frame.substr(6, end - 6) != "[DONE]") {
        const auto chunk = json::parse(frame.substr(6, end - 6));
        if (chunk.contains("choices"))
          for (const auto &choice : chunk.at("choices"))
            delta = delta || !choice.at("delta").value("content", "").empty();
      }
      frame.erase(0, end + 2);
      if (delta)
        break;
    }
    if (!delta)
      return 1;
    held = true;
    socket_owner release{open((directory + "/release").c_str(),
                              O_RDONLY | O_NONBLOCK | O_CLOEXEC)};
    require(release.fd >= 0, "Missing transport release FIFO");
    pollfd pending[] = {{release.fd, POLLIN, 0}, {fd, POLLIN, 0}};
    int ready;
    do {
      ready = poll(pending, 2, 30000);
    } while (ready < 0 && errno == EINTR && !stopping);
    require(ready > 0 && !stopping,
            "Transport release/disconnect deadline exceeded");
    if (pending[1].revents) {
      char byte;
      if (recv(fd, &byte, 1, MSG_PEEK | MSG_DONTWAIT) <= 0)
        return 0;
    }
    char command;
    require(read(release.fd, &command, 1) == 1 && command == 'c',
            "Invalid transport release");
    return 1;
  }
};
int send_all(int fd, const void *bytes, size_t size) {
  const char *p = static_cast<const char *>(bytes);
  while (size) {
    const auto n = send(fd, p, size, MSG_NOSIGNAL);
    if (n < 0 && errno == EINTR)
      continue;
    if (n <= 0)
      return 0;
    p += n;
    size -= static_cast<size_t>(n);
  }
  return 1;
}
int begin(void *u, const ailo_http_response *headers) {
  const int fd = static_cast<transport *>(u)->fd;
  std::ostringstream out;
  out << "HTTP/1.1 " << headers->status
      << " Result\r\nContent-Type: " << headers->content_type
      << "\r\nCache-Control: " << headers->cache_control
      << "\r\nX-Request-ID: " << headers->request_id
      << "\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n";
  if (headers->allow)
    out << "Allow: " << headers->allow << "\r\n";
  if (headers->retry_after)
    out << "Retry-After: " << headers->retry_after << "\r\n";
  out << "\r\n";
  const auto bytes = out.str();
  return send_all(fd, bytes.data(), bytes.size());
}
int write(void *u, const void *bytes, size_t size) {
  try {
    auto &connection = *static_cast<transport *>(u);
    const int fd = connection.fd;
    char length[32];
    const auto n = std::snprintf(length, sizeof(length), "%zx\r\n", size);
    return send_all(fd, length, static_cast<size_t>(n)) &&
           send_all(fd, bytes, size) && send_all(fd, "\r\n", 2) &&
           connection.observe(bytes, size);
  } catch (const std::exception &e) {
    std::fprintf(stderr, "transport observer: %s\n", e.what());
    return -1;
  }
}
int finish(void *u) {
  return send_all(static_cast<transport *>(u)->fd, "0\r\n\r\n", 5);
}
void abort_response(void *u) {
  shutdown(static_cast<transport *>(u)->fd, SHUT_RDWR);
}
std::string lower(std::string value) {
  for (auto &c : value)
    if (c >= 'A' && c <= 'Z')
      c += 'a' - 'A';
  return value;
}
std::string receive(int fd, size_t maximum) {
  char bytes[4096];
  const auto n = recv(fd, bytes, sizeof(bytes), 0);
  require(n > 0 && static_cast<size_t>(n) <= maximum,
          "Invalid fixture request size/read");
  return {bytes, static_cast<size_t>(n)};
}
void request(int fd, const char *model, unsigned number) {
  current_request = number;
  std::ofstream(directory + "/sequence") << number << '\n';
  constexpr size_t limit = 8 * 1024 * 1024;
  std::string input;
  while (input.find("\r\n\r\n") == std::string::npos) {
    input += receive(fd, 32768 - input.size());
    require(input.size() < 32768, "Fixture request headers exceed 32 KiB");
  }
  const auto boundary = input.find("\r\n\r\n") + 4;
  std::istringstream lines(input.substr(0, boundary));
  std::string line, method, target, version;
  std::getline(lines, line);
  std::istringstream first(line);
  first >> method >> target >> version;
  require(version == "HTTP/1.1", "Fixture requires HTTP/1.1");
  std::map<std::string, std::string> headers;
  while (std::getline(lines, line) && line != "\r") {
    const auto colon = line.find(':');
    require(colon != std::string::npos, "Invalid fixture header");
    auto value = line.substr(colon + 1);
    if (!value.empty() && value.back() == '\r')
      value.pop_back();
    const auto start = value.find_first_not_of(" \t");
    value = start == std::string::npos ? "" : value.substr(start);
    require(headers.emplace(lower(line.substr(0, colon)), value).second,
            "Duplicate fixture header");
  }
  require(!headers.count("transfer-encoding"),
          "Fixture expects a fixed-length request body");
  size_t length = 0;
  if (headers.count("content-length")) {
    const auto &text = headers.at("content-length");
    const auto parsed =
        std::from_chars(text.data(), text.data() + text.size(), length);
    require(parsed.ec == std::errc() &&
                parsed.ptr == text.data() + text.size() && length <= limit,
            "Invalid fixture Content-Length");
  }
  while (input.size() - boundary < length)
    input += receive(fd, limit + 32768 - input.size());
  require(input.size() - boundary == length,
          "Fixture rejects pipelined/trailing request bytes");
  // Request inputs are explicitly materialized. Generated output is never
  // captured or spooled here; the transport forwards each borrowed byte range.
  std::ofstream saved(directory + "/request-" + std::to_string(number) +
                      ".json");
  saved.write(input.data() + boundary, static_cast<std::streamsize>(length));
  saved.close();
  const auto control = serving_test::load(directory + "/control.json");
  serving_test::repository repository(directory, control.value("fault", ""));
  auto config = ailo_client_default_config();
  config.source.local_path = model;
  config.model_id = "ailo-fixture";
  config.lazy = 1;
  config.backend = AILO_BACKEND_CPU;
  config.context_tokens = 32768;
  config.threads = 1;
  auto api = ailo_openai_default_config();
  const auto ns = control.value("namespace", "tenant-A");
  api.namespace_id = ns.c_str();
  const auto pouch_root = directory + "/pouch";
  const auto pouch_key = directory + "/pouch-config/pouch.key";
  if (control.value("pouch", false)) {
    api.state_directory = pouch_root.c_str();
    api.encryption_key_file = pouch_key.c_str();
  } else
    api.repository = &repository.api;
  auto policy = ailo_compaction_default_policy();
  policy.target_tokens = control.value("target_tokens", 9000u);
  policy.recent_turns = control.value("recent_turns", 0u);
  policy.summary_tokens = control.value("summary_tokens", 512u);
  policy.summary_context_tokens = 32768;
  api.compaction = &policy;
  config.openai = &api;
  const std::string key(32,
                        'k'); // Public test key, never used outside fixtures.
  auto keys = ailo_compactor_default_config();
  keys.namespace_id = "ailo-serving-tests";
  keys.current_key = {"fixture", key.data(), key.size()};
  ailo_compactor *owner = nullptr;
  ailo_error error{};
  require(ailo_compactor_new(&keys, &owner, &error) == AILO_OK, error.message);
  std::unique_ptr<ailo_compactor, decltype(&ailo_compactor_destroy)> compactor(
      owner, ailo_compactor_destroy);
  config.compactor = owner;
  configure_model(config);
  ailo_client *raw = nullptr;
  require(ailo_client_new(&config, &raw, &error) == AILO_OK, error.message);
  std::unique_ptr<ailo_client, decltype(&ailo_client_destroy)> client(
      raw, ailo_client_destroy);
  auto handler_config = ailo_handler_default_config();
  handler_config.client = raw;
  ailo_handler *serving = nullptr;
  require(ailo_handler_new(&handler_config, &serving, &error) == AILO_OK,
          error.message);
  std::unique_ptr<ailo_handler, decltype(&ailo_handler_destroy)> handler(
      serving, ailo_handler_destroy);
  ailo_http_request incoming{};
  incoming.method = method.c_str();
  incoming.target = target.c_str();
  incoming.body = input.data() + boundary;
  incoming.body_size = length;
  incoming.content_type = headers.count("content-type")
                              ? headers.at("content-type").c_str()
                              : nullptr;
  incoming.content_encoding = headers.count("content-encoding")
                                  ? headers.at("content-encoding").c_str()
                                  : nullptr;
  transport connection{fd, control.value("hold_transport", false), false, {}};
  ailo_http_transport outgoing{&connection, begin, write, finish,
                               abort_response};
  ailo_handler_info info{};
  if (method == "POST" && target == "/v1/responses" &&
      control.value("inspect_compaction", false))
    inspect_model_input(raw, std::string(input.data() + boundary, length));
  const auto status =
      serving->handle(serving, &incoming, &outgoing, &info, &error);
  auto receipt = repository.stats;
  receipt["target"] = target;
  receipt["method"] = method;
  receipt["status"] = info.http_status;
  receipt["result"] = ailo_status_name(status);
  receipt["stored"] = bool(info.api.stored);
  receipt["id"] = info.api.id;
  receipt["generations"] = generations;
  receipt["summaries"] = summaries;
  receipt["completed"] = bool(info.completed);
  receipt["prompt_tokens"] =
      serving_test::integer(info.api.run.usage.prompt_tokens);
  receipt["completion_tokens"] =
      serving_test::integer(info.api.run.usage.completion_tokens);
  receipt["compaction_passes"] = info.api.run.usage.compaction_passes;
  observe_model(raw, receipt);
  serving_test::save(directory + "/receipt-" + std::to_string(number) + ".json",
                     receipt);
  serving_test::save(directory + "/latest.json", receipt);
  notify("receipts", number);
  std::fprintf(stderr,
               "request=%u method=%s target=%s status=%d result=%s "
               "completed=%d bytes=%u diagnostic=%s\n",
               number, method.c_str(), target.c_str(), info.http_status,
               ailo_status_name(status), info.completed, info.bytes_written.lo,
               error.message);
}
} // namespace serving_test

int main(int argc, char **argv) {
  using namespace serving_test;
  try {
    require(argc == 3, "Usage: ailo_serving_fixture MODEL WORK_DIRECTORY");
    directory = argv[2];
    socket_owner listener{socket(AF_INET, SOCK_STREAM, 0)};
    require(listener.fd >= 0, "Fixture socket failed");
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    require(bind(listener.fd, reinterpret_cast<sockaddr *>(&address),
                 sizeof(address)) == 0,
            "Fixture bind failed");
    socklen_t length = sizeof(address);
    require(getsockname(listener.fd, reinterpret_cast<sockaddr *>(&address),
                        &length) == 0 &&
                listen(listener.fd, 4) == 0,
            "Fixture listen failed");
    std::signal(SIGTERM, stop);
    std::signal(SIGINT, stop);
    {
      std::ofstream ready(directory + "/port");
      ready << ntohs(address.sin_port) << '\n';
    }
    std::ofstream(directory + "/pid") << getpid() << '\n';
    notify("ready", ntohs(address.sin_port));
    unsigned count = 0;
    std::ifstream(directory + "/sequence") >> count;
    while (!stopping && count < 256) {
      pollfd pending{listener.fd, POLLIN, 0};
      if (poll(&pending, 1, 250) <= 0)
        continue;
      socket_owner peer{accept(listener.fd, nullptr, nullptr)};
      require(peer.fd >= 0, "Fixture accept failed");
      const int enabled = 1;
      const timeval timeout{10, 0};
      setsockopt(peer.fd, IPPROTO_TCP, TCP_NODELAY, &enabled, sizeof(enabled));
      setsockopt(peer.fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
      setsockopt(peer.fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
      request(peer.fd, argv[1], ++count);
    }
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "serving fixture: %s\n", e.what());
    return 1;
  }
}
