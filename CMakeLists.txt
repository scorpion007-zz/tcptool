# Top level project for tcptool.
#
project (tcptool)

cmake_minimum_required (VERSION 3.3)

include (common.cmake)

set (SOURCES 
	src/main.cpp
	)

set (HEADERS
	)

set (LIBS ws2_32)

source_group("Headers" FILES ${HEADERS})

add_executable(tcptool ${SOURCES} ${HEADERS})
target_link_libraries (tcptool ${LIBS})

# Subprojects
#
# add_subdirectory(foo)
