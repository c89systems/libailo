#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
preset=${1:-x86_64-linux-gnu-release}
backend=${2:-cpu}
mode=${3:-offline}
[[ $# -le 3 && "$preset" =~ ^[a-zA-Z0-9_-]+$ && "$backend" =~ ^(cpu|vulkan)$ && "$mode" =~ ^(offline|download)$ ]] || {
  printf 'Usage: test_media_inference.sh [built-preset] [cpu|vulkan] [offline|download]\n' >&2; exit 2;
}
binary="$root/build/$preset/ailo_media_inference_test"
[[ -x "$binary" ]] || { printf 'media: build the selected preset first\n' >&2; exit 2; }
work="$root/build/media-$preset-$backend"
mkdir -p "$work/fixtures" "$root/build/media-fixtures"
python3 "$root/tests/media_fixture.py" "$work/fixtures"
speech="$root/build/media-fixtures/jfk.wav"
speech_sha=59dfb9a4acb36fe2a2affc14bacbee2920ff435cb13cc314a08c13f66ba7860e
if [[ ! -e "$speech" ]]; then
  [[ "$mode" == download ]] || { printf 'media: speech fixture is absent; explicitly select download once\n' >&2; exit 2; }
  trap 'cmake -E rm -f "$speech.partial"' EXIT
  curl --fail --silent --show-error --location --proto '=https' --max-time 30 \
    'https://raw.githubusercontent.com/ggml-org/whisper.cpp/b0a11594aec50892a02cd8d129eee2dfe93a8bb8/samples/jfk.wav' -o "$speech.partial"
  printf '%s  %s\n' "$speech_sha" "$speech.partial" | sha256sum --check --status
  mv -- "$speech.partial" "$speech"
fi
printf '%s  %s\n' "$speech_sha" "$speech" | sha256sum --check --status
"$binary" vision "$root/build/media-model-cache" "$mode" "$backend" "$work/fixtures/red.png" "$work/vision"
"$binary" audio "$root/build/media-model-cache" "$mode" "$backend" "$speech" "$work/audio"
