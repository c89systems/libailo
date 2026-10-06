set(AILO_VERSION_OVERRIDE "" CACHE STRING "Explicit local candidate version; exact lightweight HEAD tag takes precedence")
execute_process(COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/scripts/release_version.sh" --source-root "${CMAKE_CURRENT_SOURCE_DIR}" --override "${AILO_VERSION_OVERRIDE}"
  RESULT_VARIABLE version_result OUTPUT_VARIABLE AILO_RESOLVED_VERSION ERROR_VARIABLE version_error OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT version_result EQUAL 0)
  message(FATAL_ERROR "Ailo version resolution failed: ${version_error}")
endif()
