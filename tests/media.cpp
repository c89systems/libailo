#include "ailo_compaction_internal.hpp"
#include "ailo_media_internal.hpp"
#include "ailo_openai_internal.hpp"
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

using namespace ailo::client;
#define CHECK(c)                                                               \
  do {                                                                         \
    if (!(c))                                                                  \
      throw std::runtime_error(std::string(#c) + ":" +                         \
                               std::to_string(__LINE__));                      \
  } while (0)
template <class Work> void reject(ailo_status status, Work work) {
  try {
    work();
  } catch (const failure &e) {
    CHECK(e.status == status);
    return;
  }
  throw std::runtime_error("Expected media rejection");
}
struct resolver_fixture {
  std::string bytes;
  state *owner = nullptr;
  bool cancel = false, oversize = false;
  unsigned calls = 0;
  static ailo_status resolve(void *p, ailo_media_reference kind,
                             const char *ref, size_t maximum, long timeout,
                             ailo_media_sink sink, void *user, ailo_error *) {
    auto &s = *static_cast<resolver_fixture *>(p);
    CHECK(kind == AILO_MEDIA_URL || kind == AILO_MEDIA_FILE_ID);
    CHECK(ref && *ref && maximum && timeout > 0);
    ++s.calls;
    if (s.cancel)
      s.owner->cancelled.store(true);
    if (s.oversize) {
      std::string large(maximum + 1, 'x');
      (void)sink(user, large.data(), large.size());
    } else {
      for (const auto &ch : s.bytes)
        if (sink(user, &ch, 1) <= 0)
          break;
    }
    // Intentionally ignore sink rejection: libailo must still fail.
    return AILO_OK;
  }
};
int main(int argc, char **argv) {
  try {
    CHECK(argc == 3);
    std::string transcript_text;
    uint64_t first_origin = UINT64_MAX;
    audio_text_stream speech([&](const char *p, size_t n, uint64_t at) {
      if (transcript_text.empty())
        first_origin = at;
      transcript_text.append(p, n);
    });
    const std::string speech_header = "language English<asr_text>";
    for (size_t i = 0; i < speech_header.size(); ++i) {
      speech.push(speech_header.data() + i, 1, i);
      CHECK(transcript_text.empty());
    }
    speech.push("Hello", 5, speech_header.size());
    CHECK(transcript_text == "Hello" && first_origin == speech_header.size());
    speech.push(" world", 6, speech_header.size() + 5);
    CHECK(transcript_text == "Hello world");
    speech.finish(false);
    transcript_text.clear();
    audio_text_stream direct([&](const char *p, size_t n, uint64_t) {
      CHECK(n != 1);
      transcript_text.append(p, n);
    });
    direct.push("\xe4\xbd\xa0", 3, 0);
    direct.finish(false);
    CHECK(transcript_text == "\xe4\xbd\xa0");
    audio_text_stream bad_header([](const char *, size_t, uint64_t) {});
    bad_header.push("language English", 16, 0);
    reject(AILO_MODEL_FAILED, [&] { bad_header.finish(false); });
    bad_header.finish(true);
    auto fixture = [&](const char *name) {
      std::ifstream input(std::string(argv[2]) + "/" + name, std::ios::binary);
      CHECK(bool(input));
      return std::string(std::istreambuf_iterator<char>(input),
                         std::istreambuf_iterator<char>());
    };
    const std::string png =
        media_unbase64("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR"
                       "42mP8/x8AAwMCAO+/l1sAAAAASUVORK5CYII=",
                       1024);
    CHECK(media_format(png.data(), png.size(), AILO_CONTENT_IMAGE) == "png");
    const auto base64 = media_base64(png.data(), png.size());
    for (size_t width :
         {size_t(1), size_t(2), size_t(3), size_t(7), size_t(4096)}) {
      media_base64_decoder decoder(png.size());
      for (size_t offset = 0; offset < base64.size(); offset += width)
        decoder.push(base64.data() + offset,
                     std::min(width, base64.size() - offset));
      CHECK(decoder.finish() == png);
    }
    for (const auto &bad :
         {"", "A", "AAA", "====", "AA=A", "AB==", "AAAA\n", "AA==AAAA"})
      reject(AILO_INVALID_ARGUMENT, [&] { (void)media_unbase64(bad, 1024); });
    reject(AILO_OUT_OF_RANGE, [&] { (void)media_unbase64(base64, 1); });
    auto config = ailo_client_default_config();
    config.source.local_path = argv[1];
    config.lazy = 1;
    config.backend = AILO_BACKEND_CPU;
    ailo_multimodal_config media{};
    media.projector.local_path = argv[1];
    media.backend = AILO_BACKEND_CPU;
    config.multimodal = &media;
    state owner(config, nullptr);
    CHECK(owner.media->config.max_parts == 16 &&
          owner.media->config.resolver_timeout_ms == 15000);
    ailo_media_metadata metadata{};
    metadata.detail = AILO_MEDIA_DETAIL_LOW;
    ailo_content contents[] = {
        {AILO_CONTENT_TEXT, "literal <__media__>", 19, nullptr},
        {AILO_CONTENT_IMAGE, png.data(), png.size(), &metadata},
        {AILO_CONTENT_TEXT, "after", 5, nullptr}};
    ailo_item item{};
    item.content = contents;
    item.content_count = 3;
    auto request = ailo_request_default();
    request.items = &item;
    request.item_count = 1;
    auto resolved = resolve_media(owner, request);
    auto views = resolved->views();
    request.items = views.data();
    const auto input = prepare_conversation(request);
    CHECK(input.media.size() == 1 &&
          input.media[0]->detail == AILO_MEDIA_DETAIL_LOW);
    CHECK(input.messages[0].content.find("literal <__media__>") == 0);
    CHECK(input.messages[0].content.find(input.media[0]->marker) !=
          std::string::npos);
    const auto before = fingerprint(views[0]);
    metadata.detail = AILO_MEDIA_DETAIL_HIGH;
    CHECK(views[0].content[1].media->detail == AILO_MEDIA_DETAIL_LOW);
    CHECK(fingerprint(views[0]) == before);
    auto changed = views[0];
    auto changed_parts = std::vector<ailo_content>(contents, contents + 3);
    changed.content = changed_parts.data();
    CHECK(fingerprint(changed) != before);
    size_t reserved = 0;
    std::unique_ptr<mtmd_bitmap, decltype(&mtmd_bitmap_free)> bitmap(
        decode_media(owner, *input.media[0], [&](size_t n) { reserved += n; }),
        mtmd_bitmap_free);
    CHECK(bitmap && mtmd_bitmap_get_nx(bitmap.get()) == 1 && reserved >= 6);
    reject(AILO_OUT_OF_RANGE, [&] {
      decode_media(owner, *input.media[0], [](size_t) {
        throw failure(AILO_OUT_OF_RANGE, "fixture allocation budget");
      });
    });
    auto expansion = *input.media[0];
    expansion.bytes = fixture("inflate.png");
    size_t charged = 0;
    reject(AILO_OUT_OF_RANGE, [&] {
      decode_media(owner, expansion, [&](size_t n) {
        if (n > 65536 - charged)
          throw failure(AILO_OUT_OF_RANGE, "fixture allocation budget");
        charged += n;
      });
    });
    CHECK(charged <= 65536);
    auto animated = *input.media[0];
    animated.bytes = fixture("animated.png");
    reject(AILO_UNSUPPORTED,
           [&] { decode_media(owner, animated, [](size_t) {}); });
    auto jpeg = *input.media[0];
    jpeg.bytes = fixture("red.jpg");
    std::unique_ptr<mtmd_bitmap, decltype(&mtmd_bitmap_free)> rgb(
        decode_media(owner, jpeg, [](size_t) {}), mtmd_bitmap_free);
    CHECK(rgb && mtmd_bitmap_get_nx(rgb.get()) == 32);
    const auto *pixel = mtmd_bitmap_get_data(rgb.get());
    CHECK(pixel[0] > 240 && pixel[1] < 16 && pixel[2] < 16);
    jpeg.bytes.resize(jpeg.bytes.size() - 2);
    reject(AILO_INVALID_ARGUMENT,
           [&] { decode_media(owner, jpeg, [](size_t) {}); });
    media_part audio_part{AILO_CONTENT_AUDIO, fixture("tone.wav"), "",
                          AILO_MEDIA_DETAIL_AUTO};
    std::unique_ptr<mtmd_bitmap, decltype(&mtmd_bitmap_free)> pcm(
        decode_audio(owner, audio_part, 16000, [](size_t) {}),
        mtmd_bitmap_free);
    CHECK(pcm && mtmd_bitmap_is_audio(pcm.get()));
    reject(AILO_OUT_OF_RANGE, [&] {
      decode_audio(owner, audio_part, 16000, [](size_t) {
        throw failure(AILO_OUT_OF_RANGE, "fixture audio allocation budget");
      });
    });
    for (const char *name : {"tone.mp3", "tone.flac", "stereo.wav"}) {
      auto encoded = audio_part;
      encoded.bytes = fixture(name);
      std::unique_ptr<mtmd_bitmap, decltype(&mtmd_bitmap_free)> decoded(
          decode_audio(owner, encoded, 16000, [](size_t) {}), mtmd_bitmap_free);
      CHECK(decoded && mtmd_bitmap_is_audio(decoded.get()));
    }
    owner.media->config.max_audio_seconds = 1;
    reject(AILO_OUT_OF_RANGE,
           [&] { decode_audio(owner, audio_part, 16000, [](size_t) {}); });
    owner.media->config.max_audio_seconds = 60;
    audio_part.bytes = fixture("nan.wav");
    reject(AILO_INVALID_ARGUMENT,
           [&] { decode_audio(owner, audio_part, 16000, [](size_t) {}); });
    auto broken = *input.media[0];
    broken.bytes.resize(9);
    reject(AILO_INVALID_ARGUMENT,
           [&] { decode_media(owner, broken, [](size_t) {}); });
    const std::string url = "data:image/png;base64," + base64;
    const std::string image = "{\"type\":\"input_image\",\"image_url\":\"" +
                              url + "\",\"detail\":\"low\"}";
    const std::string response =
        "{\"input\":[{\"role\":\"user\",\"content\":[" + image + "]}]}";
    openai_request r(openai_protocol::responses, response.data(),
                     response.size(), "local-model", nullptr, true);
    CHECK(r.items[0].content[0].kind == AILO_CONTENT_IMAGE &&
          r.items[0].content[0].size == png.size());
    CHECK(!std::memcmp(r.items[0].content[0].data, png.data(), png.size()));
    CHECK(r.wire_items.size() == 1 &&
          r.wire_items[0].find(url) != std::string::npos);
    std::string canonical = "{\"input\":[";
    wire_json writer([&](const void *p, size_t n) {
      canonical.append(static_cast<const char *>(p), n);
    });
    api_canonical_item(writer, r.items[0]);
    writer.finish();
    canonical += "]}";
    openai_request replay(openai_protocol::responses, canonical.data(),
                          canonical.size(), "local-model", "input");
    CHECK(fingerprint(r.items[0]) == fingerprint(replay.items[0]));
    const auto retained = canonical.substr(0, canonical.size() - 1) +
                          ",\"output\":[{\"role\":\"assistant\",\"content\":[{"
                          "\"type\":\"output_text\",\"text\":\"red\"}]}]}";
    openai_request selected(openai_protocol::responses, retained.data(),
                            retained.size(), "local-model", "output");
    CHECK(selected.items.size() == 1 &&
          selected.items[0].role == AILO_ROLE_ASSISTANT);
    openai_request header(openai_protocol::responses, retained.data(),
                          retained.size(), "local-model", "__header__");
    CHECK(header.items.empty());
    const std::string chat =
        "{\"model\":\"local-model\",\"messages\":[{\"role\":\"user\","
        "\"content\":[{\"type\":\"image_url\",\"image_url\":{\"url\":\"" +
        url + "\",\"detail\":\"low\"}}]}]}";
    openai_request c(openai_protocol::chat, chat.data(), chat.size(),
                     "local-model");
    CHECK(c.items[0].content[0].size == png.size());
    const std::string audio =
        R"({"model":"local-model","messages":[{"role":"user","content":[{"type":"input_audio","input_audio":{"data":"UklGRgAAAABXQVZF","format":"wav"}}]}]})";
    openai_request a(openai_protocol::chat, audio.data(), audio.size(),
                     "local-model");
    CHECK(a.items[0].content[0].kind == AILO_CONTENT_AUDIO &&
          a.items[0].content[0].size == 12);
    const auto mp3 = fixture("tone.mp3");
    const std::string mp3_chat = "{\"model\":\"local-model\",\"messages\":[{"
                                 "\"role\":\"user\",\"content\":[{"
                                 "\"type\":\"input_audio\",\"input_audio\":{"
                                 "\"format\":\"mp3\",\"data\":\"" +
                                 media_base64(mp3.data(), mp3.size()) +
                                 "\"}}]}]}";
    openai_request compressed(openai_protocol::chat, mp3_chat.data(),
                              mp3_chat.size(), "local-model");
    CHECK(compressed.items[0].content[0].size == mp3.size() &&
          !std::strcmp(compressed.items[0].content[0].media->format, "mp3") &&
          !std::memcmp(compressed.items[0].content[0].data, mp3.data(),
                       mp3.size()));
    const std::string bad_response =
        R"({"input":[{"role":"user","content":[{"type":"input_audio","input_audio":{"data":"UklGRgAAAABXQVZF","format":"wav"}}]}]})";
    reject(AILO_UNSUPPORTED, [&] {
      openai_request bad(openai_protocol::responses, bad_response.data(),
                         bad_response.size(), "local-model");
    });
    const std::string external =
        R"({"input":[{"role":"user","content":[{"type":"input_image","file_id":"file-one"}]}]})";
    openai_request reference(openai_protocol::responses, external.data(),
                             external.size(), "local-model");
    reject(AILO_UNSUPPORTED, [&] { resolve_media(owner, reference); });
    resolver_fixture resolver{png, &owner};
    owner.media->config.resolver = resolver_fixture::resolve;
    owner.media->config.user_data = &resolver;
    resolve_media(owner, reference);
    CHECK(resolver.calls == 1 &&
          reference.items[0].content[0].size == png.size());
    resolver.oversize = true;
    openai_request too_large(openai_protocol::responses, external.data(),
                             external.size(), "local-model");
    owner.media->config.max_part_bytes = 16;
    reject(AILO_OUT_OF_RANGE, [&] { resolve_media(owner, too_large); });
    resolver.oversize = false;
    resolver.cancel = true;
    reject(AILO_CANCELLED, [&] { resolve_media(owner, too_large); });
    owner.cancelled.store(false);
    resolver.cancel = false;
    owner.media->config.max_part_bytes = 1024;
    resolve_media(owner, too_large);
    CHECK(too_large.items[0].content[0].size == png.size());
    config.multimodal = nullptr;
    state text_only(config, nullptr);
    reject(AILO_UNSUPPORTED, [&] { resolve_media(text_only, request); });
    media.max_parts = 4097;
    reject(AILO_OUT_OF_RANGE, [&] { media_state invalid(media); });
    config.multimodal = &media;
    media.max_parts = 0;
    config.lazy = 0;
    ailo_client *invalid = nullptr;
    ailo_error error{};
    const std::string quiet_path = std::string(argv[2]) + "/quiet.log";
    struct capture {
      int out = dup(STDOUT_FILENO), err = dup(STDERR_FILENO), file;
      explicit capture(const char *path)
          : file(open(path, O_CREAT | O_TRUNC | O_RDWR | O_CLOEXEC, 0600)) {
        CHECK(out >= 0 && err >= 0 && file >= 0);
        std::fflush(stdout);
        std::fflush(stderr);
        CHECK(dup2(file, STDOUT_FILENO) >= 0 && dup2(file, STDERR_FILENO) >= 0);
      }
      ~capture() {
        std::fflush(stdout);
        std::fflush(stderr);
        (void)dup2(out, STDOUT_FILENO);
        (void)dup2(err, STDERR_FILENO);
        close(out);
        close(err);
        close(file);
      }
    };
    {
      capture quiet(quiet_path.c_str());
      CHECK(ailo_client_new(&config, &invalid, &error) == AILO_MODEL_FAILED &&
            !invalid);
      struct stat captured{};
      std::fflush(stdout);
      std::fflush(stderr);
      CHECK(fstat(quiet.file, &captured) == 0 && captured.st_size == 0);
    }
    std::puts("Media parsing, decoding, ownership, limits, resolver "
              "cancellation and projector rejection passed");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
