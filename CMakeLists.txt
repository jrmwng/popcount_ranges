cmake_minimum_required(VERSION 3.10)

project(popcount_ranges LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
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

# Add AVX2 flags depending on compiler
if (MSVC)
    target_compile_options(popcount_ranges PRIVATE /arch:AVX2)
else()
    target_compile_options(popcount_ranges PRIVATE -mavx2)
endif()

if(BUILD_TESTING)
    add_executable(popcount_set_tests
        tests/popcount_set_tests.cpp
    )

    target_include_directories(popcount_set_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    if (MSVC)
        target_compile_options(popcount_set_tests PRIVATE /arch:AVX2)
    else()
        target_compile_options(popcount_set_tests PRIVATE -mavx2)
    endif()

    add_test(NAME popcount_set_tests
             COMMAND $<TARGET_FILE:popcount_set_tests>)
endif()
