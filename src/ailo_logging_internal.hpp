#ifndef AILO_LOGGING_INTERNAL_HPP
#define AILO_LOGGING_INTERNAL_HPP

#include "ggml/src/ggml-log-scope.h"
#include <array>
#include <memory>
#include <pslog.h>
#include <string>

namespace ailo {
namespace client {
enum class log_phase { load, generate, metadata, devices, destroy, compact };
struct log_context {
  pslog_logger *logger;
  const char *phase;
};
struct logger_deleter {
  void operator()(pslog_logger *value) const {
    if (value)
      value->destroy(value);
  }
};
struct logging_state {
  std::unique_ptr<pslog_logger, logger_deleter> logger;
  std::array<log_context, 6> contexts;
  logging_state(pslog_logger *parent, const std::string &model_id);
};

/** Contexts are receiver-owned and remain live until workers have joined. */
class log_scope {
  ggml_log_scope previous;
  const log_context *previous_owner;
  pslog_level previous_level;
  const log_context *current;

public:
  log_scope(logging_state *logging, log_phase phase);
  ~log_scope();
  log_scope(const log_scope &) = delete;
  void trace(const char *message) const noexcept;
};
} // namespace client
} // namespace ailo
#endif
