cmake_minimum_required(VERSION 3.10)

project(popcount_ranges LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

option(POPCOUNT_RANGES_BUILD_BENCHMARKS "Build benchmarks" OFF)

function(configure_popcount_target target)
    target_include_directories(${target}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    if (MSVC)
        target_compile_options(${target} PRIVATE /arch:AVX2)
    else()
        target_compile_options(${target} PRIVATE -mavx2)
    endif()
endfunction()

add_executable(popcount_ranges
    src/main.cpp
)

configure_popcount_target(popcount_ranges)

if(BUILD_TESTING)
    add_executable(popcount_set_tests
        tests/popcount_set_tests.cpp
    )

    configure_popcount_target(popcount_set_tests)

    add_test(NAME popcount_set_tests
             COMMAND $<TARGET_FILE:popcount_set_tests>)

    add_executable(lower_bound_tests
        tests/lower_bound_tests.cpp
    )

    configure_popcount_target(lower_bound_tests)

    add_test(NAME lower_bound_tests
             COMMAND $<TARGET_FILE:lower_bound_tests>)

    add_executable(data_par_simd_tests
        tests/data_par_simd_tests.cpp
    )

    configure_popcount_target(data_par_simd_tests)

    add_test(NAME data_par_simd_tests
             COMMAND $<TARGET_FILE:data_par_simd_tests>)
endif()

if(POPCOUNT_RANGES_BUILD_BENCHMARKS)
    add_executable(lower_bound_benchmark
        benchmarks/lower_bound_benchmark.cpp
    )

    configure_popcount_target(lower_bound_benchmark)

    add_executable(popcount_set_benchmark
        benchmarks/popcount_set_benchmark.cpp
    )

    configure_popcount_target(popcount_set_benchmark)
endif()
