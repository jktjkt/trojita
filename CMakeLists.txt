project(trojita)

find_package(Qt4 REQUIRED)

ADD_DEFINITIONS(${QT4_DEFINITIONS})

INCLUDE_DIRECTORIES(${QT_INCLUDES})

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})

set(libImap_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/Imap/Parser.cpp)

set(trojita_SRCS main.cpp ${libImap_SRCS})

qt4_automoc(${trojita_SRCS})

add_executable(trojita ${trojita_SRCS})

target_link_libraries(trojita ${QT_QTCORE_LIBRARY} ${QT_QTNETWORK_LIBRARY})
