#ifndef AILO_SCHEMA_INTERNAL_HPP
#define AILO_SCHEMA_INTERNAL_HPP
#include "ailo_client_internal.hpp"
#include <lonejson.h>

namespace ailo {
namespace client {
std::string output_grammar(const ailo_request &request);
std::string schema_document(const char *source, bool normalize);

// Push-fed lonejson validation with a discard sink. No output is retained.
class json_validation {
  std::unique_ptr<lonejson, decltype(&lonejson_free)> runtime{nullptr,
                                                              lonejson_free};
  lonejson_writer writer{};
  lonejson_writer_value_stream stream{};

public:
  explicit json_validation(size_t limit);
  ~json_validation();
  void push(const char *data, size_t size);
  void finish();
};
} // namespace client
} // namespace ailo
#endif
