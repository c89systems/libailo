cmake_minimum_required(VERSION 3.24)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
  file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" LUA_AILO_VERSION LIMIT_COUNT 1)
else()
  set(LUA_AILO_VERSION 0.0.0)
endif()
project(lua_ailo VERSION ${LUA_AILO_VERSION} LANGUAGES C)
find_package(ailo CONFIG REQUIRED COMPONENTS shared)
find_package(pslog 0.12.0 EXACT CONFIG REQUIRED)
find_package(Lua 5.5.1 EXACT CONFIG REQUIRED)
find_path(PSLOG_LUA_INCLUDE_DIR pslog_lua.h REQUIRED)
add_library(ailo_lua MODULE src/ailo_lua.c)
set_target_properties(ailo_lua PROPERTIES PREFIX "" OUTPUT_NAME core C_STANDARD 11 C_STANDARD_REQUIRED YES C_VISIBILITY_PRESET hidden INSTALL_RPATH "" INSTALL_RPATH_USE_LINK_PATH FALSE)
target_include_directories(ailo_lua PRIVATE "${LUA_INCLUDE_DIR}" "${PSLOG_LUA_INCLUDE_DIR}")
target_link_libraries(ailo_lua PRIVATE ailo::shared pslog::pslog_shared)
target_compile_options(ailo_lua PRIVATE -Wall -Wextra -Werror -Wpedantic)
set(AILO_LUA_CMODULE_DIR "lib/lua/5.5" CACHE STRING "Native module install directory")
set(AILO_LUA_MODULE_DIR "share/lua/5.5" CACHE STRING "Lua module install directory")
install(TARGETS ailo_lua LIBRARY DESTINATION "${AILO_LUA_CMODULE_DIR}/ailo")
install(FILES ailo/init.lua DESTINATION "${AILO_LUA_MODULE_DIR}/ailo")
option(AILO_LUA_TESTS "Build local installed-ABI tests" OFF)
if(AILO_LUA_TESTS)
  foreach(variable AILO_LUA_PROVIDER_SOURCE AILO_LUA_RUNTIME_PATHS AILO_LUA_LOADER AILO_LUA_MODEL AILO_LUA_WORK)
    if(NOT DEFINED ${variable})
      message(FATAL_ERROR "Missing local test input: ${variable}")
    endif()
  endforeach()
  enable_testing()
  add_library(pslog_lua_test MODULE "${AILO_LUA_PROVIDER_SOURCE}/lua/src/pslog_lua.c")
  set_target_properties(pslog_lua_test PROPERTIES PREFIX "" OUTPUT_NAME core LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/provider/pslog" C_STANDARD 99)
  target_include_directories(pslog_lua_test PRIVATE "${LUA_INCLUDE_DIR}" "${PSLOG_LUA_INCLUDE_DIR}")
  target_link_libraries(pslog_lua_test PRIVATE pslog::pslog_shared)
  install(TARGETS pslog_lua_test LIBRARY DESTINATION lib/lua/5.5/pslog)
  install(FILES "${AILO_LUA_PROVIDER_SOURCE}/lua/pslog/init.lua" DESTINATION share/lua/5.5/pslog)
  add_executable(ailo_lua_runner tests/runner.c)
  target_link_libraries(ailo_lua_runner PRIVATE cpkt::lua_shared m ${CMAKE_DL_LIBS})
  set_target_properties(ailo_lua_runner PROPERTIES C_STANDARD 99)
  set_target_properties(ailo_lua_runner PROPERTIES BUILD_RPATH "${AILO_LUA_RUNTIME_PATHS}" LINK_FLAGS "-Wl,--dynamic-linker=${AILO_LUA_LOADER} -Wl,--disable-new-dtags")
  set(cpath "${CMAKE_INSTALL_PREFIX}/lib/lua/5.5/?.so")
  string(REPLACE ";" ":" runtime "${AILO_LUA_RUNTIME_PATHS}")
  add_test(NAME lua-receiver COMMAND "${AILO_LUA_LOADER}" --library-path "${runtime}" $<TARGET_FILE:ailo_lua_runner>
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/receiver.lua" "${CMAKE_INSTALL_PREFIX}/share/lua/5.5/?.lua;${CMAKE_INSTALL_PREFIX}/share/lua/5.5/?/init.lua" "${cpath}" "${AILO_LUA_MODEL}" "${AILO_LUA_WORK}")
  add_test(NAME lua-public-imports COMMAND "${CMAKE_COMMAND}"
    "-DREADELF=${CMAKE_READELF}" "-DNM=${CMAKE_NM}"
    "-DMODULE=${CMAKE_INSTALL_PREFIX}/lib/lua/5.5/ailo/core.so"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/imports.cmake")
  # Host ICD allocations are outside this facade ownership gate.
  if(NOT ailo_VULKAN)
    find_program(VALGRIND_EXECUTABLE valgrind REQUIRED)
    add_test(NAME lua-memory COMMAND "${VALGRIND_EXECUTABLE}" --error-exitcode=99
      "--suppressions=${CMAKE_CURRENT_SOURCE_DIR}/tests/runtime.supp"
      --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all
      $<TARGET_FILE:ailo_lua_runner>
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/receiver.lua"
      "${CMAKE_INSTALL_PREFIX}/share/lua/5.5/?.lua;${CMAKE_INSTALL_PREFIX}/share/lua/5.5/?/init.lua"
      "${cpath}" "${AILO_LUA_MODEL}" "${AILO_LUA_WORK}")
    set_tests_properties(lua-memory PROPERTIES TIMEOUT 180 LABELS "lua;memory;local")
  endif()
  get_target_property(pslog_library pslog::pslog_shared IMPORTED_LOCATION)
  get_filename_component(pslog_prefix "${pslog_library}/../.." ABSOLUTE)
  # The local SDK carries the main repository's toolchain as a configured input.
  add_test(NAME lua-rocks COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tests/rocks.sh"
    "${CMAKE_CURRENT_SOURCE_DIR}" "${AILO_LUA_PROVIDER_SOURCE}" "${ailo_DIR}/../../.."
    "${LUA_INCLUDE_DIR}/.." "${pslog_prefix}" "${PSLOG_LUA_INCLUDE_DIR}"
    "${CMAKE_TOOLCHAIN_FILE}" $<TARGET_FILE:ailo_lua_runner> "${AILO_LUA_MODEL}")
  set_tests_properties(lua-rocks PROPERTIES TIMEOUT 180 LABELS "lua;packaging;local")
  set_tests_properties(lua-receiver lua-public-imports PROPERTIES TIMEOUT 60 LABELS "lua;integration;local")
endif()
