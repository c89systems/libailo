#include "ailo/openai.h"
#include <cstdio>
#include <cstring>
#include <lonejson.h>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>

#define CHECK(c)                                                               \
  do {                                                                         \
    if (!(c))                                                                  \
      throw std::runtime_error(std::string(#c) + " at " +                      \
                               std::to_string(__LINE__));                      \
  } while (0)
ailo_error error{};
using client = std::unique_ptr<ailo_client, decltype(&ailo_client_destroy)>;
client create(const char *path, const ailo_openai_config *api = nullptr,
              bool lazy = false, const char *source = nullptr) {
  auto cfg = ailo_client_default_config();
  cfg.source.local_path = path;
  cfg.backend = AILO_BACKEND_CPU;
  cfg.threads = 1;
  cfg.lazy = lazy;
  cfg.openai = api;
  cfg.context_tokens = 1024;
  cfg.chat_template = source;
  ailo_client *result = nullptr;
  CHECK(ailo_client_new(&cfg, &result, &error) == AILO_OK);
  return {result, ailo_client_destroy};
}
struct output {
  std::string bytes;
  ailo_api_info *info = nullptr;
  int action = 1;
  bool first = false;
  ailo_client *receiver = nullptr;
  bool busy = false;
  bool cancel_after_commit = false;
  static int send(void *u, const void *p, size_t n) {
    auto &out = *static_cast<output *>(u);
    if (out.cancel_after_commit && out.info && out.info->stored)
      return 0;
    out.bytes.append(static_cast<const char *>(p), n);
    if (out.info && !out.first &&
        out.bytes.find("\"content\":\"a") != std::string::npos) {
      out.first = true;
      if (out.receiver) {
        char *metadata = nullptr;
        ailo_error problem{};
        out.busy = out.receiver->capabilities(out.receiver, &metadata,
                                              &problem) == AILO_BUSY &&
                   !metadata;
        if (!out.busy)
          return -1;
      }
      if (out.info->run.usage.completion_tokens.lo != 1)
        return -1;
      return out.action;
    }
    return 1;
  }
};
void valid_json(const std::string &json) {
  std::unique_ptr<lonejson, decltype(&lonejson_free)> parser(
      lonejson_new(nullptr, nullptr), lonejson_free);
  CHECK(parser &&
        lonejson_validate_buffer(parser.get(), json.data(), json.size(),
                                 nullptr) == LONEJSON_STATUS_OK);
}
void valid_sse(const std::string &bytes, bool padding) {
  size_t cursor = 0, frames = 0;
  while (cursor < bytes.size()) {
    CHECK(bytes.compare(cursor, 6, "data: ") == 0);
    const auto end = bytes.find("\n\n", cursor);
    CHECK(end != std::string::npos);
    const auto body = bytes.substr(cursor + 6, end - cursor - 6);
    if (body == "[DONE]") {
      CHECK(end + 2 == bytes.size());
      break;
    }
    valid_json(body);
    ++frames;
    CHECK((body.find("\"obfuscation\"") != std::string::npos) == padding);
    if (padding)
      CHECK(body.size() % 256 == 0);
    cursor = end + 2;
  }
  CHECK(frames >= 2 && bytes.find("data: [DONE]\n\n") != std::string::npos);
}
std::string tool_field(const std::string &sse, const char *field) {
  struct collector {
    const char *field;
    std::string value;
  } collected{field, {}};
  auto visitor = lonejson_default_path_value_visitor();
  visitor.string_chunk = [](void *u, const lonejson_value_path *p,
                            const char *data, size_t size, lonejson_error *) {
    auto &out = *static_cast<collector *>(u);
    if (p->segment_count == 7 && p->segments[6].len == std::strlen(out.field) &&
        !std::memcmp(p->segments[6].data, out.field, p->segments[6].len))
      out.value.append(data, size);
    return LONEJSON_STATUS_OK;
  };
  std::unique_ptr<lonejson, decltype(&lonejson_free)> parser(
      lonejson_new(nullptr, nullptr), lonejson_free);
  for (size_t cursor = 0; cursor < sse.size();) {
    const size_t end = sse.find("\n\n", cursor);
    CHECK(end != std::string::npos);
    const std::string body = sse.substr(cursor + 6, end - cursor - 6);
    if (body == "[DONE]")
      break;
    CHECK(lonejson_visit_path_value_buffer(parser.get(), body.data(),
                                           body.size(), &visitor, &collected,
                                           nullptr) == LONEJSON_STATUS_OK);
    cursor = end + 2;
  }
  return collected.value;
}
struct repository {
  struct record {
    std::string ns, id, parts[3];
    ailo_u64 expires{};
  };
  std::map<std::string, record> records;
  unsigned reserves = 0, commits = 0, aborts = 0, opens = 0, closes = 0;
  int fail = 0;
  ailo_record_repository api{};
  repository() {
    api.user_data = this;
    api.reserve = [](void *u, const ailo_record_metadata *m, void **tx,
                     ailo_error *) {
      auto &s = *static_cast<repository *>(u);
      ++s.reserves;
      auto p = new record{m->namespace_id, m->id, {}, m->expires_at};
      *tx = p;
      return s.fail == 1 ? AILO_OUT_OF_RANGE : AILO_OK;
    };
    api.write = [](void *u, void *tx, ailo_record_section part, const void *p,
                   size_t n, ailo_error *) {
      auto &s = *static_cast<repository *>(u);
      if (s.fail == 2)
        return AILO_IO_FAILED;
      static_cast<record *>(tx)->parts[part].append(
          static_cast<const char *>(p), n);
      return AILO_OK;
    };
    api.commit = [](void *u, void *tx, ailo_error *) {
      auto &s = *static_cast<repository *>(u);
      if (s.fail == 3)
        return AILO_IO_FAILED;
      auto &r = *static_cast<record *>(tx);
      s.records[r.ns + "/" + r.id] = r;
      ++s.commits;
      delete &r;
      return AILO_OK;
    };
    api.abort = [](void *u, void *tx) {
      ++static_cast<repository *>(u)->aborts;
      delete static_cast<record *>(tx);
    };
    api.open = [](void *u, const char *ns, ailo_record_protocol, const char *id,
                  void **handle, ailo_u64 *expires, ailo_error *) {
      auto &s = *static_cast<repository *>(u);
      auto found = s.records.find(std::string(ns) + "/" + id);
      if (found == s.records.end())
        return AILO_NOT_FOUND;
      ++s.opens;
      *handle = &found->second;
      *expires = found->second.expires;
      return AILO_OK;
    };
    api.read = [](void *, void *handle, ailo_record_section section,
                  ailo_output_sink sink, void *u, ailo_error *) {
      auto &bytes = static_cast<record *>(handle)->parts[section];
      for (size_t i = 0; i < bytes.size(); i += 7) {
        int result =
            sink(u, bytes.data() + i, std::min(size_t(7), bytes.size() - i));
        if (result <= 0)
          return result ? AILO_SINK_FAILED : AILO_CANCELLED;
      }
      return AILO_OK;
    };
    api.close = [](void *u, void *) { ++static_cast<repository *>(u)->closes; };
    api.erase = [](void *u, const char *ns, ailo_record_protocol,
                   const char *id, ailo_error *) {
      return static_cast<repository *>(u)->records.erase(std::string(ns) + "/" +
                                                         id)
                 ? AILO_OK
                 : AILO_NOT_FOUND;
    };
  }
};
int main(int argc, char **argv) {
  try {
    CHECK(argc == 2);
    auto receiver = create(argv[1]);
    const std::string ordinary =
        R"({"model":"local-model","messages":[{"role":"user","content":"Hi"}],"store":false,"stream":true,"max_completion_tokens":8,"temperature":0,"logit_bias":{"100":100},"logprobs":true,"top_logprobs":2,"stream_options":{"include_usage":true,"include_obfuscation":false}})";
    ailo_api_info info{};
    CHECK(receiver->chat_completions(receiver.get(), ordinary.data(),
                                     ordinary.size(), nullptr, nullptr, &info,
                                     &error) == AILO_INVALID_ARGUMENT &&
          info.http_status == 400 && !info.started);
    output seen;
    seen.info = &info;
    seen.receiver = receiver.get();
    CHECK(receiver->chat_completions(receiver.get(), ordinary.data(),
                                     ordinary.size(), output::send, &seen,
                                     &info, &error) == AILO_OK);
    valid_sse(seen.bytes, false);
    CHECK(seen.first && seen.busy && info.http_status == 200 && !info.stored &&
          info.run.usage.completion_tokens.lo == 8);
    CHECK(seen.bytes.find("\"token\":\"a\"") != std::string::npos &&
          seen.bytes.find("\"usage\":{\"prompt_tokens\"") != std::string::npos);
    for (int action : {0, -1}) {
      seen = {};
      seen.info = &info;
      seen.action = action;
      CHECK(receiver->chat_completions(receiver.get(), ordinary.data(),
                                       ordinary.size(), output::send, &seen,
                                       &info, &error) ==
            (action ? AILO_SINK_FAILED : AILO_CANCELLED));
      CHECK(info.run.usage.completion_tokens.lo == 1 &&
            seen.bytes.find("[DONE]") == std::string::npos &&
            seen.bytes.find("\"finish_reason\":\"length\"") ==
                std::string::npos);
    }
    std::string padded = ordinary;
    auto flag = padded.find("\"include_obfuscation\":false");
    padded.replace(flag, std::strlen("\"include_obfuscation\":false"),
                   "\"include_obfuscation\":true");
    seen = {};
    CHECK(receiver->chat_completions(receiver.get(), padded.data(),
                                     padded.size(), output::send, &seen, &info,
                                     &error) == AILO_OK);
    valid_sse(seen.bytes, true);
    std::string buffered = ordinary;
    flag = buffered.find("\"stream\":true");
    buffered.replace(flag, std::strlen("\"stream\":true"), "\"stream\":false");
    flag = buffered.find(",\"stream_options\"");
    buffered.erase(flag);
    buffered += "}";
    seen = {};
    CHECK(receiver->chat_completions(receiver.get(), buffered.data(),
                                     buffered.size(), output::send, &seen,
                                     &info, &error) == AILO_OK);
    valid_json(seen.bytes);
    CHECK(seen.bytes.find("\"content\":\"aaaaaaaa\"") != std::string::npos &&
          !std::strcmp(info.content_type, "application/json"));
    buffered.insert(buffered.size() - 1, ",\"n\":2");
    seen = {};
    CHECK(receiver->chat_completions(receiver.get(), buffered.data(),
                                     buffered.size(), output::send, &seen,
                                     &info, &error) == AILO_OK);
    valid_json(seen.bytes);
    CHECK(info.run.completed_candidates == 2 &&
          seen.bytes.find("\"index\":1") != std::string::npos);
    std::string stopped = ordinary;
    stopped.insert(stopped.size() - 1, ",\"stop\":\"aa\"");
    seen = {};
    CHECK(receiver->chat_completions(receiver.get(), stopped.data(),
                                     stopped.size(), output::send, &seen, &info,
                                     &error) == AILO_OK);
    valid_sse(seen.bytes, false);
    CHECK(info.run.usage.completion_tokens.lo == 2 &&
          seen.bytes.find("\"token\":") == std::string::npos);
    auto limited_api = ailo_openai_default_config();
    limited_api.max_output_memory_bytes = 256;
    auto limited = create(argv[1], &limited_api);
    const std::string long_request =
        R"({"model":"local-model","messages":[{"role":"user","content":"Hi"}],"store":false,"stream":true,"max_completion_tokens":512,"temperature":0,"stream_options":{"include_obfuscation":false}})";
    seen = {};
    CHECK(limited->chat_completions(limited.get(), long_request.data(),
                                    long_request.size(), output::send, &seen,
                                    &info, &error) == AILO_OK);
    valid_sse(seen.bytes, false);
    CHECK(info.run.usage.completion_tokens.lo == 512 &&
          seen.bytes.size() > 256);
    std::string too_large =
        R"({"model":"local-model","messages":[{"role":"user","content":"Hi"}],"store":false,"max_completion_tokens":512,"temperature":0})";
    seen = {};
    CHECK(limited->chat_completions(limited.get(), too_large.data(),
                                    too_large.size(), output::send, &seen,
                                    &info, &error) == AILO_OUT_OF_RANGE &&
          !info.started);
    const char *tool_source =
        "{% if tools %}{{tools | tojson}}{% endif %}{% for m in messages "
        "%}{{m.role}}:{{m.content}}{% if m.tool_calls is defined %}{% for c in "
        "m.tool_calls %}<call>{\"name\":{{c.function.name | "
        "tojson}},\"arguments\":{% if c.function.arguments is string "
        "%}{{c.function.arguments}}{% else %}{{c.function.arguments | "
        "tojson}}{% endif %}}</call>{% endfor %}{% endif %}|{% endfor %}{% if "
        "add_generation_prompt %}assistant:{% endif %}";
    std::string tool_path = std::string(argv[1]) + "-auto-compaction";
    auto tool_client = create(tool_path.c_str(), nullptr, false, tool_source);
    const std::string tools =
        R"({"model":"local-model","messages":[{"role":"user","content":"Hi"}],"store":false,"stream":true,"max_completion_tokens":32,"temperature":0,"logit_bias":{"2":100},"tools":[{"type":"function","function":{"name":"lookup","strict":true,"parameters":{"type":"object","properties":{"text":{"const":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}},"required":["text"],"additionalProperties":false}}}],"tool_choice":{"type":"function","function":{"name":"lookup"}},"parallel_tool_calls":false,"stream_options":{"include_obfuscation":false}})";
    seen = {};
    CHECK(tool_client->chat_completions(tool_client.get(), tools.data(),
                                        tools.size(), output::send, &seen,
                                        &info, &error) == AILO_OK);
    valid_sse(seen.bytes, false);
    CHECK(info.run.finish_reason == AILO_FINISH_TOOL_CALLS);
    CHECK(tool_field(seen.bytes, "name") == "lookup" &&
          tool_field(seen.bytes, "arguments") ==
              "{\"text\":\"" + std::string(32, 'a') + "\"}");
    const std::string snow =
        R"({"model":"local-model","messages":[{"role":"user","content":"Hi"}],"store":false,"stream":true,"max_completion_tokens":64,"temperature":0,"logprobs":true,"logit_bias":{"37":100,"229":100,"155":100,"134":100,"2":100},"response_format":{"type":"json_schema","json_schema":{"name":"Snow","schema":{"type":"object","properties":{"snow":{"const":"☃"}},"required":["snow"],"additionalProperties":false},"strict":true}},"stream_options":{"include_obfuscation":false}})";
    seen = {};
    CHECK(receiver->chat_completions(receiver.get(), snow.data(), snow.size(),
                                     output::send, &seen, &info,
                                     &error) == AILO_OK);
    valid_sse(seen.bytes, false);
    CHECK(seen.bytes.find("[226]") != std::string::npos &&
          seen.bytes.find("[152]") != std::string::npos &&
          seen.bytes.find("[131]") != std::string::npos);
    const char *thinking =
        "{% for m in messages %}{{m.role}}:{% if m.reasoning_content is "
        "defined %}<think>{{m.reasoning_content}}</think>{% endif "
        "%}{{m.content}}|{% endfor %}{% if add_generation_prompt "
        "%}assistant:{% if enable_thinking %}<think>{% endif %}{% endif %}";
    std::string thinking_path = std::string(argv[1]) + "-reasoning";
    auto reasoning = create(thinking_path.c_str(), nullptr, false, thinking);
    const std::string thought =
        R"({"model":"local-model","messages":[{"role":"user","content":"Hi"}],"store":false,"stream":true,"max_completion_tokens":32,"temperature":0,"logprobs":true,"logit_bias":{"260":100,"2":80},"response_format":{"type":"json_schema","json_schema":{"name":"Answer","schema":{"const":{}}}},"stream_options":{"include_obfuscation":false}})";
    seen = {};
    CHECK(reasoning->chat_completions(reasoning.get(), thought.data(),
                                      thought.size(), output::send, &seen,
                                      &info, &error) == AILO_OK);
    valid_sse(seen.bytes, false);
    CHECK(info.run.usage.reasoning_tokens.lo > 0 &&
          seen.bytes.find("<think>") == std::string::npos &&
          seen.bytes.find("</think>") == std::string::npos &&
          seen.bytes.find("\"token\":\"r\"") == std::string::npos);
    std::string diverse =
        R"({"model":"local-model","messages":[{"role":"user","content":"Hi"}],"store":false,"max_completion_tokens":64,"temperature":1,"top_p":1,"seed":7,"logprobs":true,"logit_bias":{"100":-100)";
    for (unsigned id = 101; id <= 125; ++id)
      diverse += ",\"" + std::to_string(id) + "\":100";
    diverse += "}}";
    seen = {};
    CHECK(receiver->chat_completions(receiver.get(), diverse.data(),
                                     diverse.size(), output::send, &seen, &info,
                                     &error) == AILO_OK);
    valid_json(seen.bytes);
    CHECK(seen.bytes.find("\"logprob\":-9999") != std::string::npos &&
          seen.bytes.find("\"top_logprobs\":[]") != std::string::npos);
    repository repo;
    auto api = ailo_openai_default_config();
    api.repository = &repo.api;
    api.namespace_id = "tenant-A";
    auto retained = create(argv[1], &api);
    char *advertised = nullptr;
    CHECK(retained->capabilities(retained.get(), &advertised, &error) ==
          AILO_OK);
    std::unique_ptr<char, decltype(&ailo_string_free)> caps(advertised,
                                                            ailo_string_free);
    CHECK(std::strstr(caps.get(), "\"chat_completions\":true") &&
          std::strstr(caps.get(), "\"responses\":false") &&
          std::strstr(caps.get(), "\"retained_chat_records\":true"));
    std::string stored = ordinary;
    stored.insert(stored.size() - 1, ",\"metadata\":{\"ticket\":\"T-1\"}");
    flag = stored.find("\"store\":false");
    stored.replace(flag, std::strlen("\"store\":false"), "\"store\":true");
    seen = {};
    seen.info = &info;
    CHECK(retained->chat_completions(retained.get(), stored.data(),
                                     stored.size(), output::send, &seen, &info,
                                     &error) == AILO_OK);
    CHECK(seen.first && info.stored && repo.commits == 1 &&
          repo.records.size() == 1);
    valid_json(repo.records.begin()->second.parts[AILO_RECORD_METADATA]);
    CHECK(repo.records.begin()->second.parts[AILO_RECORD_METADATA].find(
              "T-1") != std::string::npos);
    const std::string id = info.id;
    output retrieved;
    CHECK(retained->chat_retrieve(retained.get(), id.c_str(), 0, output::send,
                                  &retrieved, &info, &error) == AILO_OK);
    valid_json(retrieved.bytes);
    CHECK(retrieved.bytes.find("aaaaaaaa") != std::string::npos &&
          repo.opens == repo.closes);
    retrieved = {};
    CHECK(retained->chat_retrieve(retained.get(), id.c_str(), 1, output::send,
                                  &retrieved, &info, &error) == AILO_OK);
    valid_json(retrieved.bytes);
    CHECK(retrieved.bytes.find("Hi") != std::string::npos);
    api.namespace_id = "tenant-B";
    auto isolated = create(argv[1], &api);
    retrieved = {};
    CHECK(isolated->chat_retrieve(isolated.get(), id.c_str(), 0, output::send,
                                  &retrieved, &info,
                                  &error) == AILO_NOT_FOUND &&
          info.http_status == 404 && !info.started);
    for (int fail : {1, 2, 3}) {
      repo.fail = fail;
      seen = {};
      const unsigned before = repo.aborts;
      auto status = retained->chat_completions(retained.get(), stored.data(),
                                               stored.size(), output::send,
                                               &seen, &info, &error);
      CHECK(status == (fail == 1 ? AILO_OUT_OF_RANGE : AILO_IO_FAILED) &&
            repo.aborts == before + 1 && !info.stored &&
            seen.bytes.find("[DONE]") == std::string::npos);
      if (fail < 3)
        CHECK(info.run.usage.completion_tokens.lo == 0 && !info.started);
      else
        CHECK(info.run.usage.completion_tokens.lo == 8 &&
              seen.bytes.find("\"finish_reason\":\"length\"") ==
                  std::string::npos);
    }
    repo.fail = 0;
    unsigned aborted = repo.aborts, committed = repo.commits;
    seen = {};
    seen.info = &info;
    seen.action = 0;
    CHECK(retained->chat_completions(retained.get(), stored.data(),
                                     stored.size(), output::send, &seen, &info,
                                     &error) == AILO_CANCELLED &&
          !info.stored && repo.aborts == aborted + 1 &&
          repo.commits == committed);
    seen = {};
    seen.info = &info;
    seen.cancel_after_commit = true;
    CHECK(retained->chat_completions(retained.get(), stored.data(),
                                     stored.size(), output::send, &seen, &info,
                                     &error) == AILO_CANCELLED &&
          info.stored && repo.commits == committed + 1);
    CHECK(repo.records.erase(std::string("tenant-A/") + info.id) == 1);
    repo.records.begin()->second.expires = ailo_u64_from_ulong(1);
    retrieved = {};
    CHECK(retained->chat_retrieve(retained.get(), id.c_str(), 0, output::send,
                                  &retrieved, &info,
                                  &error) == AILO_NOT_FOUND &&
          repo.opens == repo.closes && !info.started);
    CHECK(retained->chat_delete(retained.get(), id.c_str(), output::send,
                                &retrieved, &info, &error) == AILO_NOT_FOUND &&
          !info.started && repo.opens == repo.closes);
    repo.records.begin()->second.expires = ailo_u64_from_ulong(0);
    CHECK(retained->chat_delete(retained.get(), id.c_str(), output::send,
                                &retrieved, &info, &error) == AILO_OK &&
          repo.records.empty());
    valid_json(retrieved.bytes);
    api.repository = nullptr;
    api.namespace_id = nullptr;
    api.zero_data_retention = 1;
    auto zdr = create(argv[1], &api);
    seen = {};
    CHECK(zdr->chat_completions(zdr.get(), stored.data(), stored.size(),
                                output::send, &seen, &info,
                                &error) == AILO_OK &&
          !info.stored);
    // Repository admission happens before attempting a lazy model load.
    auto absent = create("/missing/never-loaded.gguf", nullptr, true);
    seen = {};
    CHECK(absent->chat_completions(absent.get(), stored.data(), stored.size(),
                                   output::send, &seen, &info,
                                   &error) != AILO_OK &&
          !info.started && !info.run.usage.completion_tokens.lo);
    std::puts(
        "Public Chat JSON/SSE, first-token delivery, logprobs/padding, storage "
        "transactions, namespace/expiry, failure and ZDR: passed");
  } catch (const std::exception &e) {
    std::fprintf(stderr, "openai chat: %s (%s)\n", e.what(), error.message);
    return 1;
  }
}
