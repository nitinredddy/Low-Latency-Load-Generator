cmake_minimum_required(VERSION 3.16)
project(low_latency_load_generator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(echo_server src/echo_server.cpp)
add_executable(load_generator src/load_generator.cpp)

target_compile_options(echo_server PRIVATE -Wall -Wextra -Wpedantic)
target_compile_options(load_generator PRIVATE -Wall -Wextra -Wpedantic)

enable_testing()
add_test(NAME python_smoke
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/smoke_test.py
            $<TARGET_FILE:echo_server>
            $<TARGET_FILE:load_generator>)
