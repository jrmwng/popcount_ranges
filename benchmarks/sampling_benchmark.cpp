#include <popcount_ranges/set.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <random>
#include <string_view>
#include <vector>

namespace
{
std::uint64_t volatile sink = 0;

class counting_mt19937
{
    std::mt19937 m_engine;
    std::size_t m_calls{};

public:
    using result_type = std::mt19937::result_type;

    explicit counting_mt19937(result_type seed) noexcept
        : m_engine(seed)
    {}

    static constexpr result_type min() noexcept
    {
        return std::mt19937::min();
    }

    static constexpr result_type max() noexcept
    {
        return std::mt19937::max();
    }

    result_type operator()() noexcept
    {
        ++m_calls;
        return m_engine();
    }

    std::size_t calls() const noexcept
    {
        return m_calls;
    }
};

double measure_mt19937_ns_per_sample(std::size_t calls, std::size_t repetitions)
{
    std::mt19937 generator(0x5eedu);
    std::uint64_t checksum = 0;

    auto const started = std::chrono::steady_clock::now();
    for (std::size_t call = 0; call < calls; ++call)
        checksum += generator();
    auto const elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - started).count();

    sink += checksum;
    return elapsed / static_cast<double>(repetitions);
}

template <typename T, T R>
std::uint64_t popcount_sample(std::vector<std::uint32_t> const& population,
                              counting_mt19937& generator)
{
    using set_type = jrmwng::popcount_ranges::set<R>;
    constexpr auto count = jrmwng::popcount_ranges::nCr_read<T>(
        std::numeric_limits<T>::digits, std::integral_constant<std::size_t, R>{});
    std::uniform_int_distribution<std::uintmax_t> randev(0, count - 1);

    std::uint64_t checksum = 0;
    for (std::size_t position :
         jrmwng::popcount_ranges::std::set_bit_positions(set_type{}[randev(generator)]))
        checksum += population[position];
    return checksum;
}

template <typename T, T R>
void run_case(std::string_view type_name, std::size_t repetitions)
{
    constexpr auto N = static_cast<std::size_t>(std::numeric_limits<T>::digits);
    std::vector<std::uint32_t> population(N);
    for (std::size_t i = 0; i < N; ++i)
        population[i] = static_cast<std::uint32_t>(i + 1);

    counting_mt19937 popcount_generator(0x5eedu);
    auto const popcount_calls_before = popcount_generator.calls();
    std::uint64_t popcount_checksum = 0;
    auto const popcount_started = std::chrono::steady_clock::now();
    for (std::size_t repetition = 0; repetition < repetitions; ++repetition)
        popcount_checksum += popcount_sample<T, R>(population, popcount_generator);
    auto const popcount_elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - popcount_started).count();
    auto const popcount_calls = popcount_generator.calls() - popcount_calls_before;
    auto const popcount_mt19937_elapsed =
        measure_mt19937_ns_per_sample(popcount_calls, repetitions);

    counting_mt19937 standard_generator(0x5eedu);
    auto const standard_calls_before = standard_generator.calls();
    std::vector<std::uint32_t> standard_result(static_cast<std::size_t>(R));
    std::uint64_t standard_checksum = 0;
    auto const standard_started = std::chrono::steady_clock::now();
    for (std::size_t repetition = 0; repetition < repetitions; ++repetition)
    {
        std::sample(population.begin(), population.end(), standard_result.begin(),
                    static_cast<std::size_t>(R), standard_generator);
        for (std::uint32_t value : standard_result)
            standard_checksum += value;
    }
    auto const standard_elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - standard_started).count();
    auto const standard_calls = standard_generator.calls() - standard_calls_before;
    auto const standard_mt19937_elapsed =
        measure_mt19937_ns_per_sample(standard_calls, repetitions);

    sink += popcount_checksum + standard_checksum;
    std::cout << type_name << ',' << N << ',' << static_cast<std::size_t>(R) << ",popcount_ranges,"
              << std::fixed << std::setprecision(2)
              << popcount_elapsed / static_cast<double>(repetitions) << ','
              << popcount_mt19937_elapsed << ','
              << (popcount_elapsed / static_cast<double>(repetitions) - popcount_mt19937_elapsed) << ','
              << static_cast<double>(popcount_calls) / static_cast<double>(repetitions) << ','
              << popcount_checksum << '\n';
    std::cout << type_name << ',' << N << ',' << static_cast<std::size_t>(R) << ",std_sample,"
              << std::fixed << std::setprecision(2)
              << standard_elapsed / static_cast<double>(repetitions) << ','
              << standard_mt19937_elapsed << ','
              << (standard_elapsed / static_cast<double>(repetitions) - standard_mt19937_elapsed) << ','
              << static_cast<double>(standard_calls) / static_cast<double>(repetitions) << ','
              << standard_checksum << '\n';
}

template <typename T, T R>
void benchmark_case(std::string_view type_name, bool quick)
{
    run_case<T, R>(type_name, quick ? 1'000u : 100'000u);
}
}

int main(int argc, char const* argv[])
{
    bool const quick = argc > 1 && std::string_view(argv[1]) == "--quick";
    std::cout << "type,N,R,implementation,sample_ns_per_sample,mt19937_ns_per_sample,"
                 "non_mt19937_ns_per_sample,mt19937_calls_per_sample,checksum\n";

    benchmark_case<std::uint8_t, std::uint8_t{1}>("uint8", quick);
    benchmark_case<std::uint8_t, std::uint8_t{4}>("uint8", quick);
    benchmark_case<std::uint16_t, std::uint16_t{2}>("uint16", quick);
    benchmark_case<std::uint16_t, std::uint16_t{8}>("uint16", quick);
    benchmark_case<std::uint32_t, std::uint32_t{2}>("uint32", quick);
    benchmark_case<std::uint32_t, std::uint32_t{16}>("uint32", quick);
    benchmark_case<std::uint64_t, std::uint64_t{2}>("uint64", quick);
    benchmark_case<std::uint64_t, std::uint64_t{32}>("uint64", quick);

    using uint128 = jrmwng::popcount_ranges::std::uint<128>;
    using uint256 = jrmwng::popcount_ranges::std::uint<256>;
    using uint512 = jrmwng::popcount_ranges::std::uint<512>;
    benchmark_case<uint128, uint128{2}>("uint128", quick);
    benchmark_case<uint128, uint128{8}>("uint128", quick);
    benchmark_case<uint256, uint256{2}>("uint256", quick);
    benchmark_case<uint256, uint256{8}>("uint256", quick);
    benchmark_case<uint512, uint512{2}>("uint512", quick);
    benchmark_case<uint512, uint512{8}>("uint512", quick);

    std::cerr << "sink=" << sink << '\n';
}
