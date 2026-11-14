cmake_minimum_required(VERSION 3.20)

project(popcount_ranges VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(POPCOUNT_RANGES_SIMD "Use the C++26 <simd> implementation" OFF)
option(POPCOUNT_RANGES_BUILD_BENCHMARKS "Build the benchmarks" ON)

include(CTest)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(POPCOUNT_RANGES_SAMPLING_THRESHOLDS "1;2;4;8;12;16;20;24;28;32" CACHE STRING
    "Thresholds used to build sampling benchmark variants")

add_library(popcount_ranges INTERFACE)
add_library(jrmwng::popcount_ranges ALIAS popcount_ranges)

target_compile_features(popcount_ranges INTERFACE cxx_std_23)
if(POPCOUNT_RANGES_SIMD)
    if(MSVC)
        target_compile_options(popcount_ranges INTERFACE /std:c++latest)
    else()
        target_compile_options(popcount_ranges INTERFACE -std=c++26)
    endif()
endif()
target_compile_definitions(popcount_ranges INTERFACE
    POPCOUNT_RANGES_SIMD=$<BOOL:${POPCOUNT_RANGES_SIMD}>
)
target_include_directories(popcount_ranges INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if(MSVC)
    target_compile_options(popcount_ranges INTERFACE /arch:AVX2)
else()
    target_compile_options(popcount_ranges INTERFACE -mavx2)
endif()

function(configure_popcount_target target)
    target_link_libraries(${target} PRIVATE jrmwng::popcount_ranges)
endfunction()

function(configure_sampling_benchmark target)
    configure_popcount_target(${target})
    target_compile_definitions(${target} PRIVATE
        "POPCOUNT_RANGES_BENCHMARK_COMPILER=\"${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}\""
    )
endfunction()

set(popcount_ranges_cmake_dir "${CMAKE_INSTALL_LIBDIR}/cmake/popcount_ranges")
configure_package_config_file(
    cmake/popcount_rangesConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/popcount_rangesConfig.cmake"
    INSTALL_DESTINATION "${popcount_ranges_cmake_dir}"
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/popcount_rangesConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion
)

install(TARGETS popcount_ranges EXPORT popcount_rangesTargets)
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(FILES LICENSE
    DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/${PROJECT_NAME}"
)
install(EXPORT popcount_rangesTargets
    FILE popcount_rangesTargets.cmake
    NAMESPACE jrmwng::
    DESTINATION "${popcount_ranges_cmake_dir}"
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/popcount_rangesConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/popcount_rangesConfigVersion.cmake"
    DESTINATION "${popcount_ranges_cmake_dir}"
)

if(BUILD_TESTING)
    add_executable(uint_tests
        tests/uint_tests.cpp
    )

    configure_popcount_target(uint_tests)

    add_test(NAME uint_tests
             COMMAND $<TARGET_FILE:uint_tests>)

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

    add_executable(ncr_tests tests/ncr_tests.cpp)
    configure_popcount_target(ncr_tests)
    add_test(NAME ncr_tests COMMAND $<TARGET_FILE:ncr_tests>)

    add_executable(ncr_std_lower_bound_tests tests/ncr_tests.cpp)
    configure_popcount_target(ncr_std_lower_bound_tests)
    target_compile_definitions(ncr_std_lower_bound_tests PRIVATE POPCOUNT_RANGES_LOWER_BOUND_STD)
    add_test(NAME ncr_std_lower_bound_tests COMMAND $<TARGET_FILE:ncr_std_lower_bound_tests>)

    add_executable(ncr_descent_tests tests/ncr_tests.cpp)
    configure_popcount_target(ncr_descent_tests)
    target_compile_definitions(ncr_descent_tests PRIVATE POPCOUNT_RANGES_LOWER_BOUND_DESCENT)
    add_test(NAME ncr_descent_tests COMMAND $<TARGET_FILE:ncr_descent_tests>)

    add_executable(set_application_tests tests/set_application_tests.cpp)
    configure_popcount_target(set_application_tests)
    add_test(NAME set_application_tests COMMAND $<TARGET_FILE:set_application_tests>)

    add_executable(pdep_tests tests/pdep_tests.cpp)
    configure_popcount_target(pdep_tests)
    add_test(NAME pdep_tests COMMAND $<TARGET_FILE:pdep_tests>)
endif()

if(POPCOUNT_RANGES_BUILD_BENCHMARKS)
    add_executable(lower_bound_benchmark
        benchmarks/lower_bound_benchmark.cpp
    )

    configure_popcount_target(lower_bound_benchmark)

    add_executable(lower_bound_search_benchmark EXCLUDE_FROM_ALL
        benchmarks/lower_bound_search_benchmark.cpp
    )
    configure_sampling_benchmark(lower_bound_search_benchmark)

    add_executable(lower_bound_tuning EXCLUDE_FROM_ALL
        benchmarks/lower_bound_tuning.cpp
        benchmarks/lower_bound_tuning_uint8.cpp
        benchmarks/lower_bound_tuning_uint16.cpp
        benchmarks/lower_bound_tuning_uint32.cpp
        benchmarks/lower_bound_tuning_uint64.cpp
        benchmarks/lower_bound_tuning_float.cpp
        benchmarks/lower_bound_tuning_double.cpp
    )
    configure_sampling_benchmark(lower_bound_tuning)
    if(CMAKE_GENERATOR MATCHES "Ninja")
        set_property(GLOBAL APPEND PROPERTY JOB_POOLS lower_bound_tuning_compile=3)
        set_property(TARGET lower_bound_tuning
            PROPERTY JOB_POOL_COMPILE lower_bound_tuning_compile)
    endif()

    add_executable(popcount_set_benchmark
        benchmarks/popcount_set_benchmark.cpp
    )

    configure_popcount_target(popcount_set_benchmark)

    add_executable(set_operations_benchmark benchmarks/set_operations_benchmark.cpp)
    configure_popcount_target(set_operations_benchmark)

    add_executable(sampling_benchmark
        benchmarks/sampling_benchmark.cpp
    )

    configure_sampling_benchmark(sampling_benchmark)

    add_executable(sampling_benchmark_experimental EXCLUDE_FROM_ALL
        benchmarks/sampling_benchmark.cpp
    )
    configure_sampling_benchmark(sampling_benchmark_experimental)
    target_compile_definitions(sampling_benchmark_experimental PRIVATE
        POPCOUNT_RANGES_SAMPLING_EXPERIMENTAL
    )

    add_executable(subset_sum_benchmark
        benchmarks/subset_sum_benchmark.cpp
    )
    configure_popcount_target(subset_sum_benchmark)

    add_executable(simd_adapter_benchmark EXCLUDE_FROM_ALL
        benchmarks/simd_adapter_benchmark.cpp
    )

    configure_popcount_target(simd_adapter_benchmark)

    foreach(threshold IN LISTS POPCOUNT_RANGES_SAMPLING_THRESHOLDS)
        if(NOT threshold MATCHES "^[0-9]+$")
            message(FATAL_ERROR "Invalid sampling benchmark threshold: ${threshold}")
        endif()

        set(target sampling_benchmark_threshold_${threshold})
        add_executable(${target} EXCLUDE_FROM_ALL
            benchmarks/sampling_benchmark.cpp
        )
        configure_sampling_benchmark(${target})
        target_compile_definitions(${target} PRIVATE
            POPCOUNT_RANGES_LOWER_BOUND_DESCENT
            POPCOUNT_RANGES_LOWER_BOUND_THRESHOLD=${threshold}u
        )
    endforeach()

    add_executable(sampling_benchmark_std_lower_bound EXCLUDE_FROM_ALL
        benchmarks/sampling_benchmark.cpp
    )
    configure_sampling_benchmark(sampling_benchmark_std_lower_bound)
    target_compile_definitions(sampling_benchmark_std_lower_bound PRIVATE
        POPCOUNT_RANGES_LOWER_BOUND_STD
    )
endif()
