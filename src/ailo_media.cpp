#include "ailo_cache_internal.hpp"
#include "ailo_media_internal.hpp"
#include "ailo_model_internal.hpp"
#include "ailo_openai_internal.hpp"
#include "ailo_openai_request_internal.hpp"
#include "ggml/src/ggml-log-scope.h"
#include "mtmd-helper.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <climits>
#include <cstring>
#include <openssl/evp.h>

namespace ailo {
namespace client {
namespace {
std::atomic<size_t> decoded_bytes{0};
constexpr size_t decoded_process_limit = 512 * 1024 * 1024;
void cancelled(state &s) {
  if (s.is_cancelled())
    throw failure(AILO_CANCELLED, "Media operation cancelled");
}
bool progress(float, void *p) {
  return !static_cast<state *>(p)->is_cancelled();
}
bool evaluate_node(ggml_tensor *, bool ask, void *p) {
  const bool stop = static_cast<state *>(p)->is_cancelled();
  // Asking to observe a node forces backend synchronization/readback. Request
  // that only when cancellation needs a scheduler observation boundary.
  return ask ? stop : !stop;
}
struct resolution {
  state &owner;
  size_t limit;
  long timeout;
  std::string bytes;
  std::exception_ptr error;
  std::chrono::steady_clock::time_point start =
      std::chrono::steady_clock::now();
  void check() {
    cancelled(owner);
    if (std::chrono::steady_clock::now() - start >=
        std::chrono::milliseconds(timeout))
      throw failure(AILO_IO_FAILED, "Media resolver deadline exceeded");
  }
  static int sink(void *p, const void *data, size_t size) noexcept {
    auto &s = *static_cast<resolution *>(p);
    if (s.error)
      return -1;
    try {
      s.check();
      if ((!data && size) || size > s.limit - s.bytes.size())
        throw failure(AILO_OUT_OF_RANGE,
                      "Resolved media exceeds its byte limit");
      if (size)
        s.bytes.append(static_cast<const char *>(data), size);
      return 1;
    } catch (...) {
      s.error = std::current_exception();
      return -1;
    }
  }
};
} // namespace

media_state::media_state(const ailo_multimodal_config &input)
    : source(input.projector), config(input),
      device(input.device
                 ? std::string(input.device, strnlen(input.device, 129))
                 : "") {
  if (input.backend < AILO_BACKEND_AUTO ||
      input.backend > AILO_BACKEND_VULKAN || input.resolver_timeout_ms < 0 ||
      device.size() > 128 ||
      (input.backend == AILO_BACKEND_CPU && !device.empty()))
    throw failure(AILO_INVALID_ARGUMENT,
                  "Invalid multimodal placement or deadline");
  config.projector = source.value;
  config.device = device.empty() ? nullptr : device.c_str();
#define DEFAULT(field, value)                                                  \
  if (!config.field)                                                           \
  config.field = value
  DEFAULT(max_parts, 16);
  DEFAULT(max_part_bytes, 16 * 1024 * 1024);
  DEFAULT(max_total_bytes, 32 * 1024 * 1024);
  DEFAULT(max_image_pixels, 16000000);
  DEFAULT(max_decoded_bytes, 128 * 1024 * 1024);
  DEFAULT(max_audio_seconds, 60);
  DEFAULT(resolver_timeout_ms, 15000);
#undef DEFAULT
  if (config.max_parts > 4096 || config.max_part_bytes > INT_MAX ||
      config.max_total_bytes > 256 * 1024 * 1024 ||
      config.max_image_pixels > 64000000 ||
      config.max_decoded_bytes > decoded_process_limit ||
      config.max_audio_seconds > 3600 || config.resolver_timeout_ms > 3600000)
    throw failure(AILO_OUT_OF_RANGE,
                  "Multimodal resource limits exceed supported bounds");
  char *validated = nullptr;
  ailo_error error{};
  const auto status = ailo_model_cache_path(&source.value, &validated, &error);
  ailo_string_free(validated);
  if (status != AILO_OK)
    throw failure(status, error.message);
}

void media_state::load(state &self) {
  if (context)
    return;
  cancelled(self);
  auto params = mtmd_context_params_default();
  params.use_gpu = false;
  params.warmup = false;
  params.print_timings = false;
  params.n_threads = self.config.threads ? self.config.threads : 2;
  params.cb_eval = evaluate_node;
  params.cb_eval_user_data = &self;
  params.progress_callback = progress;
  params.progress_callback_user_data = &self;
  const auto available = gpu_devices();
  if (config.backend != AILO_BACKEND_CPU) {
    const std::string preferred = !device.empty() ? device
                                  : self.selected_info.empty()
                                      ? ""
                                      : self.selected_info.front().id;
    auto chosen = std::find_if(
        available.begin(), available.end(), [&](const device_info &d) {
          return d.eligible &&
                 (preferred.empty() ? config.backend == AILO_BACKEND_VULKAN ||
                                          self.actual == AILO_BACKEND_VULKAN
                                    : d.id == preferred);
        });
    if (chosen != available.end()) {
      params.use_gpu = true;
      params.device = chosen->device;
      actual_device = chosen->id;
    } else if (!device.empty() || config.backend == AILO_BACKEND_VULKAN)
      throw failure(AILO_UNSUPPORTED,
                    "Selected multimodal Vulkan device is unavailable");
  }
  if (!params.use_gpu)
    actual_device = "cpu";
  if (path.empty()) {
    auto options = source.value;
    struct acquisition {
      state &self;
      ailo_model_progress callback;
      void *user;
    } work{self, options.progress, options.user_data};
    options.user_data = &work;
    options.progress = [](void *p, unsigned n, unsigned count, ailo_u64 got,
                          ailo_u64 total) {
      auto &w = *static_cast<acquisition *>(p);
      return w.self.is_cancelled() ? 0
             : w.callback          ? w.callback(w.user, n, count, got, total)
                                   : 1;
    };
    char *acquired = nullptr;
    ailo_error error{};
    const auto status = ailo_model_acquire(&options, &acquired, &error);
    std::unique_ptr<char, decltype(&ailo_string_free)> owned(acquired,
                                                             ailo_string_free);
    if (status != AILO_OK)
      throw failure(status, error.message);
    path = acquired;
  }
  // Always bind actual bytes, even for local sources without supplied digests.
  digest = loaded_file_digest(self, path);
  context.reset(mtmd_init_from_file(path.c_str(), self.model.get(), params));
  cancelled(self);
  if (!context && params.use_gpu && config.backend == AILO_BACKEND_AUTO &&
      device.empty()) {
    params.use_gpu = false;
    params.device = nullptr;
    actual_device = "cpu";
    context.reset(mtmd_init_from_file(path.c_str(), self.model.get(), params));
    cancelled(self);
  }
  if (!context)
    throw failure(AILO_MODEL_FAILED,
                  "Matched multimodal encoder/projector setup failed");
  if (!mtmd_support_vision(context.get()) &&
      !mtmd_support_audio(context.get())) {
    context.reset();
    throw failure(AILO_UNSUPPORTED,
                  "Projector has no supported input modality");
  }
  gguf_init_params gguf_params{true, nullptr};
  std::unique_ptr<gguf_context, decltype(&gguf_free)> header(
      gguf_init_from_file(path.c_str(), gguf_params), gguf_free);
  const auto key =
      header ? gguf_find_key(header.get(), "clip.audio.projector_type") : -1;
  llama_token marker = LLAMA_TOKEN_NULL;
  asr_output =
      !mtmd_support_vision(context.get()) &&
      mtmd_support_audio(context.get()) &&
      metadata(self.model.get(), "general.architecture") == "qwen3vl" &&
      key >= 0 && gguf_get_kv_type(header.get(), key) == GGUF_TYPE_STRING &&
      !std::strcmp(gguf_get_val_str(header.get(), key), "qwen3a") &&
      llama_tokenize(llama_model_get_vocab(self.model.get()), "<asr_text>", 10,
                     &marker, 1, false, true) == 1;
}

std::shared_ptr<transcript> resolve_media(state &self,
                                          const ailo_request &request) {
  if (request.item_count && !request.items)
    throw failure(AILO_INVALID_ARGUMENT, "Request items are missing");
  if (request.item_count > 4096)
    throw failure(AILO_OUT_OF_RANGE, "Request exceeds 4096 items");
  bool present = false;
  for (size_t i = 0; i < request.item_count; ++i) {
    const auto &item = request.items[i];
    if (item.content_count > 4096 || (item.content_count && !item.content))
      throw failure(AILO_INVALID_ARGUMENT, "Invalid content array");
    for (size_t j = 0; j < item.content_count; ++j)
      present |= item.content[j].kind == AILO_CONTENT_IMAGE ||
                 item.content[j].kind == AILO_CONTENT_AUDIO;
  }
  if (!present)
    return {};
  if (!self.media)
    throw failure(AILO_UNSUPPORTED,
                  "Media input requires a matched multimodal projector");
  auto output = std::make_shared<transcript>();
  size_t count = 0, total = 0;
  for (size_t i = 0; i < request.item_count; ++i) {
    cancelled(self);
    auto item = request.items[i];
    std::vector<ailo_content> parts;
    std::vector<std::string> resolved(item.content_count);
    for (size_t j = 0; j < item.content_count; ++j) {
      auto part = item.content[j];
      if (part.kind == AILO_CONTENT_IMAGE || part.kind == AILO_CONTENT_AUDIO) {
        if (++count > self.media->config.max_parts)
          throw failure(AILO_OUT_OF_RANGE,
                        "Request exceeds its media part limit");
        const auto *meta = part.media;
        if (meta && (meta->detail < AILO_MEDIA_DETAIL_AUTO ||
                     meta->detail > AILO_MEDIA_DETAIL_ORIGINAL ||
                     meta->reference < AILO_MEDIA_INLINE ||
                     meta->reference > AILO_MEDIA_FILE_ID))
          throw failure(AILO_INVALID_ARGUMENT,
                        "Unknown media detail or reference kind");
        if (meta && meta->format && strnlen(meta->format, 33) > 32)
          throw failure(AILO_INVALID_ARGUMENT, "Media format name is too long");
        if (meta && meta->reference != AILO_MEDIA_INLINE) {
          if (!meta->value || !*meta->value ||
              strnlen(meta->value, 16385) > 16384)
            throw failure(AILO_INVALID_ARGUMENT, "Invalid media reference");
          require_utf8(meta->value, std::strlen(meta->value));
        }
        if (!part.size && meta && meta->reference != AILO_MEDIA_INLINE) {
          if (!meta->value || !*meta->value ||
              strnlen(meta->value, 16385) > 16384)
            throw failure(AILO_INVALID_ARGUMENT, "Invalid media reference");
          if (!self.media->config.resolver)
            throw failure(AILO_UNSUPPORTED,
                          "Media reference requires an application resolver");
          resolution work{self,
                          std::min(self.media->config.max_part_bytes,
                                   self.media->config.max_total_bytes - total),
                          self.media->config.resolver_timeout_ms,
                          {},
                          {}};
          ailo_error error{};
          const auto status = self.media->config.resolver(
              self.media->config.user_data, meta->reference, meta->value,
              work.limit, work.timeout, resolution::sink, &work, &error);
          if (work.error)
            std::rethrow_exception(work.error);
          work.check();
          if (status != AILO_OK)
            throw failure(status, "Application media resolution failed");
          resolved[j] = std::move(work.bytes);
          part.data = resolved[j].data();
          part.size = resolved[j].size();
        }
        if (!part.data || !part.size)
          throw failure(AILO_INVALID_ARGUMENT,
                        "Encoded media bytes are required");
        if (part.size > self.media->config.max_part_bytes ||
            part.size > self.media->config.max_total_bytes - total)
          throw failure(AILO_OUT_OF_RANGE, "Request media byte limit exceeded");
        total += part.size;
        const auto format = media_format(part.data, part.size, part.kind);
        if (meta && meta->format && format != meta->format)
          throw failure(AILO_INVALID_ARGUMENT,
                        "Declared media format contradicts the payload");
        if (part.kind == AILO_CONTENT_AUDIO && meta &&
            meta->detail != AILO_MEDIA_DETAIL_AUTO)
          throw failure(AILO_INVALID_ARGUMENT,
                        "Image detail cannot accompany audio");
      } else if (part.media)
        throw failure(AILO_INVALID_ARGUMENT,
                      "Media metadata requires image or audio content");
      parts.push_back(part);
    }
    item.content = parts.data();
    output->items.push_back(std::make_shared<owned_item>(
        item, output->bytes, 320 * 1024 * 1024, output->parts));
  }
  return output;
}
void resolve_media(state &self, openai_request &request) {
  const auto resolved = resolve_media(self, request.generation);
  if (!resolved)
    return;
  request.input = *resolved;
  request.items = request.input.views();
  request.generation.items = request.items.data();
  request.generation.item_count = request.items.size();
}

void media_write_base64(const void *data, size_t size,
                        const std::function<void(const char *, size_t)> &sink) {
  const auto *bytes = static_cast<const unsigned char *>(data);
  std::array<unsigned char, 4097> encoded{};
  for (size_t offset = 0; offset < size;) {
    const size_t n = std::min(size - offset, size_t(3072));
    const auto written =
        EVP_EncodeBlock(encoded.data(), bytes + offset, static_cast<int>(n));
    if (written < 0)
      throw failure(AILO_INTERNAL_ERROR, "Media base64 encoding failed");
    sink(reinterpret_cast<const char *>(encoded.data()),
         static_cast<size_t>(written));
    offset += n;
  }
}
std::string media_base64(const void *data, size_t size) {
  std::string output;
  media_write_base64(data, size,
                     [&](const char *p, size_t n) { output.append(p, n); });
  return output;
}
void media_base64_decoder::push(const char *p, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    const unsigned char ch = static_cast<unsigned char>(p[i]);
    if (padded ||
        !((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
          (ch >= '0' && ch <= '9') || ch == '+' || ch == '/' || ch == '='))
      throw failure(AILO_INVALID_ARGUMENT, "Media base64 is malformed");
    pending += static_cast<char>(ch);
    if (pending.size() != 4)
      continue;
    unsigned char decoded[3], canonical[5];
    const int decoded_size = EVP_DecodeBlock(
        decoded, reinterpret_cast<const unsigned char *>(pending.data()), 4);
    const size_t padding = pending[2] == '=' ? 2 : pending[3] == '=' ? 1 : 0;
    const size_t count = 3 - padding;
    if (decoded_size != 3 ||
        EVP_EncodeBlock(canonical, decoded, static_cast<int>(count)) != 4 ||
        std::memcmp(canonical, pending.data(), 4))
      throw failure(AILO_INVALID_ARGUMENT, "Media base64 is not canonical");
    if (count > limit - output.size())
      throw failure(AILO_OUT_OF_RANGE, "Decoded media exceeds its byte limit");
    output.append(reinterpret_cast<const char *>(decoded), count);
    padded = padding != 0;
    pending.clear();
  }
}
std::string media_base64_decoder::finish() {
  if (!pending.empty() || output.empty())
    throw failure(AILO_INVALID_ARGUMENT, "Media base64 is empty or incomplete");
  return std::move(output);
}
std::string media_unbase64(const std::string &s, size_t limit) {
  media_base64_decoder decoder(limit);
  decoder.push(s.data(), s.size());
  return decoder.finish();
}

media_prompt::media_prompt(state &self, const conversation &input,
                           const std::string &prompt)
    : owner(self) {
  if (!self.media)
    throw failure(AILO_UNSUPPORTED, "Multimodal projector is not configured");
  self.media->load(self);
  const auto preprocessing_start = std::chrono::steady_clock::now();
  try {
    if (input.media.size() > self.media->config.max_parts)
      throw failure(AILO_OUT_OF_RANGE,
                    "Conversation exceeds its media part limit");
    std::vector<std::string> texts;
    texts.reserve(input.media.size() + 1);
    size_t offset = 0, encoded = 0;
    std::string key;
    for (const auto &part : input.media) {
      cancelled(self);
      if (part->bytes.size() > self.media->config.max_part_bytes ||
          part->bytes.size() > self.media->config.max_total_bytes - encoded)
        throw failure(AILO_OUT_OF_RANGE,
                      "Conversation exceeds its media byte limit");
      encoded += part->bytes.size();
      if ((part->kind == AILO_CONTENT_IMAGE &&
           !mtmd_support_vision(self.media->context.get())) ||
          (part->kind == AILO_CONTENT_AUDIO &&
           !mtmd_support_audio(self.media->context.get())))
        throw failure(AILO_UNSUPPORTED,
                      "Loaded projector does not support this input modality");
      const size_t at = prompt.find(part->marker, offset);
      if (at == std::string::npos ||
          prompt.find(part->marker, at + part->marker.size()) !=
              std::string::npos)
        throw failure(AILO_UNSUPPORTED,
                      "Chat template drops or duplicates a media part");
      texts.emplace_back(prompt.substr(offset, at - offset));
      offset = at + part->marker.size();
      bitmaps.emplace_back(
          decode_media(
              self, *part,
              [&](size_t bytes) {
                cancelled(self);
                if (bytes > self.media->config.max_decoded_bytes - reserved)
                  throw failure(AILO_OUT_OF_RANGE,
                                "Decoded media request budget exceeded");
                size_t used = decoded_bytes.load();
                do {
                  if (bytes > decoded_process_limit - used)
                    throw failure(
                        AILO_BUSY,
                        "Concurrent decoded media budget is exhausted");
                } while (
                    !decoded_bytes.compare_exchange_weak(used, used + bytes));
                reserved += bytes;
              }),
          mtmd_bitmap_free);
      if (!bitmaps.back())
        throw failure(AILO_MODEL_FAILED,
                      "Cannot allocate decoded media bitmap");
      const auto digest =
          model::digest_hex(part->bytes.data(), part->bytes.size());
      mtmd_bitmap_set_id(bitmaps.back().get(), digest.c_str());
      key += std::to_string(part->kind) + ':' + std::to_string(part->detail) +
             ':' + digest + ':';
    }
    texts.emplace_back(prompt.substr(offset));
    std::vector<mtmd_input_text> text_views;
    std::vector<mtmd_input_part> parts;
    text_views.reserve(texts.size());
    parts.reserve(texts.size() + bitmaps.size());
    for (size_t i = 0; i < texts.size(); ++i) {
      text_views.push_back({texts[i].data(), texts[i].size(), false, true});
      parts.push_back({&text_views.back(), nullptr});
      key += std::to_string(texts[i].size()) + ':' + texts[i];
      if (i < bitmaps.size())
        parts.push_back({nullptr, bitmaps[i].get()});
    }
    std::vector<const mtmd_input_part *> views;
    for (const auto &part : parts)
      views.push_back(&part);
    chunks.reset(mtmd_input_chunks_init());
    if (!chunks ||
        mtmd_tokenize_from_parts(self.media->context.get(), chunks.get(),
                                 views.data(), views.size(), true))
      throw failure(AILO_MODEL_FAILED, "Multimodal prompt tokenization failed");
    tokens = mtmd_helper_get_n_tokens(chunks.get());
    const auto n_pos = mtmd_helper_get_n_pos(chunks.get());
    if (!tokens || n_pos <= 0 || tokens > INT_MAX)
      throw failure(AILO_CONTEXT_LIMIT,
                    "Multimodal token/position count is invalid");
    positions = static_cast<size_t>(n_pos);
    for (size_t i = 0; i < mtmd_input_chunks_size(chunks.get()); ++i) {
      const auto *chunk = mtmd_input_chunks_get(chunks.get(), i);
      if (mtmd_input_chunk_get_type(chunk) == MTMD_INPUT_CHUNK_TYPE_TEXT) {
        size_t n = 0;
        const auto *p = mtmd_input_chunk_get_tokens_text(chunk, &n);
        text_tokens.insert(text_tokens.end(), p, p + n);
      } else if (mtmd_decode_use_non_causal(self.media->context.get(), chunk) &&
                 mtmd_input_chunk_get_n_tokens(chunk) >
                     llama_n_ubatch(self.context.get()))
        throw failure(
            AILO_CONTEXT_LIMIT,
            "Non-causal media requires a larger physical evaluation batch");
    }
    const auto *last = mtmd_input_chunks_get(
        chunks.get(), mtmd_input_chunks_size(chunks.get()) - 1);
    if (mtmd_input_chunk_get_type(last) != MTMD_INPUT_CHUNK_TYPE_TEXT ||
        text_tokens.size() < 2)
      throw failure(
          AILO_UNSUPPORTED,
          "Multimodal chat template must end with a text generation prefix");
    identity = model::digest_hex(key.data(), key.size());
    self.media->template_validated = true;
    const auto log = ggml_log_scope_get();
    if (log.callback) {
      const auto message = "media_preprocess_us=" +
                           std::to_string(elapsed_us(preprocessing_start));
      log.callback(GGML_LOG_LEVEL_TRACE, message.c_str(), log.user_data);
    }
  } catch (...) {
    chunks.reset();
    bitmaps.clear();
    decoded_bytes.fetch_sub(reserved);
    reserved = 0;
    throw;
  }
}
media_prompt::~media_prompt() {
  chunks.reset();
  bitmaps.clear();
  decoded_bytes.fetch_sub(reserved);
}
void media_prompt::evaluate_prefix() {
  llama_pos past = 0;
  for (size_t i = 0; i < mtmd_input_chunks_size(chunks.get()); ++i) {
    cancelled(owner);
    const auto *chunk = mtmd_input_chunks_get(chunks.get(), i);
    if (i + 1 == mtmd_input_chunks_size(chunks.get())) {
      size_t n = 0;
      const auto *tokens = mtmd_input_chunk_get_tokens_text(chunk, &n);
      std::unique_ptr<llama_batch_ext, decltype(&llama_batch_ext_free)> batch(
          llama_batch_ext_init(owner.context.get()), llama_batch_ext_free);
      if (!batch)
        throw std::bad_alloc();
      for (size_t at = 0; at + 1 < n;) {
        cancelled(owner);
        llama_batch_ext_clear(batch.get());
        const size_t end =
            std::min(n - 1, at + llama_n_batch(owner.context.get()));
        for (; at < end; ++at) {
          const auto index =
              llama_batch_ext_add_token(batch.get(), 0, tokens[at]);
          if (index < 0)
            throw failure(AILO_MODEL_FAILED,
                          "Cannot construct multimodal text evaluation batch");
          llama_batch_ext_set_pos(batch.get(), index, &past);
          ++past;
        }
        if (llama_process(owner.context.get(), LLAMA_PROCESS_TYPE_DECODE,
                          batch.get()))
          throw failure(owner.is_cancelled() ? AILO_CANCELLED
                                             : AILO_MODEL_FAILED,
                        "Multimodal trailing text evaluation failed");
      }
      break;
    }
    if (mtmd_helper_eval_chunk_single(
            owner.media->context.get(), owner.context.get(), chunk, past, 0,
            static_cast<int32_t>(llama_n_batch(owner.context.get())), false,
            &past))
      throw failure(owner.is_cancelled() ? AILO_CANCELLED : AILO_MODEL_FAILED,
                    "Multimodal encoder/evaluation failed");
  }
  if (past != static_cast<llama_pos>(positions - 1))
    throw failure(AILO_MODEL_FAILED, "Multimodal evaluation position mismatch");
  if (owner.cache)
    owner.cache->evaluated_tokens.fetch_add(tokens - 1);
}
size_t media_token_count(state &self, const conversation &input,
                         const std::string &prompt) {
  media_prompt prepared(self, input, prompt);
  return std::max(prepared.tokens, prepared.positions);
}
void write_media_part(wire_json &out, const ailo_content &part, bool chat,
                      bool original_reference) {
  const auto format = media_format(part.data, part.size, part.kind);
  const auto *meta = part.media;
  const bool image = part.kind == AILO_CONTENT_IMAGE;
  out.object();
  out.key("type");
  out.string(image ? (chat ? "image_url" : "input_image") : "input_audio");
  out.key(image ? "image_url" : "input_audio");
  if (chat || !image)
    out.object();
  if (chat && image)
    out.key("url");
  if (!image)
    out.key("data");
  if (image && original_reference && meta &&
      meta->reference == AILO_MEDIA_URL && meta->value)
    out.string(meta->value);
  else {
    out.string_begin();
    if (image) {
      const std::string prefix =
          std::string("data:") + media_mime(format) + ";base64,";
      out.string_chunk(prefix.data(), prefix.size());
    }
    media_write_base64(part.data, part.size, [&](const char *p, size_t n) {
      out.string_chunk(p, n);
    });
    out.string_end();
  }
  if (image) {
    out.key("detail");
    const auto detail = meta ? meta->detail : AILO_MEDIA_DETAIL_AUTO;
    out.string(detail == AILO_MEDIA_DETAIL_LOW        ? "low"
               : detail == AILO_MEDIA_DETAIL_HIGH     ? "high"
               : detail == AILO_MEDIA_DETAIL_ORIGINAL ? "original"
                                                      : "auto");
  } else {
    out.key("format");
    out.string(format);
  }
  if (chat || !image)
    out.end_object();
  out.end_object();
}
void audio_text_stream::push(const char *data, size_t size, uint64_t at) {
  if (complete) {
    if (size)
      sink(data, size, at);
    return;
  }
  static const std::string prefix = "language ", delimiter = "<asr_text>";
  for (size_t i = 0; i < size; ++i) {
    if (pending.empty())
      origin = at + i;
    pending += data[i];
    if (pending.size() <= prefix.size() &&
        prefix.compare(0, pending.size(), pending)) {
      complete = true;
      if (pending.size() > 1)
        sink(pending.data(), pending.size() - 1, origin);
      pending.clear();
      sink(data + i, size - i, at + i);
      return;
    }
    if (pending.size() > 128)
      throw failure(AILO_MODEL_FAILED,
                    "ASR control header exceeds its bounded lookahead");
    const auto marker = pending.find(delimiter);
    if (marker != std::string::npos) {
      if (marker <= prefix.size())
        throw failure(AILO_MODEL_FAILED, "ASR language header is empty");
      for (size_t j = prefix.size(); j < marker; ++j) {
        const unsigned char c = static_cast<unsigned char>(pending[j]);
        if (c < 32 || c > 126 || c == '<' || c == '>')
          throw failure(AILO_MODEL_FAILED, "ASR language header is malformed");
      }
      complete = true;
      pending.clear();
      if (i + 1 < size)
        sink(data + i + 1, size - i - 1, at + i + 1);
      return;
    }
  }
}
void audio_text_stream::finish(bool truncated) {
  if (!complete && !pending.empty() && !truncated)
    throw failure(AILO_MODEL_FAILED,
                  "ASR control header ended before transcript content");
  pending.clear();
}
} // namespace client
} // namespace ailo
