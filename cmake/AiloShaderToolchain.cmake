# This toolchain is used exclusively for the non-shipped native shader generator.
set(AILO_TARGET_ID x86_64-linux-gnu CACHE STRING "Native build helper target" FORCE)
include("${CMAKE_CURRENT_LIST_DIR}/AiloToolchain.cmake")
file(GLOB loaders "${CMAKE_SYSROOT}/lib/ld-linux*.so*")
list(GET loaders 0 loader)
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,--dynamic-linker=${loader},--disable-new-dtags,-rpath,${CMAKE_SYSROOT}/lib:${CMAKE_SYSROOT}/usr/lib:${AILO_BOOTLIN_ROOT}/lib:${AILO_BOOTLIN_ROOT}/lib64")
