#include "ailo_openai_retention_internal.hpp"
#include <limits>

namespace ailo {
namespace client {
openai_retention_plan resolve_retention(const openai_request &request,
                                        const openai_retention_policy &policy,
                                        uint64_t created_at) {
  if (policy.response_retention_seconds < openai_response_retention_seconds)
    throw failure(AILO_INVALID_ARGUMENT,
                  "Stored Responses require at least 30 days of retention");
  openai_retention_plan result;
  const bool responses = request.protocol == openai_protocol::responses;
  result.store_response =
      !policy.zero_data_retention &&
      (request.store < 0 ? responses || policy.chat_store_by_default
                         : request.store != 0);
  result.read_previous_response = !request.previous_response_id.empty();
  result.read_conversation = !request.conversation_id.empty();
  // Like OpenAI, explicit conversations are outside the ZDR-eligible response
  // contract. store=false prevents a response record, not conversation items.
  result.append_conversation = result.read_conversation;
  result.reserve_record_storage =
      result.store_response || result.append_conversation;
  const bool reads = result.read_previous_response || result.read_conversation;
  if (reads || result.reserve_record_storage) {
    if (policy.namespace_id.empty() || policy.namespace_id.size() > 256 ||
        policy.namespace_id.find('\0') != std::string::npos)
      throw failure(
          AILO_INVALID_ARGUMENT,
          "Retained API state requires a trusted repository namespace");
    require_utf8(policy.namespace_id.data(), policy.namespace_id.size());
    if ((reads && !policy.repository_readable) ||
        (result.reserve_record_storage && !policy.repository_writable))
      throw failure(
          AILO_UNSUPPORTED,
          "A repository with the required state operations is unavailable");
  }
  if (result.store_response) {
    const uint64_t ttl = responses ? policy.response_retention_seconds
                                   : policy.chat_retention_seconds;
    if (ttl > std::numeric_limits<uint64_t>::max() - created_at)
      throw failure(AILO_OUT_OF_RANGE,
                    "Stored response expiry exceeds its timestamp range");
    result.response_expires_at = ttl ? created_at + ttl : 0;
  }
  // Responses needs full terminal repeats even when store=false. Chat streaming
  // without storage never needs a complete output collector.
  result.retain_output_in_memory =
      responses || !request.streaming || result.store_response;
  return result;
}
} // namespace client
} // namespace ailo
