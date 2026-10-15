#ifndef AILO_CONTINUATION_INTERNAL_HPP
#define AILO_CONTINUATION_INTERNAL_HPP
#include "ailo/continuation.h"
#include "ailo_client_internal.hpp"

namespace ailo {
namespace client {
struct owned_part {
  ailo_content_kind kind;
  std::string data;
};
struct owned_item {
  ailo_item_kind kind;
  ailo_role role;
  ailo_bool pinned;
  std::string id, call_id, name, arguments, encrypted_content;
  std::vector<owned_part> parts;
  std::vector<ailo_content> views;
  ailo_item view() const;
  owned_item(const ailo_item &, size_t &bytes, size_t limit, size_t &parts);
  ~owned_item();
  owned_item(const owned_item &) = delete;
  owned_item &operator=(const owned_item &) = delete;
};
struct transcript {
  std::vector<std::shared_ptr<const owned_item>> items;
  size_t bytes = 0, parts = 0, pending = 0;
  uint64_t revision = 0;
  std::vector<ailo_item> views() const;
};
struct continuation_state {
  std::atomic<bool> active{false};
  ailo_continuation_config config;
  std::string identity;
  std::shared_ptr<compaction_state> compactor;
  std::shared_ptr<const transcript> current;
};
struct continuation_guard {
  continuation_state &owner;
  explicit continuation_guard(continuation_state &);
  ~continuation_guard();
  continuation_guard(const continuation_guard &) = delete;
  continuation_guard &operator=(const continuation_guard &) = delete;
};
continuation_state &get_continuation(ailo_continuation *);
ailo_continuation *new_window(state &, const ailo_continuation_config &,
                              const ailo_item *, size_t);
std::shared_ptr<const transcript> stage_append(continuation_state &,
                                               const ailo_item *, size_t);
struct continuation_transaction {
  continuation_state &owner;
  continuation_guard guard;
  std::shared_ptr<const transcript> candidate;
  std::shared_ptr<compaction_state> compactor;
  std::vector<ailo_item> items;
  ailo_request input;
  continuation_transaction(state &, const ailo_request &);
  void commit() noexcept;
  void replace(const continuation_state &);
};
} // namespace client
} // namespace ailo
#endif
