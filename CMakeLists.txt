cmake_minimum_required(VERSION 3.25)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(COMPILE_OPTS -Wall -Wextra -Werror -Wshadow -Wcast-align)

project(sqlite VERSION 1.0 LANGUAGES C)

add_library(internals src/pager.c)
target_include_directories(internals PUBLIC include)
target_compile_options(internals PRIVATE ${COMPILE_OPTS})

add_executable(sqlite_toy sqlite.c)
target_link_libraries(sqlite_toy internals)
target_compile_options(sqlite_toy PRIVATE ${COMPILE_OPTS})

add_executable(tests tests/pager_test.c)
target_link_libraries(tests internals)
target_compile_options(tests PRIVATE ${COMPILE_OPTS})