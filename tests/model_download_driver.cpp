#include "ailo_model_internal.hpp"
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include <unistd.h>

struct progress_state {
  std::string mode;
  std::string destination;
  bool created = false;
  unsigned calls = 0;
  ailo_u64 last_total = {};
};
static int progress(void *opaque, unsigned, unsigned, ailo_u64 received,
                    ailo_u64 total) {
  auto &s = *static_cast<progress_state *>(opaque);
  ++s.calls;
  s.last_total = total;
  if (s.mode == "cancel-before" ||
      (s.mode == "cancel-transfer" && (received.hi || received.lo)) ||
      (s.mode == "cancel-lock" && s.calls > 1))
    return 0;
  if (s.mode == "throw-before" ||
      (s.mode == "throw-transfer" && (received.hi || received.lo)))
    throw std::runtime_error("SYNTHETIC_SECRET");
  if (s.mode == "clobber" && !s.created && (received.hi || received.lo)) {
    std::ofstream out(s.destination, std::ios::binary);
    out << "caller-owned sentinel";
    s.created = true;
  }
  return 1;
}

int main(int argc, char **argv) {
  if (argc != 13) {
    std::cerr << "Usage: driver origin ca root repo revision filename hashes "
                 "allow mode max-bytes attempts timeout-ms\n";
    return 2;
  }
  try {
    ailo_model_source source = {};
    source.repo = argv[4];
    source.revision = argv[5];
    source.filename = argv[6];
    source.cache_root = std::string(argv[3]) == "-" ? nullptr : argv[3];
    source.allow_download = std::stoi(argv[8]);
    source.max_bytes = ailo::detail::from_unsigned(std::stoull(argv[10]));
    source.max_attempts = static_cast<unsigned>(std::stoul(argv[11]));
    source.connect_timeout_ms = 2000;
    source.transfer_timeout_ms = std::stol(argv[12]);
    source.bearer_token = "SYNTHETIC_SECRET";
    progress_state state = {};
    state.mode = argv[9];
    source.progress = progress;
    source.user_data = &state;
    std::vector<std::string> strings;
    std::vector<const char *> hashes;
    if (std::string(argv[7]) != "-") {
      std::istringstream input(argv[7]);
      std::string part;
      while (std::getline(input, part, ','))
        strings.push_back(part);
      for (const auto &s : strings)
        hashes.push_back(s == "-" ? nullptr : s.c_str());
      source.sha256 = hashes.data();
      source.sha256_count = hashes.size();
    }
    source.split_shards = state.mode == "split";
    if (state.mode == "busy-lock")
      source.connect_timeout_ms = 25;
    std::string local;
    if (state.mode == "sparse") {
      local = std::string(argv[3]) + "/sparse.gguf";
      std::filesystem::create_directories(argv[3]);
      ailo::model::fd file(
          open(local.c_str(), O_CREAT | O_TRUNC | O_WRONLY | O_CLOEXEC, 0600));
      unsigned char header[24] = {'G', 'G', 'U', 'F', 3};
      if (!file ||
          write(file.get(), header, sizeof(header)) != sizeof(header) ||
          ftruncate(file.get(), off_t(1) << 33))
        return 2;
      source.local_path = local.c_str();
      source.repo = source.revision = source.filename = nullptr;
      source.sha256 = nullptr;
      source.sha256_count = 0;
    }
    if (state.mode == "bad-token")
      source.bearer_token = "SYNTHETIC_SECRET\r\nInjected: forbidden";
    ailo_error error = {};
    char *output = nullptr;
    ailo_status status = ailo_model_cache_path(&source, &output, &error);
    if (output) {
      state.destination = output;
      ailo_string_free(output);
      output = nullptr;
    }
    if (status == AILO_OK && state.mode == "threaded") {
      ailo_model_source inputs[2] = {source, source};
      progress_state progress_states[2] = {};
      progress_states[0].mode = progress_states[1].mode = "normal";
      inputs[0].user_data = &progress_states[0];
      inputs[1].user_data = &progress_states[1];
      inputs[1].filename = "thread-other.gguf";
      char *paths[2] = {};
      ailo_error errors[2] = {};
      ailo_status statuses[2] = {};
      auto work = [&](unsigned index) {
        statuses[index] = ailo::model::acquire(
            &inputs[index], &paths[index], &errors[index],
            {argv[1], std::string(argv[2]) == "-" ? nullptr : argv[2]});
      };
      std::thread first(work, 0);
      try {
        std::thread second(work, 1);
        second.join();
      } catch (...) {
        first.join();
        ailo_string_free(paths[0]);
        throw;
      }
      first.join();
      if (statuses[0] == AILO_OK && statuses[1] == AILO_OK) {
        output = paths[0];
      } else {
        unsigned index = statuses[0] != AILO_OK ? 0 : 1;
        status = statuses[index];
        error = errors[index];
        ailo_string_free(paths[0]);
      }
      ailo_string_free(paths[1]);
    } else if (status == AILO_OK && state.mode != "path")
      status = ailo::model::acquire(
          &source, &output, &error,
          {argv[1], std::string(argv[2]) == "-" ? nullptr : argv[2]});
    std::cout << "status=" << ailo_status_name(status) << "\n"
              << "path=" << (output ? output : state.destination) << "\n"
              << "output_null=" << (output == nullptr) << "\n"
              << "message=" << error.message << "\n"
              << "calls=" << state.calls << "\n"
              << "total_hi=" << state.last_total.hi << "\n";
    ailo_string_free(output);
    return 0;
  } catch (...) {
    std::cerr << "Model test driver setup failed\n";
    return 2;
  }
}
