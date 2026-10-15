#include "ailo_client_internal.hpp"
#include "ailo_model_internal.hpp"
#include "src/llama-model.h"
#include <cstring>
#include <lonejson.h>
#include <map>
#include <sys/stat.h>

namespace ailo {
namespace client {
namespace {
class json {
  std::unique_ptr<lonejson, decltype(&lonejson_free)> runtime{nullptr,
                                                              lonejson_free};
  lonejson_writer writer{};
  std::string buffer;

  static lonejson_status append(void *user, const void *data, size_t size,
                                lonejson_error *) {
    try {
      auto &self = *static_cast<json *>(user);
      if (size > 4 * 1024 * 1024 - self.buffer.size())
        return LONEJSON_STATUS_OVERFLOW;
      self.buffer.append(static_cast<const char *>(data), size);
      return LONEJSON_STATUS_OK;
    } catch (...) {
      return LONEJSON_STATUS_ALLOCATION_FAILED;
    }
  }
  static void check(lonejson_status status) {
    if (status != LONEJSON_STATUS_OK)
      throw failure(status == LONEJSON_STATUS_ALLOCATION_FAILED
                        ? AILO_NO_MEMORY
                        : AILO_OUT_OF_RANGE,
                    "Cannot serialize bounded receiver metadata JSON");
  }

public:
  json() {
    lonejson_config config = lonejson_default_config();
    config.write_max_output_bytes = 4 * 1024 * 1024;
    runtime.reset(lonejson_new(&config, nullptr));
    if (!runtime)
      throw std::bad_alloc();
    check(lonejson_writer_init_sink(runtime.get(), &writer, append, this,
                                    nullptr));
  }
  ~json() { lonejson_writer_cleanup(&writer); }
  void object() { check(writer.begin_object(&writer, nullptr)); }
  void end_object() { check(writer.end_object(&writer, nullptr)); }
  void array() { check(writer.begin_array(&writer, nullptr)); }
  void end_array() { check(writer.end_array(&writer, nullptr)); }
  void key(const char *name) {
    check(writer.key(&writer, name, std::strlen(name), nullptr));
  }
  void string(const std::string &value) {
    check(writer.string(&writer, value.data(), value.size(), nullptr));
  }
  void nullable(const std::string &value) {
    if (value.empty())
      null();
    else
      string(value);
  }
  void null() { check(writer.null_fn(&writer, nullptr)); }
  void boolean(bool value) { check(writer.bool_fn(&writer, value, nullptr)); }
  void number(uint64_t value) { check(writer.u64(&writer, value, nullptr)); }
  std::string finish() {
    check(lonejson_writer_finish(&writer, nullptr));
    return std::move(buffer);
  }
};

void feature_fields(json &out) {
  out.key("text");
  out.boolean(true);
  out.key("streaming");
  out.boolean(true);
  out.key("multiple_candidates");
  out.boolean(true);
  out.key("logit_bias");
  out.boolean(true);
  out.key("logprobs");
  out.boolean(true);
  out.key("tools");
  out.boolean(true);
  out.key("tool_template_qualification");
  out.string("probed_json_envelope");
  out.key("max_function_tools");
  out.number(128);
  out.key("max_tool_calls");
  out.number(128);
  out.key("structured_output");
  out.boolean(true);
  out.key("schema_keywords");
  out.array();
  for (const char *keyword :
       {"type", "properties", "required", "additionalProperties", "items",
        "minItems", "maxItems", "enum", "const", "anyOf", "$ref", "$defs",
        "definitions", "minimum", "maximum"})
    out.string(keyword);
  out.end_array();
  out.key("schema_limits");
  out.object();
  out.key("max_schema_bytes");
  out.number(65536);
  out.key("max_expanded_nodes");
  out.number(4096);
  out.key("max_depth");
  out.number(32);
  out.key("max_properties");
  out.number(256);
  out.key("max_enum_values");
  out.number(128);
  out.key("max_alternatives");
  out.number(32);
  out.key("references");
  out.string("same_document_acyclic");
  out.key("numeric_bounds");
  out.string("integer_only_inclusive_within_plus_minus_10^15");
  out.key("primitive_number_components");
  out.string("up_to_16_digits;const_and_enum_literals_may_be_longer");
  out.key("combinations");
  out.string("anyOf_and_ref_require_no_constraint_siblings;const_and_enum_"
             "allow_type_only");
  out.key("max_output_json_depth");
  out.number(64);
  out.key("max_output_key_bytes");
  out.number(1024);
  out.key("max_output_number_bytes");
  out.number(1024);
  out.key("max_output_parser_heap_bytes");
  out.number(4 * 1024 * 1024);
  out.end_object();
  out.key("reasoning_separation");
  out.boolean(false);
  out.key("compaction");
  out.boolean(false);
  out.key("prompt_cache");
  out.boolean(true);
  out.key("default_prompt_cache_bytes");
  out.number(64 * 1024 * 1024);
  out.key("default_prompt_cache_entries");
  out.number(8);
  out.key("default_prompt_cache_idle_seconds");
  out.number(300);
  out.key("persistent_snapshots");
  out.boolean(false);
  out.key("media");
  out.array();
  out.end_array();
}

} // namespace

std::string capabilities(state &) {
  json out;
  out.object();
  out.key("scope");
  out.string("receiver_implementation");
  feature_fields(out);
  out.key("max_candidates");
  out.number(16);
  out.key("max_request_text_bytes");
  out.number(4 * 1024 * 1024);
  out.key("max_top_logprobs");
  out.number(20);
  out.key("max_logit_bias_entries");
  out.number(4096);
  out.key("default_max_output_bytes");
  out.number(16 * 1024 * 1024);
  out.key("max_stop_strings");
  out.number(4);
  out.end_object();
  return out.finish();
}

std::string devices() {
  json out;
  out.array();
  for (size_t i = 0; i < ggml_backend_dev_count(); ++i) {
    ggml_backend_dev_t device = ggml_backend_dev_get(i);
    ggml_backend_dev_props props{};
    ggml_backend_dev_get_props(device, &props);
    const auto reg = ggml_backend_dev_backend_reg(device);
    const char *provider = ggml_backend_reg_name(reg);
    if (std::strcmp(provider, "CPU") && std::strcmp(provider, "Vulkan"))
      continue;
    out.object();
    out.key("provider");
    out.string(provider);
    out.key("name");
    out.nullable(props.name ? props.name : "");
    out.key("description");
    out.nullable(props.description ? props.description : "");
    out.key("pci_id");
    out.nullable(props.device_id ? props.device_id : "");
    out.key("device_uuid");
    out.null();
    out.key("memory_total_bytes");
    if (props.memory_total)
      out.string(std::to_string(props.memory_total));
    else
      out.null();
    out.key("memory_free_bytes");
    if (props.memory_total)
      out.string(std::to_string(props.memory_free));
    else
      out.null();
    out.key("dedicated_memory_bytes");
    out.null();
    out.key("async");
    out.boolean(props.caps.async);
    out.key("events");
    out.boolean(props.caps.events);
    out.end_object();
  }
  out.end_array();
  return out.finish();
}

std::string model_card(state &self) {
  const auto *model = self.model.get();
  const std::string architecture = metadata(model, "general.architecture");
  const std::string license = metadata(model, "general.license");
  const std::string license_name = metadata(model, "general.license.name");
  const std::string license_url = metadata(model, "general.license.link");
  json out;
  out.object();
  out.key("id");
  out.string(self.id);
  out.key("name");
  out.nullable(metadata(model, "general.name"));
  out.key("source");
  out.object();
  out.key("kind");
  out.string(self.source.local.empty() ? "huggingface" : "local");
  out.key("repo");
  out.nullable(self.source.repo);
  out.key("requested_revision");
  out.nullable(self.source.revision);
  out.key("resolved_revision");
  bool immutable = self.source.revision.size() == 40;
  for (char ch : self.source.revision)
    immutable =
        immutable && ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'));
  out.nullable(immutable ? self.source.revision : "");
  out.end_object();
  out.key("license");
  if (license.empty() && license_name.empty() && license_url.empty()) {
    out.null();
  } else {
    out.object();
    out.key("id");
    out.nullable(license);
    out.key("name");
    out.nullable(license_name);
    out.key("url");
    out.nullable(license_url);
    out.key("origin");
    out.string("gguf");
    out.end_object();
  }
  out.key("architecture");
  out.nullable(architecture);
  out.key("parameter_count");
  out.string(std::to_string(llama_model_n_params(model)));
  out.key("quantization");
  out.object();
  out.key("file_type");
  out.nullable(metadata(model, "general.file_type"));
  out.key("tensor_types");
  out.array();
  std::map<ggml_type, uint64_t> types;
  for (const auto &entry : model->tensors_by_name)
    ++types[entry.second->type];
  for (const auto &entry : types) {
    out.object();
    out.key("type");
    out.string(ggml_type_name(entry.first));
    out.key("count");
    out.string(std::to_string(entry.second));
    out.end_object();
  }
  out.end_array();
  out.end_object();
  out.key("context");
  out.object();
  out.key("trained_tokens");
  out.number(static_cast<uint64_t>(llama_model_n_ctx_train(model)));
  out.key("original_tokens");
  out.null();
  out.key("scaling");
  out.nullable(metadata(model, (architecture + ".rope.scaling.type").c_str()));
  out.key("configured_tokens");
  if (self.config.context_tokens)
    out.number(self.config.context_tokens);
  else
    out.null();
  out.key("effective_tokens");
  out.number(llama_n_ctx(self.context.get()));
  out.end_object();
  out.key("chat_template");
  out.object();
  const char *tmpl = self.config.chat_template
                         ? self.config.chat_template
                         : llama_model_chat_template(model, nullptr);
  out.key("present");
  out.boolean(tmpl != nullptr);
  out.key("identity");
  out.nullable(tmpl ? model::digest_hex(tmpl, std::strlen(tmpl)) : "");
  out.key("origin");
  out.string(self.config.chat_template ? "caller" : "gguf");
  out.key("supported");
  out.boolean(template_supported(tmpl));
  out.end_object();
  out.key("capabilities");
  out.object();
  feature_fields(out);
  out.end_object();
  out.key("files");
  out.array();
  // Describe the files actually acquired, independently of later cwd/XDG
  // changes.
  auto resolved = self.source.value;
  resolved.local_path = self.path.c_str();
  resolved.repo = resolved.revision = resolved.filename = nullptr;
  resolved.cache_root = resolved.bearer_token = nullptr;
  const auto plan = model::make_plan(&resolved);
  for (const auto &file : plan.files) {
    out.object();
    out.key("filename");
    std::string name = file.name.substr(file.name.find_last_of('/') + 1);
    if (self.source.local.empty()) {
      const auto slash = self.source.filename.find_last_of('/');
      if (slash != std::string::npos)
        name = self.source.filename.substr(0, slash + 1) + name;
    }
    out.string(name);
    out.key("bytes");
    struct stat status{};
    if (!stat(file.path.c_str(), &status) && status.st_size >= 0)
      out.string(std::to_string(static_cast<uint64_t>(status.st_size)));
    else
      out.null();
    out.key("sha256");
    out.nullable(file.sha256);
    out.key("checksum_validated");
    out.boolean(!file.sha256.empty());
    out.end_object();
  }
  out.end_array();
  out.key("provenance");
  out.object();
  out.key("model_facts");
  out.string("loaded_gguf_native_model");
  out.key("publisher_metadata");
  out.null();
  out.key("conflicts");
  out.array();
  out.end_array();
  out.end_object();
  out.end_object();
  return out.finish();
}
} // namespace client
} // namespace ailo
