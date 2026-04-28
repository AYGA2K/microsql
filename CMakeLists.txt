cmake_minimum_required(VERSION 3.16)
project(microsql CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# set(SOURCES
#     # src/lexer/lexer.cpp
# )

set(TEST_SOURCES
    # tests/lexer_test.cpp
)

# add_library(microsql_lib ${SOURCES})
# target_include_directories(microsql_lib PUBLIC src)
# target_compile_options(microsql_lib PRIVATE -Wall -Wextra -Wpedantic)

add_executable(microsql src/main.cpp)
# target_link_libraries(microsql PRIVATE microsql_lib)

foreach(TEST_SRC ${TEST_SOURCES})
  get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)
  add_executable(${TEST_NAME} ${TEST_SRC})
  target_link_libraries(${TEST_NAME} PRIVATE microsql_lib)
endforeach()
