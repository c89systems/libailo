.NOTPARALLEL:
PRESET ?= debug
TARGET ?= x86_64-linux-gnu
DEPENDENCY ?=
.PHONY: help deps build build-debug build-release test test-debug format format-check upstream-inventory finalize-slice valgrind
help:
	@printf '%s\n' 'deps [DEPENDENCY=name] [TARGET=id] - acquire verified pinned dependencies' 'build [PRESET=name] - serialized dependency/native build' 'build-debug - default Vulkan+CPU build' 'build-release - native release build' 'test - deterministic CTest suite' 'test-debug - debug tests' 'valgrind - native backend enumeration and C89 policy memory checks (requires host valgrind)' 'upstream-inventory - compare every pinned public C declaration' 'format - format project C/C++ sources' 'format-check - assert formatting' 'finalize-slice - format and deterministic tests'
deps:
	python3 scripts/deps.py --target $(TARGET) $(if $(DEPENDENCY),--component $(DEPENDENCY))
build:
	python3 scripts/build.py --preset $(PRESET)
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
	python3 scripts/format.py
format-check:
	python3 scripts/format.py --check
finalize-slice: format test format-check
valgrind: build
	valgrind --error-exitcode=99 --leak-check=full --show-leak-kinds=definite --errors-for-leak-kinds=definite build/$(PRESET)/ailo_backend_test
	valgrind --error-exitcode=99 --leak-check=full --show-leak-kinds=definite --errors-for-leak-kinds=definite build/$(PRESET)/ailo_c89_policy
