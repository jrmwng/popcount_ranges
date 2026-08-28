cmake_minimum_required(VERSION 3.10)

project(popcount_ranges LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

add_executable(popcount_ranges
    src/main.cpp
)

target_include_directories(popcount_ranges
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(BUILD_TESTING)
    add_executable(popcount_set_tests
        tests/popcount_set_tests.cpp
    )

    target_include_directories(popcount_set_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    add_test(NAME popcount_set_tests
             COMMAND $<TARGET_FILE:popcount_set_tests>)
endif()