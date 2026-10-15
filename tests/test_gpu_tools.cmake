cmake_minimum_required(VERSION 3.24)
get_filename_component(root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(work "${root}/build/tests/gpu-tools")
set(tools "${root}/.cache/deps/host/spirv-tools/install")
set(translator "${root}/.cache/deps/host/spirv-llvm-translator/install/bin/llvm-spirv")
set(glslang "${root}/.cache/deps/host/glslang/install/bin/glslangValidator")
set(mesa "${root}/.cache/deps/host/mesa/install/bin")
find_program(pkg_config NAMES pkg-config pkgconf REQUIRED)
find_program(cxx NAMES c++ REQUIRED)
find_program(llvm_config NAMES llvm-config-20 llvm-config REQUIRED)
execute_process(COMMAND "${llvm_config}" --bindir COMMAND_ERROR_IS_FATAL ANY
  OUTPUT_VARIABLE llvm_bin OUTPUT_STRIP_TRAILING_WHITESPACE)
file(MAKE_DIRECTORY "${work}")
file(WRITE "${work}/shader.comp" "#version 450\nlayout(local_size_x=1) in;\nvoid main() {}\n")
execute_process(COMMAND "${glslang}" -V --target-env vulkan1.3 -o "${work}/shader.spv" "${work}/shader.comp"
  COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${CMAKE_COMMAND}" -E env PKG_CONFIG_PATH=
  "PKG_CONFIG_LIBDIR=${tools}/lib/pkgconfig" PKG_CONFIG_SYSROOT_DIR=
  "${pkg_config}" --static --cflags --libs SPIRV-Tools
  COMMAND_ERROR_IS_FATAL ANY OUTPUT_VARIABLE flags OUTPUT_STRIP_TRAILING_WHITESPACE)
separate_arguments(flags UNIX_COMMAND "${flags}")
execute_process(COMMAND "${cxx}" -std=c++17 "${CMAKE_CURRENT_LIST_DIR}/test_gpu_tools.cpp"
  -o "${work}/validate" ${flags} COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${work}/validate" "${work}/shader.spv" COMMAND_ERROR_IS_FATAL ANY)

# Mesa's OpenCL compiler uses the LLVM translator library from this same build.
# Prove its installed executable can translate and reverse a real kernel.
file(WRITE "${work}/kernel.ll" "target triple = \"spir64-unknown-unknown\"\ndefine spir_kernel void @test_kernel() {\n ret void\n}\n")
execute_process(COMMAND "${llvm_bin}/llvm-as" "${work}/kernel.ll" -o "${work}/kernel.bc" COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${translator}" "${work}/kernel.bc" -o "${work}/kernel.spv" COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${translator}" -r "${work}/kernel.spv" -o "${work}/roundtrip.bc" COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${llvm_bin}/llvm-dis" "${work}/roundtrip.bc" -o "${work}/roundtrip.ll" COMMAND_ERROR_IS_FATAL ANY)
file(READ "${work}/roundtrip.ll" roundtrip)
if(NOT roundtrip MATCHES "spir_kernel void @test_kernel")
  message(FATAL_ERROR "Installed LLVM translator did not preserve the kernel entry point")
endif()
file(WRITE "${work}/invalid.comp" "#version 450\nthis is invalid GLSL\n")
execute_process(COMMAND "${glslang}" -V "${work}/invalid.comp" -o "${work}/invalid.spv"
  RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
if(NOT result MATCHES "^[1-9][0-9]*$")
  message(FATAL_ERROR "Invalid shader did not fail normally: ${result}")
endif()

file(WRITE "${work}/library.cl" "void test_kernel(global uint *buffer) { buffer[0] = 42u; }\n")
execute_process(COMMAND "${mesa}/mesa_clc" -o "${work}/library.spv" "${work}/library.cl" -- -cl-std=cl2.0
  COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${mesa}/vtn_bindgen2" "${work}/library.spv" "${work}/bindings.c" "${work}/bindings.h"
  COMMAND_ERROR_IS_FATAL ANY)
file(READ "${work}/bindings.h" bindings)
file(SIZE "${work}/bindings.c" size)
if(NOT bindings MATCHES "test_kernel" OR size LESS 100)
  message(FATAL_ERROR "Mesa generator chain did not produce the expected function binding")
endif()
file(WRITE "${work}/invalid.cl" "this is invalid OpenCL\n")
execute_process(COMMAND "${mesa}/mesa_clc" -o "${work}/invalid-cl.spv" "${work}/invalid.cl"
  RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
if(NOT result MATCHES "^[1-9][0-9]*$")
  message(FATAL_ERROR "Invalid OpenCL did not fail normally: ${result}")
endif()
execute_process(COMMAND "${mesa}/vtn_bindgen2" "${work}/missing-input.spv" "${work}/missing.c" "${work}/missing.h"
  RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
if(NOT result MATCHES "^[1-9][0-9]*$")
  message(FATAL_ERROR "Missing SPIR-V input did not fail normally: ${result}")
endif()
message(STATUS "Host shader, LLVM and Mesa generator chains and invalid-input failures passed")
