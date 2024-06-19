cmake_minimum_required(VERSION 3.22 FATAL_ERROR)

project(TopDec LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(topdec)
target_sources(topdec PRIVATE
	compress.cpp
	compress.h
	decompress.cpp
	decompress.h
	main.cpp
	file.cpp
	file.h
	text.cpp
	text.h
)
target_compile_definitions(topdec
	PUBLIC FILE_WRAPPER_WITH_STD_FILESYSTEM
)
