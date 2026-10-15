#include "ailo_json_stream_internal.hpp"
#include <cstdio>
#include <cstring>
#include <stdexcept>

using namespace ailo::client;
#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition))                                                          \
      throw std::runtime_error(#condition);                                    \
  } while (0)
struct observation {
  size_t bytes = 0, ends = 0;
  bool fail = false;
  bool cancel = false;
};
lonejson_status chunk(void *user, const lonejson_value_path *path,
                      const char *data, size_t size, lonejson_error *) {
  auto &seen = *static_cast<observation *>(user);
  CHECK(path->segment_count == 1);
  CHECK(path->segments[0].len == 9 &&
        !std::memcmp(path->segments[0].data, "arguments", 9));
  for (size_t i = 0; i < size; ++i)
    CHECK(data[i] == 'a');
  seen.bytes += size;
  if (seen.cancel)
    throw failure(AILO_CANCELLED, "cancelled from JSON callback");
  return seen.fail ? LONEJSON_STATUS_IO_ERROR : LONEJSON_STATUS_OK;
}
lonejson_status end(void *user, const lonejson_value_path *, lonejson_error *) {
  ++static_cast<observation *>(user)->ends;
  return LONEJSON_STATUS_OK;
}

int main() {
  try {
    auto visitor = lonejson_default_path_value_visitor();
    visitor.string_chunk = chunk;
    visitor.object_end = end;
    observation seen;
    {
      json_stream stream(visitor, &seen, 2 * 1024 * 1024);
      stream.push("{\"arguments\":\"", 14);
      const std::string block(4096, 'a');
      for (size_t i = 0; i < 256; ++i) {
        const auto before = seen.bytes;
        stream.push(block.data(), block.size());
        CHECK(seen.bytes > before); // Delivery before string/object completion.
        CHECK(!seen.ends);
      }
      stream.push("\"}", 2);
      CHECK(seen.ends == 1 && seen.bytes == 1024 * 1024);
      stream.finish();
    }
    for (int mode = 0; mode < 4; ++mode) {
      seen = {};
      seen.fail = mode == 3;
      try {
        json_stream stream(visitor, &seen, mode == 2 ? 1 : 1024);
        if (mode == 0)
          stream.push("{", 1);
        if (mode == 1)
          stream.push("{} true", 7);
        if (mode == 2)
          stream.push("{}", 2);
        if (mode == 3)
          stream.push("{\"arguments\":\"a\"}", 17);
        stream.finish();
        CHECK(false);
      } catch (const failure &e) {
        CHECK(e.status == AILO_MODEL_FAILED);
      }
    }
    seen = {};
    seen.cancel = true;
    try {
      json_stream stream(visitor, &seen, 1024);
      stream.push("{\"arguments\":\"a\"}", 17);
      CHECK(false);
    } catch (const failure &e) {
      CHECK(e.status == AILO_CANCELLED);
    }
    std::puts("Push-fed public JSON visitor: bounded incremental delivery and "
              "failures passed");
  } catch (const std::exception &e) {
    std::fprintf(stderr, "json stream: %s\n", e.what());
    return 1;
  }
}
