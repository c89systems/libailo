# Compatibility identity for native state, not a build/release receipt. Bind
# declared native sources/patches, context creation and effective build settings.
set(cache_inputs cmake/dependencies.json CMakeLists.txt cmake/AiloPolicy.cmake
  cmake/AiloToolchain.cmake cmake/AiloCacheIdentity.cmake src/ailo_client.cpp
  src/ailo_cache.cpp src/ailo_cache_internal.hpp src/ailo_cache_identity.cpp
  src/ailo_snapshot.cpp src/ailo_snapshot_internal.hpp
  src/ailo_gpu.cpp src/ailo_gpu_internal.hpp
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
if(AILO_VULKAN)
  # Bind native state to the actual linked driver/compiler bytes and loader.
  # No filesystem path participates in this part of the identity.
  foreach(archive IN LISTS AILO_GPU_ARCHIVES)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${archive}")
    file(SHA256 "${archive}" digest)
    get_filename_component(name "${archive}" NAME)
    string(APPEND cache_identity ";gpu:${name}:${digest}")
  endforeach()
endif()
string(SHA256 AILO_CACHE_BUILD_ID "${cache_identity}")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(AILO_CACHE_BUILD_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/ailo-cache-build.h")
if(AILO_VULKAN)
  # Shader bytes depend on the host compiler too. Hash the produced archive
  # after its native CMake target builds, before compiling either facade.
  set(base "${CMAKE_CURRENT_BINARY_DIR}/generated/ailo-cache-source-id.txt")
  file(CONFIGURE OUTPUT "${base}" CONTENT "${AILO_CACHE_BUILD_ID}\n" @ONLY)
  add_custom_command(OUTPUT "${AILO_CACHE_BUILD_HEADER}"
    COMMAND "${CMAKE_COMMAND}" "-DAILO_CACHE_BASE_FILE=${base}"
      "-DAILO_CACHE_SHADER_ARCHIVE=$<TARGET_FILE:ggml-vulkan>"
      "-DAILO_CACHE_HEADER=${AILO_CACHE_BUILD_HEADER}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/AiloCacheBuild.cmake"
    DEPENDS "${base}" "$<TARGET_FILE:ggml-vulkan>"
      "${CMAKE_CURRENT_SOURCE_DIR}/cmake/AiloCacheBuild.cmake"
      "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cache-build.h.in"
    VERBATIM)
else()
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/cache-build.h.in"
    "${AILO_CACHE_BUILD_HEADER}" @ONLY)
endif()
