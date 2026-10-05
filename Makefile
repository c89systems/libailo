.NOTPARALLEL:
PRESET ?= debug
TARGET ?= x86_64-linux-gnu
DEPENDENCY ?=
.PHONY: help deps build build-debug build-release test test-debug format format-check upstream-inventory finalize-slice valgrind test-scalar-matrix example-smoke-local
help:
	@scripts/help.sh
deps:
	scripts/deps.sh --target "$(TARGET)" --component "$(DEPENDENCY)"
build:
	scripts/build.sh --preset "$(PRESET)"
build-debug:
	$(MAKE) build PRESET=debug
build-release:
	$(MAKE) build PRESET=x86_64-linux-gnu-release
test: build
	ctest --preset $(PRESET) --output-on-failure -j1
test-debug:
	$(MAKE) test PRESET=debug
upstream-inventory:
	python3 scripts/upstream_inventory.py
format:
	scripts/format.sh
format-check:
	scripts/format.sh --check
finalize-slice: format test format-check
valgrind: build
	valgrind --error-exitcode=99 --leak-check=full --show-leak-kinds=definite --errors-for-leak-kinds=definite build/$(PRESET)/ailo_backend_test
	valgrind --error-exitcode=99 --leak-check=full --show-leak-kinds=definite --errors-for-leak-kinds=definite build/$(PRESET)/ailo_c89_policy
	valgrind --error-exitcode=99 --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all build/$(PRESET)/ailo_values_static
	valgrind --error-exitcode=99 --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all build/$(PRESET)/ailo_values_shared
	valgrind --error-exitcode=99 --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all build/$(PRESET)/ailo_integer_bridge
test-scalar-matrix:
	scripts/test_scalar_matrix.sh
example-smoke-local: build
	ctest --preset "$(PRESET)" --output-on-failure -j1 -L example-smoke
