#!/usr/bin/env bash
set -euo pipefail
cat <<'HELP'
deps [DEPENDENCY=name] [TARGET=id]  Acquire verified pinned components.
build [PRESET=name]                Build through serialized Bash/CMake drivers.
                                  PRESET=library excludes Python test/tool checks.
build-debug                       Build default Vulkan+CPU configuration.
build-release                     Build native release configuration.
test [PRESET=name]                 Run deterministic CTest checks.
test-debug                        Run debug checks.
test-install-tree                 Verify relocated CMake/pkg-config C/C++ consumers.
test-foundation-matrix            Verify cpkt/core and misc libffi on six Linux targets.
clean                             Remove build/, dist/ and local .cache/; preserve global cache.
print-release-version             Read exact lightweight tag/candidate/archive version.
package-source                   Require an exact release tag and complete facade, then archive.
test-source-archive               Check development source archive staging/rejection/determinism.
package-source-smoke SOURCE_ARCHIVE=path
                                 Reconstruct a source archive without VCS state (CPU build).
package [PRESET=name]             Require exact tag/full facade, then stage a stripped SDK.
package-checksums ARTIFACT_DIRECTORY=path ARTIFACT_VERSION=X.Y.Z ARTIFACT_LIST=path
                                 Write the sole checksum manifest from explicit artifact names.
package-verify CHECKSUM_MANIFEST=path
                                 Verify every checksum-listed source/SDK archive.
release-matrix                   Require exact tag/full facade; build/verify six Linux SDKs serially.
valgrind                          Check native executables (requires Valgrind).
valgrind-model                    Check static/shared GGUF inference in the native CPU build.
valgrind-acquisition              Check C89 model cache and TLS transport ownership.
inference-smoke-vulkan            Require GGUF decode on a real Vulkan device.
test-scalar-matrix                 Run scalar/bridge checks on six Linux targets.
                                  Cross execution requires qemu-arm/aarch64.
example-smoke-local                Build and exercise local C89 examples.
upstream-inventory                Compare pinned public compiler AST declarations.
generate-native                   Regenerate tracked bindings from the AST inventory.
facade-coverage                   Require zero binding gaps (currently incomplete).
format                            Format project C/C++ sources.
format-check                      Assert project formatting.
finalize-slice                    Format and run deterministic checks.
HELP
