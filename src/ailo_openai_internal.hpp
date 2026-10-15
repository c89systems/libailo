#ifndef AILO_OPENAI_INTERNAL_HPP
#define AILO_OPENAI_INTERNAL_HPP
#include "ailo/openai.h"
#include "ailo_openai_retention_internal.hpp"
#include <functional>
#include <lonejson.h>

namespace ailo {
namespace client {
using byte_sink = std::function<void(const void *, size_t)>;
// Direct lonejson sink writer. No output collector, including per SSE frame.
class wire_json {
  std::unique_ptr<lonejson, decltype(&lonejson_free)> runtime{nullptr,
                                                              lonejson_free};
  lonejson_writer writer{};
  byte_sink sink;
  std::exception_ptr problem;
  static lonejson_status forward(void *, const void *, size_t,
                                 lonejson_error *) noexcept;
  void check(lonejson_status);

public:
  explicit wire_json(byte_sink, size_t limit = 64 * 1024 * 1024);
  ~wire_json();
  wire_json(const wire_json &) = delete;
  void object();
  void end_object();
  void array();
  void end_array();
  void key(const char *);
  void key(const std::string &);
  void string(const std::string &);
  void string(const void *, size_t);
  void number(uint64_t);
  void real(double);
  void boolean(bool);
  void null();
  void finish();
};
struct openai_state {
  openai_retention_policy policy;
  ailo_record_repository repository{};
  size_t memory_limit = 32 * 1024 * 1024, record_limit = 64 * 1024 * 1024;
  explicit openai_state(const ailo_openai_config *);
};
uint64_t api_now();
void api_identity(ailo_api_info &, const char *prefix);
void api_check(ailo_status);
int api_http_status(ailo_status);
byte_sink api_sink(state &, ailo_output_sink, void *, ailo_api_info &);
class record_transaction {
  openai_state &owner;
  void *transaction = nullptr;
  size_t bytes = 0;

public:
  record_transaction(openai_state &, const ailo_record_metadata &);
  ~record_transaction();
  byte_sink section(ailo_record_section);
  void commit();
};
} // namespace client
} // namespace ailo
#endif
