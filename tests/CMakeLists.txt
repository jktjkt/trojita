# shamelessly stolen from KDE SVN

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})

include_directories( ${CMAKE_SOURCE_DIR}/Imap )
set( EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR} )

macro(trojita_test fname)
    set(test_${fname}_SRCS test_${fname}.cpp)
    qt4_automoc(${test_${fname}_SRCS})
    add_executable(test_${fname} ${test_${fname}_SRCS})
    target_link_libraries(test_${fname} Imap ${QT_QTCORE_LIBRARY} ${QT_QTTEST_LIBRARY} ${QT_QTNETWORK_LIBRARY})
    add_test(test_${fname} test_${fname})
endmacro(trojita_test)

trojita_test(rfccodecs)
trojita_test(Imap_Parser_parse)
trojita_test(Imap_Responses)
trojita_test(Imap_LowLevelParser)
trojita_test(Imap_Message)
