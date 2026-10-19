#include "ailo_snapshot_internal.hpp"
#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace ailo {
namespace client {
namespace {
using digest = std::array<unsigned char, 32>;
constexpr size_t media_metadata_bytes = 80;
[[noreturn]] void io(const char *message) {
  throw failure(AILO_IO_FAILED,
                std::string(message) + ": " + std::strerror(errno));
}
[[noreturn]] void
bad(const char *message =
        "Snapshot authentication or envelope validation failed") {
  throw failure(AILO_INVALID_ARGUMENT, message);
}
void crypto(bool good) {
  if (!good)
    throw failure(AILO_INTERNAL_ERROR,
                  "Snapshot cryptographic operation failed");
}
uint64_t now() {
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::seconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count());
}
digest sha(const void *data, size_t size) {
  digest out{};
  unsigned count = 0;
  crypto(EVP_Digest(data, size, out.data(), &count, EVP_sha256(), nullptr) ==
             1 &&
         count == out.size());
  return out;
}
digest sha(const std::string &text) { return sha(text.data(), text.size()); }
std::string hex(const void *data, size_t size) {
  const auto *bytes = static_cast<const unsigned char *>(data);
  std::string out;
  for (size_t i = 0; i < size; ++i) {
    out += "0123456789abcdef"[bytes[i] >> 4];
    out += "0123456789abcdef"[bytes[i] & 15];
  }
  return out;
}
void put(unsigned char *data, uint64_t value) {
  for (unsigned i = 0; i < 8; ++i) {
    data[i] = static_cast<unsigned char>(value);
    value >>= 8;
  }
}
uint64_t get(const unsigned char *data) {
  uint64_t value = 0;
  for (unsigned i = 0; i < 8; ++i)
    value |= uint64_t(data[i]) << (8 * i);
  return value;
}
void private_file(const struct stat &st) {
  if (!S_ISREG(st.st_mode) || st.st_uid != geteuid() || (st.st_mode & 077) ||
      st.st_nlink != 1)
    throw failure(AILO_IO_FAILED,
                  "Snapshot files must be private regular files owned by this "
                  "user, without hard links");
}
model::fd open_file(int dir, const std::string &name) {
  model::fd file(openat(dir, name.c_str(),
                        O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
  if (!file)
    io("Cannot open snapshot");
  struct stat st{};
  if (fstat(file.get(), &st))
    io("Cannot inspect snapshot");
  private_file(st);
  return file;
}
void read_at(int fd, void *data, size_t size, size_t offset) {
  auto *bytes = static_cast<unsigned char *>(data);
  while (size) {
    ssize_t count = pread(fd, bytes, std::min(size, size_t(65536)),
                          static_cast<off_t>(offset));
    if (count < 0 && errno == EINTR)
      continue;
    if (count < 0)
      io("Cannot read snapshot");
    if (!count)
      bad("Snapshot is truncated");
    size -= static_cast<size_t>(count);
    offset += static_cast<size_t>(count);
    bytes += count;
  }
}
void write_at(int fd, const void *data, size_t size, size_t offset) {
  const auto *bytes = static_cast<const unsigned char *>(data);
  while (size) {
    ssize_t count = pwrite(fd, bytes, std::min(size, size_t(65536)),
                           static_cast<off_t>(offset));
    if (count < 0 && errno == EINTR)
      continue;
    if (count <= 0)
      io("Cannot write encrypted snapshot");
    size -= static_cast<size_t>(count);
    offset += static_cast<size_t>(count);
    bytes += count;
  }
}
bool reserved(const std::string &name) {
  if (name.size() != 79 || name.substr(75) != ".bin" ||
      (name.compare(0, 11, ".ailo-kv-a-") &&
       name.compare(0, 11, ".ailo-kv-s-")))
    return false;
  return std::all_of(name.begin() + 11, name.begin() + 75, [](char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
  });
}
model::fd private_directory(int parent, const std::string &name) {
  auto result = model::child_directory(parent, name, true);
  struct stat st{};
  if (fstat(result.get(), &st) || st.st_uid != geteuid() || (st.st_mode & 077))
    throw failure(AILO_IO_FAILED, "Snapshot namespace directories must be "
                                  "private and owned by this user");
  return result;
}
void derive(secret_key &key, const ailo_cache_config &config,
            const std::string &purpose) {
  const std::string info = purpose + ":" +
                           std::to_string(std::strlen(config.namespace_id)) +
                           ":" + config.namespace_id + ":" + config.key_id;
  std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> ctx(
      EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr), EVP_PKEY_CTX_free);
  if (!ctx)
    throw std::bad_alloc();
  const unsigned char salt[] = "libailo snapshot key derivation";
  size_t size = key.bytes.size();
  crypto(EVP_PKEY_derive_init(ctx.get()) == 1 &&
         EVP_PKEY_CTX_set_hkdf_md(ctx.get(), EVP_sha256()) == 1 &&
         EVP_PKEY_CTX_set1_hkdf_salt(ctx.get(), salt, sizeof(salt) - 1) == 1 &&
         EVP_PKEY_CTX_set1_hkdf_key(
             ctx.get(), static_cast<const unsigned char *>(config.key), 32) ==
             1 &&
         EVP_PKEY_CTX_add1_hkdf_info(
             ctx.get(), reinterpret_cast<const unsigned char *>(info.data()),
             static_cast<int>(info.size())) == 1 &&
         EVP_PKEY_derive(ctx.get(), key.bytes.data(), &size) == 1 &&
         size == key.bytes.size());
}
class cipher {
  std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx{
      EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free};

public:
  cipher(bool encrypt, const secret_key &key, const unsigned char *nonce,
         const snapshot_record &record) {
    if (!ctx)
      throw std::bad_alloc();
    int count = 0;
    crypto(EVP_CipherInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr,
                             nullptr, encrypt) == 1 &&
           EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, 12,
                               nullptr) == 1 &&
           EVP_CipherInit_ex(ctx.get(), nullptr, nullptr, key.bytes.data(),
                             nonce, encrypt) == 1 &&
           EVP_CipherUpdate(ctx.get(), nullptr, &count, record.header.data(),
                            224) == 1 &&
           EVP_CipherUpdate(
               ctx.get(), nullptr, &count,
               reinterpret_cast<const unsigned char *>(record.name.data()),
               static_cast<int>(record.name.size())) == 1);
  }
  void update(unsigned char *out, const unsigned char *in, size_t size) {
    int count = 0;
    crypto(size <= 65536 &&
           EVP_CipherUpdate(ctx.get(), out, &count, in,
                            static_cast<int>(size)) == 1 &&
           count == static_cast<int>(size));
  }
  void finish(unsigned char *tag, bool encrypt) {
    unsigned char tail[16];
    int size = 0;
    if (!encrypt)
      crypto(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, 16, tag) ==
             1);
    if (EVP_CipherFinal_ex(ctx.get(), tail, &size) != 1)
      bad();
    crypto(size == 0);
    if (encrypt)
      crypto(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, 16, tag) ==
             1);
  }
};
void decrypt(int fd, size_t offset, unsigned char *out, size_t size,
             const secret_key &key, snapshot_record &record, bool metadata,
             state *client) {
  cipher decoder(false, key, record.header.data() + (metadata ? 200 : 212),
                 record);
  for (size_t done = 0; done < size;) {
    if (client && client->cancelled.load())
      throw failure(AILO_CANCELLED, "Snapshot import cancelled");
    const size_t n = std::min(size - done, size_t(65536));
    read_at(fd, out + done, n, offset + done);
    decoder.update(out + done, out + done, n);
    done += n;
  }
  decoder.finish(record.header.data() + (metadata ? 224 : 240), false);
}
void encrypt(int fd, size_t offset, const unsigned char *data, size_t size,
             const secret_key &key, snapshot_record &record, bool metadata,
             state *client) {
  cipher encoder(true, key, record.header.data() + (metadata ? 200 : 212),
                 record);
  std::array<unsigned char, 65536> block{};
  for (size_t done = 0; done < size;) {
    if (client && client->cancelled.load())
      throw failure(AILO_CANCELLED, "Snapshot export cancelled");
    const size_t n = std::min(size - done, block.size());
    encoder.update(block.data(), data + done, n);
    write_at(fd, block.data(), n, offset + done);
    done += n;
  }
  encoder.finish(record.header.data() + (metadata ? 224 : 240), true);
}
struct temporary_file {
  int dir;
  std::string name;
  model::fd file;
  explicit temporary_file(int directory) : dir(directory) {
    unsigned char random[16];
    crypto(RAND_bytes(random, sizeof(random)) == 1);
    name = ".ailo-kv-t-" + hex(random, sizeof(random));
    file = model::fd(
        openat(dir, name.c_str(),
               O_CREAT | O_EXCL | O_WRONLY | O_NOFOLLOW | O_CLOEXEC, 0600));
    if (!file)
      io("Cannot create encrypted snapshot temporary file");
  }
  ~temporary_file() {
    if (file && !name.empty())
      unlinkat(dir, name.c_str(), 0);
  }
};
} // namespace
secret_key::~secret_key() { OPENSSL_cleanse(bytes.data(), bytes.size()); }
std::string checkpoint_key(const cache_entry &entry) {
  const auto part = sha(entry.partition),
             tokens = sha(entry.tokens.data(),
                          entry.tokens.size() * sizeof(llama_token));
  const std::string value = entry.identity + hex(part.data(), part.size()) +
                            hex(tokens.data(), tokens.size()) +
                            entry.media_key + ':' +
                            std::to_string(entry.positions);
  const auto sum = sha(value);
  return hex(sum.data(), sum.size());
}
snapshot_store::snapshot_store(cache_state &cache,
                               const ailo_cache_config &config)
    : owner(cache) {
  if (!config.max_disk_bytes || !config.disk_ttl_seconds ||
      !config.max_disk_entries || config.max_disk_entries > 4096 ||
      !config.max_snapshot_bytes ||
      config.max_snapshot_bytes > static_cast<size_t>(INT64_MAX) - 256 ||
      !config.key || config.key_size != 32)
    throw failure(
        AILO_INVALID_ARGUMENT,
        "Snapshots require nonzero byte/entry/TTL bounds and a 32-byte key");
  key_id = cache_partition(config.key_id);
  if (key_id.empty())
    bad("Snapshots require an explicit key ID");
  key_hash = sha(key_id);
  derive(metadata_key, config, "libailo.prompt-snapshot.metadata.v1");
  derive(state_key, config, "libailo.prompt-snapshot.state.v1");
  if (config.directory)
    root = config.directory;
  else {
    const char *xdg = std::getenv("XDG_CACHE_HOME"),
               *home = std::getenv("HOME");
    if (xdg && *xdg)
      root = std::string(xdg) + "/ailo/inference";
    else if (home && *home)
      root = std::string(home) + "/.cache/ailo/inference";
    else
      bad("Set an absolute snapshot directory, XDG_CACHE_HOME or HOME");
  }
  if (root.size() > 65536 || root.empty() || root[0] != '/')
    bad("Snapshot directory must be absolute and bounded");
  while (root.size() > 1 && root.back() == '/')
    root.pop_back();
  try {
    auto parent = model::directory(root, true);
    auto ns = sha(owner.namespace_id);
    auto scoped = private_directory(parent.get(), hex(ns.data(), ns.size()));
    directory =
        private_directory(scoped.get(), hex(key_hash.data(), key_hash.size()));
  } catch (const model::failure &error) {
    throw failure(error.status, error.what());
  }
}
model::fd snapshot_store::lock() {
  model::fd handle(
      openat(directory.get(), ".lock",
             O_CREAT | O_RDWR | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK, 0600));
  if (!handle)
    io("Cannot open snapshot lock");
  struct stat st{};
  if (fstat(handle.get(), &st))
    io("Cannot inspect snapshot lock");
  private_file(st);
  if (flock(handle.get(), LOCK_EX | LOCK_NB)) {
    if (errno == EWOULDBLOCK || errno == EAGAIN)
      throw failure(AILO_BUSY, "Snapshot namespace is active");
    io("Cannot lock snapshot namespace");
  }
  return handle;
}
void snapshot_store::remove(const std::string &name) {
  struct stat st{};
  if (fstatat(directory.get(), name.c_str(), &st, AT_SYMLINK_NOFOLLOW)) {
    if (errno == ENOENT)
      return;
    io("Cannot inspect snapshot for removal");
  }
  private_file(st);
  if (unlinkat(directory.get(), name.c_str(), 0))
    io("Cannot remove snapshot");
  owner.disk_evictions.fetch_add(1);
}
snapshot_record snapshot_store::inspect(const std::string &name) {
  if (!reserved(name))
    bad("Invalid snapshot filename");
  auto fd = open_file(directory.get(), name);
  snapshot_record record;
  record.name = name;
  struct stat st{};
  if (fstat(fd.get(), &st))
    io("Cannot inspect snapshot length");
  if (st.st_size < 256)
    bad("Snapshot is truncated");
  read_at(fd.get(), record.header.data(), record.header.size(), 0);
  const auto *header = record.header.data();
  if (std::memcmp(header, "AILOKV01", 8) ||
      std::memcmp(header + 168, key_hash.data(), 32))
    bad();
  const uint64_t count = get(header + 24), native = get(header + 32);
  const size_t cap = owner.config.max_snapshot_bytes;
  if (cap < media_metadata_bytes || !count || count > INT32_MAX ||
      count > (cap - media_metadata_bytes) / sizeof(llama_token) || !native ||
      native > cap - media_metadata_bytes - count * sizeof(llama_token))
    throw failure(AILO_OUT_OF_RANGE,
                  "Snapshot exceeds its materialization budget");
  const uint64_t size =
      256 + media_metadata_bytes + count * sizeof(llama_token) + native;
  if (size != static_cast<uint64_t>(st.st_size) ||
      size > owner.config.max_disk_bytes)
    bad("Snapshot length is invalid or exceeds its disk budget");
  record.created = get(header + 8);
  record.count = static_cast<size_t>(count);
  record.native_size = static_cast<size_t>(native);
  record.size = static_cast<size_t>(size);
  return record;
}
std::unique_ptr<cache_entry> snapshot_store::read(const snapshot_record &input,
                                                  bool native, state *client) {
  auto fd = open_file(directory.get(), input.name);
  snapshot_record record = input;
  struct stat before{}, after{};
  if (fstat(fd.get(), &before))
    io("Cannot inspect snapshot");
  std::array<unsigned char, 256> header{};
  read_at(fd.get(), header.data(), header.size(), 0);
  if (header != input.header)
    bad("Snapshot changed during lookup");
  auto entry = std::make_unique<cache_entry>();
  entry->tokens.resize(record.count);
  const size_t token_bytes =
      media_metadata_bytes + record.count * sizeof(llama_token);
  std::vector<unsigned char> payload(token_bytes);
  decrypt(fd.get(), 256, payload.data(), token_bytes, metadata_key, record,
          true, client);
  const auto token_sum = sha(payload.data(), token_bytes);
  if (std::memcmp(token_sum.data(), record.header.data() + 104, 32))
    bad("Snapshot token checksum failed");
  const auto positions = get(payload.data()),
             evaluated = get(payload.data() + 8);
  if (!positions || positions > INT_MAX || !evaluated || evaluated > INT_MAX)
    bad("Snapshot evaluation counts are invalid");
  entry->positions = static_cast<size_t>(positions);
  entry->evaluated_tokens = static_cast<size_t>(evaluated);
  if (payload[16]) {
    entry->media_key.assign(reinterpret_cast<const char *>(payload.data() + 16),
                            64);
    if (!std::all_of(entry->media_key.begin(), entry->media_key.end(),
                     [](char c) {
                       return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
                     }))
      bad("Snapshot media identity is invalid");
  } else if (std::any_of(payload.begin() + 16, payload.begin() + 80,
                         [](unsigned char c) { return c != 0; }))
    bad("Snapshot text metadata is invalid");
  if (entry->media_key.empty() &&
      (positions != record.count || evaluated != record.count))
    bad("Snapshot text counts are invalid");
  std::memcpy(entry->tokens.data(), payload.data() + media_metadata_bytes,
              record.count * sizeof(llama_token));
  OPENSSL_cleanse(payload.data(), payload.size());
  const auto expires = get(record.header.data() + 16), current = now();
  if (record.created > current || expires <= current ||
      expires <= record.created ||
      expires - record.created > owner.config.disk_ttl_seconds)
    bad("Snapshot has expired or its lifetime is incompatible");
  for (auto token : entry->tokens)
    if (token < 0)
      bad("Snapshot token ID is invalid");
  if (native) {
    entry->bytes.resize(record.native_size);
    decrypt(fd.get(), 256 + token_bytes, entry->bytes.data(),
            entry->bytes.size(), state_key, record, false, client);
    const auto sum = sha(entry->bytes.data(), entry->bytes.size());
    if (std::memcmp(sum.data(), record.header.data() + 136, 32))
      bad("Snapshot native checksum failed");
  }
  if (fstat(fd.get(), &after) || before.st_size != after.st_size ||
      before.st_mtim.tv_sec != after.st_mtim.tv_sec ||
      before.st_mtim.tv_nsec != after.st_mtim.tv_nsec ||
      before.st_ctim.tv_sec != after.st_ctim.tv_sec ||
      before.st_ctim.tv_nsec != after.st_ctim.tv_nsec)
    bad("Snapshot changed during authentication");
  return entry;
}
std::vector<snapshot_record> snapshot_store::catalog(bool prune) {
  int duplicate =
      openat(directory.get(), ".", O_RDONLY | O_CLOEXEC | O_DIRECTORY);
  if (duplicate < 0)
    io("Cannot enumerate snapshots");
  DIR *raw = fdopendir(duplicate);
  if (!raw) {
    close(duplicate);
    io("Cannot enumerate snapshots");
  }
  auto close_directory = [](DIR *dir) { closedir(dir); };
  std::unique_ptr<DIR, decltype(close_directory)> stream(raw, close_directory);
  std::vector<snapshot_record> result;
  for (;;) {
    errno = 0;
    auto *item = readdir(stream.get());
    if (!item) {
      if (errno)
        io("Cannot enumerate snapshots");
      break;
    }
    const std::string name = item->d_name;
    if (name.size() == 43 && name.compare(0, 11, ".ailo-kv-t-") == 0 &&
        std::all_of(name.begin() + 11, name.end(), [](char c) {
          return (c >= 'a' && c <= 'f') || (c >= '0' && c <= '9');
        })) {
      if (prune)
        remove(name);
      continue;
    }
    if (!reserved(name))
      continue;
    try {
      auto record = inspect(name);
      const auto expires = get(record.header.data() + 16), current = now();
      // Unauthenticated catalog fields are used only for fail-closed eviction
      // and quota selection. They never authorize native restore or token
      // reuse.
      if (record.created > current || expires <= current ||
          expires <= record.created ||
          expires - record.created > owner.config.disk_ttl_seconds)
        bad("Snapshot expired");
      if (result.size() >= 4096)
        throw failure(AILO_IO_FAILED,
                      "Snapshot namespace exceeds 4096 entries");
      result.push_back(std::move(record));
    } catch (const failure &) {
      if (!prune)
        throw;
      remove(name);
      owner.snapshot_errors.fetch_add(1);
    }
  }
  std::sort(result.begin(), result.end(),
            [](const snapshot_record &a, const snapshot_record &b) {
              return a.created != b.created ? a.created < b.created
                                            : a.name < b.name;
            });
  return result;
}
std::pair<size_t, size_t> snapshot_store::prune(bool clear) {
  auto guard = lock();
  auto records = catalog(true);
  size_t bytes = 0, count = 0;
  for (auto it = records.rbegin(); it != records.rend(); ++it) {
    if (clear || count >= owner.config.max_disk_entries ||
        it->size > owner.config.max_disk_bytes - bytes)
      remove(it->name);
    else {
      bytes += it->size;
      ++count;
    }
  }
  if (fsync(directory.get()))
    io("Cannot sync snapshot pruning");
  return {count, bytes};
}
bool snapshot_store::write(const cache_entry &entry, const std::string &name,
                           state *client, bool replace) {
  if (!reserved(name))
    bad("Invalid snapshot slot identity");
  const size_t cap = owner.config.max_snapshot_bytes;
  if (cap < media_metadata_bytes ||
      entry.tokens.size() >
          (cap - media_metadata_bytes) / sizeof(llama_token) ||
      entry.bytes.size() > cap - media_metadata_bytes -
                               entry.tokens.size() * sizeof(llama_token) ||
      entry.tokens.empty() || entry.bytes.empty())
    throw failure(AILO_OUT_OF_RANGE,
                  "Snapshot exceeds its materialization budget");
  snapshot_record record;
  record.name = name;
  record.count = entry.tokens.size();
  record.native_size = entry.bytes.size();
  record.size = 256 + media_metadata_bytes +
                entry.tokens.size() * sizeof(llama_token) + entry.bytes.size();
  record.created = now();
  if (record.size > owner.config.max_disk_bytes)
    throw failure(AILO_OUT_OF_RANGE, "Snapshot exceeds its disk budget");
  auto guard = lock();
  auto records = catalog(true);
  if (!replace && name[9] == 'a' &&
      std::any_of(records.begin(), records.end(),
                  [&](const snapshot_record &r) { return r.name == name; }))
    return true;
  auto *header = record.header.data();
  std::memcpy(header, "AILOKV01", 8);
  put(header + 8, record.created);
  put(header + 16, record.created + owner.config.disk_ttl_seconds);
  put(header + 24, record.count);
  put(header + 32, record.native_size);
  std::vector<unsigned char> payload(media_metadata_bytes +
                                     entry.tokens.size() * sizeof(llama_token));
  put(payload.data(), entry.positions);
  put(payload.data() + 8, entry.evaluated_tokens);
  if (!entry.media_key.empty()) {
    if (entry.media_key.size() != 64)
      bad("Snapshot media identity has an invalid length");
    std::memcpy(payload.data() + 16, entry.media_key.data(), 64);
  }
  std::memcpy(payload.data() + media_metadata_bytes, entry.tokens.data(),
              entry.tokens.size() * sizeof(llama_token));
  const auto model = sha(entry.identity), partition = sha(entry.partition),
             tokens = sha(payload.data(), payload.size()),
             native = sha(entry.bytes.data(), entry.bytes.size());
  std::memcpy(header + 40, model.data(), 32);
  std::memcpy(header + 72, partition.data(), 32);
  std::memcpy(header + 104, tokens.data(), 32);
  std::memcpy(header + 136, native.data(), 32);
  std::memcpy(header + 168, key_hash.data(), 32);
  crypto(RAND_bytes(header + 200, 24) == 1);
  temporary_file temporary(directory.get());
  encrypt(temporary.file.get(), 256, payload.data(), payload.size(),
          metadata_key, record, true, client);
  OPENSSL_cleanse(payload.data(), payload.size());
  encrypt(temporary.file.get(), 256 + payload.size(), entry.bytes.data(),
          entry.bytes.size(), state_key, record, false, client);
  write_at(temporary.file.get(), header, record.header.size(), 0);
  if (now() >= get(header + 16))
    bad("Snapshot expired before publication");
  if (fsync(temporary.file.get()))
    io("Cannot sync encrypted snapshot");
  size_t bytes = record.size, count = 1;
  for (auto it = records.rbegin(); it != records.rend(); ++it) {
    if (it->name == name)
      continue;
    if (count >= owner.config.max_disk_entries ||
        it->size > owner.config.max_disk_bytes - bytes)
      remove(it->name);
    else {
      bytes += it->size;
      ++count;
    }
  }
  if (renameat(directory.get(), temporary.name.c_str(), directory.get(),
               name.c_str()))
    io("Cannot atomically publish snapshot");
  temporary.name.clear();
  if (fsync(directory.get()))
    io("Cannot sync snapshot publication");
  owner.snapshot_writes.fetch_add(1);
  return true;
}
std::unique_ptr<cache_entry> snapshot_store::load(const std::string &name,
                                                  const std::string &identity,
                                                  const std::string &partition,
                                                  state *client) {
  auto guard = lock();
  const auto record = inspect(name);
  auto entry = read(record, true, client);
  const auto model = sha(identity), part = sha(partition);
  if (std::memcmp(model.data(), record.header.data() + 40, 32) ||
      std::memcmp(part.data(), record.header.data() + 72, 32))
    bad("Snapshot model/build or cache partition is incompatible");
  entry->identity = identity;
  entry->partition = partition;
  owner.snapshot_reads.fetch_add(1);
  return entry;
}
size_t snapshot_store::restore(state &client,
                               const std::vector<llama_token> &tokens,
                               const std::string &partition, size_t minimum,
                               bool trim,
                               std::unique_ptr<cache_entry> &loaded) {
  auto guard = lock();
  const auto model = sha(client.cache_identity), part = sha(partition);
  auto records = catalog(true);
  std::vector<std::pair<size_t, snapshot_record>> candidates;
  for (const auto &record : records) {
    if (record.name[9] != 'a' ||
        std::memcmp(model.data(), record.header.data() + 40, 32) ||
        std::memcmp(part.data(), record.header.data() + 72, 32))
      continue;
    if (client.cancelled.load())
      throw failure(AILO_CANCELLED, "Snapshot restore cancelled");
    try {
      auto entry = read(record, false, &client);
      if (entry->media_key != client.active_media_key ||
          (!entry->media_key.empty() &&
           entry->tokens.size() != tokens.size() - 1))
        continue;
      const auto count = matching_prefix(*entry, tokens, trim);
      if (count > minimum)
        candidates.emplace_back(count, record);
    } catch (const failure &error) {
      if (error.status == AILO_CANCELLED)
        throw;
      remove(record.name);
      owner.snapshot_errors.fetch_add(1);
    }
  }
  std::sort(candidates.begin(), candidates.end(),
            [](const auto &a, const auto &b) { return a.first > b.first; });
  for (const auto &candidate : candidates) {
    if (client.cancelled.load())
      throw failure(AILO_CANCELLED, "Snapshot restore cancelled");
    try {
      auto entry = read(candidate.second, true, &client);
      entry->identity = client.cache_identity;
      entry->partition = partition;
      bool allocation_failed = false;
      auto context = restore_checkpoint(client, *entry, candidate.first,
                                        &allocation_failed);
      if (allocation_failed)
        return 0;
      if (!context)
        bad("Snapshot native state cannot be restored");
      client.context = std::move(context);
      loaded = std::move(entry);
      owner.snapshot_reads.fetch_add(1);
      return candidate.first;
    } catch (const failure &error) {
      if (error.status == AILO_CANCELLED)
        throw;
      remove(candidate.second.name);
      owner.snapshot_errors.fetch_add(1);
    }
  }
  return 0;
}
} // namespace client
} // namespace ailo
