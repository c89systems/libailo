#include "ailo-cache-build.h"
#include "ailo_cache_internal.hpp"
#include "ailo_model_internal.hpp"
#include "src/llama-model.h"
#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <openssl/evp.h>
#include <sys/stat.h>
#include <unistd.h>

namespace ailo {
namespace client {
namespace {
std::string file_digest(state &self, const std::string &path) {
  model::fd file(
      open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
  struct stat before{}, after{};
  if (!file || fstat(file.get(), &before) || !S_ISREG(before.st_mode))
    throw failure(AILO_IO_FAILED, "Cannot hash a regular loaded model shard");
  std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> digest(
      EVP_MD_CTX_new(), EVP_MD_CTX_free);
  if (!digest)
    throw std::bad_alloc();
  if (EVP_DigestInit_ex(digest.get(), EVP_sha256(), nullptr) != 1)
    throw failure(AILO_INTERNAL_ERROR,
                  "Cannot initialize model cache identity");
  std::array<unsigned char, 65536> block;
  off_t offset = 0;
  while (offset < before.st_size) {
    if (self.cancelled.load())
      throw failure(AILO_CANCELLED, "Model hashing cancelled");
    ssize_t size;
    do {
      size = pread(file.get(), block.data(), block.size(), offset);
    } while (size < 0 && errno == EINTR);
    if (size <= 0 || size > before.st_size - offset)
      throw failure(AILO_IO_FAILED,
                    "Loaded model changed during cache hashing");
    if (EVP_DigestUpdate(digest.get(), block.data(),
                         static_cast<size_t>(size)) != 1)
      throw failure(AILO_INTERNAL_ERROR, "Cannot update model cache identity");
    offset += size;
  }
  if (fstat(file.get(), &after) || before.st_size != after.st_size ||
      before.st_mtim.tv_sec != after.st_mtim.tv_sec ||
      before.st_mtim.tv_nsec != after.st_mtim.tv_nsec ||
      before.st_ctim.tv_sec != after.st_ctim.tv_sec ||
      before.st_ctim.tv_nsec != after.st_ctim.tv_nsec)
    throw failure(AILO_IO_FAILED, "Loaded model changed during cache hashing");
  unsigned char bytes[32];
  unsigned size = 0;
  if (EVP_DigestFinal_ex(digest.get(), bytes, &size) != 1 ||
      size != sizeof(bytes))
    throw failure(AILO_INTERNAL_ERROR, "Cannot finish model cache identity");
  static const char hex[] = "0123456789abcdef";
  std::string result;
  for (auto ch : bytes) {
    result += hex[ch >> 4];
    result += hex[ch & 15];
  }
  return result;
}
} // namespace

void identify_cache(state &self) {
  if (!self.cache || !self.cache->available() || !self.cache_identity.empty())
    return;
  try {
    std::string identity;
    auto add = [&](const std::string &value) {
      identity += std::to_string(value.size());
      identity += ':';
      identity += value;
    };
    add(AILO_CACHE_BUILD_ID);
    add(llama_print_system_info());
    add(self.source.repo);
    add(self.source.revision);
    add(self.source.filename);
    ailo_model_source source = self.source.value;
    // Use acquired paths, never a possibly changed cwd/XDG environment.
    source.local_path = self.path.c_str();
    source.repo = source.revision = source.filename = nullptr;
    source.sha256 = nullptr;
    source.sha256_count = 0;
    const auto split_count = metadata(self.model.get(), "split.count");
    if (!split_count.empty() && split_count != "1")
      source.split_shards = 1;
    const auto files = model::make_plan(&source).files;
    for (size_t i = 0; i < files.size(); ++i) {
      const bool verified = files.size() == self.source.digests.size() &&
                            !self.source.digests[i].empty();
      add(verified ? self.source.digests[i] : file_digest(self, files[i].path));
    }
    const char *tmpl =
        self.config.chat_template
            ? self.config.chat_template
            : llama_model_chat_template(self.model.get(), nullptr);
    add(tmpl ? tmpl : "");
    add(std::to_string(llama_n_ctx(self.context.get())));
    add(std::to_string(llama_n_batch(self.context.get())));
    add(std::to_string(llama_n_ubatch(self.context.get())));
    add(std::to_string(self.config.threads));
    add(std::to_string(self.actual));
    add(std::to_string(self.config.gpu_layers));
    auto device_identity = [&](ggml_backend_dev_t device) {
      if (!device) {
        add("none");
        return;
      }
      ggml_backend_dev_props properties{};
      ggml_backend_dev_get_props(device, &properties);
      add(properties.name ? properties.name : "");
      add(properties.description ? properties.description : "");
      add(properties.device_id ? properties.device_id : "");
      add(ggml_backend_buft_name(ggml_backend_dev_buffer_type(device)));
    };
    for (auto device : self.selected)
      if (device)
        device_identity(device);
    // Automatic layer placement depends on available device memory at load.
    // Bind the actual placement, not only the requested device list.
    add(std::to_string(self.model->n_gpu_layers()));
    add(std::to_string(self.model->split_mode()));
    for (int i = 0; i < llama_model_n_layer(self.model.get()); ++i) {
      device_identity(self.model->dev_layer(i));
      add(ggml_backend_buft_name(self.model->select_buft(i)));
    }
    device_identity(self.model->dev_output());
    self.cache_identity = model::digest_hex(identity.data(), identity.size());
  } catch (const model::failure &error) {
    throw failure(error.status, error.what());
  }
}
} // namespace client
} // namespace ailo
