cmake_minimum_required(VERSION 3.20)

project(
	WServer
	VERSION     0.1
	DESCRIPTION "It's a web server!"
	LANGUAGES   C
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)

set(CMAKE_C_FLAGS "-Wall -Wextra -std=gnu17 -O3")

add_subdirectory(src)
