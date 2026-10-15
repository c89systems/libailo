#include "ailo_conversation_internal.hpp"
#include "ailo_openai_request_internal.hpp"
#include "ailo_openai_retention_internal.hpp"
#include "ailo_tools_internal.hpp"
#include <cstdio>
#include <cstring>
#include <limits>

using namespace ailo::client;
#define CHECK(c)                                                               \
  do {                                                                         \
    if (!(c))                                                                  \
      throw std::runtime_error(std::string(#c) + " at " +                      \
                               std::to_string(__LINE__));                      \
  } while (0)
void reject(openai_protocol p, const std::string &json, ailo_status wanted) {
  try {
    openai_request value(p, json.data(), json.size(), "local-model");
  } catch (const failure &e) {
    if (e.status != wanted)
      std::fprintf(stderr, "Rejected with %d, expected %d: %s (%s)\n", e.status,
                   wanted, e.what(), json.c_str());
    CHECK(e.status == wanted);
    return;
  }
  throw std::runtime_error("Unexpectedly accepted: " + json);
}
struct observation {
  std::string text;
  int action = 1;
  unsigned deltas = 0, completed = 0;
  static int event(void *user, const ailo_event *e) {
    auto &out = *static_cast<observation *>(user);
    if (e->kind == AILO_EVENT_TEXT_DELTA) {
      if (!out.deltas && e->usage.completion_tokens.lo != 1)
        return -1;
      ++out.deltas;
      out.text.append(static_cast<const char *>(e->data), e->size);
      return out.action;
    }
    if (e->kind == AILO_EVENT_RUN_END)
      ++out.completed;
    return 1;
  }
};
int main(int argc, char **argv) {
  try {
    CHECK(argc == 2);
    const auto chat = openai_protocol::chat,
               responses = openai_protocol::responses;
    std::string base =
        R"({"model":"local-model","messages":[{"role":"user","content":"Hi"})";
    // Same ordinary prompt mapped from both protocols, then run through the
    // real C ABI below. No adapter-only fake inference path.
    std::string simple =
        base +
        R"(],"stream":true,"store":false,"max_completion_tokens":8,"temperature":0,"logit_bias":{"100":100}})";
    openai_request c(chat, simple.data(), simple.size(), "local-model");
    CHECK(c.streaming && c.store == 0 && c.generation.item_count == 1 &&
          c.generation.max_tokens == 8 && c.generation.temperature == 0 &&
          c.generation.top_p == 1 && c.generation.logit_bias_count == 1 &&
          c.generation.logit_bias[0].token_id == 100);
    std::string rjson =
        R"({"input":"Hi","stream":true,"store":null,"temperature":0,"max_output_tokens":8,"include":["message.output_text.logprobs"],"top_logprobs":2})";
    openai_request r(responses, rjson.data(), rjson.size(), "local-model");
    CHECK(r.model == "local-model" && r.store == -1 && r.generation.logprobs &&
          r.generation.top_logprobs == 2 && r.generation.item_count == 1 &&
          !std::strcmp(static_cast<const char *>(r.items[0].content[0].data),
                       "Hi"));
    std::string history = R"({"model":"local-model","messages":[
      {"role":"system","content":"Instructions"},
      {"role":"assistant","content":null,"tool_calls":[
        {"type":"function","id":"c1","function":{"name":"lookup","arguments":"{}"}},
        {"id":"c2","function":{"arguments":"{\"x\":2}","name":"other"},"type":"function"}]},
      {"role":"tool","tool_call_id":"c2","content":[{"text":"two","type":"text"}]},
      {"role":"tool","tool_call_id":"c1","content":"one"},
      {"role":"user","name":"Alice","content":"continue"}],
      "tools":[{"function":{"name":"lookup","parameters":{"type":"object","properties":{"x":{"type":"integer","minimum":-2,"maximum":4}},"required":["x"],"additionalProperties":false},"strict":true},"type":"function"},
               {"type":"function","function":{"name":"other"}}],
      "tool_choice":{"allowed_tools":{"tools":[{"function":{"name":"lookup"},"type":"function"}],"mode":"required"},"type":"allowed_tools"},
      "parallel_tool_calls":false,"n":2,"stop":["END","FIN"],
      "frequency_penalty":-2,"presence_penalty":2,"seed":4294967295,
      "logprobs":true,"top_logprobs":20,"prompt_cache_key":"bucket",
      "metadata":{"project":"T-1"},"user":"untrusted-user","safety_identifier":"untrusted-safety",
      "stream":true,"stream_options":{"include_usage":true,"include_obfuscation":false}})";
    openai_request h(chat, history.data(), history.size(), "local-model");
    auto prepared = prepare_conversation(h.generation);
    CHECK(prepared.pending_calls == 0 && h.generation.item_count == 6 &&
          h.generation.candidates == 2 && h.generation.seed == UINT32_MAX &&
          h.items[1].kind == AILO_ITEM_FUNCTION_CALL &&
          !std::strcmp(h.items[1].call_id, "c1") &&
          h.items[3].kind == AILO_ITEM_FUNCTION_RESULT &&
          !std::strcmp(h.items[3].call_id, "c2") &&
          h.generation.allowed_tool_count == 1 &&
          h.generation.tool_choice == AILO_TOOLS_REQUIRED &&
          h.generation.stop_count == 2 && h.include_usage &&
          !h.include_obfuscation && h.metadata.at("project") == "T-1");
    auto plan = prepare_tools(h.generation);
    CHECK(plan.names.size() == 1 && plan.names[0] == "lookup" &&
          plan.limit == 1);
    std::string response_history =
        R"({"model":"local-model","instructions":"New instructions",
      "input":[{"role":"assistant","type":"message","content":[{"type":"output_text","text":"Calling","annotations":[],"logprobs":[{"token":"Calling","logprob":-1.25,"top_logprobs":[{"token":"Calling","logprob":-1.25}]}]}],"id":"m1","status":"incomplete"},
               {"type":"function_call","id":"fc1","call_id":"c1","name":"lookup","arguments":"{}"},
               {"call_id":"c1","type":"function_call_output","output":"result"},
               {"role":"user","content":[{"type":"input_text","text":"Continue"}]}],
      "tools":[{"type":"function","name":"lookup","parameters":{"type":"object","additionalProperties":false},"strict":true}],
      "tool_choice":{"type":"function","name":"lookup"},"reasoning":{"effort":"high","summary":null},
      "previous_response_id":"resp_prior","context_management":[{"compact_threshold":256,"type":"compaction"}],
      "text":{"format":{"type":"json_schema","name":"Answer","strict":true,"schema":{"const":{"answer":"☃"}}}},
      "metadata":{"a":"b"},"max_tool_calls":1,"truncation":"disabled","background":false})";
    openai_request rh(responses, response_history.data(),
                      response_history.size(), "local-model");
    CHECK(rh.generation.item_count == 5 && rh.instruction_items == 1 &&
          rh.items[0].role == AILO_ROLE_DEVELOPER &&
          rh.generation.output_format == AILO_OUTPUT_JSON_SCHEMA &&
          rh.schema.find("☃") != std::string::npos && rh.schema_strict &&
          rh.automatic_compaction && rh.compact_threshold == 256 &&
          rh.previous_response_id == "resp_prior" &&
          rh.generation.reasoning == AILO_REASONING_ENABLED &&
          !std::strcmp(rh.generation.reasoning_effort, "high") &&
          rh.generation.max_tool_calls == 0 && rh.max_builtin_tool_calls == 1);
    CHECK(prepare_conversation(rh.generation).pending_calls == 0);
    const std::string reasoning_history =
        R"({"input":[{"type":"reasoning","id":"rs_1","summary":[],"content":[{"type":"reasoning_text","text":"Consider the question."}],"status":"completed"},{"role":"assistant","content":"Answer"},{"role":"user","content":"Continue"}]})";
    openai_request reasoning_replay(responses, reasoning_history.data(),
                                    reasoning_history.size(), "local-model");
    CHECK(reasoning_replay.items.size() == 3 &&
          reasoning_replay.items[0].kind == AILO_ITEM_REASONING &&
          prepare_conversation(reasoning_replay.generation).pending_calls == 0);
    reject(
        responses,
        R"({"input":[{"role":"assistant","content":[{"type":"output_text","text":"x","logprobs":[{"token":"x","logprob":1}]}]}]})",
        AILO_INVALID_ARGUMENT);
    std::string implicit = R"({"input":"Hi","tools":[
      {"type":"function","name":"implicit","parameters":{"type":"object","properties":{"x":{"type":"integer"},"nested":{"type":"object","properties":{"s":{"type":"string"}}}}}},
      {"type":"function","name":"explicit_false","strict":false,"parameters":{"type":"object"}},
      {"type":"function","name":"best_effort","parameters":{"type":"object","properties":{"s":{"type":"string","pattern":"a+"}}}},
      {"type":"function","name":"no_parameters","parameters":null,"description":null}]})";
    openai_request normalized(responses, implicit.data(), implicit.size(),
                              "local-model");
    CHECK(normalized.tools[0].strict && !normalized.tools[1].strict &&
          !normalized.tools[2].strict && normalized.tools[3].strict);
    auto normalized_plan = prepare_tools(normalized.generation);
    CHECK(normalized_plan.definitions[0].argument_schema.find(
              "\"required\":[\"x\",\"nested\"]") != std::string::npos &&
          normalized_plan.definitions[0].argument_schema.find(
              "\"required\":[\"s\"]") != std::string::npos &&
          !h.tools[1].strict); // omitted Chat strict stays best effort
    reject(
        responses,
        R"({"input":"Hi","tools":[{"type":"function","name":"x","strict":true,"parameters":{"type":"object"}}]})",
        AILO_INVALID_ARGUMENT);
    reject(
        responses,
        R"({"input":"Hi","tools":[{"type":"function","name":"x","strict":true,"parameters":{"type":"object","additionalProperties":false,"properties":{"s":{"type":"string","pattern":"a+"}},"required":["s"]}}]})",
        AILO_UNSUPPORTED);
    std::string opaque =
        R"({"input":[{"type":"compaction","id":"cmp1","encrypted_content":"authenticated-elsewhere"},{"role":"user","content":"next"}],"conversation":{"id":"conv1"}})";
    openai_request compact(responses, opaque.data(), opaque.size(),
                           "local-model");
    CHECK(compact.items[0].kind == AILO_ITEM_COMPACTION &&
          compact.conversation_id == "conv1");
    openai_retention_policy policy;
    auto no_store = resolve_retention(c, policy, 100);
    CHECK(!no_store.store_response && !no_store.reserve_record_storage &&
          !no_store.retain_output_in_memory);
    policy.namespace_id = "trusted-tenant";
    auto policy_fails = [&](const openai_request &request, ailo_status status) {
      try {
        (void)resolve_retention(request, policy, 100);
      } catch (const failure &e) {
        CHECK(e.status == status);
        return;
      }
      throw std::runtime_error("Retention policy unexpectedly admitted state");
    };
    // Default stored Responses must fail preflight if no repository is present.
    policy_fails(r, AILO_UNSUPPORTED);
    policy.repository_writable = true;
    auto stored = resolve_retention(r, policy, 100);
    CHECK(stored.store_response && stored.reserve_record_storage &&
          stored.response_expires_at ==
              100 + openai_response_retention_seconds &&
          stored.retain_output_in_memory);
    policy.response_retention_seconds = openai_response_retention_seconds - 1;
    policy_fails(r, AILO_INVALID_ARGUMENT);
    policy.response_retention_seconds = openai_response_retention_seconds;
    policy.zero_data_retention = true;
    policy.repository_writable = false;
    r.store = 1;
    auto zdr = resolve_retention(r, policy, 100);
    CHECK(!zdr.store_response && !zdr.reserve_record_storage &&
          zdr.retain_output_in_memory);
    c.store = 1;
    CHECK(!resolve_retention(c, policy, 100).store_response &&
          !resolve_retention(c, policy, 100).retain_output_in_memory);
    c.store = 0;
    policy_fails(compact, AILO_UNSUPPORTED);
    policy.repository_readable = policy.repository_writable = true;
    auto attached = resolve_retention(compact, policy, 100);
    CHECK(!attached.store_response && attached.read_conversation &&
          attached.append_conversation && attached.reserve_record_storage &&
          attached.response_expires_at == 0);
    policy.zero_data_retention = false;
    auto chat_default = resolve_retention(h, policy, 100);
    CHECK(chat_default.store_response && chat_default.response_expires_at == 0);
    policy.chat_store_by_default = false;
    CHECK(!resolve_retention(h, policy, 100).store_response);
    h.store = 1;
    CHECK(resolve_retention(h, policy, 100).store_response);
    policy.namespace_id.clear();
    policy_fails(
        h, AILO_INVALID_ARGUMENT); // user/safety metadata cannot authorize
    policy_fails(r,
                 AILO_INVALID_ARGUMENT); // request user/safety never authorizes
    policy.namespace_id = "trusted-tenant";
    policy.response_retention_seconds = std::numeric_limits<uint64_t>::max();
    policy_fails(r, AILO_OUT_OF_RANGE);
    // Invalid JSON, duplicate decoded members, protocol leakage, wrong types,
    // contradictory controls, overflow, and meaningful unsupported semantics.
    reject(chat, simple + " true", AILO_INVALID_JSON);
    reject(chat,
           R"({"model":"local-model","model":"local-model","messages":[]})",
           AILO_INVALID_ARGUMENT);
    reject(
        chat,
        R"({"model":"local-model","\u006dodel":"local-model","messages":[]})",
        AILO_INVALID_ARGUMENT);
    reject(chat, R"({"messages":[{"role":"user","content":"x"}]})",
           AILO_INVALID_ARGUMENT);
    reject(chat, R"({"model":"other","messages":[]})", AILO_UNKNOWN_MODEL);
    reject(responses, R"({"input":"Hi","conversation":{}})",
           AILO_INVALID_ARGUMENT);
    reject(responses,
           R"({"input":"Hi","conversation":"c","previous_response_id":"p"})",
           AILO_INVALID_ARGUMENT);
    const std::string prefix = base + "],";
    for (auto field :
         {R"("stream":"true")", R"("temperature":-0.1)",
          R"("temperature":1e999)", R"("top_p":0)", R"("top_logprobs":1)",
          R"("n":17)", R"("n":1.5)", R"("n":1.00000000000000000001)",
          R"("seed":4294967296)", R"("logit_bias":{"100":101})",
          R"("logit_bias":{"1":0,"01":1})",
          R"("max_tokens":1,"max_completion_tokens":2)",
          R"("stream_options":{"include_usage":true})",
          R"("stop":["1","2","3","4","5"])", R"("stop":"")",
          R"("metadata":{"x":3})",
          R"("tools":[{"type":"function","function":{"name":"bad name"}}])",
          R"("messages":[])",
          R"("response_format":{"type":"text","json_schema":{}})"})
      reject(chat, prefix + field + "}", AILO_INVALID_ARGUMENT);
    reject(
        chat,
        prefix +
            R"("tool_choice":{"type":"function","function":{"name":"lookup"},"allowed_tools":{}}})",
        AILO_INVALID_ARGUMENT);
    for (auto field :
         {R"("unrecognized":null)", R"("max_output_tokens":null)",
          R"("prompt_cache_retention":"24h")",
          R"("prediction":{"type":"content","content":"x"})",
          R"("modalities":["audio"])", R"("tools":[{"type":"web_search"}])"})
      reject(chat, prefix + field + "}", AILO_UNSUPPORTED);
    for (
        auto field :
        {R"("background":true)", R"("truncation":"auto")", R"("n":null)",
         R"("reasoning":{"summary":"auto"})",
         R"("input":[{"role":"user","content":[{"type":"input_image","image_url":"https://example.invalid/image"}]}])"})
      reject(responses, "{\"input\":\"Hi\"," + std::string(field) + "}",
             std::strstr(field, "\"input\"") ? AILO_INVALID_ARGUMENT
                                             : AILO_UNSUPPORTED);
    const std::string legacy_reasoning =
        R"({"input":"Hi","include":["reasoning.encrypted_content"]})";
    openai_request legacy(responses, legacy_reasoning.data(),
                          legacy_reasoning.size(), "local-model");
    CHECK(!legacy.generation.logprobs);
    const std::string opaque_reasoning =
        R"({"input":[{"type":"reasoning","id":"rs_1","summary":[],"encrypted_content":"opaque"},{"role":"user","content":"Next"}]})";
    openai_request parsed_opaque(responses, opaque_reasoning.data(),
                                 opaque_reasoning.size(), "local-model");
    CHECK(parsed_opaque.items[0].kind == AILO_ITEM_REASONING &&
          parsed_opaque.items[0].content_count == 0 &&
          !std::strcmp(parsed_opaque.items[0].encrypted_content, "opaque"));
    for (
        const char *bad :
        {R"({"input":[{"type":"reasoning","summary":[],"encrypted_content":"opaque"}]})",
         R"({"input":[{"type":"reasoning","id":"rs_1","summary":[],"encrypted_content":""}]})",
         R"({"input":[{"type":"reasoning","id":"rs_1","summary":[],"encrypted_content":null}]})",
         R"({"input":"Hi","include":["reasoning.encrypted_content","reasoning.encrypted_content"]})"})
      reject(responses, bad, AILO_INVALID_ARGUMENT);
    reject(
        responses,
        R"({"input":[{"type":"function_call_output","call_id":"x","content":"wrong field"}]})",
        AILO_INVALID_ARGUMENT);
    reject(
        responses,
        R"({"input":[{"type":"message","role":"user","content":"x","name":"wrong field"}]})",
        AILO_INVALID_ARGUMENT);
    reject(
        chat,
        R"({"model":"local-model","messages":[{"role":"user","content":null}]})",
        AILO_INVALID_ARGUMENT);
    reject(
        chat,
        R"({"model":"local-model","messages":[{"role":"tool","tool_call_id":"a\u0000b","content":"x"}]})",
        AILO_INVALID_ARGUMENT);
    reject(responses, std::string(8 * 1024 * 1024 + 1, ' '),
           AILO_INVALID_ARGUMENT);
    reject(
        responses,
        R"({"input":[{"role":"user","content":[{"type":"input_image","image_url":"https://example.invalid/image"}]}]})",
        AILO_UNSUPPORTED);
    reject(
        chat,
        prefix +
            R"("response_format":{"type":"json_schema","json_schema":{"name":"x","schema":{"type":"object","type":"array"}}}})",
        AILO_INVALID_ARGUMENT);
    // Execute the mapped Chat request via the real library and prove the first
    // delta, cancellation, and sink failure happen after exactly one token.
    ailo_client_config cfg = ailo_client_default_config();
    cfg.source.local_path = argv[1];
    cfg.backend = AILO_BACKEND_CPU;
    cfg.threads = 1;
    ailo_client *raw = nullptr;
    ailo_error error{};
    CHECK(ailo_client_new(&cfg, &raw, &error) == AILO_OK);
    std::unique_ptr<ailo_client, decltype(&ailo_client_destroy)> client(
        raw, ailo_client_destroy);
    // Views must survive destruction/mutation of caller-owned JSON bytes.
    simple.assign(simple.size(), '\0');
    rjson.assign(rjson.size(), '\0');
    for (const auto *mapped : {&c.generation, &r.generation})
      for (int action : {1, 0, -1}) {
        observation seen;
        seen.action = action;
        ailo_run_info info{};
        auto status = client->generate(client.get(), mapped, observation::event,
                                       &seen, &info, &error);
        CHECK(status == (action == 1   ? AILO_OK
                         : action == 0 ? AILO_CANCELLED
                                       : AILO_SINK_FAILED));
        CHECK(action == 1 ? seen.text == "aaaaaaaa" && seen.completed == 1
                          : seen.deltas == 1 && !seen.completed &&
                                info.usage.completion_tokens.lo == 1);
      }
    std::puts("OpenAI request mapping, protocol/state controls, failure modes "
              "and immediate native inference: passed");
  } catch (const std::exception &e) {
    std::fprintf(stderr, "openai requests: %s\n", e.what());
    return 1;
  }
}
