cmake_minimum_required(VERSION 3.16)
project(microsql CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(
  fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG        12.1.0
)
FetchContent_Declare(
  doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG        v2.5.2
)
FetchContent_MakeAvailable(fmt doctest)

set(SOURCES
    src/lexer/token.cpp
    src/lexer/lexer.cpp
    src/parser/parser.cpp
    src/page/page.cpp
)

set(TEST_SOURCES
    tests/lexer_test.cpp
    tests/parser_test.cpp
    tests/page_test.cpp
)

add_library(microsql_lib ${SOURCES})
target_include_directories(microsql_lib PUBLIC src)
target_compile_options(microsql_lib PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(microsql_lib PUBLIC fmt::fmt)

add_executable(microsql src/main.cpp)
target_link_libraries(microsql PRIVATE microsql_lib)

foreach(TEST_SRC ${TEST_SOURCES})
  get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)
  add_executable(${TEST_NAME} ${TEST_SRC})
  target_link_libraries(${TEST_NAME} PRIVATE microsql_lib doctest::doctest)
endforeach()
