# Native-only installed-consumer check, including sanitizer link propagation.
function(run)
  execute_process(COMMAND ${ARGV} RESULT_VARIABLE result)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "Client package command failed (${result}): ${ARGV}")
  endif()
endfunction()
run("${CMAKE_COMMAND}" --install "${PROJECT_BUILD}" --prefix "${TEST_ROOT}/install")
run("${CMAKE_COMMAND}" -S "${TEST_SOURCE}" -B "${TEST_ROOT}/build"
  "-DCMAKE_PREFIX_PATH=${TEST_ROOT}/install" "-DCMAKE_CXX_COMPILER=${TEST_COMPILER}")
run("${CMAKE_COMMAND}" --build "${TEST_ROOT}/build")
run("${TEST_ROOT}/build/client-consumer")
