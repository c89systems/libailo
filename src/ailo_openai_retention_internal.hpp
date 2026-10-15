#ifndef AILO_OPENAI_RETENTION_INTERNAL_HPP
#define AILO_OPENAI_RETENTION_INTERNAL_HPP
#include "ailo_openai_request_internal.hpp"
#include <cstdint>

namespace ailo {
namespace client {
constexpr uint64_t openai_response_retention_seconds = 30 * 24 * 60 * 60;
// Trusted application configuration, never sourced from request JSON metadata,
// user, or safety_identifier. Repository admission/reservation is still
// required after resolving this plan and BEFORE calling the inference receiver.
struct openai_retention_policy {
  bool zero_data_retention = false;
  bool chat_store_by_default = true;
  bool repository_readable = false, repository_writable = false;
  bool conversation_readable = false, conversation_writable = false;
  std::string namespace_id;
  uint64_t response_retention_seconds = openai_response_retention_seconds;
  // No published Chat record TTL: zero means retain until explicit deletion.
  uint64_t chat_retention_seconds = 0;
};
struct openai_retention_plan {
  bool store_response = false;
  bool read_previous_response = false;
  bool read_conversation = false, append_conversation = false;
  bool reserve_record_storage = false;
  bool retain_output_in_memory = false;
  uint64_t response_expires_at = 0;
  // Conversations/items have no automatic TTL, independently of response TTL.
};
openai_retention_plan resolve_retention(const openai_request &,
                                        const openai_retention_policy &,
                                        uint64_t created_at);
} // namespace client
} // namespace ailo
#endif
