#include "ailo_model_internal.hpp"
#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <memory>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <thread>
#include <unistd.h>

namespace ailo {
namespace model {
[[noreturn]] void io_failure(const char *operation) {
  throw failure(AILO_IO_FAILED,
                std::string(operation) + ": " + std::strerror(errno));
}

fd::~fd() {
  if (value_ >= 0)
    close(value_);
}
fd &fd::operator=(fd &&other) noexcept {
  if (this != &other) {
    if (value_ >= 0)
      close(value_);
    value_ = other.value_;
    other.value_ = -1;
  }
  return *this;
}

static fd open_child(int parent, const std::string &name, bool create) {
  if (create && mkdirat(parent, name.c_str(), 0700) && errno != EEXIST)
    io_failure("Cannot create model cache directory");
  fd result(openat(parent, name.c_str(),
                   O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW));
  if (!result) {
    if (!create && errno == ENOENT)
      throw failure(AILO_DOWNLOAD_FAILED,
                    "Cached model is missing; allow_download is disabled");
    io_failure("Cannot open a real model cache directory");
  }
  return result;
}

fd child_directory(int parent, const std::string &relative, bool create) {
  fd current(fcntl(parent, F_DUPFD_CLOEXEC, 0));
  if (!current)
    io_failure("Cannot retain model cache directory");
  size_t start = 0;
  while (start < relative.size()) {
    size_t end = relative.find('/', start);
    std::string part = relative.substr(start, end - start);
    if (part.empty() || part == "." || part == "..")
      throw failure(AILO_INVALID_ARGUMENT,
                    "Cache directory components must be normalized");
    current = open_child(current.get(), part, create);
    if (end == std::string::npos)
      break;
    start = end + 1;
  }
  return current;
}

fd directory(const std::string &absolute, bool create) {
  if (absolute.empty() || absolute.front() != '/')
    throw failure(AILO_INVALID_ARGUMENT, "Model cache path must be absolute");
  fd root(open("/", O_RDONLY | O_CLOEXEC | O_DIRECTORY));
  if (!root)
    io_failure("Cannot open filesystem root");
  return child_directory(root.get(), absolute.substr(1), create);
}

fd existing_file(int parent, const std::string &name) {
  fd result(openat(parent, name.c_str(),
                   O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
  if (!result && errno != ENOENT)
    io_failure("Cannot open a regular cached model file");
  return result;
}

void validate_file(int handle, const file &f, const plan &p, unsigned shard) {
  struct stat before = {};
  if (fstat(handle, &before))
    io_failure("Cannot inspect model file");
  if (!S_ISREG(before.st_mode))
    throw failure(AILO_IO_FAILED, "Model must be a regular file");
  if (before.st_size < 24 ||
      static_cast<uint64_t>(before.st_size) > p.max_bytes)
    throw failure(
        AILO_DOWNLOAD_FAILED,
        "Model file is shorter than a GGUF header or exceeds its size limit");
  unsigned char header[24];
  ssize_t got;
  do {
    got = pread(handle, header, sizeof(header), 0);
  } while (got < 0 && errno == EINTR);
  if (got != static_cast<ssize_t>(sizeof(header)) ||
      std::memcmp(header, "GGUF", 4) || (header[4] != 2 && header[4] != 3) ||
      header[5] || header[6] || header[7])
    throw failure(AILO_DOWNLOAD_FAILED,
                  "Model does not contain a supported GGUF header");
  notify(p, shard, 0, static_cast<uint64_t>(before.st_size));
  if (!f.sha256.empty()) {
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> digest(
        EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!digest)
      throw std::bad_alloc();
    if (EVP_DigestInit_ex(digest.get(), EVP_sha256(), nullptr) != 1)
      throw failure(AILO_INTERNAL_ERROR, "Cannot initialize model checksum");
    std::array<unsigned char, 65536> buffer;
    uint64_t offset = 0;
    while (offset < static_cast<uint64_t>(before.st_size)) {
      do {
        got = pread(handle, buffer.data(), buffer.size(),
                    static_cast<off_t>(offset));
      } while (got < 0 && errno == EINTR);
      if (got < 0)
        io_failure("Cannot read model checksum bytes");
      if (!got || static_cast<uint64_t>(got) >
                      static_cast<uint64_t>(before.st_size) - offset)
        throw failure(AILO_IO_FAILED,
                      "Model changed during checksum validation");
      if (EVP_DigestUpdate(digest.get(), buffer.data(),
                           static_cast<size_t>(got)) != 1)
        throw failure(AILO_INTERNAL_ERROR, "Cannot update model checksum");
      offset += static_cast<uint64_t>(got);
      notify(p, shard, offset, static_cast<uint64_t>(before.st_size));
    }
    unsigned char bytes[EVP_MAX_MD_SIZE];
    unsigned size = 0;
    if (EVP_DigestFinal_ex(digest.get(), bytes, &size) != 1 || size != 32)
      throw failure(AILO_INTERNAL_ERROR, "Cannot finalize model checksum");
    const char *hex = "0123456789abcdef";
    std::string actual;
    for (unsigned i = 0; i < size; ++i) {
      actual += hex[bytes[i] >> 4];
      actual += hex[bytes[i] & 15];
    }
    if (actual != f.sha256)
      throw failure(
          AILO_DOWNLOAD_FAILED,
          "Model SHA-256 mismatch; existing files are never replaced");
  }
  struct stat after = {};
  if (fstat(handle, &after))
    io_failure("Cannot recheck model file");
  if (before.st_size != after.st_size ||
      before.st_mtim.tv_sec != after.st_mtim.tv_sec ||
      before.st_mtim.tv_nsec != after.st_mtim.tv_nsec ||
      before.st_ctim.tv_sec != after.st_ctim.tv_sec ||
      before.st_ctim.tv_nsec != after.st_ctim.tv_nsec)
    throw failure(AILO_IO_FAILED, "Model changed during validation");
}

publication_lock::publication_lock(int dir, const plan &p)
    : handle_(openat(dir, p.lock_name.c_str(),
                     O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK,
                     0600)) {
  if (!handle_)
    io_failure("Cannot open model publication lock");
  struct stat state = {};
  if (fstat(handle_.get(), &state))
    io_failure("Cannot inspect model publication lock");
  if (!S_ISREG(state.st_mode))
    throw failure(AILO_IO_FAILED,
                  "Model publication lock must be a regular file");
  const auto started = std::chrono::steady_clock::now();
  while (flock(handle_.get(), LOCK_EX | LOCK_NB)) {
    if (errno != EWOULDBLOCK && errno != EAGAIN && errno != EINTR)
      io_failure("Cannot lock model publication");
    notify(p, 1, 0, 0);
    if (std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started)
            .count() >= p.connect_ms)
      throw failure(AILO_BUSY, "Model publication lock timed out");
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
}

static void remove_interrupted_partials(int dir, const std::string &prefix) {
  int duplicate = openat(dir, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (duplicate < 0)
    io_failure("Cannot inspect interrupted model downloads");
  DIR *raw = fdopendir(duplicate);
  if (!raw) {
    close(duplicate);
    io_failure("Cannot inspect interrupted model downloads");
  }
  auto close_directory = [](DIR *d) { closedir(d); };
  std::unique_ptr<DIR, decltype(close_directory)> entries(raw, close_directory);
  while (true) {
    errno = 0;
    dirent *entry = readdir(entries.get());
    if (!entry) {
      if (errno)
        io_failure("Cannot list interrupted model downloads");
      break;
    }
    const std::string name = entry->d_name;
    if (name.size() != prefix.size() + 32 ||
        name.compare(0, prefix.size(), prefix))
      continue;
    bool hex = true;
    for (size_t i = prefix.size(); i < name.size(); ++i)
      hex &= (name[i] >= '0' && name[i] <= '9') ||
             (name[i] >= 'a' && name[i] <= 'f');
    if (!hex)
      continue;
    struct stat state = {};
    if (fstatat(dir, name.c_str(), &state, AT_SYMLINK_NOFOLLOW)) {
      if (errno == ENOENT)
        continue;
      io_failure("Cannot inspect an interrupted model partial");
    }
    if (!S_ISREG(state.st_mode) || state.st_uid != geteuid())
      throw failure(AILO_IO_FAILED, "Reserved model partial names must be "
                                    "regular files owned by the cache user");
    if (unlinkat(dir, name.c_str(), 0) && errno != ENOENT)
      io_failure("Cannot remove an interrupted model partial");
  }
}

partial_file::partial_file(int dir, const std::string &target)
    : directory_(dir) {
  const std::string prefix =
      ".ailo-partial-" + digest_hex(target.data(), target.size()) + "-";
  // The caller holds the canonical model/shard-set lock. Only reserved partial
  // names for this exact target can be leftovers; other models are untouched.
  remove_interrupted_partials(dir, prefix);
  unsigned char random[16];
  const char *hex = "0123456789abcdef";
  for (unsigned attempt = 0; attempt < 16; ++attempt) {
    if (RAND_bytes(random, sizeof(random)) != 1)
      throw failure(AILO_INTERNAL_ERROR,
                    "Cannot obtain exclusive partial-file identity");
    name_ = prefix;
    for (unsigned char c : random) {
      name_ += hex[c >> 4];
      name_ += hex[c & 15];
    }
    handle_ =
        fd(openat(dir, name_.c_str(),
                  O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600));
    if (handle_)
      return;
    if (errno != EEXIST)
      io_failure("Cannot create exclusive model partial file");
  }
  throw failure(AILO_IO_FAILED, "Cannot reserve a model partial filename");
}
partial_file::~partial_file() {
  if (handle_ && !name_.empty())
    unlinkat(directory_, name_.c_str(), 0);
}

void partial_file::publish(const std::string &name) {
  int result;
  do {
    result = fsync(handle_.get());
  } while (result && errno == EINTR);
  if (result)
    io_failure("Cannot sync downloaded model");
#if defined(SYS_renameat2)
  /* Linux RENAME_NOREPLACE = 1. Unlike plain rename, this cannot clobber a
   * caller-owned file even if one appears outside the advisory lock. */
  if (syscall(SYS_renameat2, directory_, name_.c_str(), directory_,
              name.c_str(), 1U)) {
    if (errno == EEXIST)
      throw failure(AILO_BUSY, "A model destination appeared during transfer; "
                               "existing file preserved");
    if (errno == ENOSYS || errno == EINVAL || errno == EOPNOTSUPP)
      throw failure(AILO_UNSUPPORTED,
                    "Model cache requires atomic no-replace rename support");
    io_failure("Cannot atomically publish downloaded model");
  }
  name_.clear();
  do {
    result = fsync(directory_);
  } while (result && errno == EINTR);
  if (result)
    io_failure("Cannot sync model cache publication");
#else
  (void)name;
  throw failure(AILO_UNSUPPORTED,
                "Model cache requires atomic no-replace rename support");
#endif
}
} // namespace model
} // namespace ailo
