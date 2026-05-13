cmake_minimum_required(VERSION 3.25)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(COMPILE_OPTS -Wall -Wextra -Werror -Wshadow -Wcast-align)

project(sqlite VERSION 1.0 LANGUAGES C)

add_library(collections src/collections/vector.c)
target_include_directories(collections PUBLIC include/collections)
target_compile_options(collections PRIVATE ${COMPILE_OPTS})

add_library(storage src/storage/pager.c src/storage/journal.c)
target_include_directories(storage PUBLIC include/storage)
target_link_libraries(storage collections)
target_compile_options(storage PRIVATE ${COMPILE_OPTS})

add_executable(sqlite_toy sqlite.c)
target_link_libraries(sqlite_toy storage)
target_compile_options(sqlite_toy PRIVATE ${COMPILE_OPTS})

add_executable(journal_tests tests/storage/journal_test.c) 
target_link_libraries(journal_tests storage)
target_compile_options(journal_tests PRIVATE ${COMPILE_OPTS})

add_executable(pager_tests tests/storage/pager_test.c) 
target_link_libraries(pager_tests storage)
target_compile_options(pager_tests PRIVATE ${COMPILE_OPTS})

add_test(NAME pager COMMAND pager_tests)
add_test(NAME journal COMMAND journal_tests)