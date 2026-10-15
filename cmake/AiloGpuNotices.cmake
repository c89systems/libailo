cmake_minimum_required(VERSION 3.24)
find_program(ninja NAMES ninja REQUIRED)
find_program(python NAMES python3 REQUIRED)
file(MAKE_DIRECTORY "${AILO_GPU_NOTICES_OUTPUT}")
foreach(component mesa libdrm)
  set(base "${AILO_GPU_BUILD_ROOT}/${component}")
  execute_process(COMMAND "${ninja}" -t inputs all WORKING_DIRECTORY "${base}/build"
    RESULT_VARIABLE result OUTPUT_FILE "${AILO_GPU_NOTICES_OUTPUT}/${component}.inputs"
    ERROR_VARIABLE error)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "GPU notice generation requires Ninja with -t inputs: ${error}")
  endif()
  execute_process(COMMAND "${ninja}" -t deps WORKING_DIRECTORY "${base}/build"
    COMMAND_ERROR_IS_FATAL ANY OUTPUT_FILE "${AILO_GPU_NOTICES_OUTPUT}/${component}.deps")
  execute_process(COMMAND "${python}" "${CMAKE_CURRENT_LIST_DIR}/../scripts/generate_gpu_notices.py"
    "${base}" "${AILO_GPU_NOTICES_OUTPUT}/${component}.inputs"
    "${AILO_GPU_NOTICES_OUTPUT}/${component}.deps" "${AILO_GPU_NOTICES_OUTPUT}/${component}-notices.txt"
    COMMAND_ERROR_IS_FATAL ANY)
endforeach()
