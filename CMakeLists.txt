cmake_minimum_required(VERSION 3.25)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(sqlite VERSION 1.0 LANGUAGES C)

add_library(internals src/pager.c)
target_include_directories(internals PUBLIC include)

add_executable(sqlite_bin sqlite.c)
target_link_libraries(sqlite_bin internals)

add_executable(tests_bin tests/pager_test.c)
target_link_libraries(tests_bin internals)