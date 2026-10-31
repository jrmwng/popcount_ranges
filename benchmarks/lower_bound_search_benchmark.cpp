#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <popcount_ranges/lower_bound.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <immintrin.h>
#include <random>
#include <string_view>
#include <type_traits>
#include <thread>
#include <vector>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#ifndef POPCOUNT_RANGES_BENCHMARK_COMPILER
#define POPCOUNT_RANGES_BENCHMARK_COMPILER "unknown"
#endif

#if defined(_MSC_VER)
#define POPCOUNT_RANGES_NOINLINE __declspec(noinline)
#else
#define POPCOUNT_RANGES_NOINLINE __attribute__((noinline))
#endif

namespace
{
volatile std::uint64_t sink = 0;
std::uint32_t benchmark_seed = 0x5eedu;
std::size_t cache_working_set_bytes = 64u * 1024u * 1024u;
std::uint32_t benchmark_cpu = 0;
double tsc_frequency_hz = 0.0;

void flush_cache_region(void const* memory, std::size_t bytes)
{
    constexpr std::uintptr_t cache_line_size = 64u;
    std::uintptr_t const begin = reinterpret_cast<std::uintptr_t>(memory);
    std::uintptr_t const end = begin + bytes;
    std::uintptr_t line = begin & ~(cache_line_size - 1u);
    for (; line < end; line += cache_line_size)
        _mm_clflush(reinterpret_cast<void const*>(line));
    _mm_mfence();
}

void write_cache_region(std::vector<std::uint64_t>& memory)
{
    constexpr std::uintptr_t cache_line_size = 64u;
    std::uintptr_t const begin =
        reinterpret_cast<std::uintptr_t>(memory.data());
    std::uintptr_t const end = begin + memory.size() * sizeof(memory[0]);
    std::uintptr_t line =
        (begin + cache_line_size - 1u) & ~(cache_line_size - 1u);
    for (; line + sizeof(std::uint64_t) <= end;
         line += cache_line_size)
    {
        auto* const value =
            reinterpret_cast<volatile std::uint64_t*>(line);
        *value = *value + 1u;
    }
    _mm_sfence();
}

template <typename T, std::size_t N>
void prefetch_array(std::array<T, N> const& values)
{
    constexpr std::size_t cache_line_size = 64u;
    auto const* const bytes =
        reinterpret_cast<char const*>(values.data());
    for (std::size_t offset = 0; offset < sizeof(values);
         offset += cache_line_size)
    {
        _mm_prefetch(bytes + offset, _MM_HINT_T0);
        (void)*reinterpret_cast<volatile T const*>(bytes + offset);
    }
    _mm_lfence();
}

std::uint64_t read_tsc_start()
{
    _mm_lfence();
    std::atomic_signal_fence(std::memory_order_seq_cst);
    return __rdtsc();
}

std::uint64_t read_tsc_end()
{
    std::atomic_signal_fence(std::memory_order_seq_cst);
    unsigned int auxiliary = 0;
    std::uint64_t const ticks = __rdtscp(&auxiliary);
    _mm_lfence();
    return ticks;
}

double calibrate_tsc_frequency()
{
    std::array<double, 3> frequencies{};
    for (double& frequency : frequencies)
    {
        auto const wall_start = std::chrono::steady_clock::now();
        std::uint64_t const tsc_start = read_tsc_start();
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        std::uint64_t const tsc_end = read_tsc_end();
        auto const wall_end = std::chrono::steady_clock::now();
        double const elapsed_seconds =
            std::chrono::duration<double>(wall_end - wall_start).count();
        frequency = static_cast<double>(tsc_end - tsc_start) /
                    elapsed_seconds;
    }
    std::sort(frequencies.begin(), frequencies.end());
    return frequencies[frequencies.size() / 2u];
}

#if defined(_WIN32)
DWORD_PTR pin_benchmark_thread()
{
    DWORD_PTR process_mask = 0;
    DWORD_PTR system_mask = 0;
    if (!GetProcessAffinityMask(
            GetCurrentProcess(), &process_mask, &system_mask) ||
        process_mask == 0)
    {
        std::cerr << "GetProcessAffinityMask failed: " << GetLastError()
                  << '\n';
        return 0;
    }

    DWORD_PTR selected = process_mask & (~process_mask + 1u);
    DWORD_PTR const previous =
        SetThreadAffinityMask(GetCurrentThread(), selected);
    if (previous == 0)
    {
        std::cerr << "SetThreadAffinityMask failed: " << GetLastError()
                  << '\n';
        return 0;
    }
    benchmark_cpu = GetCurrentProcessorNumber();
    return previous;
}
#endif

template <typename T, std::size_t N>
POPCOUNT_RANGES_NOINLINE std::uint64_t custom_search(
    std::array<T, N> const& values, T query)
{
    return static_cast<std::uint64_t>(
        jrmwng::popcount_ranges::lower_bound(
            values.data(), values.data() + N, query, std::less<>{}) - values.data());
}

template <typename T, std::size_t N>
POPCOUNT_RANGES_NOINLINE std::uint64_t std_search(
    std::array<T, N> const& values, T query)
{
    return static_cast<std::uint64_t>(
        std::lower_bound(values.begin(), values.end(), query) - values.begin());
}

template <typename T, std::size_t N, std::size_t Threshold>
POPCOUNT_RANGES_NOINLINE std::uint64_t descent_search(
    jrmwng::popcount_ranges::lower_bound_descent<
        std::array<T, N>, Threshold> const& descent,
    T query)
{
    return static_cast<std::uint64_t>(descent(0, N, query, std::less<>{}));
}

template <typename T, std::size_t N, std::size_t Threshold>
POPCOUNT_RANGES_NOINLINE std::uint64_t binary_then_custom_search(
    std::array<T, N> const& values, T query)
{
    T const* first = values.data();
    std::size_t count = N;
    while (count > Threshold)
    {
        std::size_t const half = count / 2u;
        T const* const middle = first + half;
        if (*middle < query)
        {
            first = middle;
            ++first;
            count -= half + 1u;
        }
        else
        {
            count = half;
        }
    }

    T const* const last = first + count;
    return static_cast<std::uint64_t>(
        jrmwng::popcount_ranges::lower_bound(
            first, last, query, std::less<>{}) - values.data());
}

template <typename T, std::size_t N, std::size_t Threshold>
POPCOUNT_RANGES_NOINLINE std::uint64_t old_binary_then_custom_search(
    std::array<T, N> const& values, T query)
{
    std::size_t first = 0;
    std::size_t last = N;
    while (last - first > Threshold)
    {
        std::size_t const middle = first + (last - first) / 2u;
        if (values[middle] < query)
            first = middle + 1u;
        else
            last = middle;
    }

    return static_cast<std::uint64_t>(
        jrmwng::popcount_ranges::lower_bound(
            values.data() + first, values.data() + last,
            query, std::less<>{}) - values.data());
}

template <typename T, std::size_t N, std::size_t Threshold>
POPCOUNT_RANGES_NOINLINE std::uint64_t old_binary_head_search(
    std::array<T, N> const& values, T query)
{
    std::size_t first = 0;
    std::size_t last = N;
    while (last - first > Threshold)
    {
        std::size_t const middle = first + (last - first) / 2u;
        if (values[middle] < query)
            first = middle + 1u;
        else
            last = middle;
    }
    return static_cast<std::uint64_t>(first);
}

template <int Hint, typename T, std::size_t N>
POPCOUNT_RANGES_NOINLINE std::uint64_t hinted_binary_head_search(
    std::array<T, N> const& values, T query)
{
    std::size_t first = 0;
    std::size_t last = N;
    while (first < last)
    {
        std::size_t const middle = first + (last - first) / 2u;
        if constexpr (Hint > 0)
        {
            if (values[middle] < query) [[likely]]
                first = middle + 1u;
            else
                last = middle;
        }
        else if constexpr (Hint < 0)
        {
            if (values[middle] < query) [[unlikely]]
                first = middle + 1u;
            else
                last = middle;
        }
        else
        {
            if (values[middle] < query)
                first = middle + 1u;
            else
                last = middle;
        }
    }
    return static_cast<std::uint64_t>(first);
}

template <typename T, std::size_t N, std::size_t Threshold>
POPCOUNT_RANGES_NOINLINE std::uint64_t binary_then_std_search(
    std::array<T, N> const& values, T query)
{
    T const* first = values.data();
    std::size_t count = N;
    while (count > Threshold)
    {
        std::size_t const half = count / 2u;
        T const* const middle = first + half;
        if (*middle < query)
        {
            first = middle;
            ++first;
            count -= half + 1u;
        }
        else
        {
            count = half;
        }
    }

    T const* const last = first + count;
    return static_cast<std::uint64_t>(
        std::lower_bound(first, last, query) - values.data());
}

struct SearchWindow
{
    std::size_t first;
    std::size_t last;
};

template <typename T, std::size_t N, std::size_t Threshold>
SearchWindow binary_tail_window(std::array<T, N> const& values, T query)
{
    std::size_t first = 0;
    std::size_t last = N;
    while (last - first > Threshold)
    {
        std::size_t const middle = first + (last - first) / 2u;
        if (values[middle] < query)
            first = middle + 1u;
        else
            last = middle;
    }
    return {first, last};
}

template <typename T, std::size_t N, std::size_t Threshold>
POPCOUNT_RANGES_NOINLINE std::uint64_t binary_head_search(
    std::array<T, N> const& values, T query)
{
    T const* first = values.data();
    std::size_t count = N;
    while (count > Threshold)
    {
        std::size_t const half = count / 2u;
        T const* const middle = first + half;
        if (*middle < query)
        {
            first = middle;
            ++first;
            count -= half + 1u;
        }
        else
        {
            count = half;
        }
    }
    return static_cast<std::uint64_t>(first - values.data());
}

template <typename T, std::size_t N>
POPCOUNT_RANGES_NOINLINE std::uint64_t custom_tail_search(
    std::array<T, N> const& values, T query, SearchWindow window)
{
    return static_cast<std::uint64_t>(
        jrmwng::popcount_ranges::lower_bound(
            values.data() + window.first, values.data() + window.last,
            query, std::less<>{}) - values.data());
}

template <typename T, std::size_t N>
POPCOUNT_RANGES_NOINLINE std::uint64_t std_tail_search(
    std::array<T, N> const& values, T query, SearchWindow window)
{
    return static_cast<std::uint64_t>(
        std::lower_bound(
            values.data() + window.first, values.data() + window.last, query) -
        values.data());
}

template <typename T, typename Search>
double measure(std::vector<T> const& queries, Search search,
               std::size_t repetitions, std::uint64_t& checksum)
{
    std::array<double, 5> samples{};
    checksum = 0;
    bool first_sample = true;
    std::uint64_t warmup_checksum = 0;
    for (T query : queries)
        warmup_checksum += search(query);
    sink += warmup_checksum;

    for (double& sample : samples)
    {
        std::uint64_t result = 0;
        auto const started = std::chrono::steady_clock::now();
        for (std::size_t repetition = 0; repetition < repetitions; ++repetition)
        {
            for (T query : queries)
                result += search(query);
        }
        sample = std::chrono::duration<double, std::nano>(
                     std::chrono::steady_clock::now() - started).count() /
                 static_cast<double>(repetitions * queries.size());
        if (!first_sample && checksum != result)
            std::cerr << "inconsistent checksum across measurement repeats\n";
        checksum = result;
        first_sample = false;
        sink += result;
    }

    std::sort(samples.begin(), samples.end());
    return samples[samples.size() / 2];
}

template <typename T, typename Search>
double measure_indexed(std::vector<T> const& queries, Search search,
                       std::size_t repetitions, std::uint64_t& checksum)
{
    std::array<double, 5> samples{};
    checksum = 0;
    bool first_sample = true;
    std::uint64_t warmup_checksum = 0;
    for (std::size_t index = 0; index < queries.size(); ++index)
        warmup_checksum += search(index, queries[index]);
    sink += warmup_checksum;

    for (double& sample : samples)
    {
        std::uint64_t result = 0;
        auto const started = std::chrono::steady_clock::now();
        for (std::size_t repetition = 0; repetition < repetitions; ++repetition)
        {
            for (std::size_t index = 0; index < queries.size(); ++index)
                result += search(index, queries[index]);
        }
        sample = std::chrono::duration<double, std::nano>(
                     std::chrono::steady_clock::now() - started).count() /
                 static_cast<double>(repetitions * queries.size());
        if (!first_sample && checksum != result)
            std::cerr << "inconsistent checksum across measurement repeats\n";
        checksum = result;
        first_sample = false;
        sink += result;
    }

    std::sort(samples.begin(), samples.end());
    return samples[samples.size() / 2];
}

template <typename T, typename Search>
bool report_search(std::size_t n, std::vector<T> const& queries,
                   Search search, std::size_t repetitions,
                   std::string_view implementation, std::size_t threshold,
                   std::uint64_t expected_checksum)
{
    std::uint64_t checksum = 0;
    double const ns_per_query = measure(
        queries, search, repetitions, checksum);
    if (checksum != expected_checksum)
    {
        std::cerr << implementation << " checksum mismatch for N=" << n
                  << ": expected " << expected_checksum << ", got " << checksum << '\n';
        return false;
    }

    std::cout << POPCOUNT_RANGES_BENCHMARK_COMPILER << ',' << benchmark_seed << ','
              << sizeof(T) * 8u << ',' << n << ",uniform," << queries.size() << ','
              << implementation << ',';
    if (threshold != 0)
        std::cout << threshold;
    std::cout << ','
              << std::fixed << std::setprecision(3) << ns_per_query << ','
              << checksum << '\n';
    return true;
}

template <typename T, typename Search>
bool report_indexed_search(std::size_t n, std::vector<T> const& queries,
                           Search search, std::size_t repetitions,
                           std::string_view implementation,
                           std::size_t threshold,
                           std::uint64_t expected_checksum)
{
    std::uint64_t checksum = 0;
    double const ns_per_query = measure_indexed(
        queries, search, repetitions, checksum);
    if (checksum != expected_checksum)
    {
        std::cerr << implementation << " checksum mismatch for N=" << n
                  << ": expected " << expected_checksum << ", got " << checksum << '\n';
        return false;
    }

    std::cout << POPCOUNT_RANGES_BENCHMARK_COMPILER << ',' << benchmark_seed << ','
              << sizeof(T) * 8u << ',' << n << ",uniform," << queries.size() << ','
              << implementation << ',' << threshold << ','
              << std::fixed << std::setprecision(3) << ns_per_query << ','
              << checksum << '\n';
    return true;
}

template <typename T, std::size_t N, typename Search>
bool report_branch_search(std::array<T, N> const& values,
                          std::vector<T> const& queries,
                          std::size_t repetitions,
                          std::string_view distribution,
                          std::string_view implementation,
                          Search search)
{
    std::uint64_t expected_checksum = 0;
    for (T query : queries)
        expected_checksum += static_cast<std::uint64_t>(
            std::lower_bound(values.begin(), values.end(), query) -
            values.begin());
    expected_checksum *= repetitions;

    std::uint64_t checksum = 0;
    double const ns_per_query = measure(
        queries, search, repetitions, checksum);
    if (checksum != expected_checksum)
    {
        std::cerr << implementation << " checksum mismatch for N=" << N
                  << " and " << distribution << ": expected "
                  << expected_checksum << ", got " << checksum << '\n';
        return false;
    }

    std::cout << POPCOUNT_RANGES_BENCHMARK_COMPILER << ',' << benchmark_seed
              << ',' << sizeof(T) * 8u << ',' << N << ',' << distribution
              << ',' << queries.size() << ',' << implementation
              << ",1," << std::fixed << std::setprecision(3)
              << ns_per_query << ',' << checksum << '\n';
    return true;
}

template <typename T, std::size_t N>
bool report_branch_cases(std::array<T, N> const& values,
                         std::size_t repetitions)
{
    constexpr std::size_t query_count = 4096;
    auto make_queries = [](std::size_t rank)
    {
        T const query = rank == N
            ? std::numeric_limits<T>::max()
            : static_cast<T>(rank * 2u);
        return std::vector<T>(query_count, query);
    };
    auto report_distribution = [&](std::string_view distribution,
                                   std::vector<T> const& queries)
    {
        return report_branch_search(
                   values, queries, repetitions, distribution,
                   "branch_head_unhinted",
                   [&](T query)
                   {
                       return hinted_binary_head_search<0>(values, query);
                   }) &&
               report_branch_search(
                   values, queries, repetitions, distribution,
                   "branch_head_likely_taken",
                   [&](T query)
                   {
                       return hinted_binary_head_search<1>(values, query);
                   }) &&
               report_branch_search(
                   values, queries, repetitions, distribution,
                   "branch_head_unlikely_taken",
                   [&](T query)
                   {
                       return hinted_binary_head_search<-1>(values, query);
                   });
    };

    std::vector<T> random_queries;
    random_queries.reserve(query_count);
    std::mt19937 generator(
        benchmark_seed + static_cast<std::uint32_t>(N));
    std::uniform_int_distribution<std::size_t> position(0, N);
    for (std::size_t index = 0; index < query_count; ++index)
    {
        std::size_t const rank = position(generator);
        random_queries.push_back(rank == N
            ? std::numeric_limits<T>::max()
            : static_cast<T>(rank * 2u));
    }

    constexpr std::size_t alternating_rank = []()
    {
        std::size_t rank = 0;
        std::size_t bit_index = 0;
        for (std::size_t bit = 1; bit < N; bit <<= 1u, ++bit_index)
            if ((bit_index & 1u) != 0)
                rank |= bit;
        return rank;
    }();

    return report_distribution("constant_false", make_queries(0)) &&
           report_distribution("constant_true", make_queries(N)) &&
           report_distribution(
               "alternating_path", make_queries(alternating_rank)) &&
           report_distribution("random_uniform", random_queries);
}

template <typename T, std::size_t N>
bool report_cache_state_cases(std::array<T, N>& values,
                              std::vector<std::uint64_t>& cache_working_set,
                              bool quick)
{
    struct TimedPosition
    {
        std::uint64_t cycles;
        std::size_t position;
    };

    constexpr std::size_t full_position_count = 513u;
    std::size_t const position_count = quick ? 9u : full_position_count;
    std::vector<std::size_t> positions;
    positions.reserve(position_count);
    for (std::size_t index = 0; index < position_count; ++index)
        positions.push_back(index * N / (position_count - 1u));
    std::mt19937 generator(
        benchmark_seed + static_cast<std::uint32_t>(N));
    std::shuffle(positions.begin(), positions.end(), generator);

    auto run_state = [&](std::string_view cache_state, auto prepare)
    {
        auto run_method = [&](std::string_view implementation,
                              std::size_t threshold, auto search)
        {
            std::vector<TimedPosition> timings;
            timings.reserve(positions.size());
            std::uint64_t checksum = 0;
            for (std::size_t position : positions)
            {
                prepare(values, cache_working_set);
                T const query = static_cast<T>(position * 2u);
                std::uint64_t const start = read_tsc_start();
                std::uint64_t const result = search(query);
                std::uint64_t const finish = read_tsc_end();
                checksum += result;
                timings.push_back({finish - start, position});
            }
            sink += checksum;

            std::sort(
                timings.begin(), timings.end(),
                [](TimedPosition const& left, TimedPosition const& right)
                {
                    return left.cycles < right.cycles;
                });

            TimedPosition const best = timings.front();
            TimedPosition const worst = timings.back();
            std::uint64_t const median_cycles =
                timings[timings.size() / 2u].cycles;
            std::uint64_t const p95_cycles =
                timings[(timings.size() * 95u) / 100u].cycles;
            auto cycles_to_ns = [](std::uint64_t cycles)
            {
                return static_cast<double>(cycles) * 1'000'000'000.0 /
                       tsc_frequency_hz;
            };
            std::uint64_t expected_checksum = 0;
            for (std::size_t position : positions)
                expected_checksum += position;
            if (checksum != expected_checksum)
            {
                std::cerr << implementation << " checksum mismatch for N="
                          << N << " and " << cache_state << ": expected "
                          << expected_checksum << ", got " << checksum
                          << '\n';
                return false;
            }

            std::cout << POPCOUNT_RANGES_BENCHMARK_COMPILER << ','
                      << benchmark_seed << ',' << sizeof(T) * 8u << ',' << N
                      << ',' << cache_working_set_bytes / (1024u * 1024u)
                      << ',' << benchmark_cpu << ',' << std::fixed
                      << std::setprecision(0) << tsc_frequency_hz << ','
                      << cache_state << ',' << positions.size() << ','
                      << implementation << ',' << threshold << ','
                      << best.position << ',' << best.cycles << ','
                      << std::setprecision(2) << cycles_to_ns(best.cycles)
                      << ',' << median_cycles << ','
                      << cycles_to_ns(median_cycles) << ',' << p95_cycles
                      << ',' << cycles_to_ns(p95_cycles) << ','
                      << worst.position << ',' << worst.cycles << ','
                      << cycles_to_ns(worst.cycles) << ',' << checksum << '\n';
            return true;
        };

        auto run_hybrids = [&](auto threshold_tag)
        {
            constexpr std::size_t threshold =
                decltype(threshold_tag)::value;
            return run_method(
                       "binary_then_custom", threshold,
                       [&](T query)
                       {
                           if constexpr (threshold == 16u)
                               return binary_then_custom_search<T, N, 16>(
                                   values, query);
                           else if constexpr (threshold == 32u)
                               return binary_then_custom_search<T, N, 32>(
                                   values, query);
                           else
                               return binary_then_custom_search<T, N, 64>(
                                   values, query);
                       }) &&
                   run_method(
                       "old_binary_then_custom", threshold,
                       [&](T query)
                       {
                           if constexpr (threshold == 16u)
                               return old_binary_then_custom_search<T, N, 16>(
                                   values, query);
                           else if constexpr (threshold == 32u)
                               return old_binary_then_custom_search<T, N, 32>(
                                   values, query);
                           else
                               return old_binary_then_custom_search<T, N, 64>(
                                   values, query);
                       });
        };

        return run_hybrids(std::integral_constant<std::size_t, 16>{}) &&
               run_hybrids(std::integral_constant<std::size_t, 32>{}) &&
               run_hybrids(std::integral_constant<std::size_t, 64>{}) &&
               run_method(
                   "std_lower_bound", 0,
                   [&](T query)
                   {
                       return std_search(values, query);
                   });
    };

    return run_state(
               "llc_dirty_working_set",
               [](std::array<T, N>&,
                  std::vector<std::uint64_t>& working_set)
               {
                   write_cache_region(working_set);
               }) &&
           run_state(
               "llc_flushed",
               [](std::array<T, N>& data,
                  std::vector<std::uint64_t>& working_set)
               {
                   flush_cache_region(
                       working_set.data(),
                       working_set.size() * sizeof(working_set[0]));
                   flush_cache_region(data.data(), sizeof(data));
               }) &&
           run_state(
               "llc_flushed_array_prefetched",
               [](std::array<T, N>& data,
                  std::vector<std::uint64_t>& working_set)
               {
                   flush_cache_region(
                       working_set.data(),
                       working_set.size() * sizeof(working_set[0]));
                   flush_cache_region(data.data(), sizeof(data));
                   prefetch_array(data);
               });
}

template <std::size_t N>
bool benchmark_cache_state_size(
    std::vector<std::uint64_t>& cache_working_set, bool quick)
{
    static std::array<std::uint64_t, N> values{};
    for (std::size_t index = 0; index < N; ++index)
        values[index] = index * 2u;
    return report_cache_state_cases(values, cache_working_set, quick);
}

template <typename T, std::size_t N, std::size_t Threshold>
bool report_binary_breakdown(std::array<T, N> const& values,
                             std::vector<T> const& queries,
                             std::size_t repetitions,
                             std::uint64_t expected_checksum)
{
    std::vector<SearchWindow> windows;
    windows.reserve(queries.size());
    std::uint64_t head_checksum = 0;
    for (T query : queries)
    {
        SearchWindow const window =
            binary_tail_window<T, N, Threshold>(values, query);
        windows.push_back(window);
        head_checksum += static_cast<std::uint64_t>(window.first);
    }
    head_checksum *= repetitions;

    return report_search(
               N, queries,
               [&](T query)
               {
                   return binary_head_search<T, N, Threshold>(values, query);
               },
               repetitions, "binary_head_only", Threshold, head_checksum) &&
           report_indexed_search(
               N, queries,
               [&](std::size_t index, T query)
               {
                   return custom_tail_search(values, query, windows[index]);
               },
               repetitions, "custom_tail_only", Threshold, expected_checksum) &&
           report_indexed_search(
               N, queries,
               [&](std::size_t index, T query)
               {
                   return std_tail_search(values, query, windows[index]);
               },
               repetitions, "std_tail_only", Threshold, expected_checksum) &&
           report_search(
               N, queries,
               [&](T query)
               {
                   return binary_then_std_search<T, N, Threshold>(values, query);
               },
               repetitions, "binary_then_std", Threshold, expected_checksum);
}

template <typename T, std::size_t N, std::size_t Threshold>
bool report_descent(std::array<T, N> const& values,
                    std::vector<T> const& queries,
                    std::size_t repetitions, std::uint64_t expected_checksum)
{
    using descent_type = jrmwng::popcount_ranges::lower_bound_descent<
        std::array<T, N>, Threshold>;
    static descent_type descent(values);
    descent.update();
    return report_search(
        N, queries,
        [&](T query)
        {
            return descent_search<T, N, Threshold>(descent, query);
        },
        repetitions, "custom_with_descent", Threshold, expected_checksum);
}

template <typename T, std::size_t N, std::size_t Threshold>
bool report_binary_then_custom(std::array<T, N> const& values,
                               std::vector<T> const& queries,
                               std::size_t repetitions,
                               std::uint64_t expected_checksum)
{
    return report_search(
        N, queries,
        [&](T query)
        {
            return binary_then_custom_search<T, N, Threshold>(values, query);
        },
        repetitions, "binary_then_custom", Threshold, expected_checksum);
}

template <typename T, std::size_t N, std::size_t Threshold>
bool report_named_hybrid(std::array<T, N> const& values,
                         std::vector<T> const& queries,
                         std::size_t repetitions,
                         std::uint64_t expected_checksum,
                         std::string_view implementation)
{
    return report_search(
        N, queries,
        [&](T query)
        {
            if (implementation == "old_binary_then_custom")
                return old_binary_then_custom_search<T, N, Threshold>(
                    values, query);
            return binary_then_custom_search<T, N, Threshold>(values, query);
        },
        repetitions, implementation, Threshold, expected_checksum);
}

template <typename T, std::size_t N, std::size_t Threshold>
bool report_hybrid_pair(std::array<T, N> const& values,
                        std::vector<T> const& queries,
                        std::size_t repetitions,
                        std::uint64_t expected_checksum)
{
    return report_named_hybrid<T, N, Threshold>(
               values, queries, repetitions, expected_checksum,
               "old_binary_then_custom") &&
           report_named_hybrid<T, N, Threshold>(
               values, queries, repetitions, expected_checksum,
               "new_binary_then_custom");
}

template <typename T, std::size_t N, std::size_t Threshold>
bool report_hybrid_head_pair(std::array<T, N> const& values,
                             std::vector<T> const& queries,
                             std::size_t repetitions)
{
    std::uint64_t head_checksum = 0;
    for (T query : queries)
        head_checksum += old_binary_head_search<T, N, Threshold>(values, query);
    head_checksum *= repetitions;

    return report_search(
               N, queries,
               [&](T query)
               {
                   return old_binary_head_search<T, N, Threshold>(
                       values, query);
               },
               repetitions, "old_binary_head_only", Threshold, head_checksum) &&
           report_search(
               N, queries,
               [&](T query)
               {
                   return binary_head_search<T, N, Threshold>(values, query);
               },
               repetitions, "new_binary_head_only", Threshold, head_checksum);
}

template <typename T, std::size_t N>
bool benchmark_size(std::size_t repetitions, bool breakdown_only = false,
                    bool hybrid_only = false)
{
    static std::array<T, N> values{};
    for (std::size_t index = 0; index < N; ++index)
        values[index] = static_cast<T>(index * 2u);

    std::mt19937 generator(benchmark_seed + static_cast<std::uint32_t>(N));
    std::uniform_int_distribution<std::size_t> position(0, N);
    std::size_t const query_count =
        N > 8192u ? std::max<std::size_t>(512u, 16'384u * 8192u / N) : 16'384u;
    std::vector<T> queries;
    queries.reserve(query_count);
    for (std::size_t index = 0; index < query_count; ++index)
    {
        std::size_t const insertion_index = position(generator);
        queries.push_back(static_cast<T>(insertion_index == N
            ? std::numeric_limits<T>::max()
            : insertion_index * 2u));
    }

    std::uint64_t expected_checksum = 0;
    for (T query : queries)
        expected_checksum += static_cast<std::uint64_t>(
            std::lower_bound(values.begin(), values.end(), query) - values.begin());
    expected_checksum *= repetitions;

    if (breakdown_only)
        return report_search(
                   N, queries,
                   [&](T query) { return custom_search<T, N>(values, query); },
                   repetitions, "custom_without_descent", 0, expected_checksum) &&
               report_search(
                   N, queries,
                   [&](T query) { return std_search<T, N>(values, query); },
                   repetitions, "std_lower_bound", 0, expected_checksum) &&
               report_binary_then_custom<T, N, 1>(
                   values, queries, repetitions, expected_checksum) &&
               report_binary_breakdown<T, N, 1>(
                   values, queries, repetitions, expected_checksum) &&
               report_binary_then_custom<T, N, 2>(
                   values, queries, repetitions, expected_checksum) &&
               report_binary_breakdown<T, N, 2>(
                   values, queries, repetitions, expected_checksum) &&
               report_binary_then_custom<T, N, 4>(
                   values, queries, repetitions, expected_checksum) &&
               report_binary_breakdown<T, N, 4>(
                   values, queries, repetitions, expected_checksum) &&
               report_binary_then_custom<T, N, 8>(
                   values, queries, repetitions, expected_checksum) &&
               report_binary_breakdown<T, N, 8>(
                   values, queries, repetitions, expected_checksum) &&
               report_binary_then_custom<T, N, 16>(
                   values, queries, repetitions, expected_checksum) &&
               report_binary_breakdown<T, N, 16>(
                   values, queries, repetitions, expected_checksum) &&
               report_binary_then_custom<T, N, 32>(
                   values, queries, repetitions, expected_checksum) &&
               report_binary_breakdown<T, N, 32>(
                   values, queries, repetitions, expected_checksum) &&
               report_binary_then_custom<T, N, 64>(
                   values, queries, repetitions, expected_checksum) &&
               report_binary_breakdown<T, N, 64>(
                   values, queries, repetitions, expected_checksum) &&
               report_binary_then_custom<T, N, 128>(
                   values, queries, repetitions, expected_checksum) &&
               report_binary_breakdown<T, N, 128>(
                   values, queries, repetitions, expected_checksum);

    if (hybrid_only)
    {
        bool passed = report_search(
                   N, queries,
                   [&](T query) { return std_search<T, N>(values, query); },
                   repetitions, "std_lower_bound", 0, expected_checksum) &&
               report_search(
                   N, queries,
                   [&](T query) { return custom_search<T, N>(values, query); },
                   repetitions, "custom_without_descent", 0, expected_checksum) &&
               report_binary_then_custom<T, N, 1>(
                   values, queries, repetitions, expected_checksum) &&
               report_binary_then_custom<T, N, 2>(
                   values, queries, repetitions, expected_checksum) &&
               report_binary_then_custom<T, N, 4>(
                   values, queries, repetitions, expected_checksum) &&
               report_binary_then_custom<T, N, 8>(
                   values, queries, repetitions, expected_checksum) &&
               report_binary_then_custom<T, N, 16>(
                   values, queries, repetitions, expected_checksum) &&
               report_binary_then_custom<T, N, 32>(
                   values, queries, repetitions, expected_checksum) &&
               report_binary_then_custom<T, N, 64>(
                   values, queries, repetitions, expected_checksum) &&
               report_binary_then_custom<T, N, 128>(
                   values, queries, repetitions, expected_checksum);
        if constexpr (sizeof(T) == 8u && (N == 32768u || N == 65536u))
            passed = passed &&
                     report_binary_then_custom<T, N, 3>(
                         values, queries, repetitions, expected_checksum) &&
                     report_binary_then_custom<T, N, 6>(
                         values, queries, repetitions, expected_checksum) &&
                     report_binary_then_custom<T, N, 12>(
                         values, queries, repetitions, expected_checksum) &&
                     report_binary_then_custom<T, N, 24>(
                         values, queries, repetitions, expected_checksum) &&
                     report_binary_then_custom<T, N, 48>(
                         values, queries, repetitions, expected_checksum) &&
                     report_binary_then_custom<T, N, 96>(
                         values, queries, repetitions, expected_checksum) &&
                     report_binary_then_custom<T, N, 256>(
                         values, queries, repetitions, expected_checksum) &&
                     report_hybrid_pair<T, N, 16>(
                         values, queries, repetitions, expected_checksum) &&
                     report_hybrid_head_pair<T, N, 16>(
                         values, queries, repetitions) &&
                     report_hybrid_pair<T, N, 24>(
                         values, queries, repetitions, expected_checksum) &&
                     report_hybrid_head_pair<T, N, 24>(
                         values, queries, repetitions) &&
                     report_hybrid_pair<T, N, 32>(
                         values, queries, repetitions, expected_checksum) &&
                     report_hybrid_head_pair<T, N, 32>(
                         values, queries, repetitions) &&
                     report_hybrid_pair<T, N, 48>(
                         values, queries, repetitions, expected_checksum) &&
                     report_hybrid_head_pair<T, N, 48>(
                         values, queries, repetitions) &&
                     report_hybrid_pair<T, N, 64>(
                         values, queries, repetitions, expected_checksum) &&
                     report_hybrid_head_pair<T, N, 64>(
                         values, queries, repetitions) &&
                     report_hybrid_pair<T, N, 96>(
                         values, queries, repetitions, expected_checksum) &&
                     report_hybrid_head_pair<T, N, 96>(
                         values, queries, repetitions) &&
                     report_hybrid_pair<T, N, 128>(
                         values, queries, repetitions, expected_checksum) &&
                     report_hybrid_head_pair<T, N, 128>(
                         values, queries, repetitions);
        return passed;
    }

    if (!report_search(N, queries,
                       [&](T query) { return custom_search<T, N>(values, query); },
                       repetitions,
                       "custom_without_descent", 0, expected_checksum) ||
        !report_search(N, queries,
                       [&](T query) { return std_search<T, N>(values, query); },
                       repetitions,
                       "std_lower_bound", 0, expected_checksum) ||
        !report_descent<T, N, 1>(values, queries, repetitions, expected_checksum) ||
        !report_descent<T, N, 4>(values, queries, repetitions, expected_checksum) ||
        !report_descent<T, N, 8>(values, queries, repetitions, expected_checksum) ||
        !report_descent<T, N, 16>(values, queries, repetitions, expected_checksum) ||
        !report_descent<T, N, 32>(values, queries, repetitions, expected_checksum) ||
        (sizeof(T) == 8u &&
         (!report_binary_then_custom<T, N, 1>(values, queries, repetitions, expected_checksum) ||
          !report_binary_then_custom<T, N, 2>(values, queries, repetitions, expected_checksum) ||
          !report_binary_then_custom<T, N, 4>(values, queries, repetitions, expected_checksum) ||
          !report_binary_then_custom<T, N, 8>(values, queries, repetitions, expected_checksum) ||
          !report_binary_then_custom<T, N, 16>(values, queries, repetitions, expected_checksum) ||
          !report_binary_then_custom<T, N, 32>(values, queries, repetitions, expected_checksum) ||
          !report_binary_then_custom<T, N, 64>(values, queries, repetitions, expected_checksum) ||
          !report_binary_then_custom<T, N, 128>(values, queries, repetitions, expected_checksum))) ||
        (sizeof(T) == 8u && N >= 32u && N <= 16384u &&
         (!report_binary_breakdown<T, N, 8>(values, queries, repetitions, expected_checksum) ||
          !report_binary_breakdown<T, N, 16>(values, queries, repetitions, expected_checksum) ||
          !report_binary_breakdown<T, N, 32>(values, queries, repetitions, expected_checksum) ||
          !report_binary_breakdown<T, N, 64>(values, queries, repetitions, expected_checksum) ||
          !report_binary_breakdown<T, N, 128>(values, queries, repetitions, expected_checksum))))
        return false;
    return true;
}

template <typename T>
bool benchmark_width(std::size_t repetitions)
{
    bool passed = benchmark_size<T, 1>(repetitions) &&
           benchmark_size<T, 2>(repetitions) &&
           benchmark_size<T, 4>(repetitions) &&
           benchmark_size<T, 8>(repetitions) &&
           benchmark_size<T, 16>(repetitions) &&
           benchmark_size<T, 32>(repetitions) &&
           benchmark_size<T, 64>(repetitions) &&
           benchmark_size<T, 128>(repetitions);

    if constexpr (sizeof(T) >= 2)
        passed = passed &&
                 benchmark_size<T, 256>(repetitions) &&
                 benchmark_size<T, 512>(repetitions) &&
                 benchmark_size<T, 1024>(repetitions) &&
                 benchmark_size<T, 2048>(repetitions) &&
                 benchmark_size<T, 4096>(repetitions) &&
                 benchmark_size<T, 8192>(repetitions);
    if constexpr (sizeof(T) == 8)
        passed = benchmark_size<T, 16384>(repetitions) &&
                 benchmark_size<T, 32768>(repetitions) &&
                 benchmark_size<T, 65536>(repetitions) &&
                 benchmark_size<T, 131072>(repetitions) &&
                 benchmark_size<T, 262144>(repetitions) &&
                 passed;
    return passed;
}
}

int main(int argc, char const* argv[])
{
    bool quick = false;
    bool breakdown_only = false;
    bool hybrid_only = false;
    bool hybrid_pair_only = false;
    bool branch_only = false;
    bool cache_state_only = false;
    for (int index = 1; index < argc; ++index)
    {
        std::string_view const argument(argv[index]);
        if (argument == "--quick")
        {
            quick = true;
        }
        else if (argument == "--breakdown-only")
        {
            breakdown_only = true;
        }
        else if (argument == "--hybrid-only")
        {
            hybrid_only = true;
        }
        else if (argument == "--hybrid-pair-only")
        {
            hybrid_pair_only = true;
        }
        else if (argument == "--branch-only")
        {
            branch_only = true;
        }
        else if (argument == "--cache-state-only")
        {
            cache_state_only = true;
        }
        else if (argument == "--llc-mib" && index + 1 < argc)
        {
            std::string_view const value(argv[++index]);
            std::size_t mib = 0;
            auto const [end, error] = std::from_chars(
                value.data(), value.data() + value.size(), mib);
            if (error != std::errc{} || end != value.data() + value.size() ||
                mib == 0 || mib > 4096u)
            {
                std::cerr << "invalid LLC working-set size: " << value
                          << " (expected 1..4096 MiB)\n";
                return 2;
            }
            cache_working_set_bytes = mib * 1024u * 1024u;
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
            std::cerr << "usage: lower_bound_search_benchmark [--quick] [--breakdown-only] [--hybrid-only] [--hybrid-pair-only] [--branch-only] [--cache-state-only] [--llc-mib value] [--seed value]\n";
            return 2;
        }
    }

    std::size_t const repetitions = quick ? 2u : 32u;
    if (cache_state_only)
    {
#if defined(_WIN32)
        DWORD_PTR const previous_affinity = pin_benchmark_thread();
        if (previous_affinity == 0)
            return 1;
#endif
        tsc_frequency_hz = calibrate_tsc_frequency();
        std::cout << "compiler,query_seed,type_bits,N,cache_working_set_mib,"
                     "logical_cpu,tsc_frequency_hz,cache_state,positions,"
                     "implementation,threshold,best_position,best_cycles,"
                     "best_ns,median_cycles,median_ns,p95_cycles,p95_ns,"
                     "worst_position,worst_cycles,worst_ns,checksum\n";
        std::vector<std::uint64_t> cache_working_set(
            cache_working_set_bytes / sizeof(std::uint64_t));
        bool const passed =
            benchmark_cache_state_size<4096>(cache_working_set, quick) &&
            benchmark_cache_state_size<8192>(cache_working_set, quick) &&
            benchmark_cache_state_size<16384>(cache_working_set, quick) &&
            benchmark_cache_state_size<32768>(cache_working_set, quick) &&
            benchmark_cache_state_size<65536>(cache_working_set, quick) &&
            benchmark_cache_state_size<131072>(cache_working_set, quick) &&
            benchmark_cache_state_size<262144>(cache_working_set, quick);
#if defined(_WIN32)
        if (SetThreadAffinityMask(GetCurrentThread(), previous_affinity) == 0)
        {
            std::cerr << "failed to restore thread affinity: "
                      << GetLastError() << '\n';
            return 1;
        }
#endif
        if (!passed)
            return 1;
        std::cerr << "sink=" << sink << '\n';
        return 0;
    }

    std::cout << "compiler,query_seed,type_bits,N,query_distribution,query_count,"
                 "implementation,threshold,median_ns_per_query,checksum\n";
    if (branch_only)
    {
        std::array<std::uint64_t, 32768> values_32768{};
        std::array<std::uint64_t, 65536> values_65536{};
        for (std::size_t index = 0; index < values_32768.size(); ++index)
            values_32768[index] = index * 2u;
        for (std::size_t index = 0; index < values_65536.size(); ++index)
            values_65536[index] = index * 2u;
        bool const passed =
            report_branch_cases(values_32768, repetitions) &&
            report_branch_cases(values_65536, repetitions);
        if (!passed)
            return 1;
        std::cerr << "sink=" << sink << '\n';
        return 0;
    }

    if (hybrid_pair_only)
    {
        bool const passed =
            benchmark_size<std::uint64_t, 32768>(repetitions, false, true) &&
            benchmark_size<std::uint64_t, 65536>(repetitions, false, true);
        if (!passed)
            return 1;
        std::cerr << "sink=" << sink << '\n';
        return 0;
    }

    if (hybrid_only)
    {
        bool const passed =
            benchmark_size<std::uint64_t, 32>(repetitions, false, true) &&
            benchmark_size<std::uint64_t, 64>(repetitions, false, true) &&
            benchmark_size<std::uint64_t, 128>(repetitions, false, true) &&
            benchmark_size<std::uint64_t, 256>(repetitions, false, true) &&
            benchmark_size<std::uint64_t, 512>(repetitions, false, true) &&
            benchmark_size<std::uint64_t, 1024>(repetitions, false, true) &&
            benchmark_size<std::uint64_t, 2048>(repetitions, false, true) &&
            benchmark_size<std::uint64_t, 4096>(repetitions, false, true) &&
            benchmark_size<std::uint64_t, 8192>(repetitions, false, true) &&
            benchmark_size<std::uint64_t, 16384>(repetitions, false, true) &&
            benchmark_size<std::uint64_t, 32768>(repetitions, false, true) &&
            benchmark_size<std::uint64_t, 65536>(repetitions, false, true) &&
            benchmark_size<std::uint64_t, 131072>(repetitions, false, true) &&
            benchmark_size<std::uint64_t, 262144>(repetitions, false, true);
        if (!passed)
            return 1;
        std::cerr << "sink=" << sink << '\n';
        return 0;
    }

    if (breakdown_only)
    {
        bool const passed =
            benchmark_size<std::uint64_t, 32>(repetitions, true) &&
            benchmark_size<std::uint64_t, 64>(repetitions, true) &&
            benchmark_size<std::uint64_t, 128>(repetitions, true) &&
            benchmark_size<std::uint64_t, 256>(repetitions, true) &&
            benchmark_size<std::uint64_t, 512>(repetitions, true) &&
            benchmark_size<std::uint64_t, 1024>(repetitions, true) &&
            benchmark_size<std::uint64_t, 2048>(repetitions, true) &&
            benchmark_size<std::uint64_t, 4096>(repetitions, true) &&
            benchmark_size<std::uint64_t, 8192>(repetitions, true) &&
            benchmark_size<std::uint64_t, 16384>(repetitions, true);
        if (!passed)
            return 1;
        std::cerr << "sink=" << sink << '\n';
        return 0;
    }

    if (!benchmark_width<std::uint8_t>(repetitions) ||
        !benchmark_width<std::uint16_t>(repetitions) ||
        !benchmark_width<std::uint32_t>(repetitions) ||
        !benchmark_width<std::uint64_t>(repetitions))
        return 1;
    std::cerr << "sink=" << sink << '\n';
}
