#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH='' cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
preset=${1:-x86_64-linux-gnu-release}
mode=${2:-offline}
[[ $# -le 2 && "$preset" =~ ^(cpu|debug|x86_64-linux-gnu-release)$ && "$mode" =~ ^(offline|download)$ ]] || {
  printf 'Usage: test_openai_inference.sh [native-preset] [offline|download]\n' >&2; exit 2;
}
for tool in codex timeout curl jq rg; do
  command -v "$tool" >/dev/null || { printf 'Trained serving acceptance requires host tool: %s\n' "$tool" >&2; exit 2; }
done
bin="$root/build/$preset"
for tool in ailo_serving_inference_fixture ailo_serving_stream_client ailo_model_cache_example; do
  [[ -x "$bin/$tool" ]] || { printf 'Build the selected preset first: missing %s\n' "$bin/$tool" >&2; exit 2; }
done
acquire=(--repo bartowski/Qwen2.5-0.5B-Instruct-GGUF --revision c6f6d6134e86f8c53897f3d892b3a3da0143e8bc --file Qwen2.5-0.5B-Instruct-Q4_K_M.gguf --cache "$root/build/inference-models" --sha256 6eb923e7d26e9cea28811e1a8e852009b21242fb157b26149d3b188f3a8c8653)
[[ "$mode" != download ]] || acquire+=(--download)
model=$("$bin/ailo_model_cache_example" "${acquire[@]}")
cmake -S "$root/tests/serving-inference" -B "$root/build/serving-inference-$preset" \
  -DAILO_FIXTURE="$bin/ailo_serving_inference_fixture" -DAILO_MODEL="$model" -DAILO_EVIDENCE="$bin"
ctest --test-dir "$root/build/serving-inference-$preset" --output-on-failure --stop-on-failure --no-tests=error -j1
