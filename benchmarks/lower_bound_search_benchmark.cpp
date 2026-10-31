#include <popcount_ranges/lower_bound.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <string_view>
#include <vector>

#if defined(_MSC_VER)
#define POPCOUNT_RANGES_NOINLINE __declspec(noinline)
#else
#define POPCOUNT_RANGES_NOINLINE __attribute__((noinline))
#endif

#ifndef POPCOUNT_RANGES_BENCHMARK_COMPILER
#define POPCOUNT_RANGES_BENCHMARK_COMPILER "unknown"
#endif

namespace
{
volatile std::uint64_t sink = 0;
std::uint32_t benchmark_seed = 0x5eedu;

template <typename T>
POPCOUNT_RANGES_NOINLINE std::size_t custom_search(
    std::vector<T> const& values, T query)
{
    return static_cast<std::size_t>(
        jrmwng::popcount_ranges::lower_bound(
            values.data(), values.data() + values.size(), query,
            std::less<>{}) -
        values.data());
}

template <typename T>
POPCOUNT_RANGES_NOINLINE std::size_t standard_search(
    std::vector<T> const& values, T query)
{
    return static_cast<std::size_t>(
        std::lower_bound(values.begin(), values.end(), query) -
        values.begin());
}

template <typename Search>
double measure(
    std::vector<std::uint64_t> const& queries, Search search,
    std::size_t repetitions, std::uint64_t& checksum)
{
    std::array<double, 5> samples{};
    checksum = 0;
    for (double& sample : samples)
    {
        std::uint64_t result = 0;
        auto const started = std::chrono::steady_clock::now();
        for (std::size_t repetition = 0; repetition < repetitions; ++repetition)
            for (std::uint64_t query : queries)
                result += search(query);
        sample = std::chrono::duration<double, std::nano>(
                     std::chrono::steady_clock::now() - started)
                     .count() /
                 static_cast<double>(repetitions * queries.size());
        checksum = result;
        sink += result;
    }

    std::sort(samples.begin(), samples.end());
    return samples[samples.size() / 2u];
}

bool benchmark_size(
    std::size_t size, std::size_t query_count, std::size_t repetitions)
{
    std::vector<std::uint64_t> values(size);
    for (std::size_t index = 0; index < size; ++index)
        values[index] = static_cast<std::uint64_t>(index) * 2u;

    std::mt19937 generator(
        benchmark_seed + static_cast<std::uint32_t>(size));
    std::uniform_int_distribution<std::size_t> position(0, size);
    std::vector<std::uint64_t> queries;
    queries.reserve(query_count);
    for (std::size_t index = 0; index < query_count; ++index)
    {
        std::size_t const insertion_index = position(generator);
        queries.push_back(insertion_index == size
            ? std::numeric_limits<std::uint64_t>::max()
            : static_cast<std::uint64_t>(insertion_index) * 2u);
    }

    std::uint64_t expected_checksum = 0;
    for (std::uint64_t query : queries)
        expected_checksum += static_cast<std::uint64_t>(
            std::lower_bound(values.begin(), values.end(), query) -
            values.begin());
    expected_checksum *= repetitions;

    auto report = [&](std::string_view implementation, auto search)
    {
        std::uint64_t checksum = 0;
        double const ns_per_query = measure(
            queries, search, repetitions, checksum);
        if (checksum != expected_checksum)
        {
            std::cerr << implementation << " checksum mismatch for N="
                      << size << ": expected " << expected_checksum
                      << ", got " << checksum << '\n';
            return false;
        }

        std::cout << POPCOUNT_RANGES_BENCHMARK_COMPILER << ','
                  << benchmark_seed << ",64," << size << ",uniform,"
                  << query_count << ',' << implementation << ','
                  << std::fixed << std::setprecision(3) << ns_per_query
                  << ',' << checksum << '\n';
        return true;
    };

    return report("custom", [&](std::uint64_t query)
                  { return custom_search(values, query); }) &&
           report("std", [&](std::uint64_t query)
                  { return standard_search(values, query); });
}
}

int main(int argc, char const* argv[])
{
    bool quick = false;
    for (int index = 1; index < argc; ++index)
    {
        std::string_view const argument(argv[index]);
        if (argument == "--quick")
        {
            quick = true;
        }
        else if (argument == "--seed" && index + 1 < argc)
        {
            std::string_view const value(argv[++index]);
            auto const [end, error] = std::from_chars(
                value.data(), value.data() + value.size(), benchmark_seed);
            if (error != std::errc{} || end != value.data() + value.size())
            {
                std::cerr << "invalid seed: " << value << '\n';
                return 2;
            }
        }
        else
        {
            std::cerr << "usage: lower_bound_search_benchmark "
                         "[--quick] [--seed value]\n";
            return 2;
        }
    }

    std::size_t const repetitions = quick ? 2u : 32u;
    std::cout << "compiler,query_seed,type_bits,N,query_distribution,"
                 "query_count,implementation,median_ns_per_query,checksum\n";
    bool passed = true;
    for (std::size_t size : {
             32u, 64u, 128u, 256u, 512u, 1024u, 2048u, 4096u,
             8192u, 16384u, 32768u, 65536u, 131072u, 262144u})
    {
        std::size_t const query_count = quick
            ? 512u
            : size > 8192u
                ? std::max<std::size_t>(512u, 16'384u * 8192u / size)
                : 16'384u;
        if (!benchmark_size(size, query_count, repetitions))
        {
            passed = false;
            break;
        }
    }

    std::cerr << "sink=" << sink << '\n';
    return passed ? 0 : 1;
}
