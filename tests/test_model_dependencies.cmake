cmake_minimum_required(VERSION 3.24)
execute_process(COMMAND "${AILO_NM}" -g --defined-only "${AILO_STATIC_LIBRARY}"
  RESULT_VARIABLE result OUTPUT_VARIABLE symbols ERROR_VARIABLE error)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Cannot inspect model static dependency boundary: ${error}")
endif()
if(symbols MATCHES "[ \t](curl_|Curl_|EVP_|OPENSSL_|SSL_|X509_|lonejson_|ffi_|pslog_)[A-Za-z0-9_]+([\r\n]|$)")
  message(FATAL_ERROR "libailo.a contains SDK dependency implementation objects")
endif()
if(DEFINED AILO_SHARED_LIBRARY)
  execute_process(COMMAND "${AILO_READELF}" -d "${AILO_SHARED_LIBRARY}"
    RESULT_VARIABLE result OUTPUT_VARIABLE metadata ERROR_VARIABLE error)
  if(NOT result EQUAL 0 OR NOT metadata MATCHES "NEEDED[^\r\n]*libcurl\\.so" OR
     NOT metadata MATCHES "NEEDED[^\r\n]*libcrypto\\.so" OR
     NOT metadata MATCHES "NEEDED[^\r\n]*liblonejson\\.so" OR
     NOT metadata MATCHES "NEEDED[^\r\n]*libffi\\.so" OR
     NOT metadata MATCHES "NEEDED[^\r\n]*libpslog\\.so")
    message(FATAL_ERROR "Shared model dependencies must remain separate SDK libraries: ${metadata}${error}")
  endif()
endif()
message(STATUS "Static/shared model transport dependencies remain independent SDK libraries")
