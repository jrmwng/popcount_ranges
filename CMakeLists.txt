cmake_minimum_required(VERSION 3.20)

project(popcount_ranges VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(POPCOUNT_RANGES_SAMPLING_THRESHOLDS "1;2;4;8;12;16;20;24;28;32" CACHE STRING
    "Thresholds used to build sampling benchmark variants")

add_library(popcount_ranges INTERFACE)
add_library(jrmwng::popcount_ranges ALIAS popcount_ranges)

target_compile_features(popcount_ranges INTERFACE cxx_std_23)
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

    add_executable(sampling_benchmark
        benchmarks/sampling_benchmark.cpp
    )

    configure_popcount_target(sampling_benchmark)
    target_compile_definitions(sampling_benchmark PRIVATE POPCOUNT_RANGES_LOWER_BOUND_THRESHOLD=4u)

    foreach(threshold IN LISTS POPCOUNT_RANGES_SAMPLING_THRESHOLDS)
        if(NOT threshold MATCHES "^[0-9]+$")
            message(FATAL_ERROR "Invalid sampling benchmark threshold: ${threshold}")
        endif()

        if(threshold STREQUAL "4")
            continue()
        endif()

        set(target sampling_benchmark_threshold_${threshold})
        add_executable(${target}
            benchmarks/sampling_benchmark.cpp
        )
        configure_popcount_target(${target})
        target_compile_definitions(${target} PRIVATE POPCOUNT_RANGES_LOWER_BOUND_THRESHOLD=${threshold}u)
    endforeach()
endif()
