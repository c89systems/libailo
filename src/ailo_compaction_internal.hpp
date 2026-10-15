#ifndef AILO_COMPACTION_INTERNAL_HPP
#define AILO_COMPACTION_INTERNAL_HPP
#include "ailo/compaction.h"
#include "ailo_client_internal.hpp"
#include <array>

namespace ailo {
namespace client {
using compaction_digest = std::array<unsigned char, 32>;
struct compaction_secret {
  std::string id;
  compaction_digest id_hash{}, bytes{};
  ~compaction_secret();
};
struct sealed_summary {
  std::string id, encrypted_content;
};
struct compaction_payload {
  std::string summary;
  /** Fingerprints of every retained item, in window order. The sealed item
   * sits at cut; subsequent requests may append only after this bound prefix.
   */
  std::vector<compaction_digest> retained;
  size_t cut = 0, source_items = 0, removed_items = 0;
  ~compaction_payload();
};
struct compaction_state {
  std::string namespace_id;
  compaction_digest namespace_hash{};
  std::vector<std::unique_ptr<compaction_secret>> keys;
  bool ephemeral;
  size_t max_item_bytes;
  ailo_u32 ttl_seconds;
  explicit compaction_state(const ailo_compactor_config &);
  sealed_summary seal(const compaction_payload &, const std::string &identity,
                      state *client = nullptr) const;
  std::unique_ptr<compaction_payload> open(const char *id,
                                           const char *encrypted,
                                           const std::string &identity,
                                           state *client = nullptr) const;
};
std::shared_ptr<compaction_state> retain_compactor(ailo_compactor *);
compaction_digest fingerprint(const ailo_item &);
void verify_compaction_binding(const compaction_payload &, const ailo_item *,
                               size_t count, size_t position);
} // namespace client
} // namespace ailo
#endif
