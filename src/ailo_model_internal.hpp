#ifndef AILO_MODEL_INTERNAL_HPP
#define AILO_MODEL_INTERNAL_HPP
#include "ailo/model.h"
#include "ailo_error_internal.h"
#include "ailo_integer_bridge.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace ailo {
namespace model {
struct failure : std::runtime_error {
  ailo_status status;
  failure(ailo_status code, const std::string &text)
      : std::runtime_error(text), status(code) {}
};
[[noreturn]] void io_failure(const char *operation);

struct file {
  std::string name;
  std::string path;
  std::string sha256;
};
struct plan {
  bool local;
  bool allow_download;
  std::string directory;
  std::string lock_name;
  std::string repo;
  std::string revision;
  std::string token;
  std::vector<file> files;
  uint64_t max_bytes;
  long connect_ms;
  long transfer_ms;
  unsigned attempts;
  ailo_model_progress progress;
  void *user_data;
};
plan make_plan(const ailo_model_source *source);
std::string digest_hex(const void *bytes, size_t size);
std::string encode_path(const std::string &value, bool preserve_slashes);
void notify(const plan &p, unsigned shard, uint64_t received, uint64_t total);

class fd {
  int value_;

public:
  explicit fd(int value = -1) : value_(value) {}
  ~fd();
  fd(const fd &) = delete;
  fd &operator=(const fd &) = delete;
  fd(fd &&other) noexcept : value_(other.value_) { other.value_ = -1; }
  fd &operator=(fd &&other) noexcept;
  int get() const { return value_; }
  explicit operator bool() const { return value_ >= 0; }
};
fd directory(const std::string &absolute, bool create);
fd child_directory(int parent, const std::string &relative, bool create);
fd existing_file(int parent, const std::string &name);
void validate_file(int handle, const file &f, const plan &p, unsigned shard);

class publication_lock {
  fd handle_;

public:
  publication_lock(int directory, const plan &p);
};
class partial_file {
  int directory_;
  std::string name_;
  fd handle_;

public:
  partial_file(int directory, const std::string &target);
  ~partial_file();
  partial_file(const partial_file &) = delete;
  partial_file &operator=(const partial_file &) = delete;
  int get() const { return handle_.get(); }
  void publish(const std::string &name);
};

/* Private transport injection for deterministic TLS tests. Both policy values
 * remain private; the exported API always selects Hugging Face and system CA.
 * The injected origin is still required to use verified HTTPS. */
struct transport_policy {
  const char *origin;
  const char *ca_file;
};
void download(int destination, const file &f, const plan &p, unsigned shard,
              const transport_policy &policy);
std::string download_metadata(const plan &p, const transport_policy &policy);
ailo_status acquire(const ailo_model_source *source, char **output,
                    ailo_error *error, const transport_policy &policy);
} // namespace model
} // namespace ailo
#endif
