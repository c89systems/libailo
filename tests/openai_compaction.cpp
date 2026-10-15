#include "ailo_openai_internal.hpp"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <thread>
using namespace ailo::client;
#define CHECK(c)                                                               \
  do {                                                                         \
    if (!(c))                                                                  \
      throw std::runtime_error(std::string(#c) + " at " +                      \
                               std::to_string(__LINE__));                      \
  } while (0)
ailo_error error{};
using client_owner =
    std::unique_ptr<ailo_client, decltype(&ailo_client_destroy)>;
using key_owner =
    std::unique_ptr<ailo_compactor, decltype(&ailo_compactor_destroy)>;
const char *plain =
    "{% for m in messages %}{{m.role}}:{{m.content}}|{% endfor %}";
key_owner keys(const std::string &key, unsigned ttl = 0,
               size_t limit = 1024 * 1024, const std::string *old = nullptr) {
  auto config = ailo_compactor_default_config();
  config.namespace_id = "key-domain";
  config.current_key = {old ? "new" : "one", key.data(), key.size()};
  config.ttl_seconds = ttl;
  config.max_item_bytes = limit;
  ailo_compaction_key prior{"one", old ? old->data() : nullptr,
                            old ? old->size() : 0};
  if (old) {
    config.old_keys = &prior;
    config.old_key_count = 1;
  }
  ailo_compactor *result = nullptr;
  CHECK(ailo_compactor_new(&config, &result, &error) == AILO_OK);
  return {result, ailo_compactor_destroy};
}
client_owner create(const std::string &path, const ailo_openai_config &profile,
                    ailo_compactor *owner = nullptr,
                    const char *source = plain) {
  auto config = ailo_client_default_config();
  config.source.local_path = path.c_str();
  config.backend = AILO_BACKEND_CPU;
  config.threads = 1;
  config.context_tokens = 4096;
  config.chat_template = source;
  config.openai = &profile;
  config.compactor = owner;
  ailo_client *result = nullptr;
  CHECK(ailo_client_new(&config, &result, &error) == AILO_OK);
  return {result, ailo_client_destroy};
}
struct output {
  std::string bytes;
  int action = 1;
  ailo_client *receiver = nullptr;
  bool busy = false;
  ailo_api_info *info = nullptr;
  size_t boundary = 0, first_delta = SIZE_MAX, control_at = SIZE_MAX;
  int control_action = 1;
  static int send(void *u, const void *p, size_t n) {
    auto &out = *static_cast<output *>(u);
    out.bytes.append(static_cast<const char *>(p), n);
    size_t end;
    while ((end = out.bytes.find("\n\n", out.boundary)) != std::string::npos) {
      const auto frame = out.bytes.substr(out.boundary, end - out.boundary);
      out.boundary = end + 2;
      if (out.info && frame.find("event: response.output_text.delta") == 0 &&
          frame.find("\"delta\":\"\"") == std::string::npos &&
          out.first_delta == SIZE_MAX)
        out.first_delta = out.info->run.usage.completion_tokens.lo;
      if (frame.find("event: response.output_item.added") == 0 &&
          frame.find("\"type\":\"compaction\"") != std::string::npos) {
        if (out.info)
          out.control_at = out.info->run.usage.completion_tokens.lo;
        if (out.control_action != 1)
          return out.control_action;
      }
    }
    if (out.receiver) {
      char *text = nullptr;
      out.busy = out.receiver->capabilities(out.receiver, &text, nullptr) ==
                     AILO_BUSY &&
                 !text;
    }
    return out.action;
  }
};
struct history {
  std::vector<std::string> text{"Finish ticket T-1. " + std::string(1000, 'x'),
                                "old answer",
                                "older",
                                "answer",
                                "recent",
                                "answer",
                                "newest",
                                "answer",
                                "current"};
  std::vector<ailo_content> parts;
  std::vector<ailo_item> items;
  history() {
    for (const auto &s : text)
      parts.push_back({AILO_CONTENT_TEXT, s.data(), s.size()});
    for (size_t i = 0; i < parts.size(); ++i) {
      ailo_item item{};
      item.role = i % 2 ? AILO_ROLE_ASSISTANT : AILO_ROLE_USER;
      item.content = &parts[i];
      item.content_count = 1;
      items.push_back(item);
    }
  }
};
std::string request(const std::vector<ailo_item> &items,
                    const char *instructions, bool compact = false) {
  std::string bytes;
  wire_json out([&](const void *p, size_t n) {
    bytes.append(static_cast<const char *>(p), n);
  });
  out.object();
  out.key("model");
  out.string("local-model");
  out.key("input");
  out.array();
  for (const auto &i : items)
    api_canonical_item(out, i);
  out.end_array();
  if (instructions) {
    out.key("instructions");
    out.string(instructions);
  }
  if (!compact) {
    out.key("store");
    out.boolean(false);
    out.key("stream");
    out.boolean(true);
    out.key("max_output_tokens");
    out.number(2);
    out.key("temperature");
    out.real(0);
    out.key("stream_options");
    out.object();
    out.key("include_obfuscation");
    out.boolean(false);
    out.end_object();
  }
  out.end_object();
  out.finish();
  return bytes;
}
ailo_status generate(ailo_client *client, const std::string &json, output &seen,
                     ailo_api_info &info) {
  return client->responses(client, json.data(), json.size(), output::send,
                           &seen, &info, &error);
}
std::string terminal(const std::string &frames) {
  const auto event = frames.rfind("event: response.");
  CHECK(event != std::string::npos);
  const auto begin = frames.find("\"response\":", event);
  CHECK(begin != std::string::npos);
  const auto end = frames.find("\n\n", begin);
  CHECK(end != std::string::npos && frames[end - 1] == '}');
  return frames.substr(begin + 11, end - begin - 12);
}
std::string automatic(std::string body, size_t threshold) {
  CHECK(body.back() == '}');
  body.pop_back();
  return body +
         ",\"context_management\":[{\"type\":\"compaction\","
         "\"compact_threshold\":" +
         std::to_string(threshold) + "}]}";
}
void automatic_cases(const std::string &path, ailo_openai_config profile,
                     ailo_compactor *owner) {
  auto receiver = create(path, profile, owner);
  history source;
  auto body = request(source.items, "Only this automatic request");
  const auto count = body.find("\"max_output_tokens\":2");
  CHECK(count != std::string::npos);
  body.replace(count, std::strlen("\"max_output_tokens\":2"),
               "\"max_output_tokens\":128,\"text\":{\"format\":{"
               "\"type\":\"json_schema\",\"name\":\"answer\",\"strict\":true,"
               "\"schema\":{\"const\":{\"text\":\"" +
                   std::string(32, 'a') + "\"}}}}");
  body.pop_back();
  body += ",\"include\":[\"message.output_text.logprobs\"],\"top_logprobs\":2}";
  ailo_api_info info{};
  output baseline;
  CHECK(generate(receiver.get(), body, baseline, info) == AILO_OK);
  const auto prompt = info.run.usage.prompt_tokens.lo;
  auto baseline_json = terminal(baseline.bytes);
  openai_request reference(openai_protocol::responses, baseline_json.data(),
                           baseline_json.size(), "local-model", "output");
  const std::string expected = "{\"text\":\"" + std::string(32, 'a') + "\"}";
  CHECK(reference.items.size() == 1 &&
        reference.items[0].content[0].size == expected.size() &&
        std::memcmp(reference.items[0].content[0].data, expected.data(),
                    expected.size()) == 0);
  const auto completion_tokens = info.run.usage.completion_tokens.lo;
  // The same active message spans the in-flight rebuild without replacing its
  // ID or replaying already delivered bytes. Its completed opaque item also
  // preserves that whole message if a caller prunes everything before it.
  const auto inflight = automatic(body, prompt + 3);
  output seen;
  seen.info = &info;
  CHECK(generate(receiver.get(), inflight, seen, info) == AILO_OK &&
        seen.first_delta == 1 && seen.control_at == 3 &&
        info.run.usage.compaction_passes == 1 &&
        info.run.usage.context_replay_tokens.lo &&
        info.run.usage.completion_tokens.lo == completion_tokens);
  auto json = terminal(seen.bytes);
  CHECK(json.find("\"logprobs\":[{") != std::string::npos);
  openai_request result(openai_protocol::responses, json.data(), json.size(),
                        "local-model", "output");
  CHECK(result.items.size() == 2 && result.items[0].kind == AILO_ITEM_MESSAGE &&
        result.items[1].kind == AILO_ITEM_COMPACTION &&
        result.items[0].content[0].size == expected.size() &&
        std::memcmp(result.items[0].content[0].data,
                    reference.items[0].content[0].data, expected.size()) == 0);
  auto full = source.items;
  full.insert(full.end(), result.items.begin(), result.items.end());
  std::vector<ailo_item> pruned{result.items.back()};
  ailo_content next{AILO_CONTENT_TEXT, "Continue", 8};
  ailo_item user{};
  user.role = AILO_ROLE_USER;
  user.content = &next;
  user.content_count = 1;
  full.push_back(user);
  pruned.push_back(user);
  seen = {};
  CHECK(generate(receiver.get(), request(full, "Replacement instructions"),
                 seen, info) == AILO_OK);
  const auto full_usage = info.run.usage.prompt_tokens.lo;
  const auto full_json = terminal(seen.bytes);
  seen = {};
  CHECK(generate(receiver.get(), request(pruned, "Replacement instructions"),
                 seen, info) == AILO_OK &&
        info.run.usage.prompt_tokens.lo == full_usage);
  const auto replay_json = terminal(seen.bytes);
  openai_request replay(openai_protocol::responses, replay_json.data(),
                        replay_json.size(), "local-model", "output");
  openai_request original(openai_protocol::responses, full_json.data(),
                          full_json.size(), "local-model", "output");
  CHECK(replay.items[0].content[0].size == original.items[0].content[0].size &&
        std::memcmp(replay.items[0].content[0].data,
                    original.items[0].content[0].data,
                    replay.items[0].content[0].size) == 0);
  // Compare with an independently compacted canonical input followed by the
  // COMPLETE active output, not merely the three-byte prefix at the checkpoint.
  const auto compact_body =
      request(source.items, "Only this automatic request", true);
  seen = {};
  CHECK(receiver->response_compact(receiver.get(), compact_body.data(),
                                   compact_body.size(), output::send, &seen,
                                   &info, &error) == AILO_OK);
  openai_request compacted(openai_protocol::responses, seen.bytes.data(),
                           seen.bytes.size(), "local-model", "output");
  auto equivalent = compacted.items;
  equivalent.push_back(result.items[0]);
  equivalent.push_back(user);
  seen = {};
  CHECK(generate(receiver.get(),
                 request(equivalent, "Replacement instructions"), seen,
                 info) == AILO_OK &&
        info.run.usage.prompt_tokens.lo == full_usage);
  auto corrupted = std::string(pruned[0].encrypted_content);
  corrupted[corrupted.size() / 2] =
      corrupted[corrupted.size() / 2] == 'A' ? 'B' : 'A';
  auto wrong = pruned;
  wrong[0].encrypted_content = corrupted.c_str();
  seen = {};
  CHECK(generate(receiver.get(), request(wrong, nullptr), seen, info) ==
            AILO_INVALID_ARGUMENT &&
        !info.started);
  // Compaction envelopes remain usable after reconstructing the client.
  receiver = create(path, profile, owner);
  seen = {};
  CHECK(generate(receiver.get(), request(pruned, "Replacement instructions"),
                 seen, info) == AILO_OK);
  for (size_t threshold : {size_t(900), size_t(0)}) {
    // Zero chooses the native default trigger and does not compact this input.
    const auto before =
        threshold ? automatic(body, threshold)
                  : body.substr(0, body.size() - 1) +
                        ",\"context_management\":[{\"type\":\"compaction\"}]}";
    seen = {};
    seen.info = &info;
    CHECK(generate(receiver.get(), before, seen, info) == AILO_OK);
    CHECK(threshold ? seen.control_at == 0 : seen.control_at == SIZE_MAX);
  }
  for (int action : {0, -1}) {
    seen = {};
    seen.control_action = action;
    CHECK(generate(receiver.get(), inflight, seen, info) ==
          (action ? AILO_SINK_FAILED : AILO_CANCELLED));
    CHECK(seen.bytes.find("event: response.incomplete") == std::string::npos &&
          seen.bytes.find("event: response.completed") == std::string::npos &&
          seen.bytes.find("event: response.failed") == std::string::npos);
  }
  seen = {};
  CHECK(generate(receiver.get(), automatic(body, 64), seen, info) ==
            AILO_INVALID_ARGUMENT &&
        !info.started);
  auto failed_policy = *profile.compaction;
  failed_policy.summary_tokens = 1;
  profile.compaction = &failed_policy;
  auto failed = create(path, profile, owner);
  seen = {};
  CHECK(generate(failed.get(), inflight, seen, info) == AILO_MODEL_FAILED &&
        seen.bytes.find("event: response.failed") != std::string::npos &&
        seen.bytes.find("event: response.completed") == std::string::npos &&
        seen.bytes.find("event: response.incomplete") == std::string::npos);
  profile.compaction = nullptr;
  profile.max_output_memory_bytes = 1024;
  auto quota = create(path, profile, owner);
  seen = {};
  CHECK(generate(quota.get(), inflight, seen, info) == AILO_OUT_OF_RANGE &&
        seen.bytes.find("event: response.failed") != std::string::npos &&
        seen.bytes.find("event: response.completed") == std::string::npos &&
        seen.bytes.find("event: response.incomplete") == std::string::npos);
  seen = {};
  CHECK(generate(receiver.get(), inflight, seen, info) == AILO_OK);
}
int main(int argc, char **argv) {
  try {
    CHECK(argc == 2);
    const std::string path = std::string(argv[1]) + "-auto-compaction";
    auto policy = ailo_compaction_default_policy();
    policy.target_tokens = 800;
    policy.summary_context_tokens = 8192;
    auto profile = ailo_openai_default_config();
    profile.namespace_id = "tenant-A";
    profile.compaction = &policy;
    const std::string key(32, 'k');
    auto owner = keys(key);
    auto client = create(path, profile, owner.get());
    owner.reset();
    history input;
    const auto body = request(input.items, "Only the compact request", true);
    output seen;
    seen.receiver = client.get();
    ailo_api_info info{};
    // The policy is copied; later caller mutation cannot change this receiver.
    policy.target_tokens = 1;
    CHECK(client->response_compact(client.get(), body.data(), body.size(),
                                   output::send, &seen, &info,
                                   &error) == AILO_OK);
    CHECK(seen.busy && info.started && !info.stored &&
          info.run.usage.compaction_passes == 1 &&
          info.run.usage.prompt_tokens.lo > 0);
    CHECK(seen.bytes.find("\"object\":\"response.compaction\"") !=
          std::string::npos);
    openai_request window(openai_protocol::responses, seen.bytes.data(),
                          seen.bytes.size(), "local-model", "output");
    CHECK(window.items.size() == 6 &&
          window.items[0].kind == AILO_ITEM_COMPACTION);
    CHECK(seen.bytes.find("Only the compact request") == std::string::npos);
    const auto wire_id = std::string(window.items[0].id),
               encrypted = std::string(window.items[0].encrypted_content);
    CHECK(encrypted.find("Finish ticket") == std::string::npos);
    const auto prompt_tokens = info.run.usage.prompt_tokens.lo;
    const auto directed =
        request(input.items,
                "Focus the compacted history on commitments and preserve all "
                "important ticket identifiers and dependencies.",
                true);
    seen = {};
    CHECK(client->response_compact(client.get(), directed.data(),
                                   directed.size(), output::send, &seen, &info,
                                   &error) == AILO_OK &&
          info.run.usage.prompt_tokens.lo > prompt_tokens);
    const auto replay = request(window.items, "New instructions");
    seen = {};
    CHECK(generate(client.get(), replay, seen, info) == AILO_OK &&
          seen.bytes.find("event: response.incomplete") != std::string::npos);
    policy.target_tokens = 800;
    client.reset();
    owner = keys(key);
    client = create(path, profile, owner.get());
    owner.reset();
    seen = {};
    CHECK(generate(client.get(), replay, seen, info) == AILO_OK);
    // Canonical system messages remain authenticated while per-request
    // instructions can be replaced on replay.
    auto persistent = input.items;
    ailo_content text{AILO_CONTENT_TEXT, "Permanent canonical policy", 26};
    ailo_item system{};
    system.role = AILO_ROLE_SYSTEM;
    system.content = &text;
    system.content_count = 1;
    persistent.insert(persistent.begin(), system);
    const auto with_system = request(persistent, nullptr, true);
    seen = {};
    CHECK(client->response_compact(client.get(), with_system.data(),
                                   with_system.size(), output::send, &seen,
                                   &info, &error) == AILO_OK);
    openai_request protected_window(openai_protocol::responses,
                                    seen.bytes.data(), seen.bytes.size(),
                                    "local-model", "output");
    CHECK(protected_window.items[0].role == AILO_ROLE_SYSTEM &&
          protected_window.items[1].kind == AILO_ITEM_COMPACTION);
    seen = {};
    CHECK(generate(client.get(),
                   request(protected_window.items, "New per-request"), seen,
                   info) == AILO_OK);
    auto pruned = protected_window.items;
    pruned.erase(pruned.begin());
    seen = {};
    CHECK(generate(client.get(), request(pruned, nullptr), seen, info) ==
              AILO_INVALID_ARGUMENT &&
          !info.started);
    const char *tools =
        R"({% if tools %}{{tools | tojson}}{% endif %}{% for m in messages %}{{m.role}}:{{m.content}}{% if m.tool_calls is defined %}{% for c in m.tool_calls %}<call>{"name":{{c.function.name | tojson}},"arguments":{% if c.function.arguments is string %}{{c.function.arguments}}{% else %}{{c.function.arguments | tojson}}{% endif %}}</call>{% endfor %}{% endif %}|{% endfor %}{% if add_generation_prompt %}assistant:{% endif %})";
    auto tool_key = keys(key);
    auto tool_client = create(path, profile, tool_key.get(), tools);
    auto tool_history = input.items;
    ailo_item call{};
    call.kind = AILO_ITEM_FUNCTION_CALL;
    call.call_id = "c1";
    call.name = "lookup";
    call.arguments = "{}";
    ailo_content result_text{AILO_CONTENT_TEXT, "toolresult", 10};
    ailo_item result{};
    result.kind = AILO_ITEM_FUNCTION_RESULT;
    result.call_id = "c1";
    result.content = &result_text;
    result.content_count = 1;
    tool_history.insert(tool_history.begin() + 5, {call, result});
    const auto tool_body =
        request(tool_history, "Only the compact request", true);
    seen = {};
    CHECK(tool_client->response_compact(tool_client.get(), tool_body.data(),
                                        tool_body.size(), output::send, &seen,
                                        &info, &error) == AILO_OK);
    openai_request tool_window(openai_protocol::responses, seen.bytes.data(),
                               seen.bytes.size(), "local-model", "output");
    bool kept_call = false, kept_result = false;
    for (const auto &i : tool_window.items) {
      if (i.kind == AILO_ITEM_FUNCTION_CALL)
        kept_call = std::string(i.call_id) == "c1" &&
                    std::string(i.name) == "lookup" &&
                    std::string(i.arguments) == "{}";
      if (i.kind == AILO_ITEM_FUNCTION_RESULT)
        kept_result = std::string(i.call_id) == "c1" && i.content_count == 1 &&
                      i.content[0].size == 10;
    }
    CHECK(kept_call && kept_result);
    seen = {};
    CHECK(generate(tool_client.get(),
                   request(tool_window.items, "New instructions"), seen,
                   info) == AILO_OK);
    auto changed_call = tool_window.items;
    for (auto &i : changed_call)
      if (i.kind == AILO_ITEM_FUNCTION_CALL)
        i.arguments = "{\"changed\":true}";
    seen = {};
    CHECK(generate(tool_client.get(), request(changed_call, nullptr), seen,
                   info) == AILO_INVALID_ARGUMENT &&
          !info.started);
    auto bad = [&](const std::vector<ailo_item> &items) {
      seen = {};
      CHECK(generate(client.get(), request(items, "New instructions"), seen,
                     info) == AILO_INVALID_ARGUMENT &&
            !info.started && seen.bytes.empty());
    };
    for (size_t at : {size_t(0), size_t(101), encrypted.size() / 2}) {
      auto changed = encrypted;
      changed[at] = changed[at] == 'A' ? 'B' : 'A';
      auto items = window.items;
      items[0].encrypted_content = changed.c_str();
      bad(items);
    }
    auto changed = window.items;
    changed[0].id = "cmp_wrong";
    bad(changed);
    changed = window.items;
    changed.push_back(changed[0]);
    bad(changed);
    changed = window.items;
    changed.erase(changed.begin() + 1);
    bad(changed);
    changed = window.items;
    ailo_content altered{AILO_CONTENT_TEXT, "substituted", 11};
    changed[1].content = &altered;
    changed[1].content_count = 1;
    bad(changed);
    auto other_profile = profile;
    other_profile.namespace_id = "tenant-B";
    auto same_key = keys(key);
    auto other = create(path, other_profile, same_key.get());
    seen = {};
    CHECK(generate(other.get(), replay, seen, info) == AILO_INVALID_ARGUMENT &&
          !info.started);
    const std::string wrong(32, 'q');
    auto wrong_key = keys(wrong);
    other = create(path, profile, wrong_key.get());
    seen = {};
    CHECK(generate(other.get(), replay, seen, info) == AILO_INVALID_ARGUMENT &&
          !info.started);
    other =
        create(path, profile, same_key.get(),
               "{% for m in messages %}{{m.role}}:{{m.content}}!{% endfor %}");
    seen = {};
    CHECK(generate(other.get(), replay, seen, info) == AILO_INVALID_ARGUMENT &&
          !info.started);
    auto rotated = keys(wrong, 0, 1024 * 1024, &key);
    other = create(path, profile, rotated.get());
    seen = {};
    CHECK(generate(other.get(), replay, seen, info) == AILO_OK);
    for (int action : {0, -1}) {
      seen = {};
      seen.action = action;
      CHECK(client->response_compact(client.get(), body.data(), body.size(),
                                     output::send, &seen, &info, &error) ==
                (action ? AILO_SINK_FAILED : AILO_CANCELLED) &&
            info.started && !info.stored);
    }
    const std::string short_body = R"({"model":"local-model","input":"Hi"})";
    seen = {};
    CHECK(client->response_compact(client.get(), short_body.data(),
                                   short_body.size(), output::send, &seen,
                                   &info, &error) == AILO_OK &&
          !info.run.usage.compaction_passes);
    openai_request unchanged(openai_protocol::responses, seen.bytes.data(),
                             seen.bytes.size(), "local-model", "output");
    CHECK(unchanged.items.size() == 1 &&
          unchanged.items[0].kind == AILO_ITEM_MESSAGE);
    const std::string oversized = "{\"model\":\"local-model\",\"input\":\"" +
                                  std::string(5000, 'x') + "\"}";
    seen = {};
    CHECK(client->response_compact(client.get(), oversized.data(),
                                   oversized.size(), output::send, &seen, &info,
                                   &error) == AILO_CONTEXT_LIMIT &&
          !info.started);
    for (auto json : {R"({"input":"Hi"})",
                      R"({"model":"local-model","input":"Hi","stream":true})",
                      R"({"model":"local-model","input":"Hi","store":true})"}) {
      seen = {};
      CHECK(client->response_compact(client.get(), json, std::strlen(json),
                                     output::send, &seen, &info,
                                     &error) != AILO_OK &&
            !info.started);
    }
    const std::string empty_model = R"({"model":"","input":"Hi"})";
    seen = {};
    CHECK(client->response_compact(client.get(), empty_model.data(),
                                   empty_model.size(), output::send, &seen,
                                   &info, &error) == AILO_INVALID_ARGUMENT &&
          !info.started);
    const std::string default_model = R"({"model":null,"input":"Hi"})";
    seen = {};
    CHECK(client->response_compact(client.get(), default_model.data(),
                                   default_model.size(), output::send, &seen,
                                   &info, &error) == AILO_OK);
    auto failed_policy = policy;
    failed_policy.summary_tokens = 1;
    auto failed_profile = profile;
    failed_profile.compaction = &failed_policy;
    auto failed = create(path, failed_profile, same_key.get());
    seen = {};
    CHECK(failed->response_compact(failed.get(), body.data(), body.size(),
                                   output::send, &seen, &info,
                                   &error) == AILO_MODEL_FAILED &&
          !info.started);
    failed_profile = profile;
    failed_profile.max_output_memory_bytes = 256;
    failed = create(path, failed_profile, same_key.get());
    seen = {};
    CHECK(failed->response_compact(failed.get(), body.data(), body.size(),
                                   output::send, &seen, &info,
                                   &error) == AILO_OUT_OF_RANGE &&
          !info.started);
    auto small_key = keys(key, 0, 512);
    failed = create(path, profile, small_key.get());
    seen = {};
    CHECK(failed->response_compact(failed.get(), body.data(), body.size(),
                                   output::send, &seen, &info,
                                   &error) == AILO_OUT_OF_RANGE &&
          !info.started);
    auto expiring_key = keys(key, 2);
    failed = create(path, profile, expiring_key.get());
    seen = {};
    CHECK(failed->response_compact(failed.get(), body.data(), body.size(),
                                   output::send, &seen, &info,
                                   &error) == AILO_OK);
    openai_request expires(openai_protocol::responses, seen.bytes.data(),
                           seen.bytes.size(), "local-model", "output");
    std::this_thread::sleep_for(std::chrono::seconds(3));
    seen = {};
    CHECK(generate(failed.get(), request(expires.items, nullptr), seen, info) ==
              AILO_INVALID_ARGUMENT &&
          !info.started);
    seen = {};
    CHECK(generate(client.get(), replay, seen, info) == AILO_OK);
    CHECK(std::string(window.items[0].id) == wire_id &&
          std::string(window.items[0].encrypted_content) == encrypted);
    auto automatic_key = keys(key);
    automatic_cases(path, profile, automatic_key.get());
    std::puts("Responses standalone compaction, canonical replay/instructions, "
              "reconstruction/rotation, authenticated failures/expiry, quotas "
              "and cancellation: passed");
  } catch (const std::exception &e) {
    std::fprintf(stderr, "openai compaction: %s (%s)\n", e.what(),
                 error.message);
    return 1;
  }
}
