# Map compiler-generated source/debug filenames at their source. Runtime lookup
# paths remain a separate install-time policy; never sanitize linked artifacts.
set(ailo_privacy_maps
  "${CMAKE_CURRENT_SOURCE_DIR}=ailo"
  "${AILO_BOOTLIN_ROOT}=bootlin"
  "${CMAKE_CURRENT_SOURCE_DIR}/.cache/deps/${AILO_TARGET_ID}/cpkt/install=cpkt"
  "${CMAKE_CURRENT_SOURCE_DIR}/.cache/deps-build/${AILO_TARGET_ID}/llama/source=llama"
  "${CMAKE_CURRENT_SOURCE_DIR}/.cache/deps-build/${AILO_TARGET_ID}/llama/build=llama-generated")
foreach(mapping IN LISTS ailo_privacy_maps)
  add_compile_options("$<$<COMPILE_LANGUAGE:C,CXX>:-ffile-prefix-map=${mapping}>")
endforeach()
