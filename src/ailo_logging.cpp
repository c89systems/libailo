#include "ailo_logging_internal.hpp"
#include <cstring>

namespace ailo {
namespace client {
namespace {
thread_local const log_context *continuation_owner = nullptr;
thread_local pslog_level continuation_level = PSLOG_LEVEL_INFO;

void native_log(ggml_log_level level, const char *text, void *data) noexcept {
  if (!text || !data)
    return;
  const auto &context = *static_cast<const log_context *>(data);
  if (continuation_owner != &context) {
    continuation_owner = &context;
    continuation_level = PSLOG_LEVEL_INFO;
  }
  pslog_level mapped = PSLOG_LEVEL_NOLEVEL;
  switch (level) {
  case GGML_LOG_LEVEL_TRACE:
    mapped = PSLOG_LEVEL_TRACE;
    break;
  case GGML_LOG_LEVEL_DEBUG:
    mapped = PSLOG_LEVEL_DEBUG;
    break;
  case GGML_LOG_LEVEL_INFO:
    mapped = PSLOG_LEVEL_INFO;
    break;
  case GGML_LOG_LEVEL_WARN:
    mapped = PSLOG_LEVEL_WARN;
    break;
  case GGML_LOG_LEVEL_ERROR:
    mapped = PSLOG_LEVEL_ERROR;
    break;
  case GGML_LOG_LEVEL_CONT:
    mapped = continuation_level;
    break;
  case GGML_LOG_LEVEL_NONE:
    break;
  }
  continuation_level = mapped;
  size_t size = std::strlen(text);
  if (size && text[size - 1] == '\n')
    --size;
  if (size && text[size - 1] == '\r')
    --size;
  try {
    pslog_field fields[] = {
        pslog_str("phase", context.phase),
        pslog_bool("continuation", level == GGML_LOG_LEVEL_CONT)};
    const pslog_string_view message{text, size};
    context.logger->log_view(context.logger, mapped, message, fields, 2);
  } catch (...) {
    // Foreign C callbacks must not throw/longjmp. Never unwind a native worker.
  }
}
} // namespace

logging_state::logging_state(pslog_logger *parent,
                             const std::string &model_id) {
  const pslog_field fields[] = {pslog_str("component", "libailo"),
                                pslog_str("model_id", model_id.c_str())};
  logger.reset(pslog_with(parent, fields, 2));
  if (!logger)
    throw std::bad_alloc();
  const char *names[] = {"load", "generate", "metadata", "devices", "destroy"};
  for (size_t i = 0; i < contexts.size(); ++i)
    contexts[i] = {logger.get(), names[i]};
}

log_scope::log_scope(logging_state *logging, log_phase phase)
    : previous(ggml_log_scope_get()), previous_owner(continuation_owner),
      previous_level(continuation_level),
      current(logging ? &logging->contexts[static_cast<size_t>(phase)]
                      : nullptr) {
  continuation_owner = current;
  continuation_level = PSLOG_LEVEL_INFO;
  ggml_log_scope_set(
      {current ? native_log : nullptr, const_cast<log_context *>(current)});
}

log_scope::~log_scope() {
  ggml_log_scope_set(previous);
  continuation_owner = previous_owner;
  continuation_level = previous_level;
}

void log_scope::trace(const char *message) const noexcept {
  if (current)
    native_log(GGML_LOG_LEVEL_TRACE, message,
               const_cast<log_context *>(current));
}
} // namespace client
} // namespace ailo
