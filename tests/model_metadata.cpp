#include "ailo/client.h"
#include "ailo_client_internal.hpp"
#include "ailo_model_metadata_internal.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x))                                                                  \
      throw std::runtime_error(#x);                                            \
  } while (0)
namespace fs = std::filesystem;
using namespace ailo::client;
namespace {
int cancel(void *, unsigned, unsigned, ailo_u64 received, ailo_u64) {
  return !received.lo && !received.hi;
}
std::string card(ailo_client *client) {
  char *text = nullptr;
  ailo_error error{};
  CHECK(client->model_card(client, &text, &error) == AILO_OK);
  std::unique_ptr<char, decltype(&ailo_string_free)> owned(text,
                                                           ailo_string_free);
  return text;
}
fs::path sidecar(const fs::path &directory) {
  for (const auto &entry : fs::directory_iterator(directory))
    if (entry.path().filename().string().find(".ailo-metadata-") == 0)
      return entry.path();
  return {};
}
void no_partial(const fs::path &directory) {
  for (const auto &entry : fs::directory_iterator(directory))
    CHECK(entry.path().filename().string().find(".ailo-partial-") != 0);
}
} // namespace
int main(int argc, char **argv) {
  try {
    CHECK(argc == 5);
    const std::string cache = argv[2], revision(40, 'a');
    const ailo::model::transport_policy transport{argv[3], argv[4]};
    auto config = ailo_client_default_config();
    config.source.local_path = argv[1];
    config.backend = AILO_BACKEND_CPU;
    config.lazy = 1;
    for (const auto *bad :
         {"[]", "null", "{", "{\"name\":1}", "{\"name\":\"\xc0\x80\"}",
          "{\"architecture\":\"fake\"}", "{\"name\":null,\"name\":\"x\"}",
          "{\"license\":{\"url\":\"https://secret@example.com/license\"}}",
          "{\"license\":{\"url\":\"file:///home/private\"}}",
          "{\"license\":{\"url\":\"https:///\"}}",
          "{\"source\":{\"base_models\":[\"../private\"]}}"}) {
      config.model_metadata = bad;
      ailo_client *client = reinterpret_cast<ailo_client *>(1);
      ailo_error error{};
      CHECK(ailo_client_new(&config, &client, &error) ==
                AILO_INVALID_ARGUMENT &&
            !client);
    }
    std::string too_large(1024 * 1024 + 1, ' ');
    config.model_metadata = too_large.c_str();
    ailo_client *client = nullptr;
    ailo_error error{};
    CHECK(ailo_client_new(&config, &client, &error) == AILO_OUT_OF_RANGE &&
          !client);
    CHECK(parse_supplemental_metadata("{}").license_id.empty());

    config.model_metadata = nullptr;
    config.source.local_path = nullptr;
    config.source.repo = "fixture/good";
    config.source.revision = revision.c_str();
    config.source.filename = "nested/model.gguf";
    config.source.cache_root = cache.c_str();
    const auto prepare = [&](const char *repo) {
      config.source.repo = repo;
      auto plan = ailo::model::make_plan(&config.source);
      fs::create_directories(fs::path(plan.files[0].path).parent_path());
      fs::copy_file(argv[1], plan.files[0].path,
                    fs::copy_options::overwrite_existing);
      return plan;
    };
    auto plan = prepare("fixture/good");
    auto result =
        load_publisher_metadata(config.source, plan.files[0].path, transport);
    CHECK(result.status == "offline_missing");
    CHECK(sidecar(plan.directory).empty());
    config.source.allow_download = 1;
    result =
        load_publisher_metadata(config.source, plan.files[0].path, transport);
    CHECK(result.status == "fetched" &&
          result.claims.license_id == "apache-2.0" &&
          result.claims.base_models ==
              std::vector<std::string>{"fixture/base"});
    const auto metadata_file = sidecar(plan.directory);
    CHECK(!metadata_file.empty());
    std::ifstream input(metadata_file);
    const std::string bytes((std::istreambuf_iterator<char>(input)), {});
    CHECK(bytes.find("SYNTHETIC_SECRET") == std::string::npos);
    CHECK(bytes.find("ignored_private") == std::string::npos);
    config.source.allow_download = 0;
    const ailo::model::transport_policy forbidden{
        "http://must-not-connect.invalid", nullptr};
    result =
        load_publisher_metadata(config.source, plan.files[0].path, forbidden);
    CHECK(result.status == "cached" &&
          result.claims.license_id == "apache-2.0");
    const char *old_xdg = std::getenv("XDG_CACHE_HOME");
    const std::string saved_xdg = old_xdg ? old_xdg : "";
    CHECK(setenv("XDG_CACHE_HOME", "relative-invalid-cache", 1) == 0);
    config.source.cache_root = nullptr;
    CHECK(load_publisher_metadata(config.source, plan.files[0].path, forbidden)
              .status == "cached");
    config.source.cache_root = cache.c_str();
    CHECK((old_xdg ? setenv("XDG_CACHE_HOME", saved_xdg.c_str(), 1)
                   : unsetenv("XDG_CACHE_HOME")) == 0);
    const std::string other_digest(64, '0');
    const char *digest_view = other_digest.c_str();
    config.source.sha256 = &digest_view;
    config.source.sha256_count = 1;
    CHECK(load_publisher_metadata(config.source, plan.files[0].path, forbidden)
              .status == "offline_missing");
    config.source.sha256 = nullptr;
    config.source.sha256_count = 0;
    config.model_metadata = "{\"license\":{\"id\":\"MIT\"},\"source\":{\"base_"
                            "models\":[\"fixture/caller\"]}}";
    CHECK(ailo_client_new(&config, &client, &error) == AILO_OK);
    auto output = card(client);
    CHECK(output.find("\"origin\":\"caller\"") != std::string::npos);
    CHECK(output.find("\"claim\":\"apache-2.0\"") != std::string::npos);
    CHECK(output.find("\"field\":\"source.base_models\"") != std::string::npos);
    CHECK(output.find(cache) == std::string::npos);
    ailo_client_destroy(client);

    // Existing corrupt or foreign sidecars are advisory, never
    // trusted/replaced.
    std::string foreign = bytes;
    const auto revision_at = foreign.find(revision);
    CHECK(revision_at != std::string::npos);
    foreign[revision_at] = 'b';
    std::ofstream(metadata_file) << foreign;
    result =
        load_publisher_metadata(config.source, plan.files[0].path, forbidden);
    CHECK(result.status == "invalid" && result.claims.license_id.empty());
    std::ofstream(metadata_file) << "{}";
    result =
        load_publisher_metadata(config.source, plan.files[0].path, forbidden);
    CHECK(result.status == "invalid" && result.claims.license_id.empty());
    std::ofstream(metadata_file) << too_large;
    CHECK(load_publisher_metadata(config.source, plan.files[0].path, forbidden)
              .status == "invalid");
    fs::remove(metadata_file);
    fs::create_symlink(argv[1], metadata_file);
    CHECK(load_publisher_metadata(config.source, plan.files[0].path, forbidden)
              .status == "unavailable");
    CHECK(fs::is_symlink(metadata_file));
    fs::remove(metadata_file);
    config.source.revision = "main";
    CHECK(load_publisher_metadata(config.source, plan.files[0].path, forbidden)
              .status == "unresolved_revision");
    config.source.revision = revision.c_str();
    config.model_metadata = nullptr;
    config.source.allow_download = 1;
    for (const auto *repo :
         {"fixture/mismatch-sha", "fixture/mismatch-repo", "fixture/bad-json",
          "fixture/duplicate", "fixture/too-large", "fixture/http-error"}) {
      plan = prepare(repo);
      result =
          load_publisher_metadata(config.source, plan.files[0].path, transport);
      CHECK((result.status == "invalid" || result.status == "unavailable") &&
            result.claims.license_id.empty());
      CHECK(sidecar(plan.directory).empty());
      no_partial(plan.directory);
      config.source.allow_download = 0;
      CHECK(ailo_client_new(&config, &client, &error) == AILO_OK);
      CHECK(card(client).find("\"architecture\":\"llama\"") !=
            std::string::npos);
      ailo_client_destroy(client);
      config.source.allow_download = 1;
    }
    plan = prepare("fixture/cancel");
    config.source.progress = cancel;
    try {
      load_publisher_metadata(config.source, plan.files[0].path, transport);
      throw std::runtime_error("Metadata cancellation was ignored");
    } catch (const failure &e) {
      CHECK(e.status == AILO_CANCELLED);
    }
    CHECK(sidecar(plan.directory).empty());
    no_partial(plan.directory);
    std::puts(
        "Model metadata: constructor validation, provenance, revision binding, "
        "HTTPS, offline cache, failure isolation and cancellation passed");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
