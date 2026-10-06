#include "ailo_model_internal.hpp"
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <memory>

namespace ailo {
namespace model {
template <typename Function>
static ailo_status boundary(char **output, ailo_error *error, Function work) {
  if (!output)
    return ailo_error_set(error, AILO_INVALID_ARGUMENT,
                          "Model path output is required");
  *output = nullptr;
  try {
    std::string path = work();
    char *owned = static_cast<char *>(std::malloc(path.size() + 1));
    if (!owned)
      throw std::bad_alloc();
    std::memcpy(owned, path.c_str(), path.size() + 1);
    *output = owned;
    ailo_error_clear(error);
    return AILO_OK;
  } catch (const failure &e) {
    return ailo_error_set(error, e.status, e.what());
  } catch (const std::bad_alloc &) {
    return ailo_error_set(error, AILO_NO_MEMORY,
                          "Model acquisition allocation failed");
  } catch (...) {
    return ailo_error_set(error, AILO_INTERNAL_ERROR,
                          "Model acquisition raised an exception");
  }
}

ailo_status acquire(const ailo_model_source *source, char **output,
                    ailo_error *error, const transport_policy &policy) {
  return boundary(output, error, [&]() {
    const plan p = make_plan(source);
    notify(p, 1, 0, 0);
    if (p.local) {
      for (size_t i = 0; i < p.files.size(); ++i) {
        fd handle(
            open(p.files[i].path.c_str(), O_RDONLY | O_CLOEXEC | O_NONBLOCK));
        if (!handle)
          io_failure("Cannot open caller-owned local model");
        validate_file(handle.get(), p.files[i], p,
                      static_cast<unsigned>(i + 1));
      }
    } else {
      fd base = directory(p.directory, p.allow_download);
      std::unique_ptr<publication_lock> lock;
      if (p.allow_download)
        lock.reset(new publication_lock(base.get(), p));
      for (size_t i = 0; i < p.files.size(); ++i) {
        const file &f = p.files[i];
        size_t slash = f.name.find_last_of('/');
        std::string parent =
            slash == std::string::npos ? "" : f.name.substr(0, slash);
        std::string name =
            slash == std::string::npos ? f.name : f.name.substr(slash + 1);
        fd dir = child_directory(base.get(), parent, p.allow_download);
        fd existing = existing_file(dir.get(), name);
        if (existing) {
          validate_file(existing.get(), f, p, static_cast<unsigned>(i + 1));
          continue;
        }
        if (!p.allow_download)
          throw failure(AILO_DOWNLOAD_FAILED,
                        "Cached model is missing; allow_download is disabled");
        partial_file partial(dir.get(), f.name);
        download(partial.get(), f, p, static_cast<unsigned>(i + 1), policy);
        validate_file(partial.get(), f, p, static_cast<unsigned>(i + 1));
        notify(p, static_cast<unsigned>(i + 1), 0, 0);
        partial.publish(name);
      }
    }
    return p.files.front().path;
  });
}
} // namespace model
} // namespace ailo

extern "C" __attribute__((visibility("default"))) ailo_status
ailo_model_cache_path(const ailo_model_source *source, char **output,
                      ailo_error *error) {
  return ailo::model::boundary(output, error, [&]() {
    return ailo::model::make_plan(source).files.front().path;
  });
}
extern "C" __attribute__((visibility("default"))) ailo_status
ailo_model_acquire(const ailo_model_source *source, char **output,
                   ailo_error *error) {
  return ailo::model::acquire(source, output, error,
                              {"https://huggingface.co", nullptr});
}
