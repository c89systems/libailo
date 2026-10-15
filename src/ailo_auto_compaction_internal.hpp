#ifndef AILO_AUTO_COMPACTION_INTERNAL_HPP
#define AILO_AUTO_COMPACTION_INTERNAL_HPP
#include "ailo_compaction_internal.hpp"
#include "ailo_continuation_internal.hpp"
namespace ailo {
namespace client {
struct compaction_change {
  std::unique_ptr<ailo_continuation, decltype(&ailo_continuation_destroy)>
      window{nullptr, ailo_continuation_destroy};
  std::vector<ailo_item> items;
  ailo_compaction_info info{};
};
struct automatic_compaction {
  state &self;
  continuation_transaction &history;
  ailo_auto_compaction_policy policy;
  unsigned passes = 0;
  automatic_compaction(state &, continuation_transaction &,
                       const ailo_auto_compaction_policy &);
  compaction_change propose(size_t unfinished);
  void adopt(const compaction_change &);
};
} // namespace client
} // namespace ailo
#endif
