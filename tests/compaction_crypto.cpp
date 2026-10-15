#include "ailo_compaction_internal.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <openssl/evp.h>
#include <stdexcept>
#include <thread>

using namespace ailo::client;
#define CHECK(c)                                                               \
  do {                                                                         \
    if (!(c))                                                                  \
      throw std::runtime_error(std::string(#c) + " at " +                      \
                               std::to_string(__LINE__));                      \
  } while (0)
template <class Work> void fails(ailo_status status, Work work) {
  bool failed = false;
  try {
    work();
  } catch (const failure &e) {
    failed = e.status == status;
  }
  CHECK(failed);
}
std::string mutate(const std::string &text, size_t offset) {
  std::string padded = text;
  for (char &ch : padded)
    if (ch == '-')
      ch = '+';
    else if (ch == '_')
      ch = '/';
  while (padded.size() % 4)
    padded += '=';
  std::vector<unsigned char> bytes(padded.size() / 4 * 3);
  int n = EVP_DecodeBlock(
      bytes.data(), reinterpret_cast<const unsigned char *>(padded.data()),
      static_cast<int>(padded.size()));
  CHECK(n >= 0);
  bytes.resize(static_cast<size_t>(n) - (padded.size() - text.size()));
  CHECK(offset < bytes.size());
  bytes[offset] ^= 1;
  std::string out(4 * ((bytes.size() + 2) / 3), '\0');
  CHECK(EVP_EncodeBlock(reinterpret_cast<unsigned char *>(out.data()),
                        bytes.data(), static_cast<int>(bytes.size())) ==
        static_cast<int>(out.size()));
  while (out.back() == '=')
    out.pop_back();
  for (char &ch : out)
    if (ch == '+')
      ch = '-';
    else if (ch == '/')
      ch = '_';
  return out;
}
int main() {
  try {
    std::array<unsigned char, 32> key{};
    key.fill(42);
    auto config = ailo_compactor_default_config();
    config.namespace_id = "tenant-a";
    config.current_key = {"one", key.data(), key.size()};
    compaction_state owner(config);
    ailo_content before{AILO_CONTENT_TEXT, "keep exactly", 12},
        after{AILO_CONTENT_TEXT, "recent", 6};
    ailo_item items[4]{};
    items[0].role = AILO_ROLE_SYSTEM;
    items[0].id = "instructions";
    items[0].pinned = 1;
    items[0].content = &before;
    items[0].content_count = 1;
    items[1].kind = AILO_ITEM_COMPACTION;
    items[2].id = "recent";
    items[2].content = &after;
    items[2].content_count = 1;
    items[3].content = &after;
    items[3].content_count = 1;
    compaction_payload payload;
    payload.summary = "{\"facts\":[\"Finish T-1.\"]}";
    payload.source_items = 4;
    payload.removed_items = 2;
    payload.cut = 1;
    payload.retained = {fingerprint(items[0]), fingerprint(items[2])};
    const std::string identity(64, 'a');
    const auto sealed = owner.seal(payload, identity),
               another = owner.seal(payload, identity);
    CHECK(sealed.id != another.id &&
          sealed.encrypted_content != another.encrypted_content);
    auto opened = owner.open(sealed.id.c_str(),
                             sealed.encrypted_content.c_str(), identity);
    CHECK(opened->summary == payload.summary &&
          opened->retained == payload.retained && opened->cut == 1);
    verify_compaction_binding(*opened, items, 3, 1);
    verify_compaction_binding(*opened, items, 4, 1); // legitimate append/fork
    verify_compaction_binding(*opened, items, 3, 1); // replay is not single-use
    for (size_t offset :
         {size_t(0), size_t(8), size_t(20), size_t(52), size_t(84), size_t(116),
          size_t(124), size_t(132), size_t(140), size_t(156)}) {
      const auto changed = mutate(sealed.encrypted_content, offset);
      fails(AILO_INVALID_ARGUMENT,
            [&] { owner.open(sealed.id.c_str(), changed.c_str(), identity); });
    }
    fails(AILO_INVALID_ARGUMENT, [&] {
      owner.open("renamed", sealed.encrypted_content.c_str(), identity);
    });
    fails(AILO_INVALID_ARGUMENT, [&] {
      owner.open(sealed.id.c_str(), sealed.encrypted_content.c_str(),
                 std::string(64, 'b'));
    });
    for (auto suffix : {"=", "!"}) {
      const auto changed = sealed.encrypted_content + suffix;
      fails(AILO_INVALID_ARGUMENT,
            [&] { owner.open(sealed.id.c_str(), changed.c_str(), identity); });
    }
    fails(AILO_INVALID_ARGUMENT, [&] {
      owner.open(sealed.id.c_str(), "OpenAI-encrypted-content", identity);
    });
    const auto truncated =
        sealed.encrypted_content.substr(0, sealed.encrypted_content.size() - 4);
    fails(AILO_INVALID_ARGUMENT,
          [&] { owner.open(sealed.id.c_str(), truncated.c_str(), identity); });
    auto wrong = config;
    wrong.namespace_id = "tenant-b";
    compaction_state foreign(wrong);
    fails(AILO_INVALID_ARGUMENT, [&] {
      foreign.open(sealed.id.c_str(), sealed.encrypted_content.c_str(),
                   identity);
    });
    auto other_key = key;
    other_key[0] ^= 1;
    wrong = config;
    wrong.current_key.bytes = other_key.data();
    compaction_state different(wrong);
    fails(AILO_INVALID_ARGUMENT, [&] {
      different.open(sealed.id.c_str(), sealed.encrypted_content.c_str(),
                     identity);
    });
    auto rotated = config;
    rotated.current_key = {"two", other_key.data(), 32};
    rotated.old_keys = &config.current_key;
    rotated.old_key_count = 1;
    compaction_state next(rotated);
    CHECK(
        next.open(sealed.id.c_str(), sealed.encrypted_content.c_str(), identity)
            ->summary == payload.summary);
    const auto new_item = next.seal(payload, identity);
    fails(AILO_INVALID_ARGUMENT, [&] {
      owner.open(new_item.id.c_str(), new_item.encrypted_content.c_str(),
                 identity);
    });
    rotated.old_keys = &rotated.current_key;
    fails(AILO_INVALID_ARGUMENT, [&] { compaction_state duplicate(rotated); });
    wrong = config;
    wrong.current_key.size = 31;
    fails(AILO_INVALID_ARGUMENT, [&] { compaction_state short_key(wrong); });
    wrong = config;
    wrong.max_item_bytes = 256;
    compaction_state small(wrong);
    fails(AILO_OUT_OF_RANGE, [&] { small.seal(payload, identity); });
    items[0].pinned = 0;
    fails(AILO_INVALID_ARGUMENT,
          [&] { verify_compaction_binding(*opened, items, 3, 1); });
    items[0].pinned = 1;
    items[0].role = AILO_ROLE_DEVELOPER;
    fails(AILO_INVALID_ARGUMENT,
          [&] { verify_compaction_binding(*opened, items, 3, 1); });
    items[0].role = AILO_ROLE_SYSTEM;
    before.data = "Keep exactly";
    fails(AILO_INVALID_ARGUMENT,
          [&] { verify_compaction_binding(*opened, items, 3, 1); });
    before.data = "keep exactly";
    items[2].id = "different-id";
    fails(AILO_INVALID_ARGUMENT,
          [&] { verify_compaction_binding(*opened, items, 3, 1); });
    items[2].id = "recent";
    fails(AILO_INVALID_ARGUMENT,
          [&] { verify_compaction_binding(*opened, items, 2, 1); });
    fails(AILO_INVALID_ARGUMENT,
          [&] { verify_compaction_binding(*opened, items, 3, 0); });
    items[3].kind = AILO_ITEM_COMPACTION;
    fails(AILO_INVALID_ARGUMENT,
          [&] { verify_compaction_binding(*opened, items, 4, 1); });
    items[3].kind = AILO_ITEM_MESSAGE;
    std::swap(items[0], items[2]);
    fails(AILO_INVALID_ARGUMENT,
          [&] { verify_compaction_binding(*opened, items, 3, 1); });
    std::swap(items[0], items[2]);
    wrong = config;
    wrong.ttl_seconds = 1;
    compaction_state expiring(wrong);
    const auto expired = expiring.seal(payload, identity);
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    fails(AILO_INVALID_ARGUMENT, [&] {
      expiring.open(expired.id.c_str(), expired.encrypted_content.c_str(),
                    identity);
    });
    compaction_state ephemeral(ailo_compactor_default_config()),
        separate(ailo_compactor_default_config());
    const auto private_item = ephemeral.seal(payload, identity);
    fails(AILO_INVALID_ARGUMENT, [&] {
      separate.open(private_item.id.c_str(),
                    private_item.encrypted_content.c_str(), identity);
    });
    CHECK(ephemeral
              .open(private_item.id.c_str(),
                    private_item.encrypted_content.c_str(), identity)
              ->summary == payload.summary);
    ailo_compactor *receiver = nullptr;
    ailo_error error{};
    CHECK(ailo_compactor_new(&config, &receiver, &error) == AILO_OK);
    ailo_compactor_info info{};
    CHECK(receiver->info(receiver, &info, &error) == AILO_OK &&
          !info.ephemeral && info.decryption_keys == 1 && !info.ttl_seconds);
    auto retained = retain_compactor(receiver);
    ailo_client_config client_config{};
    client_config.source.local_path = "unused-compaction-model.gguf";
    client_config.lazy = 1;
    client_config.disable_prompt_cache = 1;
    client_config.compactor = receiver;
    ailo_client *client = nullptr;
    CHECK(ailo_client_new(&client_config, &client, &error) == AILO_OK);
    std::unique_ptr<ailo_client, decltype(&ailo_client_destroy)> client_owner(
        client, ailo_client_destroy);
    receiver->destroy(receiver);
    CHECK(retained
              ->open(sealed.id.c_str(), sealed.encrypted_content.c_str(),
                     identity)
              ->summary == payload.summary);
    retained.reset();
    auto &self = *static_cast<state *>(client->state);
    CHECK(self.compactor
              ->open(sealed.id.c_str(), sealed.encrypted_content.c_str(),
                     identity)
              ->summary == payload.summary);
    self.cancelled.store(true);
    fails(AILO_CANCELLED,
          [&] { self.compactor->seal(payload, identity, &self); });
    fails(AILO_CANCELLED, [&] {
      self.compactor->open(sealed.id.c_str(), sealed.encrypted_content.c_str(),
                           identity, &self);
    });
    ailo_compactor_destroy(nullptr);
    std::puts("Compaction envelope authentication, retained bindings, "
              "rotation, expiry, bounds and owner lifetime passed");
  } catch (const std::exception &e) {
    std::fprintf(stderr, "compaction-crypto: %s\n", e.what());
    return 1;
  }
}
