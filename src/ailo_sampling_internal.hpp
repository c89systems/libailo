#ifndef AILO_SAMPLING_INTERNAL_HPP
#define AILO_SAMPLING_INTERNAL_HPP

#include "ailo_client_internal.hpp"

namespace ailo {
namespace client {
void validate_sampling(const ailo_request &request);
std::string token_piece(const llama_vocab *vocab, llama_token token);

// Owns only vocabulary-sized scratch and one token's bounded alternatives.
// No generated transcript is retained. A fresh instance owns each candidate.
class sampling {
  using owner = std::unique_ptr<llama_sampler, decltype(&llama_sampler_free)>;
  const llama_vocab *vocab;
  owner processors{nullptr, llama_sampler_free};
  owner selector{nullptr, llama_sampler_free};
  std::vector<llama_token_data> scratch;
  std::vector<float> before;
  std::vector<size_t> ranking;
  std::vector<std::string> pieces;
  std::vector<ailo_token_probability> alternatives;
  std::string chosen_piece;
  unsigned top_count;
  bool report;

public:
  sampling(const llama_vocab *vocab, const ailo_request &request,
           unsigned candidate, size_t context_tokens);
  void accept(llama_token token);
  llama_token sample(llama_context *context, ailo_token_info &info);
};
} // namespace client
} // namespace ailo
#endif
