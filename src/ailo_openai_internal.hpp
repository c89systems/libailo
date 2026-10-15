#ifndef AILO_OPENAI_INTERNAL_HPP
#define AILO_OPENAI_INTERNAL_HPP
#include "ailo/compaction.h"
#include "ailo/openai.h"
#include "ailo_openai_retention_internal.hpp"
#include <functional>
#include <lonejson.h>

namespace ailo {
namespace client {
using byte_sink = std::function<void(const void *, size_t)>;
// Direct lonejson sink writer. No output collector, including per SSE frame.
class wire_json {
  std::unique_ptr<lonejson, decltype(&lonejson_free)> runtime{nullptr,
                                                              lonejson_free};
  lonejson_writer writer{};
  byte_sink sink;
  std::exception_ptr problem;
  static lonejson_status forward(void *, const void *, size_t,
                                 lonejson_error *) noexcept;
  void check(lonejson_status);

public:
  explicit wire_json(byte_sink, size_t limit = 64 * 1024 * 1024);
  ~wire_json();
  wire_json(const wire_json &) = delete;
  void object();
  void end_object();
  void array();
  void end_array();
  void key(const char *);
  void key(const std::string &);
  void string(const std::string &);
  void string(const void *, size_t);
  void string_begin();
  void string_chunk(const void *, size_t);
  void string_end();
  void number_text(const std::string &);
  void number(uint64_t);
  void real(double);
  void boolean(bool);
  void null();
  void value(const std::string &);
  void finish();
};
struct openai_state {
  openai_retention_policy policy;
  ailo_record_repository repository{};
  size_t memory_limit = 32 * 1024 * 1024, record_limit = 64 * 1024 * 1024;
  ailo_compaction_policy compaction = ailo_compaction_default_policy();
  explicit openai_state(const ailo_openai_config *);
};
uint64_t api_now();
void api_identity(ailo_api_info &, const char *prefix);
void api_check(ailo_status);
int api_http_status(ailo_status);
byte_sink api_sink(state &, ailo_output_sink, void *, ailo_api_info &);
void read_record(openai_state &, ailo_record_protocol, const char *,
                 ailo_record_section, byte_sink);
void erase_record(openai_state &, ailo_record_protocol, const char *);
void query_conversation(openai_state &, const ailo_conversation_query &,
                        byte_sink);
void query_records(openai_state &, const ailo_record_query &, byte_sink);
void read_record_handle(openai_state &, void *, ailo_record_section, byte_sink);
void read_conversation_snapshot(openai_state &, const char *, byte_sink state,
                                byte_sink header);
void api_canonical_item(wire_json &, const ailo_item &);
void api_prior_input(state &, openai_request &);
void api_expand_compaction(state &, openai_request &);
void api_expand_reasoning(state &, openai_request &);
void api_prepare_reasoning(state &, const openai_request &, bool store,
                           bool forever = false);
void api_require_replay_storage(state &, bool forever = false);
std::string api_seal_reasoning(state &, const std::string &id,
                               const std::string &text);
struct sealed_summary;
transcript api_compacted_window(state &, const std::vector<ailo_item> &,
                                size_t instructions);
sealed_summary api_automatic_window(state &, const std::vector<ailo_item> &,
                                    size_t instructions,
                                    const std::vector<ailo_item> &output,
                                    const std::string &id);
class record_transaction {
  openai_state &owner;
  void *transaction = nullptr;
  size_t bytes = 0;

public:
  record_transaction(openai_state &, const ailo_record_metadata &);
  record_transaction(openai_state &, const ailo_conversation_change &);
  record_transaction(openai_state &, const ailo_record_edit &, void **reader);
  ~record_transaction();
  byte_sink section(ailo_record_section);
  void commit();
};
struct conversation_snapshot {
  std::string id;
  uint64_t created = 0, revision = 0;
  bool deleted = false, rebuild = false;
  std::map<std::string, std::string> metadata;
  std::string state_json;
  transcript incoming;
  std::vector<std::string> wire_items;
  std::vector<const char *> input_ids;
};
std::unique_ptr<conversation_snapshot>
api_load_conversation(openai_state &, const char *, bool deleted = false);
std::unique_ptr<conversation_snapshot>
api_attach_conversation(state &, openai_request &);
void api_conversation_header(wire_json &, const conversation_snapshot &);
void api_conversation_items(wire_json &, const conversation_snapshot &,
                            const std::vector<ailo_item> &canonical);
void api_project_items(const openai_request &, const char *, byte_sink, size_t,
                       const std::function<void(byte_sink)> &);
void api_response_input_page(state &, openai_request &, record_transaction &);
void api_input_wire(wire_json &, const openai_request &, const ailo_item &,
                    const std::string &, bool has_status);
} // namespace client
} // namespace ailo
#endif
