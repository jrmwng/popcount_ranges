#include <data_par/simd_range.hpp>

#include <algorithm>
#include <array>
#include <bitset>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <vector>

namespace
{
constexpr std::size_t mask_count = 64;
constexpr std::size_t iterations = 200'000;
constexpr std::size_t repetitions = 7;

std::uint64_t sink = 0;

template <typename T>
std::string_view type_name();

template <>
std::string_view type_name<std::int8_t>() { return "int8"; }
template <>
std::string_view type_name<std::uint8_t>() { return "uint8"; }
template <>
std::string_view type_name<std::int16_t>() { return "int16"; }
template <>
std::string_view type_name<std::uint16_t>() { return "uint16"; }
template <>
std::string_view type_name<std::int32_t>() { return "int32"; }
template <>
std::string_view type_name<std::uint32_t>() { return "uint32"; }
template <>
std::string_view type_name<std::int64_t>() { return "int64"; }
template <>
std::string_view type_name<std::uint64_t>() { return "uint64"; }

template <typename T, typename Toperation>
double measure(Toperation operation)
{
    std::array<double, repetitions> timings{};
    std::uint64_t checksum = 0;

    for (std::size_t repeat = 0; repeat < repetitions; ++repeat)
    {
        auto const started = std::chrono::steady_clock::now();
        checksum += operation();
        auto const elapsed = std::chrono::duration<double, std::nano>(
            std::chrono::steady_clock::now() - started).count();
        timings[repeat] = elapsed / static_cast<double>(iterations);
    }

    sink += checksum;
    std::sort(timings.begin(), timings.end());
    return timings[repetitions / 2];
}

template <typename T>
void benchmark_type()
{
    using traits = jrmwng::data_par::simd_range_traits<T>;
    using simd_type = typename traits::simd_type;
    using mask_type = typename traits::mask_type;
    constexpr std::size_t width = traits::simd_width;

    std::array<typename traits::bitset_type, mask_count> bitsets{};
    using comparison_mask_type = decltype(simd_type{} < simd_type{});
    std::array<comparison_mask_type, mask_count> comparisons{};
    std::array<mask_type, mask_count> masks{};
    std::array<T, width * 2> values{};
    for (std::size_t index = 0; index < values.size(); ++index)
        values[index] = static_cast<T>(index * 7 + 3);

    auto const vector = jrmwng::data_par::detail::simd_range_load<simd_type>(
        values.data(), width);

    for (std::size_t mask_index = 0; mask_index < mask_count; ++mask_index)
    {
        for (std::size_t lane = 0; lane < width; ++lane)
            bitsets[mask_index].set(lane, ((mask_index * 13 + lane * 7) % 11) < 5);

        auto const target = simd_type(static_cast<T>(mask_index * 3));
        auto const comparison = vector < target;
        comparisons[mask_index] = comparison;
#if POPCOUNT_RANGES_SIMD && POPCOUNT_RANGES_SIMD_MASK_BITSET
        masks[mask_index] = mask_type(comparison);
#else
        masks[mask_index] = comparison;
#endif
    }

#if POPCOUNT_RANGES_SIMD
    constexpr std::string_view backend =
#if POPCOUNT_RANGES_SIMD_MASK_BITSET
        "std_simd_bitset";
#else
        "std_simd_native";
#endif
#else
    constexpr std::string_view backend = "data_par";
#endif

    auto const make_ns = measure<T>([&]
    {
        std::uint64_t checksum = 0;
        for (std::size_t index = 0; index < iterations; ++index)
        {
            auto const mask = traits::make_mask(bitsets[index % mask_count]);
            checksum += mask[index % width] ? 1u : 0u;
        }
        return checksum;
    });

    auto const convert_ns = measure<T>([&]
    {
        std::uint64_t checksum = 0;
        for (std::size_t index = 0; index < iterations; ++index)
        {
            auto const mask = mask_type(comparisons[index % mask_count]);
            checksum += mask[index % width] ? 1u : 0u;
        }
        return checksum;
    });

    auto const reduce_ns = measure<T>([&]
    {
        std::uint64_t checksum = 0;
        for (std::size_t index = 0; index < iterations; ++index)
            checksum += static_cast<std::uint64_t>(
                jrmwng::data_par::reduce_count(masks[index % mask_count]));
        return checksum;
    });

    auto const load_ns = measure<T>([&]
    {
        std::uint64_t checksum = 0;
        for (std::size_t index = 0; index < iterations; ++index)
        {
            auto const loaded = jrmwng::data_par::detail::simd_range_load<simd_type>(
                values.data() + index % 2, width);
            checksum += static_cast<std::uint64_t>(loaded[index % width]);
        }
        return checksum;
    });

    std::cout << backend << ',' << type_name<T>() << ",make_mask_bitset,"
              << std::fixed << std::setprecision(2) << make_ns << '\n'
              << backend << ',' << type_name<T>() << ",comparison_mask_conversion,"
              << convert_ns << '\n'
              << backend << ',' << type_name<T>() << ",reduce_count_comparison,"
              << reduce_ns << '\n'
              << backend << ',' << type_name<T>() << ",simd_range_load_full,"
              << load_ns << '\n';
}
}

int main()
{
    std::cout << "backend,type,operation,ns_per_operation\n";
    benchmark_type<std::int8_t>();
    benchmark_type<std::uint8_t>();
    benchmark_type<std::int16_t>();
    benchmark_type<std::uint16_t>();
    benchmark_type<std::int32_t>();
    benchmark_type<std::uint32_t>();
    benchmark_type<std::int64_t>();
    benchmark_type<std::uint64_t>();
    std::cerr << "sink=" << sink << '\n';
}
