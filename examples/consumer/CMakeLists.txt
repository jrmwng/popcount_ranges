cmake_minimum_required(VERSION 3.20)

project(popcount_ranges_consumer LANGUAGES CXX)

find_package(popcount_ranges CONFIG REQUIRED)

add_executable(popcount_ranges_consumer main.cpp)
target_link_libraries(popcount_ranges_consumer PRIVATE jrmwng::popcount_ranges)
