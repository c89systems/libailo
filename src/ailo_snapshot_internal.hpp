#ifndef AILO_SNAPSHOT_INTERNAL_HPP
#define AILO_SNAPSHOT_INTERNAL_HPP
#include "ailo_cache_internal.hpp"
#include "ailo_model_internal.hpp"
#include <array>

namespace ailo {
namespace client {
std::string checkpoint_key(const cache_entry &entry);
struct snapshot_record {
  std::string name;
  std::array<unsigned char, 256> header{};
  uint64_t created = 0;
  size_t count = 0, native_size = 0, size = 0;
};
struct secret_key {
  std::array<unsigned char, 32> bytes{};
  ~secret_key();
};
class snapshot_store {
  cache_state &owner;
  model::fd directory;
  secret_key metadata_key, state_key;
  std::array<unsigned char, 32> key_hash;
  std::vector<snapshot_record> catalog(bool prune);
  std::unique_ptr<cache_entry> read(const snapshot_record &, bool native,
                                    state *client = nullptr);
  snapshot_record inspect(const std::string &name);
  void remove(const std::string &name);

public:
  std::string root, key_id;
  snapshot_store(cache_state &owner, const ailo_cache_config &config);
  model::fd lock();
  std::pair<size_t, size_t> prune(bool clear = false);
  bool write(const cache_entry &, const std::string &name,
             state *client = nullptr, bool replace = false);
  std::unique_ptr<cache_entry> load(const std::string &name,
                                    const std::string &identity,
                                    const std::string &partition,
                                    state *client = nullptr);
  size_t restore(state &client, const std::vector<llama_token> &tokens,
                 const std::string &partition, size_t minimum, bool trim,
                 std::unique_ptr<cache_entry> &loaded);
};
} // namespace client
} // namespace ailo
#endif
