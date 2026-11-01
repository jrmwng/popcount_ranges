#include <popcount_ranges/lower_bound.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <type_traits>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#endif

namespace
{
volatile std::uint64_t sink = 0;

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

class CacheFlusher
{
public:
    explicit CacheFlusher(std::size_t bytes) : buffer_(bytes) {}

    void flush() const
    {
        auto const* data =
            reinterpret_cast<volatile std::uint8_t const*>(buffer_.data());
        std::uint64_t checksum = 0;
        for (std::size_t offset = 0; offset < buffer_.size(); offset += 64)
            checksum += data[offset];
        sink += checksum;
    }

private:
    std::vector<std::uint8_t> buffer_;
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
    Search search, CacheFlusher const& flusher, std::uint64_t& checksum)
{
    constexpr std::size_t repetitions = 7;
    std::array<double, repetitions> samples{};
    std::size_t const passes = std::max<std::size_t>(
        1, (16'384 + queries.size() - 1) / queries.size());
    checksum = 0;

    for (double& sample : samples)
    {
        std::uint64_t result = 0;
        flusher.flush();
        auto const started = std::chrono::steady_clock::now();
        for (std::size_t pass = 0; pass < passes; ++pass)
            for (T query : queries)
                result += search(values, query);
        sample = std::chrono::duration<double, std::nano>(
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
bool run_size(CacheFlusher const& flusher, std::uint32_t seed)
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

    std::uint64_t unrolled_checksum = 0;
    std::uint64_t loop_checksum = 0;
    double unrolled_ns = 0;
    double loop_ns = 0;
    if ((seed & 1u) == 0)
    {
        unrolled_ns = measure(
            values, queries,
            [](auto const& range, T target)
            { return custom_search<T, true, Size>(range, target); },
            flusher, unrolled_checksum);
        loop_ns = measure(
            values, queries,
            [](auto const& range, T target)
            { return custom_search<T, false, Size>(range, target); },
            flusher, loop_checksum);
    }
    else
    {
        loop_ns = measure(
            values, queries,
            [](auto const& range, T target)
            { return custom_search<T, false, Size>(range, target); },
            flusher, loop_checksum);
        unrolled_ns = measure(
            values, queries,
            [](auto const& range, T target)
            { return custom_search<T, true, Size>(range, target); },
            flusher, unrolled_checksum);
    }

    std::size_t const passes = std::max<std::size_t>(
        1, (16'384 + queries.size() - 1) / queries.size());
    std::uint64_t expected_checksum = 0;
    for (T query : queries)
        expected_checksum += standard_search(values, query);
    expected_checksum *= passes;
    if (unrolled_checksum != loop_checksum ||
        unrolled_checksum != expected_checksum)
    {
        std::cerr << "checksum mismatch for N=" << Size << ", seed=" << seed
                  << '\n';
        return false;
    }

    auto const [threshold, pitch] =
        jrmwng::popcount_ranges::lower_bound_traits<T>::lookup_params(
            std::bit_width(Size));
    std::cout << seed << ',' << type_name<T>() << ',' << Size << ','
              << threshold << ',' << pitch << ',' << queries.size() << ','
              << std::fixed << std::setprecision(3)
              << unrolled_ns << ',' << loop_ns << ','
              << unrolled_ns / loop_ns << ',' << unrolled_checksum << '\n';
    return true;
}

template <typename T>
bool run_type(CacheFlusher const& flusher, std::uint32_t seed)
{
    return run_size<T, 256>(flusher, seed) &&
           run_size<T, 4096>(flusher, seed) &&
           run_size<T, 16384>(flusher, seed) &&
           run_size<T, 262144>(flusher, seed);
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

    CacheFlusher flusher(64u * 1024u * 1024u);
    std::cout << "seed,type,N,threshold,pitch,unique_queries,"
                 "unrolled_ns_per_query,loop_ns_per_query,ratio,checksum\n";
    bool passed = true;
    for (std::uint32_t seed : {24301u, 24302u, 24303u})
    {
        passed = run_type<std::uint8_t>(flusher, seed) && passed;
        passed = run_type<std::uint16_t>(flusher, seed) && passed;
        passed = run_type<std::uint32_t>(flusher, seed) && passed;
        passed = run_type<std::uint64_t>(flusher, seed) && passed;
        passed = run_type<float>(flusher, seed) && passed;
        passed = run_type<double>(flusher, seed) && passed;
    }
    return passed ? 0 : 1;
}
