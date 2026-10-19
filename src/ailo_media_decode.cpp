#include "ailo_media_internal.hpp"
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <exception>

namespace {
struct codec_memory {
  const std::function<void(size_t)> &reserve;
  size_t live = 0, peak = 0;
  std::exception_ptr failure;
  bool allocation_failed = false;
  void check() {
    if (failure)
      std::rethrow_exception(failure);
    if (allocation_failed)
      throw std::bad_alloc();
  }
};
struct alignas(std::max_align_t) codec_allocation {
  size_t size;
};
thread_local codec_memory *active_codec_memory = nullptr;
void *codec_malloc(size_t size) noexcept {
  auto &memory = *active_codec_memory;
  if (memory.failure || memory.allocation_failed)
    return nullptr;
  if (size > SIZE_MAX - sizeof(codec_allocation) ||
      size + sizeof(codec_allocation) > SIZE_MAX - memory.live) {
    memory.allocation_failed = true;
    return nullptr;
  }
  const size_t charge = size + sizeof(codec_allocation);
  try {
    const size_t next = memory.live + charge;
    if (next > memory.peak) {
      memory.reserve(next - memory.peak);
      memory.peak = next;
    }
  } catch (...) {
    // Let stb clean its C allocation paths before rethrowing at the bridge.
    memory.failure = std::current_exception();
    return nullptr;
  }
  auto *allocation = static_cast<codec_allocation *>(std::malloc(charge));
  if (!allocation) {
    memory.allocation_failed = true;
    return nullptr;
  }
  allocation->size = size;
  memory.live += charge;
  return allocation + 1;
}
void codec_free(void *p) noexcept {
  if (!p)
    return;
  auto *allocation = static_cast<codec_allocation *>(p) - 1;
  active_codec_memory->live -= allocation->size + sizeof(codec_allocation);
  std::free(allocation);
}
void *codec_realloc(void *p, size_t size) noexcept {
  auto *output = codec_malloc(size);
  if (output && p) {
    const auto *allocation = static_cast<codec_allocation *>(p) - 1;
    std::memcpy(output, p, std::min(size, allocation->size));
    codec_free(p);
  }
  return output;
}
struct codec_scope {
  codec_memory *previous;
  explicit codec_scope(codec_memory &m) : previous(active_codec_memory) {
    active_codec_memory = &m;
  }
  ~codec_scope() { active_codec_memory = previous; }
};
} // namespace

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_NO_STDIO
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_MALLOC(n) codec_malloc(n)
#define STBI_FREE(p) codec_free(p)
#define STBI_REALLOC_SIZED(p, old_size, new_size) codec_realloc(p, new_size)
#include <stb/stb_image.h>
#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_ENCODING
#define MA_NO_DEVICE_IO
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#define MA_NO_GENERATION
#define MA_API static
/* Isolate miniaudio's one non-MA_API global from mtmd's decoder instance. */
#define ma_atomic_global_lock ailo_media_decoder_atomic_lock
#include <miniaudio/miniaudio.h>

namespace ailo {
namespace client {
std::string media_format(const void *data, size_t size,
                         ailo_content_kind kind) {
  if (!data || !size)
    throw failure(AILO_INVALID_ARGUMENT, "Media payload is empty");
  const auto *p = static_cast<const unsigned char *>(data);
  if (kind == AILO_CONTENT_IMAGE) {
    const unsigned char png[] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (size >= sizeof(png) && !std::memcmp(p, png, sizeof(png)))
      return "png";
    if (size >= 3 && p[0] == 255 && p[1] == 216 && p[2] == 255)
      return "jpeg";
  } else if (kind == AILO_CONTENT_AUDIO) {
    if (size >= 12 && !std::memcmp(p, "RIFF", 4) &&
        !std::memcmp(p + 8, "WAVE", 4))
      return "wav";
    if (size >= 4 && !std::memcmp(p, "fLaC", 4))
      return "flac";
    if (size >= 3 &&
        (!std::memcmp(p, "ID3", 3) || (p[0] == 255 && (p[1] & 224) == 224)))
      return "mp3";
  }
  throw failure(AILO_UNSUPPORTED, "Unsupported encoded image/audio format");
}
const char *media_mime(const std::string &format) {
  if (format == "png")
    return "image/png";
  if (format == "jpeg")
    return "image/jpeg";
  if (format == "wav")
    return "audio/wav";
  if (format == "mp3")
    return "audio/mpeg";
  if (format == "flac")
    return "audio/flac";
  throw failure(AILO_UNSUPPORTED, "Unsupported media format");
}
mtmd_bitmap *decode_media(state &self, const media_part &part,
                          const std::function<void(size_t)> &reserve) {
  const auto &limits = self.media->config;
  const auto *bytes =
      reinterpret_cast<const unsigned char *>(part.bytes.data());
  if (part.kind == AILO_CONTENT_IMAGE) {
    const auto format = media_format(bytes, part.bytes.size(), part.kind);
    if (format == "png") {
      size_t offset = 8;
      bool ended = false;
      while (offset <= part.bytes.size() && part.bytes.size() - offset >= 12) {
        const auto *p = bytes + offset;
        const uint32_t length = uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 |
                                uint32_t(p[2]) << 8 | uint32_t(p[3]);
        if (length > part.bytes.size() - offset - 12)
          throw failure(AILO_INVALID_ARGUMENT, "PNG chunk is truncated");
        if (!std::memcmp(p + 4, "acTL", 4) || !std::memcmp(p + 4, "fcTL", 4) ||
            !std::memcmp(p + 4, "fdAT", 4))
          throw failure(AILO_UNSUPPORTED,
                        "Animated image input requires a video/frame adapter");
        offset += static_cast<size_t>(length) + 12;
        if (!std::memcmp(p + 4, "IEND", 4)) {
          ended = length == 0;
          break;
        }
      }
      if (!ended || offset != part.bytes.size())
        throw failure(AILO_INVALID_ARGUMENT,
                      "PNG is incomplete or has trailing data");
    } else if (part.bytes.size() < 4 || bytes[part.bytes.size() - 2] != 255 ||
               bytes[part.bytes.size() - 1] != 217)
      throw failure(AILO_INVALID_ARGUMENT,
                    "JPEG is incomplete or has trailing data");
    codec_memory memory{reserve, 0, 0, {}, false};
    codec_scope scope(memory);
    int width = 0, height = 0, channels = 0;
    if (part.bytes.size() > INT_MAX ||
        !stbi_info_from_memory(bytes, static_cast<int>(part.bytes.size()),
                               &width, &height, &channels) ||
        width <= 0 || height <= 0) {
      memory.check();
      throw failure(AILO_INVALID_ARGUMENT, "Invalid encoded image header");
    }
    if (static_cast<size_t>(width) >
        limits.max_image_pixels / static_cast<size_t>(height))
      throw failure(AILO_OUT_OF_RANGE, "Decoded image exceeds its pixel limit");
    const size_t pixels =
        static_cast<size_t>(width) * static_cast<size_t>(height);
    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> image(
        stbi_load_from_memory(bytes, static_cast<int>(part.bytes.size()),
                              &width, &height, &channels, 3),
        stbi_image_free);
    if (!image) {
      memory.check();
      throw failure(AILO_INVALID_ARGUMENT, "Image decoding failed");
    }
    if (self.is_cancelled())
      throw failure(AILO_CANCELLED, "Image decoding cancelled");
    if (part.detail == AILO_MEDIA_DETAIL_LOW && std::max(width, height) > 512) {
      const double scale = 512.0 / std::max(width, height);
      const int nx = std::max(1, static_cast<int>(width * scale));
      const int ny = std::max(1, static_cast<int>(height * scale));
      reserve(static_cast<size_t>(nx) * static_cast<size_t>(ny) * 6);
      std::vector<unsigned char> resized(static_cast<size_t>(nx) *
                                         static_cast<size_t>(ny) * 3);
      // Area averages avoid inventing texture when selecting coarse detail.
      for (int y = 0; y < ny; ++y) {
        if (self.is_cancelled())
          throw failure(AILO_CANCELLED, "Image resizing cancelled");
        const int y0 = static_cast<int>(static_cast<size_t>(y) *
                                        static_cast<size_t>(height) /
                                        static_cast<size_t>(ny));
        const int y1 = static_cast<int>(static_cast<size_t>(y + 1) *
                                        static_cast<size_t>(height) /
                                        static_cast<size_t>(ny));
        for (int x = 0; x < nx; ++x) {
          const int x0 = static_cast<int>(static_cast<size_t>(x) *
                                          static_cast<size_t>(width) /
                                          static_cast<size_t>(nx));
          const int x1 = static_cast<int>(static_cast<size_t>(x + 1) *
                                          static_cast<size_t>(width) /
                                          static_cast<size_t>(nx));
          const unsigned n = static_cast<unsigned>((x1 - x0) * (y1 - y0));
          for (unsigned c = 0; c < 3; ++c) {
            uint64_t sum = 0;
            for (int yy = y0; yy < y1; ++yy)
              for (int xx = x0; xx < x1; ++xx)
                sum += image.get()[(static_cast<size_t>(yy) *
                                        static_cast<size_t>(width) +
                                    static_cast<size_t>(xx)) *
                                       3 +
                                   c];
            resized[(static_cast<size_t>(y) * static_cast<size_t>(nx) +
                     static_cast<size_t>(x)) *
                        3 +
                    c] = static_cast<unsigned char>(sum / n);
          }
        }
      }
      return mtmd_bitmap_init(static_cast<uint32_t>(nx),
                              static_cast<uint32_t>(ny), resized.data());
    }
    reserve(pixels * 3);
    return mtmd_bitmap_init(static_cast<uint32_t>(width),
                            static_cast<uint32_t>(height), image.get());
  }
  const int rate = mtmd_get_audio_sample_rate(self.media->context.get());
  return decode_audio(self, part, rate, reserve);
}
mtmd_bitmap *decode_audio(state &self, const media_part &part, int rate,
                          const std::function<void(size_t)> &reserve) {
  const auto &limits = self.media->config;
  const auto *bytes =
      reinterpret_cast<const unsigned char *>(part.bytes.data());
  if (rate <= 0 || rate > 384000)
    throw failure(AILO_UNSUPPORTED,
                  "Audio encoder has no supported sample rate");
  auto config =
      ma_decoder_config_init(ma_format_f32, 1, static_cast<ma_uint32>(rate));
  codec_memory memory{reserve, 0, 0, {}, false};
  codec_scope scope(memory);
  config.allocationCallbacks.onMalloc = [](size_t n, void *) {
    return codec_malloc(n);
  };
  config.allocationCallbacks.onRealloc = [](void *p, size_t n, void *) {
    return codec_realloc(p, n);
  };
  config.allocationCallbacks.onFree = [](void *p, void *) { codec_free(p); };
  ma_decoder decoder;
  if (ma_decoder_init_memory(bytes, part.bytes.size(), &config, &decoder) !=
      MA_SUCCESS) {
    memory.check();
    throw failure(AILO_INVALID_ARGUMENT,
                  "Audio decoding initialization failed");
  }
  struct cleanup {
    ma_decoder &decoder;
    ~cleanup() { ma_decoder_uninit(&decoder); }
  } guard{decoder};
  ma_uint64 frames = 0;
  if (ma_decoder_get_length_in_pcm_frames(&decoder, &frames) != MA_SUCCESS ||
      !frames) {
    memory.check();
    throw failure(AILO_INVALID_ARGUMENT,
                  "Cannot determine finite decoded audio length");
  }
  const uint64_t maximum =
      uint64_t(limits.max_audio_seconds) * static_cast<unsigned>(rate);
  if (frames > maximum ||
      frames > limits.max_decoded_bytes / (2 * sizeof(float)))
    throw failure(AILO_OUT_OF_RANGE,
                  "Decoded audio exceeds its duration or memory limit");
  reserve(static_cast<size_t>(frames) * sizeof(float) * 2);
  std::vector<float> samples(static_cast<size_t>(frames));
  ma_uint64 offset = 0;
  while (offset < frames) {
    if (self.is_cancelled())
      throw failure(AILO_CANCELLED, "Audio decoding cancelled");
    const auto n = std::min(frames - offset, ma_uint64(4096));
    ma_uint64 got = 0;
    const auto result =
        ma_decoder_read_pcm_frames(&decoder, samples.data() + offset, n, &got);
    if ((result != MA_SUCCESS && result != MA_AT_END) || !got || got > n) {
      memory.check();
      throw failure(AILO_INVALID_ARGUMENT,
                    "Encoded audio is truncated or corrupt");
    }
    offset += got;
  }
  for (float value : samples)
    if (!std::isfinite(value))
      throw failure(AILO_INVALID_ARGUMENT, "Audio contains non-finite samples");
  return mtmd_bitmap_init_from_audio(samples.size(), samples.data());
}
} // namespace client
} // namespace ailo
