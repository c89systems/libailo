#include "ggml.h"
#include "gguf.h"
#include "llama.h"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

/* A complete, untrained one-layer Llama model. No source data, model downloads
 * or Python conversion are needed. Values are fixed across builds and hosts. */
static void tensor(ggml_context *ctx, gguf_context *file, const char *name,
                   int64_t width, int64_t height, bool norm = false,
                   bool embedding = false, bool output = false) {
  ggml_tensor *value =
      height ? ggml_new_tensor_2d(ctx, GGML_TYPE_F32, width, height)
             : ggml_new_tensor_1d(ctx, GGML_TYPE_F32, width);
  ggml_set_name(value, name);
  auto *data = static_cast<float *>(value->data);
  for (int64_t i = 0; i < ggml_nelements(value); ++i) {
    const float perturbation =
        static_cast<float>((i * 17 + 13) % 23 - 11) / 4096;
    data[i] = norm ? 1.0f : embedding ? 0.125f + perturbation : perturbation;
    if (output) {
      /* Token 100 is the byte token for 'a'. Keep a stable text output while
       * evaluating the full attention/feed-forward graph and mutable KV cache.
       */
      data[i] = i / width == 100 ? 0.25f : -0.015625f + perturbation;
    }
  }
  gguf_add_tensor(file, value);
}

static void build(const char *path) {
  std::unique_ptr<ggml_context, decltype(&ggml_free)> ctx(
      ggml_init({1024 * 1024, nullptr, false}), ggml_free);
  std::unique_ptr<gguf_context, decltype(&gguf_free)> file(gguf_init_empty(),
                                                           gguf_free);
  if (!ctx || !file)
    throw std::runtime_error("Cannot allocate GGUF fixture");
  gguf_set_val_str(file.get(), "general.architecture", "llama");
  gguf_set_val_str(file.get(), "general.name", "ailo-deterministic-test");
  gguf_set_val_u32(file.get(), "general.file_type", 0);
  gguf_set_val_u32(file.get(), "llama.context_length", 128);
  gguf_set_val_u32(file.get(), "llama.embedding_length", 32);
  gguf_set_val_u32(file.get(), "llama.block_count", 1);
  gguf_set_val_u32(file.get(), "llama.feed_forward_length", 64);
  gguf_set_val_u32(file.get(), "llama.attention.head_count", 2);
  gguf_set_val_u32(file.get(), "llama.attention.head_count_kv", 2);
  gguf_set_val_u32(file.get(), "llama.rope.dimension_count", 16);
  gguf_set_val_f32(file.get(), "llama.attention.layer_norm_rms_epsilon",
                   1.0e-5f);
  gguf_set_val_str(file.get(), "tokenizer.ggml.model", "llama");
  gguf_set_val_str(file.get(), "tokenizer.chat_template", "chatml");
  gguf_set_val_u32(file.get(), "tokenizer.ggml.unknown_token_id", 0);
  gguf_set_val_u32(file.get(), "tokenizer.ggml.bos_token_id", 1);
  gguf_set_val_u32(file.get(), "tokenizer.ggml.eos_token_id", 2);
  gguf_set_val_bool(file.get(), "tokenizer.ggml.add_bos_token", true);
  gguf_set_val_bool(file.get(), "tokenizer.ggml.add_eos_token", false);
  std::vector<std::string> strings = {"<unk>", "<s>", "</s>"};
  for (int i = 0; i < 256; ++i) {
    char byte[7];
    std::snprintf(byte, sizeof(byte), "<0x%02X>", i);
    strings.emplace_back(byte);
  }
  strings.emplace_back("\xe2\x96\x81");
  std::vector<const char *> tokens;
  for (const auto &s : strings)
    tokens.push_back(s.c_str());
  std::vector<float> scores(tokens.size(), -1.0f);
  std::vector<int32_t> types(tokens.size(), LLAMA_TOKEN_TYPE_BYTE);
  types[0] = LLAMA_TOKEN_TYPE_UNKNOWN;
  types[1] = types[2] = LLAMA_TOKEN_TYPE_CONTROL;
  types.back() = LLAMA_TOKEN_TYPE_NORMAL;
  gguf_set_arr_str(file.get(), "tokenizer.ggml.tokens", tokens.data(),
                   tokens.size());
  gguf_set_arr_data(file.get(), "tokenizer.ggml.scores", GGUF_TYPE_FLOAT32,
                    scores.data(), scores.size());
  gguf_set_arr_data(file.get(), "tokenizer.ggml.token_type", GGUF_TYPE_INT32,
                    types.data(), types.size());

  tensor(ctx.get(), file.get(), "token_embd.weight", 32, tokens.size(), false,
         true);
  tensor(ctx.get(), file.get(), "output.weight", 32, tokens.size(), false,
         false, true);
  tensor(ctx.get(), file.get(), "output_norm.weight", 32, 0, true);
  tensor(ctx.get(), file.get(), "blk.0.attn_norm.weight", 32, 0, true);
  tensor(ctx.get(), file.get(), "blk.0.attn_q.weight", 32, 32);
  tensor(ctx.get(), file.get(), "blk.0.attn_k.weight", 32, 32);
  tensor(ctx.get(), file.get(), "blk.0.attn_v.weight", 32, 32);
  tensor(ctx.get(), file.get(), "blk.0.attn_output.weight", 32, 32);
  tensor(ctx.get(), file.get(), "blk.0.ffn_norm.weight", 32, 0, true);
  tensor(ctx.get(), file.get(), "blk.0.ffn_gate.weight", 32, 64);
  tensor(ctx.get(), file.get(), "blk.0.ffn_up.weight", 32, 64);
  tensor(ctx.get(), file.get(), "blk.0.ffn_down.weight", 64, 32);
  if (!gguf_write_to_file(file.get(), path, false))
    throw std::runtime_error("Cannot write GGUF fixture");
  for (unsigned shard = 0; shard < 2; ++shard) {
    std::unique_ptr<gguf_context, decltype(&gguf_free)> split(gguf_init_empty(),
                                                              gguf_free);
    if (!split)
      throw std::bad_alloc();
    gguf_set_kv(split.get(), file.get());
    gguf_set_val_u16(split.get(), "split.no", static_cast<uint16_t>(shard));
    gguf_set_val_u16(split.get(), "split.count", 2);
    gguf_set_val_i32(split.get(), "split.tensors.count", 12);
    unsigned index = 0;
    for (ggml_tensor *t = ggml_get_first_tensor(ctx.get()); t;
         t = ggml_get_next_tensor(ctx.get(), t), ++index)
      if (index % 2 == shard)
        gguf_add_tensor(split.get(), t);
    const std::string split_path =
        std::string(path) +
        (shard ? "-00002-of-00002.gguf" : "-00001-of-00002.gguf");
    if (!gguf_write_to_file(split.get(), split_path.c_str(), false))
      throw std::runtime_error("Cannot write split GGUF fixture");
  }
  gguf_set_val_str(file.get(), "general.architecture",
                   "ailo-unknown-test-arch");
  if (!gguf_write_to_file(file.get(),
                          (std::string(path) + ".unsupported").c_str(), false))
    throw std::runtime_error("Cannot write unsupported-model fixture");
  auto close_file = [](FILE *f) { std::fclose(f); };
  std::unique_ptr<FILE, decltype(close_file)> truncated(
      std::fopen((std::string(path) + ".truncated").c_str(), "wb"), close_file);
  if (!truncated || std::fwrite("GGUF", 1, 4, truncated.get()) != 4)
    throw std::runtime_error("Cannot write truncated-model fixture");
}

int main(int argc, char **argv) {
  if (argc != 2) {
    std::fputs("Usage: ailo_model_fixture output.gguf\n", stderr);
    return 2;
  }
  try {
    build(argv[1]);
    std::puts("Deterministic one-layer GGUF fixture written");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
