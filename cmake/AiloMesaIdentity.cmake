cmake_minimum_required(VERSION 3.24)
include("${CMAKE_CURRENT_LIST_DIR}/AiloToolchain.cmake")
set(identity "ailo-static-mesa:1;${AILO_TARGET_ID}")
set(inputs cmake/dependencies.json scripts/build-gpu-deps.sh scripts/cpkt-toolchains.sh
  cmake/AiloToolchain.cmake cmake/AiloMesonToolchain.cmake cmake/meson-cross.ini.in
  cmake/meson-native.ini.in cmake/AiloMesaIdentity.cmake cmake/mesa-static.build
  cmake/AiloEmbedDrmIds.cmake)
foreach(component mesa libdrm)
  list(APPEND inputs "vendor/${component}/patches/series")
  file(STRINGS "${AILO_SOURCE_ROOT}/vendor/${component}/patches/series" patches)
  foreach(patch IN LISTS patches)
    list(APPEND inputs "vendor/${component}/patches/${patch}")
  endforeach()
endforeach()
foreach(input IN LISTS inputs)
  file(SHA256 "${AILO_SOURCE_ROOT}/${input}" digest)
  string(APPEND identity ";${input}:${digest}")
endforeach()
foreach(tool "${CMAKE_C_COMPILER}" "${CMAKE_CXX_COMPILER}"
  "${AILO_SOURCE_ROOT}/.cache/deps/host/mesa/install/bin/mesa_clc"
  "${AILO_SOURCE_ROOT}/.cache/deps/host/mesa/install/bin/vtn_bindgen2"
  "${AILO_SOURCE_ROOT}/.cache/deps/host/glslang/install/bin/glslangValidator")
  file(SHA256 "${tool}" digest)
  string(APPEND identity ";tool:${digest}")
endforeach()
set(header "/* Private static Mesa identity; no runtime ELF/DSO introspection. */\n")
foreach(driver ANV RADV)
  string(SHA256 digest "${driver};${identity}")
  string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${digest}")
  string(APPEND header "#define AILO_${driver}_BUILD_ID_HEX \"${digest}\"\n#define AILO_${driver}_BUILD_ID_BYTES {${bytes}}\n")
endforeach()
string(APPEND header "#define RADV_BUILD_ID_OVERRIDE AILO_RADV_BUILD_ID_HEX\n")
set(output "${AILO_SOURCE_ROOT}/.cache/deps-build/${AILO_TARGET_ID}/mesa/source/include/ailo_mesa_build_id.h")
file(CONFIGURE OUTPUT "${output}" CONTENT "${header}" @ONLY)
