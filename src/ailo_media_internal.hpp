#ifndef AILO_MEDIA_INTERNAL_HPP
#define AILO_MEDIA_INTERNAL_HPP
#include "ailo_client_internal.hpp"
#include "ailo_continuation_internal.hpp"
#include "ailo_conversation_internal.hpp"
#include "mtmd.h"
#include <functional>

namespace ailo {
namespace client {
struct media_state {
  source_copy source;
  ailo_multimodal_config config;
  std::string device, path, digest, actual_device;
  publisher_metadata publisher;
  bool publisher_requested = false;
  bool asr_output = false, template_validated = false;
  std::unique_ptr<mtmd_context, decltype(&mtmd_free)> context{nullptr,
                                                              mtmd_free};
  explicit media_state(const ailo_multimodal_config &);
  void load(state &);
};
struct media_part {
  ailo_content_kind kind;
  std::string bytes, marker;
  ailo_media_detail detail = AILO_MEDIA_DETAIL_AUTO;
};
struct media_prompt {
  state &owner;
  std::unique_ptr<mtmd_input_chunks, decltype(&mtmd_input_chunks_free)> chunks{
      nullptr, mtmd_input_chunks_free};
  std::vector<std::unique_ptr<mtmd_bitmap, decltype(&mtmd_bitmap_free)>>
      bitmaps;
  std::vector<llama_token> text_tokens;
  size_t tokens = 0, positions = 0, reserved = 0;
  std::string identity;
  media_prompt(state &, const conversation &, const std::string &);
  ~media_prompt();
  void evaluate_prefix();
  media_prompt(const media_prompt &) = delete;
};
mtmd_bitmap *decode_media(state &, const media_part &,
                          const std::function<void(size_t)> &);
mtmd_bitmap *decode_audio(state &, const media_part &, int,
                          const std::function<void(size_t)> &);
class media_base64_decoder {
  std::string output, pending;
  size_t limit;
  bool padded = false;

public:
  explicit media_base64_decoder(size_t maximum) : limit(maximum) {}
  void push(const char *, size_t);
  std::string finish();
};
std::shared_ptr<transcript> resolve_media(state &, const ailo_request &);
void resolve_media(state &, struct openai_request &);
std::string media_format(const void *, size_t, ailo_content_kind);
const char *media_mime(const std::string &);
std::string media_base64(const void *, size_t);
std::string media_unbase64(const std::string &, size_t);
void media_write_base64(const void *, size_t,
                        const std::function<void(const char *, size_t)> &);
size_t media_token_count(state &, const conversation &, const std::string &);
class wire_json;
void write_media_part(wire_json &, const ailo_content &, bool chat,
                      bool original_reference = false);
/** Bounded Qwen3-ASR control header filter. Transcript bytes flow immediately;
 * offsets continue to refer to raw sampled bytes for logprob provenance. */
class audio_text_stream {
  std::function<void(const char *, size_t, uint64_t)> sink;
  std::string pending;
  uint64_t origin = UINT64_MAX;
  bool complete = false;

public:
  explicit audio_text_stream(
      std::function<void(const char *, size_t, uint64_t)> output)
      : sink(std::move(output)) {}
  void push(const char *, size_t, uint64_t);
  void finish(bool truncated);
  uint64_t pending_offset() const {
    return pending.empty() ? UINT64_MAX : origin;
  }
};
} // namespace client
} // namespace ailo
#endif
