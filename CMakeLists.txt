cmake_minimum_required(VERSION 3.16)
project(low_latency_load_generator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# macOS AppleClang standard library headers resolution
if(APPLE)
    execute_process(
        COMMAND xcrun --show-sdk-path
        OUTPUT_VARIABLE MACOS_SDK_PATH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(EXISTS "${MACOS_SDK_PATH}/usr/include/c++/v1")
        include_directories(SYSTEM "${MACOS_SDK_PATH}/usr/include/c++/v1")
    endif()
endif()

# Global include directory
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Common compiler options
add_compile_options(-Wall -Wextra -Wpedantic -O3)

# Threading library
find_package(Threads REQUIRED)

# Executables
add_executable(echo_server src/echo_server.cpp)
target_link_libraries(echo_server PRIVATE Threads::Threads)

add_executable(load_generator src/load_generator.cpp)
target_link_libraries(load_generator PRIVATE Threads::Threads)

# Unit Tests
enable_testing()

add_executable(test_histogram tests/test_histogram.cpp)
target_link_libraries(test_histogram PRIVATE Threads::Threads)
add_test(NAME unit_histogram COMMAND test_histogram)

add_executable(test_protocol tests/test_protocol.cpp)
target_link_libraries(test_protocol PRIVATE Threads::Threads)
add_test(NAME unit_protocol COMMAND test_protocol)

# Python Smoke & Integration Tests
add_test(NAME python_smoke
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/smoke_test.py
            $<TARGET_FILE:echo_server>
            $<TARGET_FILE:load_generator>)
