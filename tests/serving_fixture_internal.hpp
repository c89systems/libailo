#ifndef AILO_SERVING_FIXTURE_INTERNAL_HPP
#define AILO_SERVING_FIXTURE_INTERNAL_HPP
#include "ailo/client.h"
#include "ailo_error_internal.h"
#include "serving_repository.hpp"
#include <csignal>
namespace serving_test {
extern volatile std::sig_atomic_t stopping;
extern unsigned generations, tool_calls, tool_results, answers, summaries,
    current_request;
extern std::string directory;
void require(bool, const char *);
void configure_model(ailo_client_config &);
void inspect_model_input(ailo_client *, const std::string &);
} // namespace serving_test
#endif
