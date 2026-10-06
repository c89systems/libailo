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
valgrind                          Check native executables (requires Valgrind).
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
