#include "ailo_cache_internal.hpp"
#include "ailo_compaction_internal.hpp"
#include "ailo_openai_crypto_internal.hpp"
#include "ailo_openai_internal.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>

namespace ailo {
namespace client {
api_plaintext::~api_plaintext() {
  if (!bytes.empty())
    OPENSSL_cleanse(bytes.data(), bytes.size());
}
namespace {
constexpr size_t header_size = 92, tag_offset = 76;
const char *domain(api_item_purpose purpose) {
  return purpose == api_item_purpose::compaction
             ? "libailo.responses.compaction:1"
             : "libailo.responses.reasoning:1";
}
[[noreturn]] void invalid() {
  throw failure(AILO_INVALID_ARGUMENT,
                "Responses encrypted item authentication failed");
}
void crypto(bool ok) {
  if (!ok)
    throw failure(AILO_INTERNAL_ERROR,
                  "Responses encrypted item cryptographic operation failed");
}
void cancelled(state &self) {
  if (self.is_cancelled())
    throw failure(AILO_CANCELLED, "Responses encrypted item cancelled");
}
void put(unsigned char *p, uint64_t n) {
  for (unsigned i = 0; i < 8; ++i) {
    p[i] = static_cast<unsigned char>(n);
    n >>= 8;
  }
}
uint64_t get(const unsigned char *p) {
  uint64_t n = 0;
  for (unsigned i = 0; i < 8; ++i)
    n |= uint64_t(p[i]) << (8 * i);
  return n;
}
struct secret {
  compaction_digest bytes{};
  ~secret() { OPENSSL_cleanse(bytes.data(), bytes.size()); }
};
std::string aad(state &self, const std::string &id, const char *purpose) {
  std::string value(purpose);
  const std::string *fields[] = {&self.openai->policy.namespace_id,
                                 &self.conversation_identity, &id};
  for (const auto *s : fields) {
    unsigned char size[8];
    put(size, s->size());
    value.append(reinterpret_cast<const char *>(size), sizeof(size));
    value += *s;
  }
  return value;
}
void derive(const compaction_secret &key, secret &out, const char *purpose) {
  std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> ctx(
      EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr), EVP_PKEY_CTX_free);
  if (!ctx)
    throw std::bad_alloc();
  size_t size = out.bytes.size();
  crypto(EVP_PKEY_derive_init(ctx.get()) == 1 &&
         EVP_PKEY_CTX_set_hkdf_md(ctx.get(), EVP_sha256()) == 1 &&
         EVP_PKEY_CTX_set1_hkdf_salt(
             ctx.get(), reinterpret_cast<const unsigned char *>(purpose),
             static_cast<int>(std::strlen(purpose))) == 1 &&
         EVP_PKEY_CTX_set1_hkdf_key(ctx.get(), key.bytes.data(),
                                    key.bytes.size()) == 1 &&
         EVP_PKEY_derive(ctx.get(), out.bytes.data(), &size) == 1 &&
         size == out.bytes.size());
}
std::string encode(const std::vector<unsigned char> &bytes) {
  std::string out(4 * ((bytes.size() + 2) / 3), '\0');
  const int size =
      EVP_EncodeBlock(reinterpret_cast<unsigned char *>(out.data()),
                      bytes.data(), static_cast<int>(bytes.size()));
  crypto(size >= 0 && static_cast<size_t>(size) == out.size());
  while (!out.empty() && out.back() == '=')
    out.pop_back();
  for (auto &c : out) {
    if (c == '+')
      c = '-';
    else if (c == '/')
      c = '_';
  }
  return out;
}
std::vector<unsigned char> decode(state &self, const std::string &text) {
  const size_t limit = self.compactor->max_item_bytes;
  if (text.empty() || text.size() > 4 * ((limit + 2) / 3) ||
      text.size() % 4 == 1)
    invalid();
  std::string padded(text);
  for (auto &c : padded) {
    if (c == '-')
      c = '+';
    else if (c == '_')
      c = '/';
    else if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9')))
      invalid();
  }
  while (padded.size() % 4)
    padded += '=';
  std::vector<unsigned char> out(padded.size() / 4 * 3);
  const int n = EVP_DecodeBlock(
      out.data(), reinterpret_cast<const unsigned char *>(padded.data()),
      static_cast<int>(padded.size()));
  if (n < 0 || static_cast<size_t>(n) < padded.size() - text.size())
    invalid();
  out.resize(static_cast<size_t>(n) - (padded.size() - text.size()));
  if (out.size() < header_size || out.size() > limit || encode(out) != text)
    invalid();
  return out;
}
void cipher(state &self, bool encrypt, const compaction_secret &key,
            std::vector<unsigned char> &envelope, const std::string &id,
            const void *source, void *destination, size_t size,
            const char *purpose) {
  secret derived;
  derive(key, derived, purpose);
  const auto associated = aad(self, id, purpose);
  std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(
      EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
  if (!ctx)
    throw std::bad_alloc();
  int n = 0;
  crypto(EVP_CipherInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr,
                           nullptr, encrypt) == 1 &&
         EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, 12, nullptr) ==
             1 &&
         EVP_CipherInit_ex(ctx.get(), nullptr, nullptr, derived.bytes.data(),
                           envelope.data() + 40, encrypt) == 1 &&
         EVP_CipherUpdate(ctx.get(), nullptr, &n, envelope.data(),
                          tag_offset) == 1 &&
         EVP_CipherUpdate(
             ctx.get(), nullptr, &n,
             reinterpret_cast<const unsigned char *>(associated.data()),
             static_cast<int>(associated.size())) == 1);
  for (size_t offset = 0; offset < size;) {
    cancelled(self);
    const size_t count = std::min(size - offset, size_t(65536));
    crypto(EVP_CipherUpdate(
               ctx.get(), static_cast<unsigned char *>(destination) + offset,
               &n, static_cast<const unsigned char *>(source) + offset,
               static_cast<int>(count)) == 1 &&
           n == static_cast<int>(count));
    offset += count;
  }
  if (!encrypt)
    crypto(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, 16,
                               envelope.data() + tag_offset) == 1);
  unsigned char tail[16];
  if (EVP_CipherFinal_ex(ctx.get(), tail, &n) != 1)
    invalid();
  crypto(n == 0);
  if (encrypt)
    crypto(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, 16,
                               envelope.data() + tag_offset) == 1);
}
} // namespace
std::string api_seal_item(state &self, api_item_purpose purpose,
                          const std::string &id,
                          const std::function<void(wire_json &)> &emit) {
  cancelled(self);
  identify_cache(self, true);
  const auto limit = self.compactor->max_item_bytes;
  api_plaintext plain;
  wire_json out(
      [&](const void *p, size_t n) {
        if (n > limit - header_size - plain.bytes.size())
          throw failure(AILO_OUT_OF_RANGE,
                        "Responses state exceeds its encrypted envelope limit");
        plain.bytes.append(static_cast<const char *>(p), n);
      },
      limit - header_size);
  emit(out);
  out.finish();
  std::vector<unsigned char> envelope(header_size + plain.bytes.size());
  std::memcpy(envelope.data(), "AILOAPI1", 8);
  const auto &key = *self.compactor->keys.front();
  std::memcpy(envelope.data() + 8, key.id_hash.data(), key.id_hash.size());
  crypto(RAND_bytes(envelope.data() + 40, 12) == 1);
  const auto issued = api_now();
  const uint64_t ttl = self.compactor->ttl_seconds;
  if (ttl > std::numeric_limits<uint64_t>::max() - issued)
    throw failure(
        AILO_OUT_OF_RANGE,
        "Responses encrypted item expiry exceeds its timestamp range");
  put(envelope.data() + 52, issued);
  put(envelope.data() + 60, ttl ? issued + ttl : 0);
  put(envelope.data() + 68, plain.bytes.size());
  cipher(self, true, key, envelope, id, plain.bytes.data(),
         envelope.data() + header_size, plain.bytes.size(), domain(purpose));
  return encode(envelope);
}
void api_open_item(state &self, api_item_purpose purpose, const char *id,
                   const char *encrypted, api_plaintext &plain) {
  if (!id || !encrypted)
    invalid();
  identify_cache(self, true);
  auto envelope = decode(self, encrypted);
  if (std::memcmp(envelope.data(), "AILOAPI1", 8) ||
      get(envelope.data() + 68) != envelope.size() - header_size)
    invalid();
  const compaction_secret *key = nullptr;
  for (const auto &candidate : self.compactor->keys)
    if (!CRYPTO_memcmp(candidate->id_hash.data(), envelope.data() + 8, 32))
      key = candidate.get();
  if (!key)
    invalid();
  plain.bytes.resize(envelope.size() - header_size);
  cipher(self, false, *key, envelope, id, envelope.data() + header_size,
         plain.bytes.data(), plain.bytes.size(), domain(purpose));
  const auto issued = get(envelope.data() + 52),
             expires = get(envelope.data() + 60), now = api_now();
  if (issued > now || (expires && (expires <= issued || expires <= now)))
    invalid();
}
} // namespace client
} // namespace ailo
