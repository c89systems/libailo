#include "ailo_model_internal.hpp"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <openssl/evp.h>
#include <unistd.h>

namespace ailo {
namespace model {
std::string digest_hex(const void *bytes, size_t size) {
  unsigned char result[EVP_MAX_MD_SIZE];
  unsigned length = 0;
  if (EVP_Digest(bytes, size, result, &length, EVP_sha256(), nullptr) != 1 ||
      length != 32)
    throw failure(AILO_INTERNAL_ERROR, "Cannot compute model identity SHA-256");
  const char *hex = "0123456789abcdef";
  std::string text;
  text.reserve(64);
  for (unsigned i = 0; i < length; ++i) {
    text += hex[result[i] >> 4];
    text += hex[result[i] & 15];
  }
  return text;
}

static bool set(const char *text) { return text && *text; }
static void relative(const std::string &text, const char *what) {
  if (text.empty() || text.front() == '/' || text.back() == '/')
    throw failure(AILO_INVALID_ARGUMENT,
                  std::string(what) + " must be relative and nonempty");
  size_t start = 0;
  do {
    size_t end = text.find('/', start);
    std::string part = text.substr(start, end - start);
    if (part.empty() || part == "." || part == "..")
      throw failure(AILO_INVALID_ARGUMENT,
                    std::string(what) +
                        " contains traversal or empty components");
    for (unsigned char ch : part)
      if (ch < 32 || ch == 127 || ch == '\\')
        throw failure(AILO_INVALID_ARGUMENT,
                      std::string(what) + " contains invalid characters");
    if (end == std::string::npos)
      break;
    start = end + 1;
  } while (true);
}

static std::string absolute(const std::string &path) {
  if (path.empty() || path.front() != '/')
    throw failure(AILO_INVALID_ARGUMENT,
                  "Model cache environment/override must be an absolute path");
  std::string normalized =
      std::filesystem::path(path).lexically_normal().string();
  while (normalized.size() > 1 && normalized.back() == '/')
    normalized.pop_back();
  return normalized;
}

static std::string cache_root(const ailo_model_source &s) {
  if (set(s.cache_root))
    return absolute(s.cache_root);
  const char *xdg = std::getenv("XDG_CACHE_HOME");
  if (set(xdg))
    return absolute(xdg) + "/ailo";
  const char *home = std::getenv("HOME");
  if (!set(home))
    throw failure(AILO_INVALID_ARGUMENT,
                  "Set an absolute cache_root, XDG_CACHE_HOME or HOME");
  return absolute(home) + "/.cache/ailo";
}

static unsigned split_count(const std::string &name) {
  size_t slash = name.find_last_of('/');
  size_t start = slash == std::string::npos ? 0 : slash + 1;
  if (name.size() - start <= 20)
    throw failure(AILO_INVALID_ARGUMENT,
                  "Split primary must be named *-00001-of-NNNNN.gguf");
  start = name.size() - 20;
  if (name.compare(start, 10, "-00001-of-") ||
      name.compare(start + 15, 5, ".gguf"))
    throw failure(AILO_INVALID_ARGUMENT,
                  "Split acquisition requires the first GGUF shard");
  unsigned result = 0;
  for (size_t i = start + 10; i < start + 15; ++i) {
    if (name[i] < '0' || name[i] > '9')
      throw failure(AILO_INVALID_ARGUMENT,
                    "Split shard count must be five decimal digits");
    result = result * 10 + static_cast<unsigned>(name[i] - '0');
  }
  if (!result)
    throw failure(AILO_INVALID_ARGUMENT, "Split shard count must be positive");
  return result;
}

static std::string publication_identity(std::string name) {
  if (name.size() > 20) {
    const size_t start = name.size() - 20;
    if (name[start] == '-' && name.compare(start + 6, 4, "-of-") == 0 &&
        name.compare(start + 15, 5, ".gguf") == 0) {
      unsigned index = 0, count = 0;
      bool digits = true;
      for (size_t i = 0; i < 5; ++i) {
        char a = name[start + 1 + i], b = name[start + 10 + i];
        if (a < '0' || a > '9' || b < '0' || b > '9') {
          digits = false;
          break;
        }
        index = index * 10 + static_cast<unsigned>(a - '0');
        count = count * 10 + static_cast<unsigned>(b - '0');
      }
      if (digits && index && index <= count)
        name.replace(start + 1, 5, "00001");
    }
  }
  return name;
}

static std::string checksum(const char *value) {
  if (!value)
    return {};
  if (std::strlen(value) != 64)
    throw failure(AILO_INVALID_ARGUMENT,
                  "Model SHA-256 must contain 64 hexadecimal characters");
  std::string result;
  for (size_t i = 0; i < 64; ++i) {
    char c = value[i];
    if (c >= 'A' && c <= 'F')
      c = static_cast<char>(c - 'A' + 'a');
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
      throw failure(AILO_INVALID_ARGUMENT,
                    "Model SHA-256 must contain hexadecimal characters");
    result += c;
  }
  return result;
}

std::string encode_path(const std::string &value, bool preserve_slashes) {
  const char *hex = "0123456789ABCDEF";
  std::string result;
  for (unsigned char c : value) {
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
        c == '~' || (preserve_slashes && c == '/')) {
      result += static_cast<char>(c);
    } else {
      result += '%';
      result += hex[c >> 4];
      result += hex[c & 15];
    }
  }
  return result;
}

plan make_plan(const ailo_model_source *source) {
  if (!source)
    throw failure(AILO_INVALID_ARGUMENT, "A model source is required");
  const auto &s = *source;
  plan p = {};
  p.local = set(s.local_path);
  p.allow_download = s.allow_download != 0;
  p.connect_ms = s.connect_timeout_ms ? s.connect_timeout_ms : 15000;
  p.transfer_ms = s.transfer_timeout_ms ? s.transfer_timeout_ms : 3600000;
  p.attempts = s.max_attempts ? s.max_attempts : 3;
  p.max_bytes = ailo::detail::to_native(s.max_bytes);
  if (!p.max_bytes)
    p.max_bytes = uint64_t(32) * 1024 * 1024 * 1024;
  if (p.connect_ms <= 0 || p.transfer_ms <= 0 || p.attempts > 6 ||
      p.max_bytes < 24)
    throw failure(AILO_INVALID_ARGUMENT,
                  "Model limits require positive timeouts, 1..6 attempts and "
                  "at least 24 bytes");
  p.progress = s.progress;
  p.user_data = s.user_data;
  if (s.bearer_token) {
    p.token = s.bearer_token;
    for (unsigned char c : p.token)
      if (c < 32 || c == 127)
        throw failure(AILO_INVALID_ARGUMENT,
                      "Bearer token contains control characters");
  }
  std::string primary;
  if (p.local) {
    if (set(s.repo) || set(s.revision) || set(s.filename))
      throw failure(AILO_INVALID_ARGUMENT,
                    "Choose local_path or Hugging Face fields, not both");
    primary = s.local_path;
    if (primary.front() != '/') {
      std::unique_ptr<char, decltype(&std::free)> cwd(getcwd(nullptr, 0),
                                                      std::free);
      if (!cwd)
        io_failure("Cannot resolve local model cwd");
      primary = std::string(cwd.get()) + "/" + primary;
    }
    primary = std::filesystem::path(primary).lexically_normal().string();
  } else {
    if (!set(s.repo) || !set(s.revision) || !set(s.filename))
      throw failure(AILO_INVALID_ARGUMENT,
                    "Hugging Face repo, revision and filename are required");
    p.repo = s.repo;
    p.revision = s.revision;
    primary = s.filename;
    relative(p.repo, "Repository");
    relative(p.revision, "Revision");
    relative(primary, "Filename");
    size_t component = 0;
    while (component < primary.size()) {
      if (primary.compare(component, 6, ".ailo-") == 0)
        throw failure(AILO_INVALID_ARGUMENT,
                      "Filename components beginning .ailo- are reserved for "
                      "model cache state");
      size_t end = primary.find('/', component);
      if (end == std::string::npos)
        break;
      component = end + 1;
    }
    size_t slash = p.repo.find('/');
    if (slash == std::string::npos ||
        p.repo.find('/', slash + 1) != std::string::npos)
      throw failure(AILO_INVALID_ARGUMENT, "Repository must be namespace/name");
    for (unsigned char c : p.repo)
      if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
            c == '/'))
        throw failure(AILO_INVALID_ARGUMENT,
                      "Repository contains invalid characters");
    std::string identity = p.repo;
    identity += '\0';
    identity += p.revision;
    p.directory = absolute(cache_root(s) + "/models/" +
                           digest_hex(identity.data(), identity.size()));
    const std::string lock_identity = publication_identity(primary);
    p.lock_name = ".ailo-" +
                  digest_hex(lock_identity.data(), lock_identity.size()) +
                  ".lock";
  }
  unsigned count = s.split_shards ? split_count(primary) : 1;
  if ((s.sha256_count && (!s.sha256 || s.sha256_count != count)) ||
      (!s.sha256_count && s.sha256))
    throw failure(AILO_INVALID_ARGUMENT,
                  "Checksum array must match the requested shard count");
  for (unsigned i = 1; i <= count; ++i) {
    std::string name = primary;
    if (s.split_shards) {
      char index[16];
      std::snprintf(index, sizeof(index), "%05u", i);
      name.replace(name.size() - 19, 5, index);
    }
    std::string sha =
        s.sha256_count ? checksum(s.sha256[i - 1]) : std::string{};
    p.files.push_back({name, p.local ? name : p.directory + "/" + name, sha});
  }
  return p;
}

void notify(const plan &p, unsigned shard, uint64_t received, uint64_t total) {
  if (p.progress &&
      !p.progress(p.user_data, shard, static_cast<unsigned>(p.files.size()),
                  ailo::detail::from_unsigned(received),
                  ailo::detail::from_unsigned(total)))
    throw failure(AILO_CANCELLED, "Model acquisition cancelled");
}
} // namespace model
} // namespace ailo
