#ifndef AILO_MODEL_METADATA_INTERNAL_HPP
#define AILO_MODEL_METADATA_INTERNAL_HPP
#include "ailo_model_internal.hpp"

namespace ailo {
namespace client {
class bounded_json;
struct metadata_claims {
  std::string name, license_id, license_name, license_url;
  std::vector<std::string> base_models;
  bool lineage_known = false;
};
struct publisher_metadata {
  metadata_claims claims;
  std::string status = "not_requested";
};
metadata_claims parse_supplemental_metadata(const char *json);
publisher_metadata load_publisher_metadata(
    const ailo_model_source &, const std::string &acquired_path,
    const model::transport_policy & = {"https://huggingface.co", nullptr});
void write_claims(bounded_json &, const metadata_claims &);
bool immutable_revision(const std::string &);
} // namespace client
} // namespace ailo
#endif
