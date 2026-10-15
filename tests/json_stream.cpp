#include "ailo_json_stream_internal.hpp"
#include "ailo_openai_internal.hpp"
#include "ailo_tool_json_internal.hpp"
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <sys/resource.h>

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
static size_t peak_rss_kib() {
  struct rusage usage{};
  CHECK(getrusage(RUSAGE_SELF, &usage) == 0 && usage.ru_maxrss >= 0);
  return static_cast<size_t>(usage.ru_maxrss);
}
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

void tools() {
  const std::string arguments =
      "{ \"city\" : "
      "\"G\xc3\xb6teborg\",\"nested\":[true,null,2,{\"name\":\"x\"}]}";
  const std::string object =
      "{\"id\":\"model-id\",\"type\":\"function\",\"function\":{\"name\":"
      "\"weather\",\"arguments\":" +
      arguments + "}}";
  for (bool array : {false, true}) {
    tool_json_format format;
    format.array = array;
    format.wrapper = "function";
    const std::string value =
        array ? "[" + object + "," + object + "]" : object;
    // Split every boundary, including escaped/UTF-8 bytes and nested values.
    for (size_t chunk : {size_t(1), size_t(7), size_t(4096)}) {
      size_t begins = 0, ends = 0;
      std::string names, raw;
      tool_json_stream stream(format, {"weather"}, 2, 4096,
                              [&](const tool_json_event &event) {
                                CHECK(event.index == ends);
                                if (event.kind == tool_json_kind::begin)
                                  ++begins;
                                if (event.kind == tool_json_kind::name)
                                  names.append(event.data, event.size);
                                if (event.kind == tool_json_kind::arguments) {
                                  require_utf8(event.data, event.size);
                                  raw.append(event.data, event.size);
                                }
                                if (event.kind == tool_json_kind::end)
                                  ++ends;
                              });
      const std::string input = value + "</tool_call>";
      size_t offset = 0;
      while (!stream.complete()) {
        auto n = stream.push(input.data() + offset,
                             std::min(chunk, input.size() - offset));
        CHECK(n);
        offset += n;
      }
      CHECK(offset == value.size());
      stream.finish();
      CHECK(begins == (array ? 2 : 1) && ends == begins);
      CHECK(names == (array ? "weatherweather" : "weather"));
      CHECK(raw == (array ? arguments + arguments : arguments));
    }
  }
  {
    // No argument capture: a large unfinished argument must reach the sink
    // immediately while no call has completed.
    size_t bytes = 0, ends = 0, encoded = 0;
    // Model-free producer -> argument parser -> API writer -> discard sink.
    // Neither the input nor the emitted JSON string is collected by this test.
    wire_json output([&](const void *, size_t n) { encoded += n; });
    output.string_begin();
    tool_json_stream stream({}, {"weather"}, 1, 32 * 1024 * 1024 + 4096,
                            [&](const tool_json_event &event) {
                              if (event.kind == tool_json_kind::arguments) {
                                bytes += event.size;
                                output.string_chunk(event.data, event.size);
                              }
                              if (event.kind == tool_json_kind::end)
                                ++ends;
                            });
    const std::string prefix =
        "{\"name\":\"weather\",\"arguments\":{\"data\":\"";
    CHECK(stream.push(prefix.data(), prefix.size()) == prefix.size());
    const std::string block(4096, 'x');
    size_t warm_rss = 0;
    for (size_t i = 0; i < 8192; ++i) {
      auto before = bytes;
      auto wire_before = encoded;
      CHECK(stream.push(block.data(), block.size()) == block.size());
      CHECK(bytes == before + block.size() && !ends);
      CHECK(encoded == wire_before + block.size());
      if (i == 255)
        warm_rss = peak_rss_kib();
    }
    CHECK(stream.push("\"}}", 3) == 3);
    stream.finish();
    output.string_end();
    output.finish();
    CHECK(ends == 1);
    const auto peak = peak_rss_kib();
    CHECK(warm_rss && peak <= warm_rss + 8 * 1024);
    std::printf("Model-free 1 -> 32 MiB argument/parser/writer RSS: %zu -> %zu "
                "KiB (growth bound 8192 KiB)\n",
                warm_rss, peak);
  }
  {
    tool_json_format format;
    format.name_is_key = true;
    format.id.clear();
    std::string name, raw;
    tool_json_stream stream(format, {"weather"}, 1, 4096,
                            [&](const tool_json_event &event) {
                              if (event.kind == tool_json_kind::name)
                                name.append(event.data, event.size);
                              if (event.kind == tool_json_kind::arguments)
                                raw.append(event.data, event.size);
                            });
    const std::string input = "{\"weather\":" + arguments + "}";
    CHECK(stream.push(input.data(), input.size()) == input.size());
    stream.finish();
    CHECK(name == "weather" && raw == arguments);
  }
  for (const std::string input :
       {"{}", "[]", "{\"name\":\"weather\",\"arguments\":[]}",
        "{\"name\":\"unknown\",\"arguments\":{}}",
        "{\"name\":\"weather\",\"arguments\":{},\"name\":\"weather\"}",
        "{\"name\":\"weather\",\"arguments\":{},\"extra\":true}",
        "{\"name\":\"weather\",\"type\":\"custom\",\"arguments\":{}}",
        "{\"name\":\"weather\",\"arguments\":{\"a\":}",
        "{\"name\":\"weather\",\"arguments\":{\"a\":\"unfinished"}) {
    bool ended = false;
    try {
      tool_json_stream stream({}, {"weather"}, 1, 4096,
                              [&](const tool_json_event &event) {
                                ended = event.kind == tool_json_kind::end;
                              });
      (void)stream.push(input.data(), input.size());
      stream.finish();
      CHECK(false);
    } catch (const failure &e) {
      CHECK(e.status == AILO_MODEL_FAILED && !ended);
    }
  }
  for (bool budget : {false, true}) {
    try {
      tool_json_stream stream(
          {}, {"weather"}, 1, budget ? 16 : 4096,
          [&](const tool_json_event &event) {
            if (!budget && event.kind == tool_json_kind::arguments)
              throw failure(AILO_CANCELLED, "cancel arguments");
          });
      const std::string input = "{\"name\":\"weather\",\"arguments\":{}}";
      (void)stream.push(input.data(), input.size());
      CHECK(false);
    } catch (const failure &e) {
      CHECK(e.status == (budget ? AILO_MODEL_FAILED : AILO_CANCELLED));
    }
  }
  {
    // Object member order is not significant; only a complete valid call ends.
    size_t ends = 0;
    std::string raw;
    tool_json_stream stream({}, {"weather"}, 1, 4096,
                            [&](const tool_json_event &event) {
                              if (event.kind == tool_json_kind::arguments)
                                raw.append(event.data, event.size);
                              if (event.kind == tool_json_kind::end)
                                ++ends;
                            });
    const std::string input = "{\"arguments\":{},\"name\":\"weather\"}";
    CHECK(stream.push(input.data(), input.size()) == input.size());
    stream.finish();
    CHECK(raw == "{}" && ends == 1);
  }
  {
    tool_json_format format;
    format.array = true;
    size_t ends = 0;
    try {
      tool_json_stream stream(format, {"weather"}, 1, 4096,
                              [&](const tool_json_event &event) {
                                if (event.kind == tool_json_kind::end)
                                  ++ends;
                              });
      const std::string input = "[{\"name\":\"weather\",\"arguments\":{}},"
                                "{\"name\":\"weather\",\"arguments\":{}}]";
      (void)stream.push(input.data(), input.size());
      CHECK(false);
    } catch (const failure &e) {
      CHECK(e.status == AILO_MODEL_FAILED && ends == 1);
    }
  }
  {
    tool_json_format format;
    format.arguments = format.name;
    try {
      tool_json_stream stream(format, {"weather"}, 1, 4096,
                              [](const tool_json_event &) { CHECK(false); });
      CHECK(false);
    } catch (const failure &e) {
      CHECK(e.status == AILO_INVALID_ARGUMENT);
    }
  }
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
    tools();
    std::puts("Push-fed public JSON visitor: bounded incremental delivery and "
              "failures passed");
  } catch (const std::exception &e) {
    std::fprintf(stderr, "json stream: %s\n", e.what());
    return 1;
  }
}
