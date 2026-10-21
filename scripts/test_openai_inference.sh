#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH='' cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
preset=${1:-x86_64-linux-gnu-release}
mode=${2:-offline}
backend=${3:-cpu}
selection=${4:-'^trained-serving-'}
[[ $# -le 4 && "$preset" =~ ^(cpu|debug|x86_64-linux-(gnu|musl)-release)$ && "$mode" =~ ^(offline|download)$ && "$backend" =~ ^(cpu|vulkan)$ ]] || {
  printf 'Usage: test_openai_inference.sh [native-preset] [offline|download] [cpu|vulkan] [test-regex]\n' >&2; exit 2;
}
for tool in codex timeout curl jq rg base64 sha256sum; do
  command -v "$tool" >/dev/null || { printf 'Trained serving acceptance requires host tool: %s\n' "$tool" >&2; exit 2; }
done
bin="$root/build/$preset"
for tool in ailo_serving_inference_fixture ailo_serving_stream_client ailo_model_cache_example; do
  [[ -x "$bin/$tool" ]] || { printf 'Build the selected preset first: missing %s\n' "$bin/$tool" >&2; exit 2; }
done
if [[ "$backend" == vulkan ]]; then
  rg -qi '^AILO_VULKAN:BOOL=(ON|TRUE|YES|1)$' "$bin/CMakeCache.txt" || { printf 'Selected preset must enable Vulkan\n' >&2; exit 2; }
fi
trace=0
if [[ "$preset" == *-musl-* ]]; then
  command -v strace >/dev/null && command -v readelf >/dev/null || { printf 'Static serving acceptance needs strace and readelf\n' >&2; exit 2; }
  trace=1
  for tool in ailo_serving_inference_fixture ailo_serving_stream_client; do
    readelf -l "$bin/$tool" > "$bin/$tool.program-headers"
    readelf -d "$bin/$tool" > "$bin/$tool.dynamic"
    if rg -q 'INTERP' "$bin/$tool.program-headers" || rg -q 'NEEDED' "$bin/$tool.dynamic"; then
      printf 'Expected a fully static musl executable: %s\n' "$tool" >&2; exit 1
    fi
  done
fi
acquire() {
  local cache=$1 repo=$2 revision=$3 file=$4 hash=$5 args=()
  [[ "$mode" != download ]] || args+=(--download)
  "$bin/ailo_model_cache_example" --repo "$repo" --revision "$revision" --file "$file" --cache "$root/build/$cache" --sha256 "$hash" "${args[@]}"
}
model=$(acquire inference-models bartowski/Qwen2.5-0.5B-Instruct-GGUF c6f6d6134e86f8c53897f3d892b3a3da0143e8bc Qwen2.5-0.5B-Instruct-Q4_K_M.gguf 6eb923e7d26e9cea28811e1a8e852009b21242fb157b26149d3b188f3a8c8653)
vision=$(acquire media-model-cache ggml-org/SmolVLM-256M-Instruct-GGUF b9e4379657e1450d04d02eec8e345667265b0a00 SmolVLM-256M-Instruct-Q8_0.gguf 2a31195d3769c0b0fd0a4906201666108834848db768af11de1d2cef7cd35e65)
vision_projector=$(acquire media-model-cache ggml-org/SmolVLM-256M-Instruct-GGUF b9e4379657e1450d04d02eec8e345667265b0a00 mmproj-SmolVLM-256M-Instruct-Q8_0.gguf 7e943f7c53f0382a6fc41b6ee0c2def63ba4fded9ab8ed039cc9e2ab905e0edd)
audio=$(acquire media-model-cache ggml-org/Qwen3-ASR-0.6B-GGUF 928ab958557df9aa2ef1c93e0e83c7ad0933fae2 Qwen3-ASR-0.6B-Q8_0.gguf bca259818b50ca7c4c05e9bdb35a5dc04fa039653a6d6f3f0f331f96f6aa1971)
audio_projector=$(acquire media-model-cache ggml-org/Qwen3-ASR-0.6B-GGUF 928ab958557df9aa2ef1c93e0e83c7ad0933fae2 mmproj-Qwen3-ASR-0.6B-Q8_0.gguf 41a342b5e4c514e968cb756de6cd1b7be39eff43c44c57a2ef5fc6522e36603d)
speech="$root/build/media-fixtures/jfk.wav"
if [[ ! -e "$speech" && "$mode" == download ]]; then
  mkdir -p "$(dirname -- "$speech")"
  trap 'cmake -E rm -f "$speech.partial"' EXIT
  curl --fail --silent --show-error --location --proto '=https' --max-time 30 'https://raw.githubusercontent.com/ggml-org/whisper.cpp/b0a11594aec50892a02cd8d129eee2dfe93a8bb8/samples/jfk.wav' -o "$speech.partial"
  printf '%s  %s\n' 59dfb9a4acb36fe2a2affc14bacbee2920ff435cb13cc314a08c13f66ba7860e "$speech.partial" | sha256sum --check --status
  mv -- "$speech.partial" "$speech"
  trap - EXIT
fi
[[ -f "$speech" ]] || { printf 'Missing speech fixture; explicitly select download once\n' >&2; exit 2; }
printf '%s  %s\n' 59dfb9a4acb36fe2a2affc14bacbee2920ff435cb13cc314a08c13f66ba7860e "$speech" | sha256sum --check --status
suite="$root/build/serving-inference-$preset-$backend"
cmake -S "$root/tests/serving-inference" -B "$suite" \
  -DAILO_FIXTURE="$bin/ailo_serving_inference_fixture" -DAILO_MODEL="$model" -DAILO_EVIDENCE="$bin" \
  -DAILO_BACKEND="$backend" -DAILO_TRACE="$trace" -DAILO_VISION_MODEL="$vision" -DAILO_VISION_PROJECTOR="$vision_projector" \
  -DAILO_AUDIO_MODEL="$audio" -DAILO_AUDIO_PROJECTOR="$audio_projector" -DAILO_AUDIO_SAMPLE="$speech"
ctest --test-dir "$suite" --output-on-failure --stop-on-failure --no-tests=error -j1 -R "$selection"
