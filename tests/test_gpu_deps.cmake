cmake_minimum_required(VERSION 3.24)
include("${CMAKE_CURRENT_LIST_DIR}/../cmake/AiloToolchain.cmake")
find_program(pkg_config NAMES pkg-config pkgconf REQUIRED)
set(prefix "${AILO_SDK_ROOT}/libdrm/install")
set(work "${AILO_SOURCE_ROOT}/build/tests/gpu-deps/${AILO_TARGET_ID}")
file(REMOVE_RECURSE "${work}/relocated")
file(MAKE_DIRECTORY "${work}/relocated")
file(COPY "${prefix}/" DESTINATION "${work}/relocated")
set(relocated "${work}/relocated")
file(GLOB_RECURSE shared "${relocated}/*.so" "${relocated}/*.so.*")
if(shared)
  message(FATAL_ERROR "GPU dependency profile installed shared libraries: ${shared}")
endif()
if(EXISTS "${prefix}/share/libdrm/amdgpu.ids")
  message(FATAL_ERROR "Dependency install still ships an external AMDGPU IDs table")
endif()
foreach(archive libdrm.a libdrm_amdgpu.a)
  if(NOT EXISTS "${relocated}/lib/${archive}")
    message(FATAL_ERROR "Missing static dependency: ${archive}")
  endif()
endforeach()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env PKG_CONFIG_PATH=
  "PKG_CONFIG_LIBDIR=${relocated}/lib/pkgconfig" PKG_CONFIG_SYSROOT_DIR=
  "${pkg_config}" --static --cflags --libs libdrm_amdgpu
  RESULT_VARIABLE result OUTPUT_VARIABLE flags ERROR_VARIABLE error OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT result EQUAL 0 OR NOT flags MATCHES "${relocated}/" OR flags MATCHES "${prefix}")
  message(FATAL_ERROR "Static dependency metadata did not relocate: ${flags} ${error}")
endif()
separate_arguments(flags UNIX_COMMAND "${flags}")
set(program "${work}/consumer")
execute_process(COMMAND "${CMAKE_COMMAND}" -E env --unset=CFLAGS --unset=CPPFLAGS
  --unset=LDFLAGS --unset=CPATH --unset=C_INCLUDE_PATH --unset=LIBRARY_PATH
  "${CMAKE_C_COMPILER}" "--sysroot=${CMAKE_SYSROOT}" -static -std=c99 -Werror
  "${CMAKE_CURRENT_LIST_DIR}/test_gpu_deps.c" -o "${program}" ${flags}
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Relocated static GPU dependency consumer failed: ${output} ${error}")
endif()
execute_process(COMMAND "${CMAKE_READELF}" -l -d "${program}"
  COMMAND_ERROR_IS_FATAL ANY OUTPUT_VARIABLE elf)
if(elf MATCHES "INTERP|NEEDED")
  message(FATAL_ERROR "GPU dependency consumer requires a dynamic loader/library: ${elf}")
endif()
# The failure path is executable on native x86-64; cross-compiled link evidence
# is intentionally separate from runtime and GPU hardware qualification.
cmake_host_system_information(RESULT host QUERY OS_PLATFORM)
if(AILO_TARGET_ID MATCHES "^x86_64-" AND host STREQUAL "x86_64")
  execute_process(COMMAND "${program}" COMMAND_ERROR_IS_FATAL ANY)
endif()
set(drm_source "${AILO_SOURCE_ROOT}/.cache/deps-build/${AILO_TARGET_ID}/libdrm/source")
set(drm_build "${AILO_SOURCE_ROOT}/.cache/deps-build/${AILO_TARGET_ID}/libdrm/build")
set(ids_test "${work}/embedded-ids")
execute_process(COMMAND "${CMAKE_COMMAND}" -E env --unset=CPATH --unset=C_INCLUDE_PATH --unset=LIBRARY_PATH
  "${CMAKE_C_COMPILER}" "--sysroot=${CMAKE_SYSROOT}" -static -std=c11 -Werror
  -include "${drm_build}/config.h" "-I${drm_source}" "-I${drm_source}/amdgpu"
  "${CMAKE_CURRENT_LIST_DIR}/test_drm_ids.c" -o "${ids_test}" ${flags}
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Embedded AMDGPU metadata test failed to link: ${output} ${error}")
endif()
if(AILO_TARGET_ID MATCHES "^x86_64-" AND host STREQUAL "x86_64")
  file(MAKE_DIRECTORY "${work}/external-ids")
  file(WRITE "${work}/external-ids/amdgpu.ids" "1.0.0\n1114, C2, external override must be ignored\n")
  execute_process(COMMAND "${CMAKE_COMMAND}" -E env "AMDGPU_ASIC_ID_TABLE_PATHS=${work}/external-ids"
    "${ids_test}" COMMAND_ERROR_IS_FATAL ANY)
endif()

# Poison host package discovery with valid packages. The target machine file
# must exclude both pkg-config and CMake fallbacks, including native x86-64.
set(poison "${work}/host-only")
file(MAKE_DIRECTORY "${poison}/lib/pkgconfig" "${poison}/lib/cmake/AiloHostLeak" "${work}/isolation")
file(WRITE "${poison}/lib/pkgconfig/ailo-host-leak.pc"
  "Name: ailo-host-leak\nDescription: must never enter target dependency discovery\nVersion: 1.0\nLibs:\nCflags:\n")
file(WRITE "${poison}/lib/cmake/AiloHostLeak/AiloHostLeakConfig.cmake"
  "add_library(AiloHostLeak::AiloHostLeak INTERFACE IMPORTED)\nset(AiloHostLeak_FOUND TRUE)\n")
file(WRITE "${work}/isolation/meson.build"
  "project('ailo-target-isolation', 'c')\n"
  "assert(dependency('ailo-host-leak', method: 'pkg-config', required: false).found() == get_option('expect_host'), 'pkg-config boundary failed')\n"
  "assert(dependency('AiloHostLeak', method: 'cmake', required: false).found() == get_option('expect_host'), 'CMake boundary failed')\n")
file(WRITE "${work}/isolation/meson.options" "option('expect_host', type: 'boolean', value: false)\n")
set(configure setup)
if(EXISTS "${work}/isolation-build/meson-private/coredata.dat")
  list(APPEND configure --reconfigure --clearcache)
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env
  "CMAKE_PREFIX_PATH=${poison}" "PKG_CONFIG_PATH=${poison}/lib/pkgconfig"
  "PKG_CONFIG_LIBDIR=${poison}/lib/pkgconfig" PKG_CONFIG_SYSROOT_DIR=
  meson ${configure} "${work}/isolation-build" "${work}/isolation"
  --cross-file "${AILO_SOURCE_ROOT}/build/deps/${AILO_TARGET_ID}/meson.ini" --wrap-mode nofallback
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Target dependency isolation failed: ${output} ${error}")
endif()
set(configure setup)
if(EXISTS "${work}/host-control/meson-private/coredata.dat")
  list(APPEND configure --reconfigure --clearcache)
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env
  "CMAKE_PREFIX_PATH=${poison}" "PKG_CONFIG_PATH=${poison}/lib/pkgconfig"
  "PKG_CONFIG_LIBDIR=${poison}/lib/pkgconfig" PKG_CONFIG_SYSROOT_DIR=
  meson ${configure} "${work}/host-control" "${work}/isolation" -Dexpect_host=true --wrap-mode nofallback
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Isolation fixture was not discoverable by host control: ${output} ${error}")
endif()
message(STATUS "${AILO_TARGET_ID}: relocated libdrm/amdgpu static linkage and ELF checks passed")
