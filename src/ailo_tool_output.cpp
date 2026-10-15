#include "ailo_tools_internal.hpp"
#include <algorithm>

namespace ailo {
namespace client {
tool_output_stream::tool_output_stream(
    const tool_plan &value, size_t byte_quota,
    std::function<void(const tool_output_event &)> callback)
    : plan(value), quota(byte_quota), sink(std::move(callback)),
      text({}, [&](const char *data, size_t size) {
        sink({tool_output_kind::text, calls, data, size});
      }) {}

void tool_output_stream::push(const char *data, size_t size) {
  for (size_t offset = 0; offset < size;) {
    if (current == stage::json) {
      offset += json->push(data + offset, size - offset);
      if (!json->complete())
        return;
      json->finish();
      json.reset();
      current = plan.close.empty() ? stage::text : stage::close;
      closing = 0;
    } else if (current == stage::close) {
      const char ch = data[offset++];
      if (!closing && (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n'))
        continue;
      if (ch != plan.close[closing++])
        throw failure(AILO_MODEL_FAILED,
                      "Generated function-call delimiter is invalid");
      if (closing == plan.close.size())
        current = stage::text;
    } else {
      pending += data[offset++];
      while (plan.open.compare(0, pending.size(), pending) != 0) {
        (void)text.push(pending.substr(0, 1));
        pending.erase(0, 1);
      }
      if (pending == plan.open) {
        (void)text.push({}, true);
        pending.clear();
        if (calls >= plan.limit || plan.names.empty() ||
            (plan.format.array && calls))
          throw failure(AILO_MODEL_FAILED,
                        "Generated function-call count or choice is invalid");
        base = calls;
        json = std::make_unique<tool_json_stream>(
            plan.format, plan.names, plan.limit - calls, quota,
            [&](const tool_json_event &event) {
              const tool_output_kind kinds[] = {
                  tool_output_kind::begin, tool_output_kind::name,
                  tool_output_kind::arguments, tool_output_kind::end};
              sink({kinds[static_cast<unsigned>(event.kind)],
                    base + event.index, event.data, event.size});
              if (event.kind == tool_json_kind::end)
                ++calls;
            });
        current = stage::json;
      }
    }
  }
}
void tool_output_stream::finish(bool truncated) {
  if (current != stage::text) {
    if (!truncated)
      throw failure(AILO_MODEL_FAILED, "Function-call output is incomplete");
    return;
  }
  // A forced call's unfinished opening marker is protocol syntax, not text.
  if (truncated && !calls &&
      (plan.choice == AILO_TOOLS_REQUIRED || plan.choice == AILO_TOOLS_NAMED))
    pending.clear();
  (void)text.push(pending, true);
  pending.clear();
  if (!truncated && !calls &&
      (plan.choice == AILO_TOOLS_REQUIRED || plan.choice == AILO_TOOLS_NAMED))
    throw failure(AILO_MODEL_FAILED, "Required function call is missing");
}
} // namespace client
} // namespace ailo
