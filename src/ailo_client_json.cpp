#include "ailo_bounded_json_internal.hpp"
#include "ailo_client_internal.hpp"
#include "ailo_model_internal.hpp"
#include "ailo_openai_internal.hpp"
#include "src/llama-ext.h"
#include "src/llama-model.h"
#include <cstring>
#include <lonejson.h>
#include <map>
#include <sys/stat.h>

namespace ailo {
namespace client {
namespace {
using json = bounded_json;

std::string device_identity(const state &self, ggml_backend_dev_t dev) {
  if (!dev)
    return {};
  for (const auto &selected : self.selected_info)
    if (selected.device == dev)
      return selected.id;
  return ggml_backend_dev_type(dev) == GGML_BACKEND_DEVICE_TYPE_CPU
             ? "cpu"
             : ggml_backend_dev_name(dev);
}
void placement(json &out, const state &self) {
  out.object();
  out.key("requested_backend");
  out.string(self.config.backend == AILO_BACKEND_CPU      ? "cpu"
             : self.config.backend == AILO_BACKEND_VULKAN ? "vulkan"
                                                          : "auto");
  out.key("actual_backend");
  out.string(self.actual == AILO_BACKEND_CPU ? "cpu" : "vulkan");
  out.key("explicit");
  out.boolean(self.gpu.required());
  out.key("split");
  out.string(self.gpu.split == AILO_GPU_SPLIT_NONE ? "none" : "layer");
  out.key("requested_gpu_layers");
  if (self.config.gpu_layers > 0)
    out.number(static_cast<uint64_t>(self.config.gpu_layers));
  else
    out.null();
  out.key("selected_devices");
  out.array();
  for (size_t n = 0; n < self.selected_info.size(); ++n) {
    const auto &device = self.selected_info[n];
    out.object();
    out.key("id");
    out.string(device.id);
    out.key("driver");
    out.string(device.driver);
    out.key("requested_weight");
    if (self.gpu.weights.empty())
      out.null();
    else
      out.real(self.gpu.weights[n]);
    out.key("normalized_weight");
    if (self.gpu.tensor_split.empty())
      out.null();
    else
      out.real(self.gpu.tensor_split[n]);
    uint64_t count = 0;
    for (uint32_t layer = 0; layer < self.model->hparams.n_layer_all; ++layer)
      count += self.model->dev_layer(static_cast<int>(layer)) == device.device;
    out.key("repeating_layers");
    out.number(count);
    out.key("output_layer");
    out.boolean(self.model->dev_output() == device.device);
    out.end_object();
  }
  out.end_array();
  out.key("layers");
  out.array();
  for (uint32_t layer = 0; layer < self.model->hparams.n_layer_all; ++layer) {
    out.object();
    out.key("index");
    out.number(layer);
    out.key("device_id");
    out.nullable(
        device_identity(self, self.model->dev_layer(static_cast<int>(layer))));
    out.key("buffer_type");
    out.string(ggml_backend_buft_name(
        self.model->select_buft(static_cast<int>(layer))));
    out.end_object();
  }
  out.end_array();
  out.key("output_device_id");
  out.nullable(device_identity(self, self.model->dev_output()));
  out.key("native_buffers");
  out.array();
  for (const auto &p : llama_get_memory_breakdown(self.context.get())) {
    out.object();
    out.key("buffer_type");
    out.string(ggml_backend_buft_name(p.first));
    out.key("device_id");
    out.nullable(device_identity(self, ggml_backend_buft_get_device(p.first)));
    out.key("host_memory");
    out.boolean(ggml_backend_buft_is_host(p.first));
    out.key("model_bytes");
    out.string(std::to_string(p.second.model));
    out.key("context_bytes");
    out.string(std::to_string(p.second.context));
    out.key("compute_bytes");
    out.string(std::to_string(p.second.compute));
    out.end_object();
  }
  out.end_array();
  out.key("additional_driver_memory_bytes");
  out.null();
  out.end_object();
}

void feature_fields(json &out) {
  out.key("text");
  out.boolean(true);
  out.key("streaming");
  out.boolean(true);
  out.key("multiple_candidates");
  out.boolean(true);
  out.key("gpu_layer_splitting");
  out.boolean(true);
  out.key("explicit_gpu_selection");
  out.boolean(true);
  out.key("gpu_row_splitting");
  out.boolean(false);
  out.key("logit_bias");
  out.boolean(true);
  out.key("logprobs");
  out.boolean(true);
  out.key("output_byte_provenance");
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
  out.boolean(true);
  out.key("reasoning_template_qualification");
  out.string("initial <think> or [THINK] block, probed delimiters and history "
             "preservation; controls qualified per request");
  out.key("compaction");
  out.boolean(true);
  out.key("automatic_compaction");
  out.boolean(true);
  out.key("compaction_replay");
  out.boolean(true);
  out.key("continuation");
  out.boolean(true);
  out.key("prompt_cache");
  out.boolean(true);
  out.key("default_prompt_cache_bytes");
  out.number(64 * 1024 * 1024);
  out.key("default_prompt_cache_entries");
  out.number(8);
  out.key("default_prompt_cache_idle_seconds");
  out.number(300);
  out.key("persistent_snapshots");
  out.boolean(true);
  out.key("media");
  out.array();
  out.end_array();
}

} // namespace

std::string capabilities(state &self) {
  json out;
  out.object();
  out.key("scope");
  out.string("receiver_implementation");
  feature_fields(out);
  out.key("chat_completions");
  out.boolean(true);
  out.key("responses");
  out.boolean(true);
  out.key("retained_response_records");
  out.boolean(self.openai->policy.repository_writable);
  out.key("response_conversations");
  out.boolean(self.openai->policy.conversation_readable);
  out.key("conversation_api");
  out.boolean(true);
  out.key("response_automatic_compaction");
  out.boolean(true);
  out.key("response_compaction_trigger");
  out.boolean(true);
  out.key("response_compaction");
  out.boolean(true);
  out.key("response_reasoning_encryption");
  out.boolean(true);
  out.key("retained_chat_records");
  out.boolean(self.openai->policy.repository_writable);
  out.key("stored_record_queries");
  out.boolean(self.openai->repository.scan != nullptr);
  out.key("stored_chat_metadata_update");
  out.boolean(self.openai->repository.edit != nullptr);
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
    const auto detail = describe_device(device);
    out.object();
    out.key("provider");
    out.string(provider);
    const bool vulkan = !std::strcmp(provider, "Vulkan");
    out.key("id");
    out.nullable(vulkan ? detail.id : "cpu");
    out.key("driver");
    out.nullable(detail.driver);
    out.key("vendor_id");
    if (detail.vendor)
      out.number(detail.vendor);
    else
      out.null();
    out.key("device_id");
    if (detail.product)
      out.number(detail.product);
    else
      out.null();
    out.key("eligible");
    out.boolean(!vulkan || detail.eligible);
    out.key("availability_reason");
    out.string(vulkan ? detail.reason : "eligible");
    out.key("memory_kind");
    out.string(vulkan ? (detail.integrated ? "shared" : "device_local")
                      : "host");
    out.key("name");
    out.nullable(props.name ? props.name : "");
    out.key("description");
    out.nullable(props.description ? props.description : "");
    out.key("pci_id");
    out.nullable(props.device_id ? props.device_id : "");
    out.key("device_uuid");
    out.nullable(detail.uuid);
    out.key("driver_uuid");
    out.nullable(detail.driver_uuid);
    out.key("memory_total_bytes");
    if (props.memory_total)
      out.string(std::to_string(props.memory_total));
    else
      out.null();
    out.key("memory_free_bytes");
    if (props.memory_total && vulkan && detail.budget_known)
      out.string(std::to_string(props.memory_free));
    else
      out.null();
    out.key("dedicated_memory_bytes");
    if (vulkan && !detail.integrated && props.memory_total)
      out.string(std::to_string(props.memory_total));
    else
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
  self.ensure_metadata();
  const auto *model = self.model.get();
  const std::string architecture = metadata(model, "general.architecture");
  metadata_claims native;
  native.name = metadata(model, "general.name");
  native.license_id = metadata(model, "general.license");
  native.license_name = metadata(model, "general.license.name");
  native.license_url = metadata(model, "general.license.link");
  const auto has_license = [](const metadata_claims &c) {
    return !c.license_id.empty() || !c.license_name.empty() ||
           !c.license_url.empty();
  };
  const auto &supplemental = self.supplemental;
  const auto &publisher = self.publisher.claims;
  const auto &name = !native.name.empty()         ? native.name
                     : !supplemental.name.empty() ? supplemental.name
                                                  : publisher.name;
  const char *name_origin = !native.name.empty()         ? "gguf"
                            : !supplemental.name.empty() ? "caller"
                                                         : "huggingface";
  const auto &license = has_license(native)         ? native
                        : has_license(supplemental) ? supplemental
                                                    : publisher;
  const char *license_origin = has_license(native)         ? "gguf"
                               : has_license(supplemental) ? "caller"
                                                           : "huggingface";
  const auto &lineage = supplemental.lineage_known ? supplemental : publisher;
  json out;
  out.object();
  out.key("id");
  out.string(self.id);
  out.key("name");
  out.nullable(name);
  out.key("source");
  out.object();
  out.key("kind");
  out.string(self.source.local.empty() ? "huggingface" : "local");
  out.key("repo");
  out.nullable(self.source.repo);
  out.key("requested_revision");
  out.nullable(self.source.revision);
  out.key("resolved_revision");
  out.nullable(immutable_revision(self.source.revision) ? self.source.revision
                                                        : "");
  out.key("base_models");
  if (lineage.lineage_known) {
    out.array();
    for (const auto &id : lineage.base_models)
      out.string(id);
    out.end_array();
  } else
    out.null();
  out.key("lineage_origin");
  if (lineage.lineage_known)
    out.string(supplemental.lineage_known ? "caller" : "huggingface");
  else
    out.null();
  out.end_object();
  out.key("license");
  if (!has_license(license)) {
    out.null();
  } else {
    out.object();
    out.key("id");
    out.nullable(license.license_id);
    out.key("name");
    out.nullable(license.license_name);
    out.key("url");
    out.nullable(license.license_url);
    out.key("origin");
    out.string(license_origin);
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
  out.key("name_origin");
  if (name.empty())
    out.null();
  else
    out.string(name_origin);
  out.key("supplemental_metadata");
  out.object();
  write_claims(out, supplemental);
  out.end_object();
  out.key("publisher_metadata");
  out.object();
  out.key("status");
  out.string(self.publisher.status);
  write_claims(out, publisher);
  out.end_object();
  out.key("conflicts");
  out.array();
  const auto conflict = [&](const char *field, const std::string &selected,
                            const char *origin, const std::string &claim,
                            const char *claim_origin) {
    if (selected.empty() || claim.empty() || selected == claim)
      return;
    out.object();
    out.key("field");
    out.string(field);
    out.key("selected_origin");
    out.string(origin);
    out.key("selected");
    out.string(selected);
    out.key("claim_origin");
    out.string(claim_origin);
    out.key("claim");
    out.string(claim);
    out.end_object();
  };
  const std::pair<const metadata_claims *, const char *> claims[] = {
      {&supplemental, "caller"}, {&publisher, "huggingface"}};
  for (const auto &claim : claims) {
    conflict("name", name, name_origin, claim.first->name, claim.second);
    conflict("license.id", license.license_id, license_origin,
             claim.first->license_id, claim.second);
    conflict("license.name", license.license_name, license_origin,
             claim.first->license_name, claim.second);
    conflict("license.url", license.license_url, license_origin,
             claim.first->license_url, claim.second);
  }
  if (supplemental.lineage_known && publisher.lineage_known &&
      supplemental.base_models != publisher.base_models) {
    out.object();
    out.key("field");
    out.string("source.base_models");
    out.key("selected_origin");
    out.string("caller");
    out.key("selected");
    out.array();
    for (const auto &id : supplemental.base_models)
      out.string(id);
    out.end_array();
    out.key("claim_origin");
    out.string("huggingface");
    out.key("claim");
    out.array();
    for (const auto &id : publisher.base_models)
      out.string(id);
    out.end_array();
    out.end_object();
  }
  out.end_array();
  out.end_object();
  out.key("placement");
  placement(out, self);
  out.end_object();
  return out.finish();
}
} // namespace client
} // namespace ailo
