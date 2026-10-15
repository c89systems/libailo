#include "ailo_auto_compaction_internal.hpp"
#include "ailo_logging_internal.hpp"
#include <algorithm>
namespace ailo {
namespace client {
automatic_compaction::automatic_compaction(
    state &client, continuation_transaction &owner,
    const ailo_auto_compaction_policy &input)
    : self(client), history(owner), policy(input) {
  policy.compaction = resolve_compaction_policy(self, policy.compaction);
  const size_t available =
      llama_n_ctx(self.context.get()) - policy.compaction.output_reserve_tokens;
  if (!policy.trigger_tokens)
    policy.trigger_tokens = available;
  if (!policy.max_passes || policy.max_passes > 16 ||
      !policy.compaction.output_reserve_tokens ||
      policy.trigger_tokens > available ||
      policy.compaction.target_tokens >= policy.trigger_tokens)
    throw failure(AILO_INVALID_ARGUMENT,
                  "Automatic compaction requires target < trigger <= context "
                  "minus a nonzero output reserve, and 1..16 passes");
}
compaction_change automatic_compaction::propose(size_t unfinished) {
  if (passes >= policy.max_passes)
    throw failure(AILO_CONTEXT_LIMIT,
                  "Automatic compaction exhausted its pass budget");
  if (unfinished + 1 >= policy.trigger_tokens)
    throw failure(AILO_CONTEXT_LIMIT,
                  "Protected unfinished output leaves no compaction target");
  auto limits = policy.compaction;
  limits.target_tokens =
      std::min(limits.target_tokens, policy.trigger_tokens - unfinished - 1);
  log_scope logs(self.logging.get(), log_phase::compact);
  logs.trace("Starting automatic semantic compaction");
  compaction_change result;
  result.window.reset(compact_window(self, history.input, limits, result.info,
                                     &history.owner.config));
  if (!result.info.changed)
    throw failure(AILO_CONTEXT_LIMIT,
                  "Automatic compaction cannot reduce the protected context");
  result.items = get_continuation(result.window.get()).current->views();
  return result;
}
void automatic_compaction::adopt(const compaction_change &change) {
  history.replace(get_continuation(change.window.get()));
  ++passes;
}
} // namespace client
} // namespace ailo
extern "C" __attribute__((visibility("default"))) ailo_auto_compaction_policy
ailo_auto_compaction_default_policy(void) {
  return {ailo_compaction_default_policy(), 0, 4};
}
