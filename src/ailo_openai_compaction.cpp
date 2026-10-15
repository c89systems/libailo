#include "ailo_bounded_json_internal.hpp"
#include "ailo_cache_internal.hpp"
#include "ailo_compaction_internal.hpp"
#include "ailo_logging_internal.hpp"
#include "ailo_openai_internal.hpp"
#include "ailo_reasoning_internal.hpp"
#include "ailo_tools_internal.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>

#define AILO_PUBLIC extern "C" __attribute__((visibility("default")))
namespace ailo {
namespace client {
namespace {
constexpr size_t header_size = 92, tag_offset = 76;
constexpr char domain[] = "libailo.responses.compaction:1";
[[noreturn]] void invalid() {
  throw failure(AILO_INVALID_ARGUMENT,
                "Responses compaction authentication or window binding failed");
}
void crypto(bool ok) {
  if (!ok)
    throw failure(AILO_INTERNAL_ERROR,
                  "Responses compaction cryptographic operation failed");
}
void cancelled(state &self) {
  if (self.is_cancelled())
    throw failure(AILO_CANCELLED, "Responses compaction cancelled");
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
struct plaintext {
  std::string bytes;
  ~plaintext() {
    if (!bytes.empty())
      OPENSSL_cleanse(bytes.data(), bytes.size());
  }
};
struct secret {
  compaction_digest bytes{};
  ~secret() { OPENSSL_cleanse(bytes.data(), bytes.size()); }
};
std::string aad(state &self, const std::string &id) {
  std::string value(domain);
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
void derive(const compaction_secret &key, secret &out) {
  std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> ctx(
      EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr), EVP_PKEY_CTX_free);
  if (!ctx)
    throw std::bad_alloc();
  size_t size = out.bytes.size();
  crypto(EVP_PKEY_derive_init(ctx.get()) == 1 &&
         EVP_PKEY_CTX_set_hkdf_md(ctx.get(), EVP_sha256()) == 1 &&
         EVP_PKEY_CTX_set1_hkdf_salt(
             ctx.get(), reinterpret_cast<const unsigned char *>(domain),
             sizeof(domain) - 1) == 1 &&
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
            const void *source, void *destination, size_t size) {
  secret derived;
  derive(key, derived);
  const auto associated = aad(self, id);
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
sealed_summary seal(state &self, const std::vector<ailo_item> &items,
                    const std::vector<ailo_item> *output = nullptr,
                    const std::string &id = {}) {
  cancelled(self);
  const auto limit = self.compactor->max_item_bytes;
  plaintext plain;
  wire_json out(
      [&](const void *p, size_t n) {
        if (n > limit - header_size - plain.bytes.size())
          throw failure(AILO_OUT_OF_RANGE,
                        "Responses compaction window exceeds its encrypted "
                        "envelope limit");
        plain.bytes.append(static_cast<const char *>(p), n);
      },
      limit - header_size);
  out.object();
  out.key("input");
  out.array();
  for (const auto &i : items)
    api_canonical_item(out, i);
  out.end_array();
  if (output) {
    out.key("ailo_automatic");
    out.boolean(true);
    out.key("output");
    out.array();
    for (const auto &i : *output)
      api_canonical_item(out, i);
    out.end_array();
  }
  out.end_object();
  out.finish();
  ailo_api_info identity{};
  if (id.empty())
    api_identity(identity, "cmp_");
  sealed_summary result{id.empty() ? identity.id : id, {}};
  std::vector<unsigned char> envelope(header_size + plain.bytes.size());
  std::memcpy(envelope.data(), "AILOAPI1", 8);
  const auto &key = *self.compactor->keys.front();
  std::memcpy(envelope.data() + 8, key.id_hash.data(), key.id_hash.size());
  crypto(RAND_bytes(envelope.data() + 40, 12) == 1);
  const auto issued = api_now();
  const uint64_t ttl = self.compactor->ttl_seconds;
  if (ttl > std::numeric_limits<uint64_t>::max() - issued)
    throw failure(AILO_OUT_OF_RANGE,
                  "Responses compaction expiry exceeds its timestamp range");
  put(envelope.data() + 52, issued);
  put(envelope.data() + 60, ttl ? issued + ttl : 0);
  put(envelope.data() + 68, plain.bytes.size());
  cipher(self, true, key, envelope, result.id, plain.bytes.data(),
         envelope.data() + header_size, plain.bytes.size());
  result.encrypted_content = encode(envelope);
  return result;
}
void open(state &self, const ailo_item &item, plaintext &plain) {
  if (!item.id || !item.encrypted_content)
    invalid();
  const auto id = std::string(item.id);
  auto envelope = decode(self, item.encrypted_content);
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
         plain.bytes.data(), plain.bytes.size());
  const auto issued = get(envelope.data() + 52),
             expires = get(envelope.data() + 60), now = api_now();
  if (issued > now || (expires && (expires <= issued || expires <= now)))
    invalid();
}
void add(transcript &out, const ailo_item &item) {
  if (out.items.size() == 4096)
    throw failure(AILO_OUT_OF_RANGE, "Responses compaction exceeds 4096 items");
  out.items.push_back(std::make_shared<owned_item>(item, out.bytes,
                                                   4 * 1024 * 1024, out.parts));
}
size_t token_position(const std::vector<ailo_item> &items, bool required) {
  size_t position = items.size();
  for (size_t i = 0; i < items.size(); ++i)
    if (items[i].kind == AILO_ITEM_COMPACTION) {
      if (position != items.size())
        invalid();
      position = i;
    }
  if (required && position == items.size())
    invalid();
  return position;
}
void bind_request(openai_request &request, transcript &&input) {
  request.input = std::move(input);
  request.items = request.input.views();
  request.generation.items = request.items.data();
  request.generation.item_count = request.items.size();
}
void shift_sources(compaction_payload &payload, size_t prefix, bool remove,
                   size_t limit) {
  if (!prefix)
    return;
  const auto facts = parse_summary(payload.summary, payload.source_items);
  bounded_json out(limit);
  out.object();
  out.key("facts");
  out.array();
  for (const auto &fact : facts) {
    if (remove && fact.source < prefix)
      invalid();
    out.object();
    out.key("source");
    out.number(remove ? fact.source - prefix : fact.source + prefix);
    for (const auto &p :
         {std::pair<const char *, const std::string &>{"kind", fact.kind},
          {"role", fact.role},
          {"id", fact.id},
          {"call_id", fact.call_id},
          {"name", fact.name},
          {"category", fact.category},
          {"text", fact.text}}) {
      out.key(p.first);
      out.string(p.second);
    }
    out.end_object();
  }
  out.end_array();
  out.end_object();
  auto summary = out.finish();
  OPENSSL_cleanse(payload.summary.data(), payload.summary.size());
  payload.summary = std::move(summary);
}
} // namespace

// Canonical wire windows exclude ephemeral top-level instructions. Native
// binding is authenticated before removing that prefix and resealing the token.
static transcript canonical_window(state &self,
                                   const std::vector<ailo_item> &items,
                                   size_t instructions) {
  if (instructions > items.size())
    invalid();
  transcript result;
  const size_t position = token_position(items, false);
  if (position == items.size()) {
    for (size_t i = instructions; i < items.size(); ++i)
      add(result, items[i]);
    return result;
  }
  if (position < instructions)
    invalid();
  self.ensure_loaded();
  identify_cache(self, true);
  auto payload = self.compactor->open(items[position].id,
                                      items[position].encrypted_content,
                                      self.conversation_identity, &self);
  verify_compaction_binding(*payload, items.data(), items.size(), position);
  if (payload->cut < instructions || payload->retained.size() < instructions ||
      payload->source_items < instructions)
    invalid();
  shift_sources(*payload, instructions, true, self.compactor->max_item_bytes);
  payload->retained.erase(payload->retained.begin(),
                          payload->retained.begin() +
                              static_cast<ptrdiff_t>(instructions));
  payload->cut -= instructions;
  payload->source_items -= instructions;
  const auto native =
      self.compactor->seal(*payload, self.conversation_identity, &self);
  std::vector<ailo_item> window(
      items.begin() + static_cast<ptrdiff_t>(instructions), items.end());
  const size_t cut = position - instructions;
  window[cut].id = native.id.c_str();
  window[cut].encrypted_content = native.encrypted_content.c_str();
  for (const auto &i : window)
    add(result, i);
  return result;
}
transcript api_compacted_window(state &self,
                                const std::vector<ailo_item> &items,
                                size_t instructions) {
  auto result = canonical_window(self, items, instructions);
  const auto window = result.views();
  const size_t cut = token_position(window, false);
  if (cut == window.size())
    return result;
  const auto wire = seal(self, window);
  auto token = window[cut];
  token.id = wire.id.c_str();
  token.encrypted_content = wire.encrypted_content.c_str();
  transcript replaced;
  for (size_t i = 0; i < window.size(); ++i)
    add(replaced, i == cut ? token : window[i]);
  return replaced;
}
sealed_summary api_automatic_window(state &self,
                                    const std::vector<ailo_item> &items,
                                    size_t instructions,
                                    const std::vector<ailo_item> &output,
                                    const std::string &id) {
  const auto window = canonical_window(self, items, instructions);
  return seal(self, window.views(), &output, id);
}
void api_expand_compaction(state &self, openai_request &request) {
  size_t position = request.items.size();
  for (size_t i = 0; i < request.items.size(); ++i)
    if (request.items[i].kind == AILO_ITEM_COMPACTION)
      position = i;
  if (position == request.items.size())
    return;
  self.ensure_loaded();
  identify_cache(self, true);
  plaintext plain;
  open(self, request.items[position], plain);
  openai_request decoded(openai_protocol::responses, plain.bytes.data(),
                         plain.bytes.size(), self.id, "input");
  const size_t cut = token_position(decoded.items, true),
               count = decoded.items.size(), prefix = request.instruction_items;
  if (!decoded.record_automatic) {
    (void)token_position(request.items, false);
    if (position != prefix + cut || count > request.items.size() - prefix)
      invalid();
    for (size_t i = 0; i < count; ++i)
      if (i != cut && fingerprint(decoded.items[i]) !=
                          fingerprint(request.items[prefix + i]))
        invalid();
  }
  auto payload = self.compactor->open(decoded.items[cut].id,
                                      decoded.items[cut].encrypted_content,
                                      self.conversation_identity, &self);
  verify_compaction_binding(*payload, decoded.items.data(), count, cut);
  if (prefix > 4096 - payload->source_items)
    throw failure(
        AILO_OUT_OF_RANGE,
        "Responses instruction prefix exceeds its compaction item budget");
  shift_sources(*payload, prefix, false, self.compactor->max_item_bytes);
  std::vector<compaction_digest> retained;
  for (size_t i = 0; i < prefix; ++i)
    retained.push_back(fingerprint(request.items[i]));
  retained.insert(retained.end(), payload->retained.begin(),
                  payload->retained.end());
  payload->retained = std::move(retained);
  payload->cut += prefix;
  payload->source_items += prefix;
  const auto native =
      self.compactor->seal(*payload, self.conversation_identity, &self);
  transcript expanded;
  for (size_t i = 0; i < prefix; ++i)
    add(expanded, request.items[i]);
  for (size_t i = 0; i < count; ++i) {
    auto item = decoded.items[i];
    if (i == cut) {
      item.id = native.id.c_str();
      item.encrypted_content = native.encrypted_content.c_str();
    }
    add(expanded, item);
  }
  if (decoded.record_automatic) {
    openai_request output(openai_protocol::responses, plain.bytes.data(),
                          plain.bytes.size(), self.id, "output");
    for (const auto &i : output.items)
      add(expanded, i);
  }
  const size_t suffix =
      decoded.record_automatic ? position + 1 : prefix + count;
  for (size_t i = suffix; i < request.items.size(); ++i)
    add(expanded, request.items[i]);
  bind_request(request, std::move(expanded));
}
} // namespace client
} // namespace ailo

AILO_PUBLIC ailo_status ailo_client_response_compact(
    ailo_client *client, const void *json, size_t size, ailo_output_sink sink,
    void *user, ailo_api_info *info, ailo_error *error) {
  using namespace ailo::client;
  if (info)
    *info = {};
  if (!sink || !info) {
    if (info)
      info->http_status = 400;
    return ailo_error_set(error, AILO_INVALID_ARGUMENT,
                          "API sink and output info are required");
  }
  const auto status = boundary(error, [&] {
    operation call(client);
    auto &self = call.self;
    openai_request request(openai_protocol::compact, json, size, self.id);
    // Compaction itself is stateless. Only an explicit prior-response read
    // needs a durable repository; no record is reserved or published here.
    request.store = 0;
    (void)resolve_retention(request, self.openai->policy, api_now());
    api_prior_input(self, request);
    api_expand_compaction(self, request);
    self.ensure_loaded();
    identify_cache(self, true);
    log_scope logs(self.logging.get(), log_phase::compact);
    auto tools = prepare_tools(request.generation);
    const auto prompt =
        render_prompt(self, prepare_replay(self, request.generation), tools,
                      request.generation)
            .prompt;
    if (prompt_token_count(self, prompt) > llama_n_ctx(self.context.get()))
      throw failure(
          AILO_CONTEXT_LIMIT,
          "Standalone Responses compaction input exceeds the model context");
    ailo_compaction_info compact{};
    ailo_usage work{};
    compaction_task task{};
    task.instructions =
        request.instructions.empty() ? nullptr : request.instructions.c_str();
    task.usage = &work;
    std::unique_ptr<ailo_continuation, decltype(&ailo_continuation_destroy)>
        window(compact_window(self, request.generation, self.openai->compaction,
                              compact, nullptr, &task),
               ailo_continuation_destroy);
    const auto canonical = api_compacted_window(
        self, get_continuation(window.get()).current->views(),
        request.instruction_items);
    if (canonical.bytes > self.openai->memory_limit)
      throw failure(AILO_OUT_OF_RANGE,
                    "Responses compaction output exceeds its memory limit");
    info->content_type = "application/json";
    api_identity(*info, "resp_");
    info->run.actual_backend = self.actual;
    info->run.usage = work;
    info->run.usage.compaction_prompt_tokens = compact.summary_prompt_tokens;
    info->run.usage.compaction_completion_tokens =
        compact.summary_completion_tokens;
    info->run.usage.compaction_passes = compact.summary_passes;
    wire_json out(api_sink(self, sink, user, *info), self.openai->record_limit);
    out.object();
    out.key("id");
    out.string(info->id);
    out.key("object");
    out.string("response.compaction");
    out.key("created_at");
    out.number(api_now());
    out.key("output");
    out.array();
    for (const auto &i : canonical.items)
      api_canonical_item(out, i->view());
    out.end_array();
    out.key("usage");
    out.object();
    const auto input_tokens =
                   ailo::detail::to_native(compact.summary_prompt_tokens),
               output_tokens =
                   ailo::detail::to_native(compact.summary_completion_tokens);
    out.key("input_tokens");
    out.number(input_tokens);
    out.key("output_tokens");
    out.number(output_tokens);
    out.key("total_tokens");
    out.number(input_tokens + output_tokens);
    out.key("input_tokens_details");
    out.object();
    out.key("cached_tokens");
    out.number(0);
    out.key("cache_write_tokens");
    out.number(0);
    out.end_object();
    out.key("output_tokens_details");
    out.object();
    out.key("reasoning_tokens");
    out.number(ailo::detail::to_native(work.reasoning_tokens));
    out.end_object();
    out.end_object();
    out.end_object();
    out.finish();
  });
  if (!info->started)
    info->http_status = api_http_status(status);
  return status;
}
