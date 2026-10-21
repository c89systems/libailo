// An actual consuming socket. A FIFO releases synchronous transport
// backpressure after the first text delta, or an RST tests cancellation at that
// exact point.
#include "vendor/nlohmann/json.hpp"
#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
namespace {
void require(bool ok, const char *message) {
  if (!ok)
    throw std::runtime_error(message);
}
struct socket_owner {
  int fd = -1;
  ~socket_owner() {
    if (fd >= 0)
      close(fd);
  }
};
std::string bytes(int fd, size_t count) {
  require(count <= 65536, "HTTP chunk exceeds observation bound");
  std::string out(count, '\0');
  size_t at = 0;
  while (at < count) {
    const auto n = recv(fd, out.data() + at, count - at, 0);
    if (n < 0 && errno == EINTR)
      continue;
    require(n > 0, "Unexpected HTTP EOF/read failure");
    at += static_cast<size_t>(n);
  }
  return out;
}
std::string line(int fd) {
  std::string out;
  while (out.size() < 8192) {
    out += bytes(fd, 1);
    if (out.size() >= 2 && out.compare(out.size() - 2, 2, "\r\n") == 0)
      return out.substr(0, out.size() - 2);
  }
  throw std::runtime_error("HTTP line exceeds bound");
}
void send_all(int fd, const std::string &text) {
  size_t at = 0;
  while (at < text.size()) {
    const auto n = send(fd, text.data() + at, text.size() - at, MSG_NOSIGNAL);
    if (n < 0 && errno == EINTR)
      continue;
    require(n > 0, "HTTP send failed");
    at += static_cast<size_t>(n);
  }
}
} // namespace
int main(int argc, char **argv) {
  try {
    require(argc == 5,
            "Usage: stream-client PORT WORK REQUEST continue|disconnect");
    const std::string mode = argv[4], work = argv[2];
    require(mode == "continue" || mode == "disconnect", "Invalid test mode");
    socket_owner connection{socket(AF_INET, SOCK_STREAM, 0)};
    require(connection.fd >= 0, "Socket failed");
    const timeval deadline{90, 0};
    setsockopt(connection.fd, SOL_SOCKET, SO_RCVTIMEO, &deadline,
               sizeof(deadline));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(static_cast<unsigned short>(std::stoul(argv[1])));
    require(connect(connection.fd, reinterpret_cast<sockaddr *>(&address),
                    sizeof(address)) == 0,
            "Connect failed");
    const std::string body =
        R"({"input":"Count from one to twenty, writing every number.","stream":true,"temperature":0,"max_output_tokens":80})";
    send_all(connection.fd,
             "POST /v1/responses HTTP/1.1\r\nHost: localhost\r\nContent-Type: "
             "application/json\r\nContent-Length: " +
                 std::to_string(body.size()) + "\r\n\r\n" + body);
    require(line(connection.fd).find("HTTP/1.1 200 ") == 0,
            "Expected HTTP 200");
    while (!line(connection.fd).empty()) {
    }
    unsigned deltas = 0, completed = 0;
    std::string pending, id;
    std::ofstream evidence(work + "/client-" + argv[3] + ".sse");
    for (;;) {
      const auto length = std::stoul(line(connection.fd), nullptr, 16);
      if (!length) {
        require(line(connection.fd).empty(), "Invalid HTTP trailer");
        break;
      }
      const auto chunk = bytes(connection.fd, length);
      require(bytes(connection.fd, 2) == "\r\n", "Invalid chunk delimiter");
      evidence << chunk;
      pending += chunk;
      require(pending.size() <= 65536, "SSE frame exceeds bound");
      size_t end;
      while ((end = pending.find("\n\n")) != std::string::npos) {
        const auto frame = pending.substr(0, end);
        pending.erase(0, end + 2);
        const auto data = frame.find("data: ");
        require(data != std::string::npos, "Missing SSE data");
        const auto event = nlohmann::json::parse(frame.substr(data + 6));
        const auto type = event.at("type").get<std::string>();
        if (type == "response.created")
          id = event.at("response").at("id");
        if (type == "response.completed")
          ++completed;
        require(type != "response.failed" && type != "error", "Stream failed");
        if (type != "response.output_text.delta")
          continue;
        if (++deltas != 1)
          continue;
        require(!id.empty() && !completed, "Text arrived after completion");
        require(
            access((work + "/receipt-" + argv[3] + ".json").c_str(), F_OK) != 0,
            "Producer completed while its first delta was held");
        if (mode == "disconnect") {
          const linger reset{1, 0};
          require(setsockopt(connection.fd, SOL_SOCKET, SO_LINGER, &reset,
                             sizeof(reset)) == 0,
                  "Cannot request reset");
          close(connection.fd);
          connection.fd = -1;
          std::ofstream(work + "/cancelled-id") << id << '\n';
          std::puts("First native text delta consumed before completion; peer "
                    "reset sent");
          return 0;
        }
        socket_owner release{
            open((work + "/release").c_str(), O_WRONLY | O_CLOEXEC)};
        require(release.fd >= 0 && write(release.fd, "c", 1) == 1,
                "Cannot release backpressure");
      }
    }
    require(deltas > 1 && completed == 1 && pending.empty(),
            "Incomplete resumed stream");
    std::puts("First native text delta consumed before completion; "
              "backpressure released; stream completed");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "stream-client: %s\n", e.what());
    return 1;
  }
}
