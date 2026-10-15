# Compatibility identity for native state, not a build/release receipt. Bind
# declared native sources/patches, context creation and effective build settings.
set(cache_inputs cmake/dependencies.json CMakeLists.txt cmake/AiloPolicy.cmake
  cmake/AiloToolchain.cmake cmake/AiloCacheIdentity.cmake src/ailo_client.cpp
  src/ailo_cache.cpp src/ailo_cache_internal.hpp src/ailo_cache_identity.cpp
  vendor/llama/patches/series)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/vendor/llama/patches/series" patches)
foreach(patch IN LISTS patches)
  if(NOT patch STREQUAL "")
    list(APPEND cache_inputs "vendor/llama/patches/${patch}")
  endif()
endforeach()
set(cache_identity "ailo-prompt-state:1;${CMAKE_C_COMPILER_ID};${CMAKE_C_COMPILER_VERSION};${CMAKE_CXX_COMPILER_ID};${CMAKE_CXX_COMPILER_VERSION}")
foreach(input IN LISTS cache_inputs)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${input}")
  file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/${input}" digest)
  string(APPEND cache_identity ";${input}:${digest}")
endforeach()
get_cmake_property(settings CACHE_VARIABLES)
list(SORT settings)
foreach(setting IN LISTS settings)
  if(setting MATCHES "^(GGML_|LLAMA_|AILO_|CMAKE_(BUILD_TYPE|C_FLAGS|CXX_FLAGS|SYSTEM_PROCESSOR|SYSROOT))")
    string(APPEND cache_identity ";${setting}=${${setting}}")
  endif()
endforeach()
string(SHA256 AILO_CACHE_BUILD_ID "${cache_identity}")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/cache-build.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/generated/ailo-cache-build.h" @ONLY)
