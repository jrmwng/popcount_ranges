#include <popcount_ranges/lower_bound.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace
{
volatile std::uint64_t sink = 0;
constexpr std::size_t cache_line_bytes = 64;

enum class CacheCase
{
    llc_cold,
    prefetched_target,
    dirty_writeback_pressure
};

constexpr std::array cache_cases{
    CacheCase::llc_cold,
    CacheCase::prefetched_target,
    CacheCase::dirty_writeback_pressure};

constexpr char const* cache_case_name(CacheCase cache_case)
{
    switch (cache_case)
    {
    case CacheCase::llc_cold:
        return "llc_cold";
    case CacheCase::prefetched_target:
        return "prefetched_target";
    case CacheCase::dirty_writeback_pressure:
        return "dirty_writeback_pressure";
    }
    return "unknown";
}

std::size_t detected_llc_bytes()
{
#if defined(_WIN32)
    DWORD required_bytes = 0;
    GetLogicalProcessorInformationEx(
        RelationCache, nullptr, &required_bytes);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || required_bytes == 0)
        throw std::runtime_error("could not query the Windows cache hierarchy");

    std::vector<std::byte> records(required_bytes);
    if (!GetLogicalProcessorInformationEx(
            RelationCache,
            reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(
                records.data()),
            &required_bytes))
        throw std::runtime_error("could not read the Windows cache hierarchy");

    PROCESSOR_NUMBER processor{};
    GetCurrentProcessorNumberEx(&processor);
    std::size_t largest_llc = 0;
    for (std::size_t offset = 0; offset < required_bytes;)
    {
        auto const* record =
            reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX const*>(
                records.data() + offset);
        if (record->Relationship == RelationCache &&
            record->Cache.Level >= 3 &&
            record->Cache.GroupMask.Group == processor.Group &&
            (record->Cache.GroupMask.Mask &
             (KAFFINITY{1} << processor.Number)) != 0)
            largest_llc = std::max<std::size_t>(
                largest_llc, record->Cache.CacheSize);
        offset += record->Size;
    }
    if (largest_llc == 0)
        throw std::runtime_error(
            "Windows reported no level-3-or-higher cache for this processor");
    return largest_llc;
#elif defined(__linux__)
    namespace fs = std::filesystem;
    std::size_t largest_llc = 0;
    for (fs::directory_entry const& entry :
         fs::directory_iterator("/sys/devices/system/cpu/cpu0/cache"))
    {
        std::ifstream level_file(entry.path() / "level");
        std::ifstream type_file(entry.path() / "type");
        std::ifstream size_file(entry.path() / "size");
        int level = 0;
        std::string type;
        std::string size_text;
        if (!(level_file >> level) || !(type_file >> type) ||
            !(size_file >> size_text) || level < 3 || type != "Unified")
            continue;

        std::size_t multiplier = 1;
        char const suffix = size_text.back();
        if (suffix == 'K' || suffix == 'k')
            multiplier = 1024u;
        else if (suffix == 'M' || suffix == 'm')
            multiplier = 1024u * 1024u;
        auto const number_end =
            suffix == 'K' || suffix == 'k' || suffix == 'M' || suffix == 'm'
                ? size_text.data() + size_text.size() - 1u
                : size_text.data() + size_text.size();
        std::size_t size = 0;
        auto const [end, error] = std::from_chars(
            size_text.data(), number_end, size);
        if (error != std::errc{} || end != number_end)
            continue;
        if (size <= std::numeric_limits<std::size_t>::max() / multiplier)
            largest_llc = std::max(largest_llc, size * multiplier);
    }
    if (largest_llc == 0)
        throw std::runtime_error(
            "could not detect an LLC in the Linux cache sysfs");
    return largest_llc;
#else
    throw std::runtime_error(
        "automatic LLC detection is unsupported on this platform");
#endif
}

template <typename T>
constexpr char const* type_name()
{
    if constexpr (std::is_same_v<T, std::uint8_t>)
        return "uint8_t";
    else if constexpr (std::is_same_v<T, std::uint16_t>)
        return "uint16_t";
    else if constexpr (std::is_same_v<T, std::uint32_t>)
        return "uint32_t";
    else if constexpr (std::is_same_v<T, std::uint64_t>)
        return "uint64_t";
    else if constexpr (std::is_same_v<T, float>)
        return "float";
    else
        return "double";
}

class CachePreparation
{
public:
    explicit CachePreparation(std::size_t llc_bytes)
        : eviction_buffer_(llc_bytes * 2u), dirty_buffer_(llc_bytes)
    {
    }

    template <typename T>
    void prepare(
        CacheCase cache_case, std::vector<T> const& target,
        std::size_t sample) const
    {
        auto const* data =
            reinterpret_cast<volatile std::uint8_t const*>(
                eviction_buffer_.data());
        std::uint64_t checksum = 0;
        for (std::size_t offset = 0; offset < eviction_buffer_.size();
             offset += cache_line_bytes)
            checksum += data[offset];
        sink += checksum;

        if (cache_case == CacheCase::prefetched_target)
        {
            auto const* target_bytes =
                reinterpret_cast<volatile std::uint8_t const*>(
                    target.data());
            for (std::size_t offset = 0;
                 offset < target.size() * sizeof(T);
                 offset += cache_line_bytes)
                checksum += target_bytes[offset];
            sink += checksum;
        }
        else if (cache_case == CacheCase::dirty_writeback_pressure)
        {
            auto* dirty_data =
                reinterpret_cast<volatile std::uint8_t*>(
                    dirty_buffer_.data());
            auto const value = static_cast<std::uint8_t>(sample + 1u);
            for (std::size_t offset = 0; offset < dirty_buffer_.size();
                 offset += cache_line_bytes)
                dirty_data[offset] ^= value;
        }
    }

private:
    std::vector<std::uint8_t> eviction_buffer_;
    mutable std::vector<std::uint8_t> dirty_buffer_;
};

template <typename T, bool Unroll, std::size_t Size>
struct comparison_traits : jrmwng::popcount_ranges::lower_bound_traits<T>
{
    inline constexpr static auto PARAMS =
        jrmwng::popcount_ranges::lower_bound_traits<T>::lookup_params(
            std::bit_width(Size));
    inline constexpr static std::size_t BINARY_SEARCH_THRESHOLD =
        PARAMS.first;
    inline constexpr static std::size_t SEQUENTIAL_SEARCH_PITCH =
        PARAMS.second;
    inline constexpr static bool UNROLL_SIMD_LOOP = Unroll;
};

template <typename T>
std::vector<T> make_values(std::size_t size)
{
    std::vector<T> values(size);
    if constexpr (std::is_floating_point_v<T>)
    {
        for (std::size_t i = 0; i < size; ++i)
            values[i] = static_cast<T>(i);
    }
    else
    {
        std::uint64_t const maximum =
            static_cast<std::uint64_t>(std::numeric_limits<T>::max());
        std::uint64_t const denominator = size - 1;
        std::uint64_t const quotient = maximum / denominator;
        std::uint64_t const remainder = maximum % denominator;
        for (std::size_t i = 0; i < size; ++i)
        {
            std::uint64_t const position = i;
            values[i] = static_cast<T>(
                quotient * position + remainder * position / denominator);
        }
    }
    return values;
}

template <typename T, bool Unroll, std::size_t Size>
std::size_t custom_search(std::vector<T> const& values, T target)
{
    using traits = comparison_traits<T, Unroll, Size>;
    return static_cast<std::size_t>(
        jrmwng::popcount_ranges::lower_bound(
            values.data(), values.data() + values.size(), target,
            std::less<>{}, traits{}) -
        values.data());
}

template <typename T>
std::size_t standard_search(std::vector<T> const& values, T target)
{
    return static_cast<std::size_t>(
        std::lower_bound(values.begin(), values.end(), target) -
        values.begin());
}

template <typename T, typename Search>
double measure(
    std::vector<T> const& values, std::vector<T> const& queries,
    Search search, CachePreparation const& cache_preparation,
    CacheCase cache_case, std::uint64_t& checksum)
{
    constexpr std::size_t repetitions = 7;
    std::array<double, repetitions> samples{};
    std::size_t const passes = std::max<std::size_t>(
        1, (16'384 + queries.size() - 1) / queries.size());
    checksum = 0;

    for (std::size_t sample_index = 0; sample_index < samples.size();
         ++sample_index)
    {
        std::uint64_t result = 0;
        cache_preparation.prepare(cache_case, values, sample_index);
        auto const started = std::chrono::steady_clock::now();
        for (std::size_t pass = 0; pass < passes; ++pass)
            for (T query : queries)
                result += search(values, query);
        samples[sample_index] = std::chrono::duration<double, std::nano>(
                     std::chrono::steady_clock::now() - started)
                     .count() /
                 static_cast<double>(passes * queries.size());
        checksum = result;
        sink += result;
    }

    std::sort(samples.begin(), samples.end());
    return samples[samples.size() / 2];
}

template <typename T, std::size_t Size>
bool run_size(
    CachePreparation const& cache_preparation, std::uint32_t seed,
    std::size_t llc_bytes)
{
    auto const values = make_values<T>(Size);
    std::vector<T> queries;
    queries.reserve(std::min<std::size_t>(16'384, Size));
    std::size_t previous = 0;
    bool have_previous = false;
    for (std::size_t query = 0;
         query < std::min<std::size_t>(16'384, Size); ++query)
    {
        std::size_t const index =
            query * (Size - 1) /
            (std::min<std::size_t>(16'384, Size) - 1);
        if (!have_previous || values[index] != values[previous])
        {
            queries.push_back(values[index]);
            previous = index;
            have_previous = true;
        }
    }

    std::mt19937 generator(seed);
    std::shuffle(queries.begin(), queries.end(), generator);

    std::size_t const passes = std::max<std::size_t>(
        1, (16'384 + queries.size() - 1) / queries.size());
    std::uint64_t expected_checksum = 0;
    for (T query : queries)
        expected_checksum += standard_search(values, query);
    expected_checksum *= passes;
    auto const [threshold, pitch] =
        jrmwng::popcount_ranges::lower_bound_traits<T>::lookup_params(
            std::bit_width(Size));

    for (std::size_t case_index = 0; case_index < cache_cases.size();
         ++case_index)
    {
        CacheCase const cache_case = cache_cases[case_index];
        std::uint64_t unrolled_checksum = 0;
        std::uint64_t loop_checksum = 0;
        double unrolled_ns = 0;
        double loop_ns = 0;
        bool const unrolled_first = ((seed + case_index) & 1u) == 0;
        if (unrolled_first)
        {
            unrolled_ns = measure(
                values, queries,
                [](auto const& range, T target)
                { return custom_search<T, true, Size>(range, target); },
                cache_preparation, cache_case, unrolled_checksum);
            loop_ns = measure(
                values, queries,
                [](auto const& range, T target)
                { return custom_search<T, false, Size>(range, target); },
                cache_preparation, cache_case, loop_checksum);
        }
        else
        {
            loop_ns = measure(
                values, queries,
                [](auto const& range, T target)
                { return custom_search<T, false, Size>(range, target); },
                cache_preparation, cache_case, loop_checksum);
            unrolled_ns = measure(
                values, queries,
                [](auto const& range, T target)
                { return custom_search<T, true, Size>(range, target); },
                cache_preparation, cache_case, unrolled_checksum);
        }

        if (unrolled_checksum != loop_checksum ||
            unrolled_checksum != expected_checksum)
        {
            std::cerr << "checksum mismatch for N=" << Size << ", seed="
                      << seed << ", cache_case="
                      << cache_case_name(cache_case) << '\n';
            return false;
        }

        std::cout << seed << ',' << type_name<T>() << ',' << Size << ','
                  << cache_case_name(cache_case) << ',' << llc_bytes << ','
                  << threshold << ',' << pitch << ',' << queries.size() << ','
                  << std::fixed << std::setprecision(3)
                  << unrolled_ns << ',' << loop_ns << ','
                  << unrolled_ns / loop_ns << ',' << unrolled_checksum
                  << '\n';
    }
    return true;
}

template <typename T>
bool run_type(
    CachePreparation const& cache_preparation, std::uint32_t seed,
    std::size_t llc_bytes)
{
    return run_size<T, 256>(cache_preparation, seed, llc_bytes) &&
           run_size<T, 4096>(cache_preparation, seed, llc_bytes) &&
           run_size<T, 16384>(cache_preparation, seed, llc_bytes) &&
           run_size<T, 262144>(cache_preparation, seed, llc_bytes);
}
}

int main()
{
#if defined(_WIN32)
    DWORD_PTR process_affinity_mask = 0;
    DWORD_PTR system_affinity_mask = 0;
    if (GetProcessAffinityMask(
            GetCurrentProcess(), &process_affinity_mask,
            &system_affinity_mask) == 0 ||
        process_affinity_mask == 0)
    {
        std::cerr << "failed to determine available processors\n";
        return 2;
    }
    DWORD_PTR const affinity_mask =
        DWORD_PTR{1} << std::countr_zero(process_affinity_mask);
    if (SetThreadAffinityMask(GetCurrentThread(), affinity_mask) == 0)
    {
        std::cerr << "failed to pin benchmark to an available processor\n";
        return 2;
    }
#endif

    try
    {
        std::size_t const llc_bytes = detected_llc_bytes();
        if (llc_bytes > std::numeric_limits<std::size_t>::max() / 2u)
            throw std::runtime_error("detected LLC size is too large");
        CachePreparation cache_preparation(llc_bytes);
        std::cerr << "detected LLC=" << llc_bytes << " bytes; cache line="
                  << cache_line_bytes << " bytes\n";
        std::cout << "seed,type,N,cache_case,llc_bytes,threshold,pitch,"
                     "unique_queries,unrolled_ns_per_query,"
                     "loop_ns_per_query,ratio,checksum\n";
        bool passed = true;
        for (std::uint32_t seed : {24301u, 24302u, 24303u})
        {
            passed = run_type<std::uint8_t>(
                         cache_preparation, seed, llc_bytes) &&
                     passed;
            passed = run_type<std::uint16_t>(
                         cache_preparation, seed, llc_bytes) &&
                     passed;
            passed = run_type<std::uint32_t>(
                         cache_preparation, seed, llc_bytes) &&
                     passed;
            passed = run_type<std::uint64_t>(
                         cache_preparation, seed, llc_bytes) &&
                     passed;
            passed = run_type<float>(cache_preparation, seed, llc_bytes) &&
                     passed;
            passed = run_type<double>(cache_preparation, seed, llc_bytes) &&
                     passed;
        }
        return passed ? 0 : 1;
    }
    catch (std::exception const& error)
    {
        std::cerr << "benchmark setup failed: " << error.what() << '\n';
        return 2;
    }
}
