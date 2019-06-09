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

set (LIBS )

source_group("Headers" FILES ${HEADERS})

add_executable(jasperchess WIN32 ${SOURCES} ${HEADERS})
target_link_libraries (jasperchess ${LIBS})

# Subprojects
#
# add_subdirectory(foo)
