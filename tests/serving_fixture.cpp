// Test-only loopback transport. Production adapters serialize every event.
// The generate definition below replaces only the static archive's inference
// translation unit in THIS executable; it introduces no product test hook.
#include "ailo/handler.h"
#include "ailo_client_internal.hpp"
#include <arpa/inet.h>
#include <charconv>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <netinet/tcp.h>
#include <poll.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace {
volatile std::sig_atomic_t stopping = 0;
unsigned generations = 0, tool_calls = 0, tool_results = 0, answers = 0;
std::string directory;
void stop(int) { stopping = 1; }
void require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
struct socket_owner {
  int fd;
  ~socket_owner() {
    if (fd >= 0)
      close(fd);
  }
};
int send_all(int fd, const void *bytes, size_t size) {
  const char *p = static_cast<const char *>(bytes);
  while (size) {
    const auto n = send(fd, p, size, MSG_NOSIGNAL);
    if (n < 0 && errno == EINTR)
      continue;
    if (n <= 0)
      return 0;
    p += n;
    size -= static_cast<size_t>(n);
  }
  return 1;
}
int begin(void *u, const ailo_http_response *headers) {
  const int fd = *static_cast<int *>(u);
  std::ostringstream out;
  out << "HTTP/1.1 " << headers->status
      << " Result\r\nContent-Type: " << headers->content_type
      << "\r\nCache-Control: " << headers->cache_control
      << "\r\nX-Request-ID: " << headers->request_id
      << "\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n";
  if (headers->allow)
    out << "Allow: " << headers->allow << "\r\n";
  if (headers->retry_after)
    out << "Retry-After: " << headers->retry_after << "\r\n";
  out << "\r\n";
  const auto bytes = out.str();
  return send_all(fd, bytes.data(), bytes.size());
}
int write(void *u, const void *bytes, size_t size) {
  const int fd = *static_cast<int *>(u);
  char length[32];
  const auto n = std::snprintf(length, sizeof(length), "%zx\r\n", size);
  return send_all(fd, length, static_cast<size_t>(n)) &&
         send_all(fd, bytes, size) && send_all(fd, "\r\n", 2);
}
int finish(void *u) { return send_all(*static_cast<int *>(u), "0\r\n\r\n", 5); }
void abort_response(void *u) { shutdown(*static_cast<int *>(u), SHUT_RDWR); }
std::string lower(std::string value) {
  for (auto &c : value)
    if (c >= 'A' && c <= 'Z')
      c += 'a' - 'A';
  return value;
}
std::string receive(int fd, size_t maximum) {
  char bytes[4096];
  const auto n = recv(fd, bytes, sizeof(bytes), 0);
  require(n > 0 && static_cast<size_t>(n) <= maximum,
          "Invalid fixture request size/read");
  return {bytes, static_cast<size_t>(n)};
}
void request(int fd, ailo_handler *handler, unsigned number) {
  constexpr size_t limit = 8 * 1024 * 1024;
  std::string input;
  while (input.find("\r\n\r\n") == std::string::npos) {
    input += receive(fd, 32768 - input.size());
    require(input.size() < 32768, "Fixture request headers exceed 32 KiB");
  }
  const auto boundary = input.find("\r\n\r\n") + 4;
  std::istringstream lines(input.substr(0, boundary));
  std::string line, method, target, version;
  std::getline(lines, line);
  std::istringstream first(line);
  first >> method >> target >> version;
  require(version == "HTTP/1.1", "Fixture requires HTTP/1.1");
  std::map<std::string, std::string> headers;
  while (std::getline(lines, line) && line != "\r") {
    const auto colon = line.find(':');
    require(colon != std::string::npos, "Invalid fixture header");
    auto value = line.substr(colon + 1);
    if (!value.empty() && value.back() == '\r')
      value.pop_back();
    const auto start = value.find_first_not_of(" \t");
    value = start == std::string::npos ? "" : value.substr(start);
    require(headers.emplace(lower(line.substr(0, colon)), value).second,
            "Duplicate fixture header");
  }
  require(!headers.count("transfer-encoding"),
          "Fixture expects a fixed-length request body");
  size_t length = 0;
  if (headers.count("content-length")) {
    const auto &text = headers.at("content-length");
    const auto parsed =
        std::from_chars(text.data(), text.data() + text.size(), length);
    require(parsed.ec == std::errc() &&
                parsed.ptr == text.data() + text.size() && length <= limit,
            "Invalid fixture Content-Length");
  }
  while (input.size() - boundary < length)
    input += receive(fd, limit + 32768 - input.size());
  require(input.size() - boundary == length,
          "Fixture rejects pipelined/trailing request bytes");
  // Request inputs are explicitly materialized. Generated output is never
  // captured or spooled here; the transport forwards each borrowed byte range.
  std::ofstream saved(directory + "/request-" + std::to_string(number) +
                      ".json");
  saved.write(input.data() + boundary, static_cast<std::streamsize>(length));
  ailo_http_request incoming{};
  incoming.method = method.c_str();
  incoming.target = target.c_str();
  incoming.body = input.data() + boundary;
  incoming.body_size = length;
  incoming.content_type = headers.count("content-type")
                              ? headers.at("content-type").c_str()
                              : nullptr;
  incoming.content_encoding = headers.count("content-encoding")
                                  ? headers.at("content-encoding").c_str()
                                  : nullptr;
  ailo_http_transport outgoing{&fd, begin, write, finish, abort_response};
  ailo_handler_info info{};
  ailo_error error{};
  const auto status =
      handler->handle(handler, &incoming, &outgoing, &info, &error);
  std::fprintf(stderr,
               "request=%u method=%s target=%s status=%d result=%s "
               "completed=%d bytes=%u diagnostic=%s\n",
               number, method.c_str(), target.c_str(), info.http_status,
               ailo_status_name(status), info.completed, info.bytes_written.lo,
               error.message);
}
} // namespace

namespace ailo {
namespace client {
void generate(state &self, const ailo_request &request, ailo_event_sink sink,
              void *user, ailo_run_info &info, continuation_transaction *,
              const compaction_task *) {
  require(++generations <= 8, "Agent fixture exceeded its generation bound");
  bool received = false;
  for (size_t n = 0; n < request.item_count; ++n) {
    const auto &item = request.items[n];
    if (item.kind != AILO_ITEM_FUNCTION_RESULT || !item.call_id ||
        std::strcmp(item.call_id, "call_ailo_agent"))
      continue;
    for (size_t c = 0; c < item.content_count; ++c) {
      const auto &part = item.content[c];
      received |= std::string(static_cast<const char *>(part.data), part.size)
                      .find("AILO_TOOL_OK") != std::string::npos;
    }
  }
  std::string name, text;
  if (received) {
    require(tool_calls == 1 && tool_results == 0,
            "Unexpected duplicate/missing tool call");
    ++tool_results;
    ++answers;
    text = "AILO_AGENT_DONE";
  } else {
    require(tool_calls == 0, "Agent did not return the expected tool result");
    for (size_t n = 0; n < request.tool_count; ++n) {
      const auto &tool = request.tools[n];
      if (!std::strcmp(tool.name, "exec_command")) {
        name = tool.name;
        text =
            R"({"cmd":"printf 'AILO_TOOL_OK\\n'","max_output_tokens":128,"yield_time_ms":1000})";
        break;
      }
      if (!std::strcmp(tool.name, "shell_command")) {
        name = tool.name;
        text = R"({"command":"printf 'AILO_TOOL_OK\\n'","timeout_ms":1000})";
        break;
      }
      if (!std::strcmp(tool.name, "shell")) {
        name = tool.name;
        text =
            R"({"command":["bash","-c","printf 'AILO_TOOL_OK\\n'"],"timeout_ms":1000})";
        break;
      }
    }
    require(!name.empty(),
            "Agent must advertise a supported function shell tool");
    ++tool_calls;
  }
  info = {};
  std::snprintf(info.id, sizeof(info.id), "00000000-0000-4000-8000-%012u",
                generations);
  info.actual_backend = AILO_BACKEND_CPU;
  ailo_event event{};
  event.model_id = self.id.c_str();
  event.run_id = info.id;
  event.item_kind = received ? AILO_ITEM_MESSAGE : AILO_ITEM_FUNCTION_CALL;
  event.item_id = received ? "msg_ailo_agent" : "fc_ailo_agent";
  event.call_id = received ? nullptr : "call_ailo_agent";
  unsigned sequence = 0, tokens = 0;
  const auto emit = [&](ailo_event_kind kind, const void *data = nullptr,
                        size_t size = 0) {
    if (self.is_cancelled())
      throw failure(AILO_CANCELLED, "Fixture generation cancelled");
    event.kind = kind;
    event.data = data;
    event.size = size;
    event.usage = info.usage;
    event.sequence = detail::from_unsigned(sequence++);
    const int accepted = sink(user, &event);
    if (accepted <= 0)
      throw failure(accepted ? AILO_SINK_FAILED : AILO_CANCELLED,
                    "Fixture event consumer stopped");
  };
  emit(AILO_EVENT_RUN_BEGIN);
  emit(AILO_EVENT_CHOICE_BEGIN);
  emit(AILO_EVENT_ITEM_BEGIN);
  if (received)
    emit(AILO_EVENT_CONTENT_BEGIN);
  const auto stream = [&](const std::string &bytes, ailo_event_kind kind) {
    for (char c : bytes) {
      ailo_token_info token{};
      token.chosen.data = &c;
      token.chosen.size = 1;
      token.chosen.token_id = static_cast<unsigned char>(c);
      token.index = detail::from_unsigned(tokens);
      token.byte_offset = detail::from_unsigned(tokens);
      event.raw_offset = detail::from_unsigned(tokens);
      event.raw_offset_valid = 1;
      info.usage.completion_tokens = detail::from_unsigned(++tokens);
      event.token = &token;
      if (request.logprobs)
        emit(AILO_EVENT_TOKEN);
      event.token = nullptr;
      emit(kind, &c, 1);
      if (request.logprobs) {
        event.raw_offset = detail::from_unsigned(tokens);
        emit(AILO_EVENT_OUTPUT_PROGRESS);
      }
    }
  };
  if (!received)
    stream(name, AILO_EVENT_FUNCTION_NAME_DELTA);
  stream(text, received ? AILO_EVENT_TEXT_DELTA
                        : AILO_EVENT_FUNCTION_ARGUMENTS_DELTA);
  event.finish_reason = received ? AILO_FINISH_STOP : AILO_FINISH_TOOL_CALLS;
  if (received)
    emit(AILO_EVENT_CONTENT_END);
  emit(AILO_EVENT_ITEM_END);
  emit(AILO_EVENT_CHOICE_END);
  info.finish_reason = event.finish_reason;
  info.completed_candidates = 1;
  emit(AILO_EVENT_USAGE);
  emit(AILO_EVENT_RUN_END);
  std::fprintf(stderr, "fixture calls=%u results=%u answers=%u tokens=%u\n",
               tool_calls, tool_results, answers, tokens);
}
} // namespace client
} // namespace ailo

int main(int argc, char **argv) {
  try {
    require(argc == 3, "Usage: ailo_serving_fixture MODEL WORK_DIRECTORY");
    directory = argv[2];
    auto config = ailo_client_default_config();
    config.source.local_path = argv[1];
    config.model_id = "ailo-fixture";
    config.lazy = 1;
    config.backend = AILO_BACKEND_CPU;
    config.context_tokens = 1024;
    config.threads = 1;
    auto api = ailo_openai_default_config();
    api.zero_data_retention = 1;
    config.openai = &api;
    config.chat_template =
        "{% if tools %}{{tools | tojson}}{% endif %}{% for m in messages "
        "%}{{m.role}}:{{m.content}}{% if m.tool_calls is defined %}{% for c in "
        "m.tool_calls %}<call>{\"name\":{{c.function.name | "
        "tojson}},\"arguments\":{% if c.function.arguments is string "
        "%}{{c.function.arguments}}{% else %}{{c.function.arguments | "
        "tojson}}{% endif %}}</call>{% endfor %}{% endif %}|{% endfor %}{% if "
        "add_generation_prompt %}assistant:{% endif %}";
    ailo_error error{};
    ailo_client *raw = nullptr;
    require(ailo_client_new(&config, &raw, &error) == AILO_OK, error.message);
    std::unique_ptr<ailo_client, decltype(&ailo_client_destroy)> client(
        raw, ailo_client_destroy);
    auto handler_config = ailo_handler_default_config();
    handler_config.client = raw;
    ailo_handler *serving = nullptr;
    require(ailo_handler_new(&handler_config, &serving, &error) == AILO_OK,
            error.message);
    std::unique_ptr<ailo_handler, decltype(&ailo_handler_destroy)> handler(
        serving, ailo_handler_destroy);
    socket_owner listener{socket(AF_INET, SOCK_STREAM, 0)};
    require(listener.fd >= 0, "Fixture socket failed");
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    require(bind(listener.fd, reinterpret_cast<sockaddr *>(&address),
                 sizeof(address)) == 0,
            "Fixture bind failed");
    socklen_t length = sizeof(address);
    require(getsockname(listener.fd, reinterpret_cast<sockaddr *>(&address),
                        &length) == 0 &&
                listen(listener.fd, 4) == 0,
            "Fixture listen failed");
    std::signal(SIGTERM, stop);
    std::signal(SIGINT, stop);
    {
      std::ofstream ready(directory + "/port");
      ready << ntohs(address.sin_port) << '\n';
    }
    unsigned count = 0;
    while (!stopping && count < 12) {
      pollfd pending{listener.fd, POLLIN, 0};
      if (poll(&pending, 1, 250) <= 0)
        continue;
      socket_owner peer{accept(listener.fd, nullptr, nullptr)};
      require(peer.fd >= 0, "Fixture accept failed");
      const int enabled = 1;
      const timeval timeout{10, 0};
      setsockopt(peer.fd, IPPROTO_TCP, TCP_NODELAY, &enabled, sizeof(enabled));
      setsockopt(peer.fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
      setsockopt(peer.fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
      request(peer.fd, serving, ++count);
    }
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "serving fixture: %s\n", e.what());
    return 1;
  }
}
