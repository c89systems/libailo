#include "ailo_cache_internal.hpp"
#include "ailo_compaction_internal.hpp"
#include <algorithm>
#include <climits>
#include <cstring>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>
#include <set>

#define AILO_PUBLIC extern "C" __attribute__((visibility("default")))
namespace ailo {
namespace client {
namespace {
constexpr size_t header_size = 156, authenticated_size = 140;
[[noreturn]] void
invalid(const char *message =
            "Compaction item authentication or binding validation failed") {
  throw failure(AILO_INVALID_ARGUMENT, message);
}
void crypto(bool good) {
  if (!good)
    throw failure(AILO_INTERNAL_ERROR,
                  "Compaction cryptographic operation failed");
}
void cancelled(state *client) {
  if (client && client->cancelled.load())
    throw failure(AILO_CANCELLED, "Compaction cancelled");
}
uint64_t now() {
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::seconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count());
}
void put(unsigned char *data, uint64_t n) {
  for (unsigned i = 0; i < 8; ++i) {
    data[i] = static_cast<unsigned char>(n);
    n >>= 8;
  }
}
uint64_t get(const unsigned char *data) {
  uint64_t n = 0;
  for (unsigned i = 0; i < 8; ++i)
    n |= uint64_t(data[i]) << (8 * i);
  return n;
}
compaction_digest sha(const void *data, size_t size) {
  compaction_digest out{};
  unsigned n = 0;
  crypto(EVP_Digest(data, size, out.data(), &n, EVP_sha256(), nullptr) == 1 &&
         n == out.size());
  return out;
}
compaction_digest sha(const std::string &data) {
  return sha(data.data(), data.size());
}
std::string hex(const void *data, size_t size) {
  const auto *p = static_cast<const unsigned char *>(data);
  std::string out;
  for (size_t i = 0; i < size; ++i) {
    out += "0123456789abcdef"[p[i] >> 4];
    out += "0123456789abcdef"[p[i] & 15];
  }
  return out;
}
struct plaintext {
  std::vector<unsigned char> bytes;
  ~plaintext() {
    if (!bytes.empty())
      OPENSSL_cleanse(bytes.data(), bytes.size());
  }
};
std::string encode(const std::vector<unsigned char> &bytes) {
  std::string out(4 * ((bytes.size() + 2) / 3), '\0');
  const int n = EVP_EncodeBlock(reinterpret_cast<unsigned char *>(out.data()),
                                bytes.data(), static_cast<int>(bytes.size()));
  crypto(n >= 0 && static_cast<size_t>(n) == out.size());
  while (!out.empty() && out.back() == '=')
    out.pop_back();
  for (char &ch : out)
    if (ch == '+')
      ch = '-';
    else if (ch == '/')
      ch = '_';
  return out;
}
std::vector<unsigned char> decode(const char *input, size_t limit) {
  if (!input)
    invalid("Compaction encrypted content is required");
  const size_t cap = 4 * ((limit + 2) / 3), size = strnlen(input, cap + 1);
  if (!size || size > cap || size % 4 == 1)
    invalid("Compaction encrypted content has an invalid length");
  std::string padded(input, size);
  for (char &ch : padded) {
    if (ch == '-')
      ch = '+';
    else if (ch == '_')
      ch = '/';
    else if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
               (ch >= '0' && ch <= '9')))
      invalid("Compaction encrypted content is not unpadded base64url");
  }
  while (padded.size() % 4)
    padded += '=';
  std::vector<unsigned char> out(padded.size() / 4 * 3);
  const int decoded = EVP_DecodeBlock(
      out.data(), reinterpret_cast<const unsigned char *>(padded.data()),
      static_cast<int>(padded.size()));
  if (decoded < 0)
    invalid();
  const size_t count = static_cast<size_t>(decoded) - (padded.size() - size);
  if (count > limit)
    throw failure(AILO_OUT_OF_RANGE,
                  "Compaction envelope exceeds its byte budget");
  out.resize(count);
  if (encode(out) != std::string(input, size))
    invalid("Compaction base64url encoding is not canonical");
  return out;
}
std::unique_ptr<compaction_secret> key(const ailo_compaction_key &input,
                                       const std::string &ns) {
  auto out = std::make_unique<compaction_secret>();
  out->id = cache_partition(input.id);
  if (out->id.empty() || !input.bytes || input.size != 32)
    invalid("Compaction keys require an ID and exactly 32 bytes");
  out->id_hash = sha(out->id);
  const std::string info =
      "libailo.semantic-compaction:1:" + std::to_string(ns.size()) + ":" + ns +
      ":" + out->id;
  std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> ctx(
      EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr), EVP_PKEY_CTX_free);
  if (!ctx)
    throw std::bad_alloc();
  static const unsigned char salt[] =
      "libailo semantic compaction key derivation";
  size_t size = 32;
  crypto(EVP_PKEY_derive_init(ctx.get()) == 1 &&
         EVP_PKEY_CTX_set_hkdf_md(ctx.get(), EVP_sha256()) == 1 &&
         EVP_PKEY_CTX_set1_hkdf_salt(ctx.get(), salt, sizeof(salt) - 1) == 1 &&
         EVP_PKEY_CTX_set1_hkdf_key(
             ctx.get(), static_cast<const unsigned char *>(input.bytes), 32) ==
             1 &&
         EVP_PKEY_CTX_add1_hkdf_info(
             ctx.get(), reinterpret_cast<const unsigned char *>(info.data()),
             static_cast<int>(info.size())) == 1 &&
         EVP_PKEY_derive(ctx.get(), out->bytes.data(), &size) == 1 &&
         size == 32);
  return out;
}
void cipher(bool encrypt, const compaction_secret &key,
            std::vector<unsigned char> &envelope, unsigned char *output,
            const unsigned char *input, size_t size, const std::string &id,
            state *client) {
  std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(
      EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
  if (!ctx)
    throw std::bad_alloc();
  int n = 0;
  crypto(EVP_CipherInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr,
                           nullptr, encrypt) == 1 &&
         EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, 12, nullptr) ==
             1 &&
         EVP_CipherInit_ex(ctx.get(), nullptr, nullptr, key.bytes.data(),
                           envelope.data() + 8, encrypt) == 1 &&
         EVP_CipherUpdate(ctx.get(), nullptr, &n, envelope.data(),
                          authenticated_size) == 1 &&
         EVP_CipherUpdate(ctx.get(), nullptr, &n,
                          reinterpret_cast<const unsigned char *>(id.data()),
                          static_cast<int>(id.size())) == 1);
  for (size_t offset = 0; offset < size;) {
    cancelled(client);
    const auto count = std::min(size - offset, size_t(65536));
    crypto(EVP_CipherUpdate(ctx.get(), output + offset, &n, input + offset,
                            static_cast<int>(count)) == 1 &&
           n == static_cast<int>(count));
    offset += count;
  }
  if (!encrypt)
    crypto(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, 16,
                               envelope.data() + 140) == 1);
  unsigned char tail[16];
  if (EVP_CipherFinal_ex(ctx.get(), tail, &n) != 1)
    invalid();
  crypto(n == 0);
  if (encrypt)
    crypto(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, 16,
                               envelope.data() + 140) == 1);
}
} // namespace
compaction_secret::~compaction_secret() {
  OPENSSL_cleanse(bytes.data(), bytes.size());
}
compaction_payload::~compaction_payload() {
  if (!summary.empty())
    OPENSSL_cleanse(summary.data(), summary.size());
}
compaction_state::compaction_state(const ailo_compactor_config &config)
    : ephemeral(!config.current_key.bytes),
      max_item_bytes(config.max_item_bytes), ttl_seconds(config.ttl_seconds) {
  if (max_item_bytes < 256 || max_item_bytes > 4 * 1024 * 1024 ||
      config.old_key_count > 8 || (!config.old_keys && config.old_key_count))
    invalid("Invalid compaction envelope or key-ring bounds");
  namespace_id = cache_partition(config.namespace_id);
  if (ephemeral) {
    if (config.current_key.id || config.current_key.size ||
        config.old_key_count)
      invalid("Compaction current key is incomplete");
    compaction_secret random;
    crypto(RAND_bytes(random.bytes.data(), 32) == 1);
    if (namespace_id.empty()) {
      unsigned char ns[16];
      crypto(RAND_bytes(ns, sizeof(ns)) == 1);
      namespace_id = "private-" + hex(ns, sizeof(ns));
    }
    keys.push_back(key({"ephemeral", random.bytes.data(), 32}, namespace_id));
  } else {
    if (namespace_id.empty())
      invalid("Stable compaction keys require an explicit namespace");
    keys.push_back(key(config.current_key, namespace_id));
  }
  std::set<std::string> ids{keys.front()->id};
  for (size_t i = 0; i < config.old_key_count; ++i) {
    auto old = key(config.old_keys[i], namespace_id);
    if (!ids.insert(old->id).second)
      invalid("Compaction key IDs must be unique");
    keys.push_back(std::move(old));
  }
  namespace_hash = sha(namespace_id);
}
sealed_summary compaction_state::seal(const compaction_payload &payload,
                                      const std::string &identity,
                                      state *client) const {
  if (identity.size() != 64 || payload.cut > payload.retained.size() ||
      payload.retained.size() > 4096 || !payload.removed_items ||
      payload.source_items < payload.removed_items ||
      payload.source_items > 4096 ||
      payload.retained.size() != payload.source_items - payload.removed_items ||
      payload.summary.empty())
    invalid("Invalid compaction payload");
  require_utf8(payload.summary.data(), payload.summary.size());
  if (payload.summary.find('\0') != std::string::npos)
    invalid("Compaction summary contains NUL");
  const size_t metadata = 40 + payload.retained.size() * 32;
  if (metadata > max_item_bytes - header_size ||
      payload.summary.size() > max_item_bytes - header_size - metadata)
    throw failure(AILO_OUT_OF_RANGE,
                  "Compaction payload exceeds its envelope budget");
  plaintext plain;
  plain.bytes.resize(metadata + payload.summary.size());
  put(plain.bytes.data(), payload.cut);
  put(plain.bytes.data() + 8, payload.source_items);
  put(plain.bytes.data() + 16, payload.removed_items);
  put(plain.bytes.data() + 24, payload.retained.size());
  put(plain.bytes.data() + 32, payload.summary.size());
  for (size_t i = 0; i < payload.retained.size(); ++i)
    std::memcpy(plain.bytes.data() + 40 + i * 32, payload.retained[i].data(),
                32);
  std::memcpy(plain.bytes.data() + metadata, payload.summary.data(),
              payload.summary.size());
  std::vector<unsigned char> envelope(header_size + plain.bytes.size());
  std::memcpy(envelope.data(), "AILOCMP1", 8);
  crypto(RAND_bytes(envelope.data() + 8, 12) == 1);
  const auto model = sha(identity);
  std::memcpy(envelope.data() + 20, keys.front()->id_hash.data(), 32);
  std::memcpy(envelope.data() + 52, namespace_hash.data(), 32);
  std::memcpy(envelope.data() + 84, model.data(), 32);
  const uint64_t issued = now();
  put(envelope.data() + 116, issued);
  put(envelope.data() + 124, ttl_seconds ? issued + ttl_seconds : 0);
  put(envelope.data() + 132, plain.bytes.size());
  unsigned char random[16];
  crypto(RAND_bytes(random, sizeof(random)) == 1);
  sealed_summary out;
  out.id = "cmp_" + hex(random, sizeof(random));
  cipher(true, *keys.front(), envelope, envelope.data() + header_size,
         plain.bytes.data(), plain.bytes.size(), out.id, client);
  out.encrypted_content = encode(envelope);
  return out;
}
std::unique_ptr<compaction_payload>
compaction_state::open(const char *id, const char *encrypted,
                       const std::string &identity, state *client) const {
  const auto item_id = cache_partition(id);
  if (item_id.empty() || identity.size() != 64)
    invalid();
  auto envelope = decode(encrypted, max_item_bytes);
  if (envelope.size() < header_size ||
      std::memcmp(envelope.data(), "AILOCMP1", 8))
    invalid("Compaction item is not a supported libailo envelope");
  const auto size = get(envelope.data() + 132);
  if (size != envelope.size() - header_size)
    invalid();
  const compaction_secret *selected = nullptr;
  for (const auto &candidate : keys)
    if (!std::memcmp(candidate->id_hash.data(), envelope.data() + 20, 32))
      selected = candidate.get();
  const auto model = sha(identity);
  if (!selected ||
      std::memcmp(namespace_hash.data(), envelope.data() + 52, 32) ||
      std::memcmp(model.data(), envelope.data() + 84, 32))
    invalid();
  plaintext plain;
  plain.bytes.resize(static_cast<size_t>(size));
  cipher(false, *selected, envelope, plain.bytes.data(),
         envelope.data() + header_size, plain.bytes.size(), item_id, client);
  // No payload field is parsed or used until authenticated decryption succeeds.
  const auto issued = get(envelope.data() + 116),
             expires = get(envelope.data() + 124), current = now();
  if (issued > current ||
      (expires && (expires <= issued || expires <= current)))
    invalid("Compaction item has expired or has an invalid lifetime");
  if (plain.bytes.size() < 40)
    invalid();
  const auto cut = get(plain.bytes.data()),
             source = get(plain.bytes.data() + 8),
             removed = get(plain.bytes.data() + 16),
             count = get(plain.bytes.data() + 24),
             text = get(plain.bytes.data() + 32);
  if (count > 4096 || cut > count || !removed || source > 4096 ||
      removed > source || count != source - removed ||
      count > (plain.bytes.size() - 40) / 32 || !text ||
      text != plain.bytes.size() - 40 - count * 32)
    invalid();
  auto out = std::make_unique<compaction_payload>();
  out->cut = static_cast<size_t>(cut);
  out->source_items = static_cast<size_t>(source);
  out->removed_items = static_cast<size_t>(removed);
  out->retained.resize(static_cast<size_t>(count));
  for (size_t i = 0; i < count; ++i)
    std::memcpy(out->retained[i].data(), plain.bytes.data() + 40 + i * 32, 32);
  out->summary.assign(
      reinterpret_cast<const char *>(plain.bytes.data() + 40 + count * 32),
      static_cast<size_t>(text));
  require_utf8(out->summary.data(), out->summary.size());
  if (out->summary.find('\0') != std::string::npos)
    invalid();
  return out;
}
compaction_digest fingerprint(const ailo_item &item) {
  std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(),
                                                              EVP_MD_CTX_free);
  if (!ctx)
    throw std::bad_alloc();
  crypto(EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) == 1);
  auto number = [&](uint64_t n) {
    unsigned char bytes[8];
    put(bytes, n);
    crypto(EVP_DigestUpdate(ctx.get(), bytes, 8) == 1);
  };
  auto bytes = [&](const void *p, size_t n) {
    if (!p && n)
      invalid();
    number(n);
    if (n)
      crypto(EVP_DigestUpdate(ctx.get(), p, n) == 1);
  };
  auto string = [&](const char *p, size_t cap) {
    number(p ? 1 : 0);
    if (p) {
      size_t n = strnlen(p, cap + 1);
      if (n > cap)
        throw failure(AILO_OUT_OF_RANGE,
                      "Compaction retained field is too long");
      bytes(p, n);
    }
  };
  number(item.kind);
  number(item.role);
  number(item.pinned != 0);
  string(item.id, 256);
  string(item.call_id, 256);
  string(item.name, 64);
  string(item.arguments, 4 * 1024 * 1024);
  if (item.content_count > 4096 || (!item.content && item.content_count))
    invalid();
  number(item.content_count);
  size_t total = 0;
  for (size_t i = 0; i < item.content_count; ++i) {
    const auto &part = item.content[i];
    if (part.size > 4 * 1024 * 1024 - total)
      invalid();
    total += part.size;
    number(part.kind);
    bytes(part.data, part.size);
  }
  compaction_digest out{};
  unsigned n = 0;
  crypto(EVP_DigestFinal_ex(ctx.get(), out.data(), &n) == 1 && n == out.size());
  return out;
}
void verify_compaction_binding(const compaction_payload &payload,
                               const ailo_item *items, size_t count,
                               size_t position) {
  if (!items || position >= count || position != payload.cut ||
      payload.retained.size() >= count || count > 4096 ||
      items[position].kind != AILO_ITEM_COMPACTION)
    invalid("Compaction retained window is missing or misordered");
  size_t total = 0;
  for (size_t i = 0; i < count; ++i) {
    if (items[i].kind == AILO_ITEM_COMPACTION) {
      if (i != position)
        invalid("Duplicate or superseded compaction item");
      continue;
    }
    if (items[i].arguments) {
      const auto n = strnlen(items[i].arguments, 4 * 1024 * 1024 - total + 1);
      if (n > 4 * 1024 * 1024 - total)
        invalid("Compaction retained text exceeds 4 MiB");
      total += n;
    }
    if (items[i].content_count > 4096 ||
        (!items[i].content && items[i].content_count))
      invalid();
    for (size_t j = 0; j < items[i].content_count; ++j) {
      const auto n = items[i].content[j].size;
      if (n > 4 * 1024 * 1024 - total)
        invalid("Compaction retained text exceeds 4 MiB");
      total += n;
    }
  }
  for (size_t i = 0; i < payload.retained.size(); ++i) {
    const auto index = i < position ? i : i + 1;
    if (fingerprint(items[index]) != payload.retained[i])
      invalid("Compaction retained context was changed or removed");
  }
}
std::shared_ptr<compaction_state> retain_compactor(ailo_compactor *value) {
  if (!value || !value->state)
    invalid("A live compactor is required");
  return *static_cast<std::shared_ptr<compaction_state> *>(value->state);
}
} // namespace client
} // namespace ailo
AILO_PUBLIC ailo_compactor_config ailo_compactor_default_config(void) {
  ailo_compactor_config out{};
  out.max_item_bytes = 1024 * 1024;
  return out;
}
AILO_PUBLIC ailo_status ailo_compactor_new(const ailo_compactor_config *config,
                                           ailo_compactor **output,
                                           ailo_error *error) {
  if (output)
    *output = nullptr;
  if (!output)
    return ailo_error_set(error, AILO_INVALID_ARGUMENT,
                          "Compactor output is required");
  return ailo::client::boundary(error, [&]() {
    auto owner =
        std::make_unique<std::shared_ptr<ailo::client::compaction_state>>(
            std::make_shared<ailo::client::compaction_state>(
                config ? *config : ailo_compactor_default_config()));
    auto shell = std::make_unique<ailo_compactor>();
    *shell = {owner.get(), ailo_compactor_get_info, ailo_compactor_destroy};
    owner.release();
    *output = shell.release();
  });
}
AILO_PUBLIC ailo_status ailo_compactor_get_info(ailo_compactor *value,
                                                ailo_compactor_info *output,
                                                ailo_error *error) {
  if (output)
    *output = {};
  if (!output)
    return ailo_error_set(error, AILO_INVALID_ARGUMENT,
                          "Compactor info output is required");
  return ailo::client::boundary(error, [&]() {
    const auto owner = ailo::client::retain_compactor(value);
    *output = {owner->ephemeral, owner->keys.size(), owner->max_item_bytes,
               owner->ttl_seconds};
  });
}
AILO_PUBLIC void ailo_compactor_destroy(ailo_compactor *value) {
  if (!value)
    return;
  delete static_cast<std::shared_ptr<ailo::client::compaction_state> *>(
      value->state);
  delete value;
}
