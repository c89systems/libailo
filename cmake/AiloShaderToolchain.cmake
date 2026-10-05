# This toolchain is used exclusively for the non-shipped native shader generator.
set(AILO_TARGET_ID x86_64-linux-gnu CACHE STRING "Native build helper target" FORCE)
include("${CMAKE_CURRENT_LIST_DIR}/AiloToolchain.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/AiloRuntime.cmake")
ailo_native_runtime(loader paths)
list(JOIN paths ":" runtime_paths)
set(CMAKE_EXE_LINKER_FLAGS "-Wl,--dynamic-linker=${loader},--disable-new-dtags,-rpath,${runtime_paths}"
  CACHE STRING "Private native helper runtime" FORCE)
