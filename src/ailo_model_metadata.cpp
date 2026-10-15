#include "ailo_bounded_json_internal.hpp"
#include "ailo_json_stream_internal.hpp"
#include "ailo_model_metadata_internal.hpp"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <curl/curl.h>
#include <set>
#include <sys/stat.h>
#include <unistd.h>

namespace ailo {
namespace client {
namespace {
constexpr size_t metadata_limit = 1024 * 1024;
using path = std::vector<std::string>;
enum class input_kind { caller, publisher, cache };
enum class value_kind { object, array, string, other, null };
[[noreturn]] void invalid() {
  throw failure(AILO_INVALID_ARGUMENT, "Invalid supplemental model metadata");
}
bool repo_id(const std::string &text) {
  const auto slash = text.find('/');
  return text.size() <= 256 && slash != std::string::npos && slash > 0 &&
         slash + 1 < text.size() &&
         text.find('/', slash + 1) == std::string::npos &&
         text.find("..") == std::string::npos &&
         std::all_of(text.begin(), text.end(), [](unsigned char c) {
           return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '/' || c == '-' || c == '_' ||
                  c == '.';
         });
}
void checked_text(const std::string &text) {
  require_utf8(text.data(), text.size());
  if (text.size() > 4096 ||
      std::any_of(text.begin(), text.end(),
                  [](unsigned char c) { return c < 32 || c == 127; }))
    invalid();
}
void checked_url(const std::string &text) {
  if (text.empty())
    return;
  checked_text(text);
  // Publisher links are public references, never local paths, credentials or
  // signed download URLs. Queries/fragments are not part of this claim profile.
  if (text.compare(0, 8, "https://") || text.size() <= 8 ||
      text.find_first_of("@?#\\ ") != std::string::npos)
    invalid();
  std::unique_ptr<CURLU, decltype(&curl_url_cleanup)> url(curl_url(),
                                                          curl_url_cleanup);
  if (!url)
    throw std::bad_alloc();
  const auto status = curl_url_set(url.get(), CURLUPART_URL, text.c_str(), 0);
  if (status == CURLUE_OUT_OF_MEMORY)
    throw std::bad_alloc();
  if (status != CURLUE_OK)
    invalid();
}
struct decoder {
  input_kind mode;
  metadata_claims claims;
  std::string repo, revision, identity, scalar, key;
  std::vector<std::set<std::string>> keys;
  size_t members = 0;
  static path location(const lonejson_value_path *p) {
    path result;
    for (size_t n = 0; n < p->segment_count; ++n)
      result.emplace_back(p->segments[n].data, p->segments[n].len);
    return result;
  }
  void value(path p, value_kind kind, const std::string &text = {}) {
    if (p.empty()) {
      if (kind != value_kind::object)
        invalid();
      return;
    }
    if (mode == input_kind::publisher) {
      if (p == path{"id"} || p == path{"sha"}) {
        if (kind != value_kind::string)
          invalid();
        (p[0] == "id" ? repo : revision) = text;
        return;
      }
      if (p[0] != "cardData")
        return;
      p.erase(p.begin());
      if (p.empty()) {
        if (kind != value_kind::object && kind != value_kind::null)
          invalid();
        return;
      }
      if (p[0] == "model_name")
        p[0] = "name";
      else if (p[0] == "license")
        p.insert(p.begin() + 1, "id");
      else if (p[0] == "license_name")
        p = {"license", "name"};
      else if (p[0] == "license_link")
        p = {"license", "url"};
      else if (p[0] == "base_model") {
        p[0] = "base_models";
        p.insert(p.begin(), "source");
      } else
        return;
    }
    if (kind == value_kind::null)
      return;
    if (mode == input_kind::cache && p.size() == 1 &&
        (p[0] == "repo" || p[0] == "revision" || p[0] == "artifact_identity")) {
      if (kind != value_kind::string)
        invalid();
      (p[0] == "repo" ? repo : p[0] == "revision" ? revision : identity) = text;
      return;
    }
    std::string *target = nullptr;
    if (p == path{"name"})
      target = &claims.name;
    else if (p == path{"license", "id"})
      target = &claims.license_id;
    else if (p == path{"license", "name"})
      target = &claims.license_name;
    else if (p == path{"license", "url"})
      target = &claims.license_url;
    if (target) {
      if (kind != value_kind::string)
        invalid();
      checked_text(text);
      if (target == &claims.license_url)
        checked_url(text);
      *target = text;
      return;
    }
    if (p == path{"license"} || p == path{"source"}) {
      if (kind != value_kind::object)
        invalid();
      return;
    }
    if (p == path{"source", "base_models"}) {
      if (kind == value_kind::string && mode == input_kind::publisher) {
        if (!repo_id(text))
          invalid();
        claims.base_models.push_back(text);
      } else if (kind != value_kind::array)
        invalid();
      claims.lineage_known = true;
      return;
    }
    if (p.size() == 3 && p[0] == "source" && p[1] == "base_models") {
      if (kind != value_kind::string || !repo_id(text) ||
          claims.base_models.size() >= 64)
        invalid();
      if (std::find(claims.base_models.begin(), claims.base_models.end(),
                    text) != claims.base_models.end())
        invalid();
      claims.base_models.push_back(text);
      return;
    }
    invalid();
  }
  template <value_kind K>
  static lonejson_status begin(void *u, const lonejson_value_path *p,
                               lonejson_error *) {
    auto &d = *static_cast<decoder *>(u);
    if (K == value_kind::object)
      d.keys.emplace_back();
    if (K == value_kind::string)
      d.scalar.clear();
    else
      d.value(location(p), K);
    return LONEJSON_STATUS_OK;
  }
  static lonejson_status object_end(void *u, const lonejson_value_path *,
                                    lonejson_error *) {
    static_cast<decoder *>(u)->keys.pop_back();
    return LONEJSON_STATUS_OK;
  }
  static lonejson_status chunk(void *u, const lonejson_value_path *,
                               const char *s, size_t n, lonejson_error *) {
    static_cast<decoder *>(u)->scalar.append(s, n);
    return LONEJSON_STATUS_OK;
  }
  static lonejson_status string_end(void *u, const lonejson_value_path *p,
                                    lonejson_error *) {
    auto &d = *static_cast<decoder *>(u);
    d.value(location(p), value_kind::string, d.scalar);
    return LONEJSON_STATUS_OK;
  }
  static lonejson_status boolean(void *u, const lonejson_value_path *p, int,
                                 lonejson_error *e) {
    return begin<value_kind::other>(u, p, e);
  }
  static lonejson_status key_begin(void *u, const lonejson_value_path *,
                                   lonejson_error *) {
    static_cast<decoder *>(u)->key.clear();
    return LONEJSON_STATUS_OK;
  }
  static lonejson_status key_chunk(void *u, const lonejson_value_path *,
                                   const char *s, size_t n, lonejson_error *) {
    static_cast<decoder *>(u)->key.append(s, n);
    return LONEJSON_STATUS_OK;
  }
  static lonejson_status key_end(void *u, const lonejson_value_path *p,
                                 lonejson_error *) {
    auto &d = *static_cast<decoder *>(u);
    if (++d.members > 32768 || !d.keys.back().insert(d.key).second)
      invalid();
    if (d.mode != input_kind::publisher) {
      const auto where = location(p);
      const bool root = where.empty();
      if (root && d.key != "name" && d.key != "license" && d.key != "source" &&
          !(d.mode == input_kind::cache &&
            (d.key == "repo" || d.key == "revision" ||
             d.key == "artifact_identity")))
        invalid();
      if (where == path{"license"} && d.key != "id" && d.key != "name" &&
          d.key != "url")
        invalid();
      if (where == path{"source"} && d.key != "base_models")
        invalid();
    }
    return LONEJSON_STATUS_OK;
  }
  decoder(const std::string &text, input_kind input) : mode(input) {
    if (text.size() > metadata_limit)
      throw failure(AILO_OUT_OF_RANGE, "Model metadata input exceeds 1 MiB");
    require_utf8(text.data(), text.size());
    lonejson_path_value_visitor v{};
    v.object_begin = begin<value_kind::object>;
    v.object_end = object_end;
    v.array_begin = begin<value_kind::array>;
    v.string_begin = begin<value_kind::string>;
    v.string_chunk = chunk;
    v.string_end = string_end;
    v.number_begin = begin<value_kind::other>;
    v.boolean_value = boolean;
    v.null_value = begin<value_kind::null>;
    v.object_key_begin = key_begin;
    v.object_key_chunk = key_chunk;
    v.object_key_end = key_end;
    try {
      json_stream parser(v, this, metadata_limit);
      parser.push(text.data(), text.size());
      parser.finish();
    } catch (const failure &e) {
      if (e.status == AILO_MODEL_FAILED)
        invalid();
      throw;
    }
  }
};
std::string read_metadata(int file) {
  struct stat status{};
  if (fstat(file, &status) || !S_ISREG(status.st_mode) || status.st_size < 0 ||
      static_cast<uint64_t>(status.st_size) > metadata_limit)
    invalid();
  std::string text(static_cast<size_t>(status.st_size), '\0');
  size_t offset = 0;
  while (offset < text.size()) {
    const auto n = pread(file, &text[offset], text.size() - offset,
                         static_cast<off_t>(offset));
    if (n < 0 && errno == EINTR)
      continue;
    if (n <= 0)
      invalid();
    offset += static_cast<size_t>(n);
  }
  return text;
}
void write_metadata(int file, const std::string &text) {
  if (ftruncate(file, 0) || lseek(file, 0, SEEK_SET) < 0)
    model::io_failure("Cannot reset publisher metadata partial");
  size_t offset = 0;
  while (offset < text.size()) {
    const auto n = write(file, text.data() + offset, text.size() - offset);
    if (n < 0 && errno == EINTR)
      continue;
    if (n <= 0)
      model::io_failure("Cannot write publisher metadata");
    offset += static_cast<size_t>(n);
  }
}
} // namespace

bool immutable_revision(const std::string &revision) {
  return revision.size() == 40 &&
         std::all_of(revision.begin(), revision.end(), [](char c) {
           return (c >= 'a' && c <= 'f') || (c >= '0' && c <= '9');
         });
}
metadata_claims parse_supplemental_metadata(const char *text) {
  if (!text)
    return {};
  const auto size = strnlen(text, metadata_limit + 1);
  if (size > metadata_limit)
    throw failure(AILO_OUT_OF_RANGE,
                  "Supplemental model metadata exceeds 1 MiB");
  return decoder(std::string(text, size), input_kind::caller).claims;
}
void write_claims(bounded_json &out, const metadata_claims &claims) {
  out.key("name");
  out.nullable(claims.name);
  out.key("license");
  out.object();
  out.key("id");
  out.nullable(claims.license_id);
  out.key("name");
  out.nullable(claims.license_name);
  out.key("url");
  out.nullable(claims.license_url);
  out.end_object();
  out.key("source");
  out.object();
  out.key("base_models");
  if (!claims.lineage_known)
    out.null();
  else {
    out.array();
    for (const auto &id : claims.base_models)
      out.string(id);
    out.end_array();
  }
  out.end_object();
}
publisher_metadata
load_publisher_metadata(const ailo_model_source &source,
                        const std::string &acquired_path,
                        const model::transport_policy &policy) {
  publisher_metadata result;
  if (source.local_path && *source.local_path) {
    result.status = "local";
    return result;
  }
  if (!immutable_revision(source.revision ? source.revision : "")) {
    result.status = "unresolved_revision";
    return result;
  }
  try {
    auto bound_source = source;
    // Planning names/checksums must not re-read a possibly changed cache
    // environment. The already acquired absolute path supplies the directory.
    bound_source.cache_root = "/";
    auto p = model::make_plan(&bound_source);
    // Bind to the acquired path, not a later cwd/XDG environment value.
    const std::string filename = source.filename;
    if (acquired_path.size() <= filename.size() ||
        acquired_path[acquired_path.size() - filename.size() - 1] != '/' ||
        acquired_path.compare(acquired_path.size() - filename.size(),
                              filename.size(), filename))
      invalid();
    p.directory =
        acquired_path.substr(0, acquired_path.size() - filename.size() - 1);
    std::string identity = p.repo + '\0' + p.revision;
    for (const auto &f : p.files)
      identity += '\0' + f.name + '\0' + f.sha256;
    identity = model::digest_hex(identity.data(), identity.size());
    const std::string name = ".ailo-metadata-" + identity + ".json";
    auto directory = model::directory(p.directory, false);
    std::unique_ptr<model::publication_lock> lock;
    if (p.allow_download)
      lock = std::make_unique<model::publication_lock>(directory.get(), p);
    auto existing = model::existing_file(directory.get(), name);
    if (existing) {
      decoder cached(read_metadata(existing.get()), input_kind::cache);
      if (cached.repo != p.repo || cached.revision != p.revision ||
          cached.identity != identity)
        invalid();
      result.claims = std::move(cached.claims);
      result.status = "cached";
      return result;
    }
    if (!p.allow_download) {
      result.status = "offline_missing";
      return result;
    }
    p.max_bytes = metadata_limit;
    p.transfer_ms = std::min(p.transfer_ms, 15000L);
    p.attempts = 1;
    decoder remote(model::download_metadata(p, policy), input_kind::publisher);
    if (remote.repo != p.repo || remote.revision != p.revision)
      invalid();
    bounded_json out(metadata_limit);
    out.object();
    out.key("repo");
    out.string(p.repo);
    out.key("revision");
    out.string(p.revision);
    out.key("artifact_identity");
    out.string(identity);
    write_claims(out, remote.claims);
    out.end_object();
    model::partial_file partial(directory.get(), name);
    write_metadata(partial.get(), out.finish());
    partial.publish(name);
    result.claims = std::move(remote.claims);
    result.status = "fetched";
  } catch (const model::failure &e) {
    if (e.status == AILO_CANCELLED)
      throw failure(e.status, "Publisher metadata acquisition cancelled");
    if (e.status == AILO_NO_MEMORY || e.status == AILO_INTERNAL_ERROR)
      throw failure(e.status, "Publisher metadata callback failed");
    result.status = "unavailable";
  } catch (const failure &e) {
    if (e.status == AILO_NO_MEMORY || e.status == AILO_CANCELLED)
      throw;
    result.status = "invalid";
  }
  return result;
}
} // namespace client
} // namespace ailo
