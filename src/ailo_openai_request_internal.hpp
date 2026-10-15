#ifndef AILO_OPENAI_REQUEST_INTERNAL_HPP
#define AILO_OPENAI_REQUEST_INTERNAL_HPP
#include "ailo_continuation_internal.hpp"
#include <map>

namespace ailo {
namespace client {
enum class openai_protocol { chat, responses };
struct openai_function {
  std::string type, name, description, parameters;
  bool strict = false;
  int requested_strict = -1;
};
// Owned, bounded INPUT only. Wire output and record admission are separate.
// Construct once, then keep alive and unchanged throughout generation.
struct openai_request {
  const openai_protocol protocol;
  ailo_request generation = ailo_request_default();
  bool streaming = false, include_usage = false, include_obfuscation = true;
  int store = -1; // -1: omitted/null; resolved by trusted retention policy
  bool automatic_compaction = false;
  size_t compact_threshold = 0;
  size_t max_builtin_tool_calls = 0;
  std::string model, previous_response_id, conversation_id, instructions;
  std::string user, safety_identifier, service_tier = "default";
  std::string schema, schema_name, schema_description, tool_name, cache_key,
      effort;
  bool schema_strict = false;
  std::map<std::string, std::string> metadata;
  transcript input;
  // Leading top-level Responses instructions affect this request only. Exclude
  // them when persisting input lineage for previous_response_id chaining.
  size_t instruction_items = 0;
  std::vector<ailo_item> items;
  std::vector<openai_function> functions;
  std::vector<ailo_function_tool> tools;
  std::vector<ailo_logit_bias> biases;
  std::vector<std::string> stops, allowed;
  std::vector<const char *> stop_views, allowed_views;
  openai_request(openai_protocol, const void *, size_t,
                 const std::string &served_model);
  openai_request(const openai_request &) = delete;
  openai_request &operator=(const openai_request &) = delete;
};
} // namespace client
} // namespace ailo
#endif
