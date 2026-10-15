#include "ailo_openai_internal.hpp"
#include <chrono>
#include <cstring>
#include <openssl/rand.h>

#define AILO_PUBLIC extern "C" __attribute__((visibility("default")))
namespace ailo {
namespace client {
lonejson_status wire_json::forward(void *user, const void *data, size_t size,
                                   lonejson_error *) noexcept {
  auto &self = *static_cast<wire_json *>(user);
  try {
    self.sink(data, size);
    return LONEJSON_STATUS_OK;
  } catch (...) {
    self.problem = std::current_exception();
    return LONEJSON_STATUS_IO_ERROR;
  }
}
void wire_json::check(lonejson_status status) {
  if (problem)
    std::rethrow_exception(problem);
  if (status != LONEJSON_STATUS_OK)
    throw failure(status == LONEJSON_STATUS_ALLOCATION_FAILED
                      ? AILO_NO_MEMORY
                      : AILO_OUT_OF_RANGE,
                  "API JSON exceeds its serialization limits");
}
wire_json::wire_json(byte_sink output, size_t limit) : sink(std::move(output)) {
  auto config = lonejson_default_config();
  config.write_max_output_bytes = limit;
  runtime.reset(lonejson_new(&config, nullptr));
  if (!runtime)
    throw std::bad_alloc();
  try {
    check(lonejson_writer_init_sink(runtime.get(), &writer, forward, this,
                                    nullptr));
  } catch (...) {
    lonejson_writer_cleanup(&writer);
    throw;
  }
}
wire_json::~wire_json() { lonejson_writer_cleanup(&writer); }
void wire_json::object() { check(writer.begin_object(&writer, nullptr)); }
void wire_json::end_object() { check(writer.end_object(&writer, nullptr)); }
void wire_json::array() { check(writer.begin_array(&writer, nullptr)); }
void wire_json::end_array() { check(writer.end_array(&writer, nullptr)); }
void wire_json::key(const char *key) {
  check(writer.key(&writer, key, std::strlen(key), nullptr));
}
void wire_json::key(const std::string &key) {
  check(writer.key(&writer, key.data(), key.size(), nullptr));
}
void wire_json::string(const std::string &value) {
  string(value.data(), value.size());
}
void wire_json::string(const void *value, size_t size) {
  check(writer.string(&writer, value, size, nullptr));
}
void wire_json::number(uint64_t n) { check(writer.u64(&writer, n, nullptr)); }
void wire_json::real(double n) { check(writer.f64(&writer, n, nullptr)); }
void wire_json::boolean(bool v) { check(writer.bool_fn(&writer, v, nullptr)); }
void wire_json::null() { check(writer.null_fn(&writer, nullptr)); }
void wire_json::finish() { check(writer.finish(&writer, nullptr)); }
openai_state::openai_state(const ailo_openai_config *input) {
  if (!input)
    return;
  policy.zero_data_retention = input->zero_data_retention != 0;
  policy.chat_store_by_default = !input->chat_store_opt_in;
  if (input->namespace_id) {
    const size_t size = strnlen(input->namespace_id, 257);
    if (size > 256)
      throw failure(AILO_INVALID_ARGUMENT,
                    "API namespace exceeds 256 UTF-8 bytes");
    policy.namespace_id.assign(input->namespace_id, size);
  }
  if (policy.namespace_id.size() > 256 ||
      policy.namespace_id.find('\0') != std::string::npos)
    throw failure(AILO_INVALID_ARGUMENT,
                  "API namespace exceeds 256 UTF-8 bytes");
  require_utf8(policy.namespace_id.data(), policy.namespace_id.size());
  const auto response_ttl =
      detail::to_native(input->response_retention_seconds);
  if (response_ttl)
    policy.response_retention_seconds = response_ttl;
  policy.chat_retention_seconds =
      detail::to_native(input->chat_retention_seconds);
  if (policy.response_retention_seconds < openai_response_retention_seconds)
    throw failure(AILO_INVALID_ARGUMENT,
                  "Stored Responses require at least 30 days");
  if (input->max_output_memory_bytes)
    memory_limit = input->max_output_memory_bytes;
  if (input->max_record_bytes)
    record_limit = input->max_record_bytes;
  if (input->repository) {
    repository = *input->repository;
    if (policy.namespace_id.empty() || !repository.reserve ||
        !repository.write || !repository.commit || !repository.abort ||
        !repository.open || !repository.read || !repository.close ||
        !repository.erase)
      throw failure(AILO_INVALID_ARGUMENT,
                    "A repository requires a namespace and all "
                    "transaction/read/delete callbacks");
    policy.repository_readable = policy.repository_writable = true;
  }
}
uint64_t api_now() {
  const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
  if (now < 0)
    throw failure(AILO_IO_FAILED, "UTC clock precedes Unix epoch");
  return static_cast<uint64_t>(now);
}
void api_identity(ailo_api_info &info, const char *prefix) {
  unsigned char bytes[16];
  if (RAND_bytes(bytes, sizeof(bytes)) != 1)
    throw failure(AILO_IO_FAILED, "Cannot create API identity");
  std::string id(prefix);
  const char *hex = "0123456789abcdef";
  for (auto byte : bytes) {
    id += hex[byte >> 4];
    id += hex[byte & 15];
  }
  std::memcpy(info.id, id.c_str(), id.size() + 1);
}
void api_check(ailo_status status) {
  if (status != AILO_OK)
    throw failure(status, "Retained API repository operation failed");
}
int api_http_status(ailo_status status) {
  switch (status) {
  case AILO_OK:
    return 200;
  case AILO_NOT_FOUND:
  case AILO_UNKNOWN_MODEL:
    return 404;
  case AILO_BUSY:
    return 503;
  case AILO_INVALID_ARGUMENT:
  case AILO_INVALID_JSON:
  case AILO_UNSUPPORTED:
  case AILO_CONTEXT_LIMIT:
    return 400;
  case AILO_OUT_OF_RANGE:
    return 413;
  default:
    return 500;
  }
}
byte_sink api_sink(state &self, ailo_output_sink sink, void *user,
                   ailo_api_info &info) {
  return [&self, sink, user, &info](const void *data, size_t size) {
    if (self.is_cancelled())
      throw failure(AILO_CANCELLED, "API operation cancelled");
    if (!size)
      return;
    if (!info.http_status)
      info.http_status = 200;
    info.started = 1;
    const int accepted = sink(user, data, size);
    if (accepted <= 0)
      throw failure(accepted ? AILO_SINK_FAILED : AILO_CANCELLED,
                    accepted ? "API output sink failed"
                             : "API output sink cancelled");
  };
}
record_transaction::record_transaction(openai_state &state,
                                       const ailo_record_metadata &metadata)
    : owner(state) {
  try {
    api_check(owner.repository.reserve(owner.repository.user_data, &metadata,
                                       &transaction, nullptr));
    if (!transaction)
      throw failure(AILO_INTERNAL_ERROR,
                    "Repository returned no reserved transaction");
  } catch (...) {
    if (transaction)
      owner.repository.abort(owner.repository.user_data, transaction);
    throw;
  }
}
record_transaction::~record_transaction() {
  if (transaction)
    owner.repository.abort(owner.repository.user_data, transaction);
}
byte_sink record_transaction::section(ailo_record_section section) {
  return [this, section](const void *data, size_t size) {
    if (size > owner.record_limit - bytes)
      throw failure(AILO_OUT_OF_RANGE,
                    "Retained record exceeds its reserved byte limit");
    api_check(owner.repository.write(owner.repository.user_data, transaction,
                                     section, data, size, nullptr));
    bytes += size;
  };
}
void record_transaction::commit() {
  api_check(owner.repository.commit(owner.repository.user_data, transaction,
                                    nullptr));
  transaction = nullptr;
}
} // namespace client
} // namespace ailo
AILO_PUBLIC ailo_openai_config ailo_openai_default_config(void) { return {}; }

namespace {
using namespace ailo::client;
void live_repository(state &self, const char *id) {
  if (!id || !*id || strnlen(id, 257) > 256)
    throw failure(AILO_INVALID_ARGUMENT,
                  "A retained completion ID is required");
  require_utf8(id, std::strlen(id));
  if (!self.openai->policy.repository_readable)
    throw failure(AILO_UNSUPPORTED,
                  "No retained-record repository is configured");
}
struct read_forward {
  byte_sink sink;
  size_t limit, bytes = 0;
  std::exception_ptr problem;
  static int send(void *u, const void *data, size_t size) noexcept {
    auto &self = *static_cast<read_forward *>(u);
    try {
      if ((!data && size) || size > self.limit - self.bytes)
        throw failure(AILO_OUT_OF_RANGE,
                      "Retained record exceeds its read limit");
      self.sink(data, size);
      self.bytes += size;
      return 1;
    } catch (...) {
      self.problem = std::current_exception();
      return -1;
    }
  }
};
struct record_handle {
  ailo_record_repository &repository;
  void *value = nullptr;
  record_handle(openai_state &owner, const char *id)
      : repository(owner.repository) {
    ailo_u64 expires{};
    try {
      api_check(repository.open(
          repository.user_data, owner.policy.namespace_id.c_str(),
          AILO_RECORD_CHAT, id, &value, &expires, nullptr));
      if (!value)
        throw failure(AILO_INTERNAL_ERROR,
                      "Repository returned no read handle");
      if (ailo::detail::to_native(expires) &&
          ailo::detail::to_native(expires) <= api_now())
        throw failure(AILO_NOT_FOUND, "Retained completion expired");
    } catch (...) {
      if (value)
        repository.close(repository.user_data, value);
      throw;
    }
  }
  ~record_handle() { repository.close(repository.user_data, value); }
};
} // namespace
AILO_PUBLIC ailo_status ailo_client_chat_retrieve(
    ailo_client *client, const char *id, ailo_bool input, ailo_output_sink sink,
    void *user, ailo_api_info *info, ailo_error *error) {
  if (info)
    *info = {};
  if (!sink || !info) {
    if (info)
      info->http_status = 400;
    return ailo_error_set(error, AILO_INVALID_ARGUMENT,
                          "API sink and output info are required");
  }
  auto status = boundary(error, [&] {
    operation call(client);
    live_repository(call.self, id);
    info->content_type = "application/json";
    auto &owner = *call.self.openai;
    record_handle handle(owner, id);
    read_forward output{
        api_sink(call.self, sink, user, *info), owner.record_limit, 0, {}};
    const auto result =
        owner.repository.read(owner.repository.user_data, handle.value,
                              input ? AILO_RECORD_INPUT : AILO_RECORD_OUTPUT,
                              read_forward::send, &output, nullptr);
    if (output.problem)
      std::rethrow_exception(output.problem);
    api_check(result);
    info->stored = 1;
  });
  if (!info->started)
    info->http_status = api_http_status(status);
  return status;
}
AILO_PUBLIC ailo_status ailo_client_chat_delete(ailo_client *client,
                                                const char *id,
                                                ailo_output_sink sink,
                                                void *user, ailo_api_info *info,
                                                ailo_error *error) {
  if (info)
    *info = {};
  if (!sink || !info) {
    if (info)
      info->http_status = 400;
    return ailo_error_set(error, AILO_INVALID_ARGUMENT,
                          "API sink and output info are required");
  }
  auto status = boundary(error, [&] {
    operation call(client);
    live_repository(call.self, id);
    auto &owner = *call.self.openai;
    {
      record_handle existing(owner, id);
    }
    api_check(owner.repository.erase(owner.repository.user_data,
                                     owner.policy.namespace_id.c_str(),
                                     AILO_RECORD_CHAT, id, nullptr));
    info->content_type = "application/json";
    wire_json out(api_sink(call.self, sink, user, *info));
    out.object();
    out.key("id");
    out.string(id);
    out.key("object");
    out.string("chat.completion.deleted");
    out.key("deleted");
    out.boolean(true);
    out.end_object();
    out.finish();
  });
  if (!info->started)
    info->http_status = api_http_status(status);
  return status;
}
