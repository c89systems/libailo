// Small selected utility dependencies of pinned llama's schema helpers.
// Keep common.cpp (CLI, downloads, subprocesses, etc.) outside this library.
#include <string>
#include <vector>

std::string string_join(const std::vector<std::string> &values,
                        const std::string &separator) {
  std::string result;
  for (size_t i = 0; i < values.size(); ++i) {
    if (i)
      result += separator;
    result += values[i];
  }
  return result;
}

std::string string_repeat(const std::string &value, size_t count) {
  std::string result;
  for (size_t i = 0; i < count; ++i)
    result += value;
  return result;
}

std::vector<std::string> string_split(const std::string &value,
                                      const std::string &separator) {
  std::vector<std::string> result;
  size_t start = 0;
  for (;;) {
    const size_t end = value.find(separator, start);
    result.push_back(value.substr(start, end - start));
    if (end == std::string::npos)
      return result;
    start = end + separator.size();
  }
}
