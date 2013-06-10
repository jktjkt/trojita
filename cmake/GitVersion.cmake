execute_process(COMMAND ${GIT_EXECUTABLE} describe --dirty --long --always
    WORKING_DIRECTORY ${SOURCE_DIR}
    OUTPUT_VARIABLE GIT_DESCRIBE_OUTPUT)
string(REPLACE "\n" "" GIT_DESCRIBE_OUTPUT ${GIT_DESCRIBE_OUTPUT})
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/gitversion.in "const char *gitVersion = \"${GIT_DESCRIBE_OUTPUT}\";\n")
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_BINARY_DIR}/gitversion.in ${CMAKE_BINARY_DIR}/gitversion.hpp)
