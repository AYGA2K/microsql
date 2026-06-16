cmake_minimum_required(VERSION 3.16)
project(microsql CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(
  doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG        v2.5.2
)
FetchContent_MakeAvailable(doctest)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/**/*.cpp)

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)

add_library(microsql_lib ${SOURCES})
target_include_directories(microsql_lib PUBLIC src)
target_compile_options(microsql_lib PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(microsql_lib PUBLIC)

add_executable(microsql src/main.cpp)
target_link_libraries(microsql PRIVATE microsql_lib)

foreach(TEST_SRC ${TEST_SOURCES})
  get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)
  add_executable(${TEST_NAME} ${TEST_SRC})
  target_link_libraries(${TEST_NAME} PRIVATE microsql_lib doctest::doctest)
endforeach()
