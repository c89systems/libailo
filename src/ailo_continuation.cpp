#include "ailo_cache_internal.hpp"
#include "ailo_compaction_internal.hpp"
#include "ailo_continuation_internal.hpp"
#include "ailo_conversation_internal.hpp"
#include "ailo_logging_internal.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <openssl/crypto.h>

#define AILO_PUBLIC extern "C" __attribute__((visibility("default")))
namespace ailo {
namespace client {
namespace {
void charge(size_t &bytes, size_t size, size_t limit) {
  if (size > limit - bytes)
    throw failure(AILO_OUT_OF_RANGE, "Continuation storage budget exceeded");
  bytes += size;
}
std::string copy(const char *source, size_t maximum, size_t &bytes,
                 size_t limit) {
  if (!source)
    return {};
  const size_t size = strnlen(source, maximum + 1);
  if (size > maximum)
    throw failure(AILO_OUT_OF_RANGE, "Continuation string exceeds its limit");
  charge(bytes, size + 1, limit);
  return {source, size};
}
ailo_continuation *shell(std::unique_ptr<continuation_state> value) {
  auto result = std::make_unique<ailo_continuation>();
  *result = {value.get(),
             ailo_continuation_clone,
             ailo_continuation_append,
             ailo_continuation_get_info,
             ailo_continuation_visit,
             ailo_continuation_destroy};
  value.release();
  return result.release();
}
} // namespace
owned_item::owned_item(const ailo_item &input, size_t &bytes, size_t limit,
                       size_t &part_count)
    : kind(input.kind), role(input.role), pinned(input.pinned) {
  charge(bytes, sizeof(owned_item) + sizeof(std::shared_ptr<const owned_item>),
         limit);
  id = copy(input.id, 256, bytes, limit);
  call_id = copy(input.call_id, 256, bytes, limit);
  name = copy(input.name, 64, bytes, limit);
  arguments = copy(input.arguments, 4 * 1024 * 1024, bytes, limit);
  encrypted_content = copy(input.encrypted_content,
                           4 * ((4 * 1024 * 1024 + 2) / 3), bytes, limit);
  if (input.content_count > 4096 - part_count)
    throw failure(AILO_OUT_OF_RANGE, "Continuation exceeds 4096 content parts");
  part_count += input.content_count;
  charge(bytes,
         input.content_count * (sizeof(owned_part) + sizeof(ailo_content)),
         limit);
  parts.reserve(input.content_count);
  views.reserve(input.content_count);
  for (size_t i = 0; i < input.content_count; ++i) {
    const auto &part = input.content[i];
    charge(bytes, part.size, limit);
    charge(bytes, 1, limit);
    parts.push_back(
        {part.kind,
         part.size
             ? std::string(static_cast<const char *>(part.data), part.size)
             : std::string{}});
  }
  for (const auto &part : parts)
    views.push_back({part.kind, part.data.data(), part.data.size()});
}
owned_item::~owned_item() {
  for (auto *text : {&id, &call_id, &name, &arguments, &encrypted_content})
    if (!text->empty())
      OPENSSL_cleanse(text->data(), text->size());
  for (auto &part : parts)
    if (!part.data.empty())
      OPENSSL_cleanse(part.data.data(), part.data.size());
}
ailo_item owned_item::view() const {
  ailo_item out{};
  out.kind = kind;
  out.role = role;
  out.pinned = pinned;
  out.id = id.empty() ? nullptr : id.c_str();
  out.call_id = call_id.empty() ? nullptr : call_id.c_str();
  out.name = name.empty() ? nullptr : name.c_str();
  out.arguments = arguments.empty() ? nullptr : arguments.c_str();
  out.encrypted_content =
      encrypted_content.empty() ? nullptr : encrypted_content.c_str();
  out.content = views.empty() ? nullptr : views.data();
  out.content_count = views.size();
  return out;
}
std::vector<ailo_item> transcript::views() const {
  std::vector<ailo_item> out;
  out.reserve(items.size());
  for (const auto &item : items)
    out.push_back(item->view());
  return out;
}
continuation_guard::continuation_guard(continuation_state &value)
    : owner(value) {
  bool expected = false;
  if (!owner.active.compare_exchange_strong(
          expected, true, std::memory_order_acquire, std::memory_order_relaxed))
    throw failure(AILO_BUSY, "Continuation is active");
}
continuation_guard::~continuation_guard() {
  owner.active.store(false, std::memory_order_release);
}
continuation_state &get_continuation(ailo_continuation *value) {
  if (!value || !value->state)
    throw failure(AILO_INVALID_ARGUMENT, "A live continuation is required");
  return *static_cast<continuation_state *>(value->state);
}
std::shared_ptr<const transcript>
stage_append(continuation_state &owner, const ailo_item *items, size_t count) {
  if (!items && count)
    throw failure(AILO_INVALID_ARGUMENT, "Continuation item array is missing");
  if (count > owner.config.max_items - owner.current->items.size())
    throw failure(AILO_OUT_OF_RANGE, "Continuation item budget exceeded");
  if (!count)
    return owner.current;
  if (owner.current->revision == std::numeric_limits<uint64_t>::max())
    throw failure(AILO_OUT_OF_RANGE, "Continuation revision exhausted");
  auto views = owner.current->views();
  views.insert(views.end(), items, items + count);
  auto request = ailo_request_default();
  request.items = views.data();
  request.item_count = views.size();
  // Validate before copying payload, including null pointers/unsupported kinds.
  const auto validated =
      prepare_replay(request, owner.compactor, owner.identity, true);
  auto next = std::make_shared<transcript>(*owner.current);
  for (size_t i = 0; i < count; ++i)
    next->items.push_back(std::make_shared<owned_item>(
        items[i], next->bytes, owner.config.max_bytes, next->parts));
  next->pending = validated.pending_calls;
  ++next->revision;
  return next;
}
continuation_transaction::continuation_transaction(state &client,
                                                   const ailo_request &request)
    : owner(get_continuation(request.continuation)), guard(owner),
      candidate(stage_append(owner, request.items, request.item_count)),
      compactor(owner.compactor), items(candidate->views()), input(request) {
  client.ensure_loaded();
  identify_cache(client, true);
  if (client.conversation_identity != owner.identity)
    throw failure(
        AILO_INVALID_ARGUMENT,
        "Continuation model/tokenizer/template identity is incompatible");
  input.items = items.data();
  input.item_count = items.size();
  input.continuation = nullptr;
}
void continuation_transaction::commit() noexcept {
  owner.current = std::move(candidate);
  owner.compactor = std::move(compactor);
}
void continuation_transaction::replace(const continuation_state &replacement) {
  if (candidate->revision == std::numeric_limits<uint64_t>::max())
    throw failure(AILO_OUT_OF_RANGE, "Continuation revision exhausted");
  auto next = std::make_shared<transcript>(*replacement.current);
  next->revision = candidate->revision + 1;
  auto next_views = next->views();
  candidate = std::move(next);
  compactor = replacement.compactor;
  items = std::move(next_views);
  input.items = items.data();
  input.item_count = items.size();
}
ailo_continuation *new_window(state &client,
                              const ailo_continuation_config &limits,
                              const ailo_item *items, size_t count) {
  auto value = std::make_unique<continuation_state>();
  value->config = limits;
  value->identity = client.conversation_identity;
  value->compactor = client.compactor;
  value->current = std::make_shared<transcript>();
  value->current = stage_append(*value, items, count);
  return shell(std::move(value));
}
} // namespace client
} // namespace ailo

AILO_PUBLIC ailo_continuation_config ailo_continuation_default_config(void) {
  return {4096, 4 * 1024 * 1024};
}
AILO_PUBLIC ailo_status ailo_continuation_new(
    ailo_client *client, const ailo_continuation_config *config,
    ailo_continuation **output, ailo_error *error) {
  if (output)
    *output = nullptr;
  if (!output)
    return ailo_error_set(error, AILO_INVALID_ARGUMENT,
                          "Continuation output is required");
  return ailo::client::boundary(error, [&]() {
    const auto limits = config ? *config : ailo_continuation_default_config();
    if (!limits.max_items || limits.max_items > 4096 || !limits.max_bytes ||
        limits.max_bytes > 64 * 1024 * 1024)
      throw ailo::client::failure(
          AILO_INVALID_ARGUMENT,
          "Continuation needs 1..4096 items and nonzero storage up to 64 MiB");
    ailo::client::operation call(client);
    ailo::client::log_scope logs(call.self.logging.get(),
                                 ailo::client::log_phase::metadata);
    call.self.ensure_loaded();
    ailo::client::identify_cache(call.self, true);
    auto value = std::make_unique<ailo::client::continuation_state>();
    value->config = limits;
    value->identity = call.self.conversation_identity;
    value->compactor = call.self.compactor;
    value->current = std::make_shared<ailo::client::transcript>();
    *output = ailo::client::shell(std::move(value));
  });
}
AILO_PUBLIC ailo_status ailo_continuation_clone(ailo_continuation *source,
                                                ailo_continuation **output,
                                                ailo_error *error) {
  if (output)
    *output = nullptr;
  if (!output)
    return ailo_error_set(error, AILO_INVALID_ARGUMENT,
                          "Continuation output is required");
  return ailo::client::boundary(error, [&]() {
    auto &self = ailo::client::get_continuation(source);
    ailo::client::continuation_guard guard(self);
    auto value = std::make_unique<ailo::client::continuation_state>();
    value->config = self.config;
    value->identity = self.identity;
    value->compactor = self.compactor;
    value->current = self.current;
    *output = ailo::client::shell(std::move(value));
  });
}
AILO_PUBLIC ailo_status ailo_continuation_append(ailo_continuation *value,
                                                 const ailo_item *items,
                                                 size_t count,
                                                 ailo_error *error) {
  return ailo::client::boundary(error, [&]() {
    auto &self = ailo::client::get_continuation(value);
    ailo::client::continuation_guard guard(self);
    self.current = ailo::client::stage_append(self, items, count);
  });
}
AILO_PUBLIC ailo_status
ailo_continuation_get_info(ailo_continuation *value,
                           ailo_continuation_info *output, ailo_error *error) {
  if (output)
    *output = {};
  if (!output)
    return ailo_error_set(error, AILO_INVALID_ARGUMENT,
                          "Continuation info output is required");
  return ailo::client::boundary(error, [&]() {
    auto &self = ailo::client::get_continuation(value);
    ailo::client::continuation_guard guard(self);
    output->item_count = self.current->items.size();
    output->storage_bytes = self.current->bytes;
    output->pending_calls = self.current->pending;
    output->revision = ailo::detail::from_unsigned(self.current->revision);
    std::memcpy(output->identity, self.identity.c_str(),
                self.identity.size() + 1);
  });
}
AILO_PUBLIC ailo_status ailo_continuation_visit(ailo_continuation *value,
                                                ailo_item_sink sink, void *user,
                                                ailo_error *error) {
  if (!sink)
    return ailo_error_set(error, AILO_INVALID_ARGUMENT,
                          "Item sink is required");
  return ailo::client::boundary(error, [&]() {
    auto &self = ailo::client::get_continuation(value);
    ailo::client::continuation_guard guard(self);
    for (const auto &item : self.current->items) {
      const auto view = item->view();
      const int result = sink(user, &view);
      if (result <= 0)
        throw ailo::client::failure(result < 0 ? AILO_SINK_FAILED
                                               : AILO_CANCELLED,
                                    "Continuation item sink stopped traversal");
    }
  });
}
AILO_PUBLIC void ailo_continuation_destroy(ailo_continuation *value) {
  if (!value)
    return;
  delete static_cast<ailo::client::continuation_state *>(value->state);
  delete value;
}
