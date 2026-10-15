cmake_minimum_required(VERSION 3.24)
get_filename_component(root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
string(RANDOM LENGTH 12 ALPHABET abcdef0123456789 random)
set(work "${root}/build/tests/drivers-${random}")
file(MAKE_DIRECTORY "${work}")

function(expect_preset preset expected)
  execute_process(COMMAND "${CMAKE_COMMAND}" "-DAILO_PRESET=${preset}" "-DAILO_PRESET_RESULT=${work}/preset"
    -P "${root}/cmake/AiloPreset.cmake" RESULT_VARIABLE result ERROR_VARIABLE error)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "Preset ${preset} failed: ${error}")
  endif()
  file(READ "${work}/preset" actual)
  if(NOT actual STREQUAL expected)
    message(FATAL_ERROR "Preset ${preset}: expected ${expected}; actual ${actual}")
  endif()
endfunction()
expect_preset(debug "target=x86_64-linux-gnu\nvulkan=ON\n")
expect_preset(valgrind "target=x86_64-linux-gnu\nvulkan=ON\n")
expect_preset(cpu "target=x86_64-linux-gnu\nvulkan=OFF\n")
foreach(target x86_64-linux-gnu x86_64-linux-musl aarch64-linux-gnu aarch64-linux-musl)
  expect_preset("${target}-release" "target=${target}\nvulkan=ON\n")
endforeach()
foreach(preset base nonexistent "../debug" armhf-linux-gnu-release armhf-linux-musl-release)
  execute_process(COMMAND "${CMAKE_COMMAND}" "-DAILO_PRESET=${preset}" "-DAILO_PRESET_RESULT=${work}/preset"
    -P "${root}/cmake/AiloPreset.cmake" RESULT_VARIABLE result ERROR_QUIET)
  if(result EQUAL 0)
    message(FATAL_ERROR "Invalid/hidden preset ${preset} was accepted")
  endif()
endforeach()
foreach(driver build deps format build-gpu-deps)
  execute_process(COMMAND "${root}/scripts/${driver}.sh" --unknown
    RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
  if(NOT result EQUAL 2)
    message(FATAL_ERROR "${driver}: invalid options must produce a usage error")
  endif()
endforeach()
foreach(arguments "--target;armhf-linux-gnu" "--component;unknown")
  execute_process(COMMAND "${root}/scripts/build-gpu-deps.sh" ${arguments}
    RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
  if(NOT result EQUAL 2)
    message(FATAL_ERROR "GPU dependency driver accepted invalid arguments: ${arguments}")
  endif()
endforeach()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env AILO_JOBS=0 "${root}/scripts/build-gpu-deps.sh"
  RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
if(NOT result EQUAL 2)
  message(FATAL_ERROR "Invalid GPU dependency worker count was accepted")
endif()
foreach(target armhf-linux-gnu armhf-linux-musl)
  execute_process(COMMAND "${root}/scripts/deps.sh" --target "${target}" --component cpkt
    RESULT_VARIABLE result OUTPUT_QUIET ERROR_VARIABLE error)
  if(result EQUAL 0 OR NOT error MATCHES "Unsupported dependency target")
    message(FATAL_ERROR "Retired ${target} acquisition was not rejected")
  endif()
  execute_process(COMMAND "${CMAKE_COMMAND}" "-DAILO_TARGET_ID=${target}" -P "${root}/cmake/AiloToolchain.cmake"
    RESULT_VARIABLE result OUTPUT_QUIET ERROR_VARIABLE error)
  if(result EQUAL 0 OR NOT error MATCHES "Unsupported libailo target")
    message(FATAL_ERROR "Retired ${target} toolchain selection was not rejected")
  endif()
  set(retired_archive "${work}/libailo-0.0.0-${target}.tar.gz")
  file(WRITE "${retired_archive}" "must not be inspected or extracted")
  execute_process(COMMAND "${root}/scripts/verify_sdk_archive.sh" "${retired_archive}"
    RESULT_VARIABLE result OUTPUT_QUIET ERROR_VARIABLE error)
  if(NOT result EQUAL 2 OR NOT error MATCHES "invalid SDK artifact name")
    message(FATAL_ERROR "Retired ${target} shipment was not rejected")
  endif()
endforeach()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env AILO_JOBS=0 "${root}/scripts/build.sh"
  RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
if(NOT result EQUAL 2)
  message(FATAL_ERROR "Invalid build worker count was accepted")
endif()

# Observe that the shell driver accepts every declared target name and reaches
# its configure step. Stop the mocked CMake before any dependency/build action.
file(MAKE_DIRECTORY "${work}/driver/scripts" "${work}/bin")
file(COPY "${root}/scripts/build.sh" DESTINATION "${work}/driver/scripts")
file(WRITE "${work}/bin/cmake" "#!/usr/bin/env bash\nexit 75\n")
file(CHMOD "${work}/bin/cmake" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
foreach(target x86_64-linux-gnu x86_64-linux-musl aarch64-linux-gnu aarch64-linux-musl)
  execute_process(COMMAND "${CMAKE_COMMAND}" -E env "PATH=${work}/bin:$ENV{PATH}" AILO_JOBS=1
    "${work}/driver/scripts/build.sh" --preset "${target}-release" --configure-only
    RESULT_VARIABLE result OUTPUT_QUIET ERROR_VARIABLE error)
  if(NOT result EQUAL 75)
    message(FATAL_ERROR "Build driver rejected ${target}-release before configuring: ${result}: ${error}")
  endif()
endforeach()

# Synthetic source acquisition uses its own counted/test archive cache only.
set(component "fixture-${random}")
file(MAKE_DIRECTORY "${work}/seed/${component}")
file(WRITE "${work}/seed/${component}/CMakeLists.txt" "# pinned synthetic source\n")
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar czf "${work}/fixture.tar.gz" "${component}"
  WORKING_DIRECTORY "${work}/seed" COMMAND_ERROR_IS_FATAL ANY)
file(SHA256 "${work}/fixture.tar.gz" sha)
file(MAKE_DIRECTORY "${work}/shared/archives/sha256/${sha}")
file(COPY "${work}/fixture.tar.gz" DESTINATION "${work}/shared/archives/sha256/${sha}")
file(WRITE "${work}/pins.json" "{\"${component}\": {\"url\": \"https://invalid.invalid/unused.tar.gz\", \"sha256\": \"${sha}\"}}")
set(prepared "${root}/.cache/deps-build/x86_64-linux-gnu/${component}")
set(install "${root}/.cache/deps/x86_64-linux-gnu/${component}")
set(contract "${root}/.cache/dependency-contracts/x86_64-linux-gnu/${component}.txt")
foreach(iteration RANGE 1 3)
  execute_process(COMMAND "${CMAKE_COMMAND}" -DAILO_TARGET_ID=x86_64-linux-gnu
    "-DAILO_DEPENDENCY_LOCK=${work}/pins.json" "-DAILO_DEPENDENCY=${component}"
    "-DCPKT_DEPENDENCY_CACHE=${work}/shared" -P "${root}/cmake/AiloDependencies.cmake"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(NOT result EQUAL 0 OR NOT EXISTS "${prepared}/source/CMakeLists.txt")
    message(FATAL_ERROR "Synthetic dependency preparation failed: ${output} ${error}")
  endif()
  if(iteration EQUAL 1)
    file(REMOVE_RECURSE "${prepared}" "${install}")
    file(REMOVE "${contract}")
  elseif(iteration EQUAL 2)
    file(WRITE "${work}/pins.json" "{\"${component}\": {\"url\": \"https://invalid.invalid/different-name.tar.gz\", \"sha256\": \"${sha}\"}}")
  endif()
endforeach()
file(REMOVE_RECURSE "${prepared}" "${install}")
file(REMOVE "${contract}" "${contract}.lock")

# The same verified archive path supports Meson without requiring CMake inputs.
# An unsupported recipe or missing build entry point must fail before a ready
# contract is published. All URLs deliberately fail if the cache is bypassed.
set(component "meson-fixture-${random}")
set(prepared "${root}/.cache/deps-build/x86_64-linux-gnu/${component}")
set(install "${root}/.cache/deps/x86_64-linux-gnu/${component}")
set(contract "${root}/.cache/dependency-contracts/x86_64-linux-gnu/${component}.txt")
file(MAKE_DIRECTORY "${work}/meson-seed/${component}")
file(WRITE "${work}/meson-seed/${component}/meson.build" "project('fixture', 'c')\n")
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar czf "${work}/meson.tar.gz" "${component}"
  WORKING_DIRECTORY "${work}/meson-seed" COMMAND_ERROR_IS_FATAL ANY)
file(SHA256 "${work}/meson.tar.gz" sha)
file(MAKE_DIRECTORY "${work}/shared/archives/sha256/${sha}")
file(COPY "${work}/meson.tar.gz" DESTINATION "${work}/shared/archives/sha256/${sha}")
foreach(build_system unknown cmake meson)
  file(WRITE "${work}/pins.json" "{\"${component}\": {\"url\": \"https://invalid.invalid/meson.tar.gz\", \"sha256\": \"${sha}\", \"build_system\": \"${build_system}\"}}")
  execute_process(COMMAND "${CMAKE_COMMAND}" -DAILO_TARGET_ID=x86_64-linux-gnu
    "-DAILO_DEPENDENCY_LOCK=${work}/pins.json" "-DAILO_DEPENDENCY=${component}"
    "-DCPKT_DEPENDENCY_CACHE=${work}/shared" -P "${root}/cmake/AiloDependencies.cmake"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(build_system STREQUAL meson)
    if(NOT result EQUAL 0 OR NOT EXISTS "${prepared}/source/meson.build" OR NOT EXISTS "${contract}")
      message(FATAL_ERROR "Meson source preparation failed: ${output} ${error}")
    endif()
  elseif(build_system STREQUAL cmake)
    if(result EQUAL 0 OR NOT error MATCHES "prepared archive is missing" OR EXISTS "${contract}")
      message(FATAL_ERROR "CMake source without build entry point was accepted: ${error}")
    endif()
  elseif(result EQUAL 0 OR NOT error MATCHES "unsupported source build system" OR EXISTS "${contract}")
    message(FATAL_ERROR "Unknown build system was accepted: ${build_system}: ${error}")
  endif()
endforeach()
file(REMOVE_RECURSE "${prepared}" "${install}")
file(REMOVE "${contract}" "${contract}.lock")
# A valid archive with the wrong build entry point must not become ready.
file(WRITE "${work}/pins.json" "{\"${component}\": {\"url\": \"https://invalid.invalid/missing.tar.gz\", \"sha256\": \"${sha}\"}}")
execute_process(COMMAND "${CMAKE_COMMAND}" -DAILO_TARGET_ID=x86_64-linux-gnu
  "-DAILO_DEPENDENCY_LOCK=${work}/pins.json" "-DAILO_DEPENDENCY=${component}"
  "-DCPKT_DEPENDENCY_CACHE=${work}/shared" -P "${root}/cmake/AiloDependencies.cmake"
  RESULT_VARIABLE result OUTPUT_QUIET ERROR_VARIABLE error)
if(result EQUAL 0 OR NOT error MATCHES "prepared archive is missing" OR EXISTS "${contract}")
  message(FATAL_ERROR "Source without required build entry point was accepted: ${error}")
endif()
file(REMOVE_RECURSE "${prepared}" "${install}" "${work}")
file(REMOVE "${contract}" "${contract}.lock")
message(STATUS "Bash/CMake drivers, preset inheritance, and offline dependency reconstruction: passed")
