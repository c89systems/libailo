cmake_minimum_required(VERSION 3.24)
include("${CMAKE_CURRENT_LIST_DIR}/../cmake/AiloToolchain.cmake")
find_program(pkg_config NAMES pkg-config pkgconf REQUIRED)
set(work "${AILO_SOURCE_ROOT}/build/tests/static-mesa/${AILO_TARGET_ID}")
file(REMOVE_RECURSE "${work}/relocated")
set(components mesa libdrm)
set(probe static_icds.c)
set(loader_flags "")
if(AILO_STATIC_LOADER)
  list(APPEND components vulkan-loader)
  set(probe static_vulkan.c)
  list(APPEND loader_flags "${work}/relocated/vulkan-loader/lib/libvulkan.a")
endif()
foreach(component IN LISTS components)
  file(MAKE_DIRECTORY "${work}/relocated/${component}")
  file(COPY "${AILO_SDK_ROOT}/${component}/install/" DESTINATION "${work}/relocated/${component}")
endforeach()
file(GLOB manifests "${work}/relocated/mesa/share/vulkan/icd.d/*.json")
file(GLOB_RECURSE shared "${work}/relocated/*.so" "${work}/relocated/*.so.*")
if(manifests OR shared)
  message(FATAL_ERROR "Static Mesa payload contains shared drivers or filesystem ICD manifests")
endif()
file(GLOB archives "${work}/relocated/mesa/lib/*.a")
if(AILO_STATIC_LOADER)
  list(APPEND archives ${loader_flags})
endif()
foreach(archive IN LISTS archives)
  file(READ "${archive}" magic LIMIT 8 HEX)
  if(NOT magic STREQUAL "213c617263683e0a")
    message(FATAL_ERROR "Mesa SDK archive is not self-contained: ${archive}")
  endif()
endforeach()
execute_process(COMMAND "${CMAKE_NM}" -g --defined-only ${archives}
  COMMAND_ERROR_IS_FATAL ANY OUTPUT_VARIABLE symbols ERROR_QUIET)
foreach(symbol nir_shader_create vk_instance_init _mesa_hash_table_create)
  string(REGEX MATCHALL "[ \t][TDB][ \t]+${symbol}(\r?\n|$)" matches "${symbols}")
  list(LENGTH matches count)
  if(NOT count EQUAL 1)
    message(FATAL_ERROR "Expected one common Mesa ${symbol} definition; found ${count}")
  endif()
endforeach()
execute_process(COMMAND "${CMAKE_NM}" -u ${archives}
  COMMAND_ERROR_IS_FATAL ANY OUTPUT_VARIABLE symbols ERROR_QUIET)
if(symbols MATCHES "[ \t]U[ \t]+(dlopen|dlmopen|dlsym|dlclose|dladdr|dl_iterate_phdr|elf_[^\n ]*|LLVM[^\n ]*|GOMP_[^\n ]*|_ZN[0-9]+llvm[^\n ]*)(\r?\n|$)")
  message(FATAL_ERROR "Static Mesa still references a forbidden loader or target dependency: ${CMAKE_MATCH_0}")
endif()
set(pc_roots "${work}/relocated/mesa/lib/pkgconfig:${work}/relocated/libdrm/lib/pkgconfig")
execute_process(COMMAND "${CMAKE_COMMAND}" -E env PKG_CONFIG_PATH= "PKG_CONFIG_LIBDIR=${pc_roots}" PKG_CONFIG_SYSROOT_DIR=
  "${pkg_config}" --static --cflags --libs ailo-mesa
  COMMAND_ERROR_IS_FATAL ANY OUTPUT_VARIABLE flags OUTPUT_STRIP_TRAILING_WHITESPACE)
if(flags MATCHES "${AILO_SDK_ROOT}|-l(LLVM|clang|elf|udev|gomp|z|zstd)( |$)")
  message(FATAL_ERROR "Static Mesa metadata escaped the relocated permissive closure: ${flags}")
endif()
separate_arguments(flags UNIX_COMMAND "${flags}")
set(program "${work}/probe")
execute_process(COMMAND "${CMAKE_COMMAND}" -E env --unset=CPATH --unset=C_INCLUDE_PATH --unset=CPLUS_INCLUDE_PATH --unset=LIBRARY_PATH
  "${CMAKE_CXX_COMPILER}" "--sysroot=${CMAKE_SYSROOT}" -x c -std=c11 -static
  "-I${AILO_SOURCE_ROOT}/.cache/deps-build/${AILO_TARGET_ID}/mesa/source/include"
  "${CMAKE_CURRENT_LIST_DIR}/${probe}" -o "${program}" -x none ${loader_flags} ${flags}
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Relocated static Mesa consumer failed: ${output} ${error}")
endif()
execute_process(COMMAND "${CMAKE_READELF}" -l -d "${program}" COMMAND_ERROR_IS_FATAL ANY OUTPUT_VARIABLE elf)
if(elf MATCHES "INTERP|NEEDED")
  message(FATAL_ERROR "Static Mesa consumer has a dynamic loader/library dependency: ${elf}")
endif()
cmake_host_system_information(RESULT host QUERY OS_PLATFORM)
if(AILO_TARGET_ID MATCHES "^x86_64-" AND host STREQUAL "x86_64")
  execute_process(COMMAND "${program}" COMMAND_ERROR_IS_FATAL ANY)
  if(AILO_STATIC_LOADER)
    find_program(strace NAMES strace REQUIRED)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env
      "VK_DRIVER_FILES=${work}/forbidden/icd.json"
      "VK_ICD_FILENAMES=${work}/forbidden/icd.json"
      "VK_ADD_DRIVER_FILES=${work}/forbidden/icd.json"
      "VK_LAYER_PATH=${work}/forbidden" "VK_ADD_LAYER_PATH=${work}/forbidden"
      "VK_IMPLICIT_LAYER_PATH=${work}/forbidden"
      VK_INSTANCE_LAYERS=VK_LAYER_ailo_external_forbidden
      "VK_LOADER_SETTINGS_PATH=${work}/forbidden" VK_LOADER_DRIVERS_DISABLE=*
      "XDG_CONFIG_HOME=${work}/forbidden" "XDG_CONFIG_DIRS=${work}/forbidden"
      "XDG_DATA_HOME=${work}/forbidden" "XDG_DATA_DIRS=${work}/forbidden"
      "${strace}" -f -e trace=%file,%process -o "${work}/trace" "${program}"
      COMMAND_ERROR_IS_FATAL ANY)
    file(READ "${work}/trace" trace)
    if(trace MATCHES "forbidden|\\.so([.\"]|$)|/vulkan/[^\n]*\\.json")
      message(FATAL_ERROR "Static Vulkan attempted external driver/layer/library discovery: ${trace}")
    endif()
  endif()
endif()
message(STATUS "${AILO_TARGET_ID}: relocated static RADV/ANV closure passed")
