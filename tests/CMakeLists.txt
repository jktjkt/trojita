
include_directories( ${CMAKE_SOURCE_DIR}/Imap )
set( EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR} )
########### next target ###############

set(testrfccodecs_SRCS testrfccodecs.cpp )

qt4_automoc(${testrfccodecs_SRCS})

add_executable(testrfccodecs ${testrfccodecs_SRCS})
add_test(testrfccodecs "testrfccodecs")

target_link_libraries(testrfccodecs Imap ${QT_QTTEST_LIBRARY})

########### install files ###############
