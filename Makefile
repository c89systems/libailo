.NOTPARALLEL:
PRESET ?= debug
TARGET ?= x86_64-linux-gnu
DEPENDENCY ?=
.PHONY: help clean test-foundation-matrix deps build build-debug build-release test test-debug format format-check upstream-inventory finalize-slice valgrind valgrind-model valgrind-acquisition test-scalar-matrix example-smoke-local generate-native facade-coverage inference-smoke-vulkan test-install-tree print-release-version package-source package-source-smoke test-source-archive package package-checksums package-verify release-matrix
help:
	@scripts/help.sh
clean:
	cmake -E rm -rf build dist .cache
test-foundation-matrix:
	scripts/test_foundation_matrix.sh
print-release-version:
	@scripts/release_version.sh --override "$(AILO_VERSION_OVERRIDE)"
package-source:
	scripts/package_source.sh
package-source-smoke:
	scripts/test_release_from_source.sh "$(SOURCE_ARCHIVE)"
test-source-archive:
	scripts/test_source_archive.sh
package:
	scripts/package.sh --preset "$(PRESET)"
package-checksums:
	scripts/package_checksums.sh "$(ARTIFACT_DIRECTORY)" "$(ARTIFACT_VERSION)" "$(ARTIFACT_LIST)"
package-verify:
	scripts/verify_release_artifacts.sh "$(CHECKSUM_MANIFEST)"
release-matrix:
	scripts/run_linux_release_matrix.sh
deps:
	scripts/deps.sh --target "$(TARGET)" --component "$(DEPENDENCY)"
.PHONY: gpu-deps test-gpu-deps
gpu-deps:
	scripts/build-gpu-deps.sh --target "$(TARGET)"
test-gpu-deps: gpu-deps
	cmake -DAILO_TARGET_ID="$(TARGET)" -P tests/test_gpu_deps.cmake
build:
	scripts/build.sh --preset "$(PRESET)"
build-debug:
	$(MAKE) build PRESET=debug
build-release:
	$(MAKE) build PRESET=x86_64-linux-gnu-release
test: build
	ctest --preset $(PRESET) --output-on-failure -j1
	case "$(PRESET)" in cpu|debug|x86_64-linux-gnu-release) scripts/lua.sh test "$(PRESET)" ;; esac
test-debug:
	$(MAKE) test PRESET=debug
test-install-tree: build
	scripts/test_install_tree.sh "$(PRESET)"
upstream-inventory:
	python3 scripts/upstream_inventory.py
format:
	scripts/format.sh
format-check:
	scripts/format.sh --check
finalize-slice: format test format-check
inference-smoke-vulkan: build
	scripts/test_model_vulkan.sh "$(PRESET)"
valgrind-model:
	scripts/test_model_memory.sh
valgrind-acquisition: build
	scripts/test_model_acquisition_memory.sh "$(PRESET)"
valgrind: build
	valgrind --error-exitcode=99 --leak-check=full --show-leak-kinds=definite --errors-for-leak-kinds=definite build/$(PRESET)/ailo_backend_test
	valgrind --error-exitcode=99 --leak-check=full --show-leak-kinds=definite --errors-for-leak-kinds=definite build/$(PRESET)/ailo_c89_policy
	valgrind --error-exitcode=99 --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all build/$(PRESET)/ailo_values_static
	valgrind --error-exitcode=99 --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all build/$(PRESET)/ailo_values_shared
	valgrind --error-exitcode=99 --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all build/$(PRESET)/ailo_integer_bridge
	valgrind --error-exitcode=99 --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all build/$(PRESET)/ailo_native_records
	valgrind --fair-sched=yes --suppressions=scripts/valgrind-libffi.supp --error-exitcode=99 --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all build/$(PRESET)/ailo_native_callbacks
	valgrind --error-exitcode=99 --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all build/$(PRESET)/ailo_native_storage
	valgrind --error-exitcode=99 --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all build/$(PRESET)/ailo_native_test
	valgrind --error-exitcode=99 --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all build/$(PRESET)/ailo_variadic_test
	valgrind --error-exitcode=99 --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all build/$(PRESET)/ailo_variadic_parity
	valgrind --error-exitcode=99 --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all build/$(PRESET)/ailo_macros_test
	valgrind --error-exitcode=99 --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all build/$(PRESET)/ailo_macros_parity
	scripts/test_model_acquisition_memory.sh "$(PRESET)"
	$(MAKE) valgrind-model
test-scalar-matrix:
	scripts/test_scalar_matrix.sh
example-smoke-local: build
	ctest --preset "$(PRESET)" --output-on-failure -j1 -L example-smoke
generate-native:
	python3 scripts/generate_native.py
facade-coverage:
	python3 scripts/generate_native.py --check --require-complete
.PHONY: lua-build lua-test package-lua
lua-build:
	scripts/lua.sh build "$(PRESET)"
lua-test:
	scripts/lua.sh test "$(PRESET)"
package-lua:
	scripts/package_lua.sh
