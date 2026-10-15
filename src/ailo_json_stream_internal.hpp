#ifndef AILO_JSON_STREAM_INTERNAL_HPP
#define AILO_JSON_STREAM_INTERNAL_HPP

#include "ailo_client_internal.hpp"
#include <exception>
#include <lonejson.h>

namespace ailo {
namespace client {
// Compose public lonejson push parsing and a mapped JSON_VALUE visitor. The
// synthetic one-item wrapper is constant framing, never a captured JSON value.
class json_stream {
  struct record {
    lonejson_json_value value;
  } item{};
  std::unique_ptr<lonejson, decltype(&lonejson_free)> runtime{nullptr,
                                                              lonejson_free};
  lonejson_array_stream *stream = nullptr;
  size_t bytes = 0, limit;
  unsigned completed = 0;
  bool finished = false;
  lonejson_path_value_visitor visitor{}, proxy{};
  void *user;
  std::exception_ptr callback_error;
  template <auto Member, typename... Args>
  static lonejson_status forward(void *data, const lonejson_value_path *path,
                                 Args... args) noexcept {
    auto &self = *static_cast<json_stream *>(data);
    try {
      const auto callback = self.visitor.*Member;
      return callback ? callback(self.user, path, args...) : LONEJSON_STATUS_OK;
    } catch (...) {
      self.callback_error = std::current_exception();
      return LONEJSON_STATUS_IO_ERROR;
    }
  }
  static const lonejson_map &map();
  static lonejson_status complete(void *user, void *);
  void feed(const void *data, size_t size);

public:
  json_stream(const lonejson_path_value_visitor &visitor, void *user,
              size_t max_bytes);
  ~json_stream();
  json_stream(const json_stream &) = delete;
  json_stream &operator=(const json_stream &) = delete;
  void push(const char *data, size_t size);
  void finish();
};
} // namespace client
} // namespace ailo
#endif
