#include "ailo_client_internal.hpp"
#include "ailo_text_stream_internal.hpp"
#include <cstdio>
#include <stdexcept>

#define CHECK(value)                                                           \
  do {                                                                         \
    if (!(value))                                                              \
      throw std::runtime_error(#value);                                        \
  } while (0)

int main() {
  try {
    using ailo::client::text_stream;
    std::string output;
    auto sink = [&](const char *data, size_t size) {
      output.append(data, size);
    };
    text_stream utf8({}, sink);
    CHECK(!utf8.push("a\xe2"));
    CHECK(output == "a");
    CHECK(!utf8.push("\x82"));
    CHECK(output == "a");
    CHECK(!utf8.push("\xac"));
    CHECK(output == "a\xe2\x82\xac");
    CHECK(!utf8.push({}, true));
    output.clear();
    text_stream stop({"END", "ENough"}, sink);
    CHECK(!stop.push("helloE"));
    CHECK(output == "hello");
    CHECK(!stop.push("N"));
    CHECK(output == "hello");
    CHECK(stop.push("Ddiscard"));
    CHECK(output == "hello");
    CHECK(stop.push("discard", true));
    CHECK(output == "hello");
    output.clear();
    text_stream unmatched({"END"}, sink);
    CHECK(!unmatched.push("aEN"));
    CHECK(output == "a");
    CHECK(!unmatched.push("X"));
    CHECK(output == "aENX");
    CHECK(!unmatched.push("E"));
    CHECK(output == "aENX");
    CHECK(!unmatched.push({}, true));
    CHECK(output == "aENXE");
    output.clear();
    text_stream unicode_stop({"\xe2\x82\xac"}, sink);
    CHECK(!unicode_stop.push("a\xe2"));
    CHECK(output == "a");
    CHECK(!unicode_stop.push("\x82"));
    CHECK(output == "a");
    CHECK(unicode_stop.push("\xac"));
    CHECK(output == "a");
    size_t bytes = 0;
    text_stream long_output({},
                            [&](const char *, size_t size) { bytes += size; });
    for (size_t i = 0; i < 100000; ++i) {
      CHECK(!long_output.push("x"));
      CHECK(bytes == i + 1);
    }
    bool failed = false;
    try {
      text_stream invalid({}, sink);
      invalid.push("\xff");
    } catch (const ailo::client::failure &e) {
      failed = e.status == AILO_MODEL_FAILED;
    }
    CHECK(failed);
    failed = false;
    try {
      text_stream incomplete({}, sink);
      incomplete.push("\xe2", true);
    } catch (const ailo::client::failure &e) {
      failed = e.status == AILO_MODEL_FAILED;
    }
    CHECK(failed);
    std::puts("Incremental UTF-8, overlapping stop prefixes and immediate "
              "long-output delivery: passed");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
