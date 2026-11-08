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

#ifndef POPCOUNT_RANGES_BENCHMARK_COMPILER
#define POPCOUNT_RANGES_BENCHMARK_COMPILER "unknown"
#endif

namespace
{
std::uint64_t volatile sink = 0;

#if defined(POPCOUNT_RANGES_LOWER_BOUND_STD)
constexpr std::string_view lower_bound_implementation = "std_lower_bound";
#elif defined(POPCOUNT_RANGES_LOWER_BOUND_DESCENT)
constexpr std::string_view lower_bound_implementation = "custom_with_descent";
#else
constexpr std::string_view lower_bound_implementation = "custom_without_descent";
#endif

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

template <typename T, T R,
          typename Set = jrmwng::popcount_ranges::set<std::numeric_limits<T>::digits,
                                                       static_cast<std::size_t>(R), T>>
std::uint64_t popcount_sample(std::vector<std::uint32_t> const& population,
                              counting_mt19937& generator)
{
    constexpr auto count = jrmwng::popcount_ranges::nCr_read<T>(
        std::numeric_limits<T>::digits, std::integral_constant<std::size_t, R>{});
    std::uniform_int_distribution<std::uintmax_t> randev(0, count - 1);

    std::uint64_t checksum = 0;
    for (std::size_t position :
         jrmwng::popcount_ranges::stdx::set_bit_positions(Set{}[randev(generator)]))
        checksum += population[position];
    return checksum;
}

template <typename T, T R,
          typename Set = jrmwng::popcount_ranges::set<std::numeric_limits<T>::digits,
                                                       static_cast<std::size_t>(R), T>>
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
        popcount_checksum += popcount_sample<T, R, Set>(population, popcount_generator);
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
    std::cout << POPCOUNT_RANGES_BENCHMARK_COMPILER << ',' << type_name << ',' << N << ','
              << static_cast<std::size_t>(R) << ',';
#ifdef POPCOUNT_RANGES_LOWER_BOUND_DESCENT
    std::cout << POPCOUNT_RANGES_LOWER_BOUND_THRESHOLD;
#endif
    std::cout << ',' << lower_bound_implementation << ','
              << std::fixed << std::setprecision(3)
              << popcount_elapsed / static_cast<double>(repetitions) << ','
              << popcount_mt19937_elapsed << ','
              << (popcount_elapsed / static_cast<double>(repetitions) - popcount_mt19937_elapsed) << ','
              << static_cast<double>(popcount_calls) / static_cast<double>(repetitions) << ','
              << popcount_checksum << '\n';
    std::cout << POPCOUNT_RANGES_BENCHMARK_COMPILER << ',' << type_name << ',' << N << ','
              << static_cast<std::size_t>(R) << ",,std_sample,"
              << std::fixed << std::setprecision(3)
              << standard_elapsed / static_cast<double>(repetitions) << ','
              << standard_mt19937_elapsed << ','
              << (standard_elapsed / static_cast<double>(repetitions) - standard_mt19937_elapsed) << ','
              << static_cast<double>(standard_calls) / static_cast<double>(repetitions) << ','
              << standard_checksum << '\n';
}

template <typename T, T R,
          typename Set = jrmwng::popcount_ranges::set<std::numeric_limits<T>::digits,
                                                       static_cast<std::size_t>(R), T>>
void benchmark_case(std::string_view type_name, std::size_t repetitions)
{
    run_case<T, R, Set>(type_name, repetitions);
}

#ifdef POPCOUNT_RANGES_SAMPLING_EXPERIMENTAL
template <typename T, std::size_t R>
std::size_t unrank_search_steps(std::size_t n, std::uintmax_t remaining)
{
    auto const [found_n, found_nCr] = jrmwng::popcount_ranges::nCr_search<T>(
        R, n, remaining, std::integral_constant<std::size_t, R>{});
    if constexpr (R == 1)
        return found_n;
    else
        return found_n + unrank_search_steps<T, R - 1>(found_n, remaining - found_nCr);
}

template <typename T, T R,
          typename Set = jrmwng::popcount_ranges::set<std::numeric_limits<T>::digits,
                                                       static_cast<std::size_t>(R), T>>
bool decompose_case(std::string_view type_name, std::size_t repetitions)
{
    constexpr std::size_t N = static_cast<std::size_t>(std::numeric_limits<T>::digits);
    constexpr std::size_t popcount = static_cast<std::size_t>(R);
    constexpr std::size_t search_popcount =
        std::min(popcount, N - popcount);
    constexpr auto count = jrmwng::popcount_ranges::nCr_read<T>(
        N, std::integral_constant<std::size_t, popcount>{});
    auto report = [&](std::string_view phase, double elapsed, std::uint64_t checksum)
    {
        sink += checksum;
        std::cout << POPCOUNT_RANGES_BENCHMARK_COMPILER << ',' << type_name << ',' << N << ',' << popcount << ','
                  << POPCOUNT_RANGES_LOWER_BOUND_THRESHOLD << ',' << phase << ','
                  << std::fixed << std::setprecision(2)
                  << elapsed / static_cast<double>(repetitions) << ',' << checksum << '\n';
    };

    std::vector<std::uintmax_t> ranks(repetitions);
    counting_mt19937 rank_generator(0x5eedu);
    std::uniform_int_distribution<std::uintmax_t> distribution(0, count - 1);
    auto const rank_started = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < repetitions; ++i)
        ranks[i] = distribution(rank_generator);
    auto const rank_elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - rank_started).count();
    std::uint64_t rank_checksum = 0;
    for (std::uintmax_t rank : ranks)
        rank_checksum += rank;
    report("rank_generation", rank_elapsed, rank_checksum);

    std::vector<std::uint32_t> population(N);
    for (std::size_t i = 0; i < N; ++i)
        population[i] = static_cast<std::uint32_t>(i + 1);

    using value_type = decltype(Set{}[std::size_t{}]);
    using positions_type = std::array<std::size_t, popcount>;
    constexpr std::size_t batch_size = 32;
    std::vector<value_type> values(batch_size);
    std::vector<positions_type> positions(repetitions);
    std::uint64_t traversal_checksum = 0;
    double traversal_elapsed = 0;
    for (std::size_t batch = 0; batch < repetitions; batch += batch_size)
    {
        std::size_t const batch_count = std::min(batch_size, repetitions - batch);
        for (std::size_t i = 0; i < batch_count; ++i)
            values[i] = Set{}[ranks[batch + i]];

        auto const traversal_started = std::chrono::steady_clock::now();
        for (std::size_t i = 0; i < batch_count; ++i)
        {
            std::size_t position_index = 0;
            for (std::size_t position :
                 jrmwng::popcount_ranges::stdx::set_bit_positions(values[i]))
            {
                if (position_index == popcount)
                {
                    std::cerr << "decomposition aborted: " << type_name << " R="
                              << popcount << " rank=" << ranks[batch + i]
                              << " produced more than " << popcount << " set bits\n";
                    return false;
                }
                positions[batch + i][position_index++] = position;
                traversal_checksum += position;
            }
            if (position_index != popcount)
            {
                std::cerr << "decomposition aborted: " << type_name << " R="
                          << popcount << " rank=" << ranks[batch + i]
                          << " produced " << position_index << " set bits\n";
                return false;
            }
        }
        traversal_elapsed += std::chrono::duration<double, std::nano>(
            std::chrono::steady_clock::now() - traversal_started).count();
    }
    report("bit_position_traversal", traversal_elapsed, traversal_checksum);

    std::uint64_t population_checksum = 0;
    auto const population_started = std::chrono::steady_clock::now();
    for (positions_type const& sample_positions : positions)
    {
        for (std::size_t position : sample_positions)
            population_checksum += population[position];
    }
    auto const population_elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - population_started).count();
    report("population_loads", population_elapsed, population_checksum);

    std::uint64_t search_checksum = 0;
    auto const search_started = std::chrono::steady_clock::now();
    for (std::uintmax_t rank : ranks)
        search_checksum += unrank_search_steps<T, search_popcount>(
            std::numeric_limits<T>::digits + 1u,
            popcount > N / 2u ? count - 1u - rank : rank);
    auto const search_elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - search_started).count();
    report("nCr_search_steps_only", search_elapsed, search_checksum);

    counting_mt19937 end_to_end_generator(0x5eedu);
    std::uniform_int_distribution<std::uintmax_t> end_distribution(0, count - 1);
    for (std::size_t i = 0; i < repetitions; ++i)
    {
        if (end_distribution(end_to_end_generator) != ranks[i])
        {
            std::cerr << "rank sequence mismatch for " << type_name
                      << " R=" << popcount << '\n';
                return false;
        }
    }
    end_to_end_generator = counting_mt19937(0x5eedu);
    std::uint64_t end_to_end_checksum = 0;
    auto const end_to_end_started = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < repetitions; ++i)
        end_to_end_checksum += popcount_sample<T, R, Set>(population, end_to_end_generator);
    auto const end_to_end_elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - end_to_end_started).count();
    if (end_to_end_checksum != population_checksum)
    {
        std::cerr << "decomposition checksum mismatch for " << type_name
                  << " R=" << popcount << '\n';
        return false;
    }
    report("end_to_end", end_to_end_elapsed, end_to_end_checksum);
    return true;
}
#endif

#ifdef POPCOUNT_RANGES_SAMPLING_EXPERIMENTAL
template <typename T, std::size_t... Rs>
void benchmark_representable_popcounts(std::string_view type_name, std::size_t repetitions,
                                       std::index_sequence<Rs...>)
{
    constexpr std::size_t N = static_cast<std::size_t>(std::numeric_limits<T>::digits);
    (benchmark_case<T, T{Rs}>(type_name, repetitions), ...);
    ((Rs != N - Rs
          ? (benchmark_case<T, T{N - Rs}>(type_name, repetitions), void())
          : void()), ...);
}

template <typename T>
void benchmark_type(std::string_view type_name, std::size_t repetitions)
{
    constexpr std::size_t max_r =
        jrmwng::popcount_ranges::nCr_max_R(std::numeric_limits<T>::digits);
    benchmark_representable_popcounts<T>(
        type_name, repetitions, std::make_index_sequence<max_r + 1>{});
}

template <std::size_t N, std::size_t R>
void benchmark_sparse_case(std::string_view type_name, std::size_t repetitions)
{
    using sparse_type = jrmwng::popcount_ranges::stdx::uint_indices<N, R>;
    benchmark_case<sparse_type, sparse_type{}>(type_name, repetitions);
}

template <std::size_t N, std::size_t HoleCount>
void benchmark_sparse_complement_case(std::string_view type_name, std::size_t repetitions)
{
    using sparse_type = jrmwng::popcount_ranges::stdx::uint_complement_indices<N, HoleCount>;
    benchmark_case<sparse_type, sparse_type{}>(type_name, repetitions);
}

template <std::size_t N, std::size_t... Rs>
void benchmark_sparse_representable_popcounts(std::string_view type_name, std::size_t repetitions,
                                              std::index_sequence<Rs...>)
{
    (benchmark_sparse_case<N, Rs>(type_name, repetitions), ...);
    (benchmark_sparse_complement_case<N, Rs>(type_name, repetitions), ...);
}

template <std::size_t N>
void benchmark_sparse_type(std::string_view type_name, std::size_t repetitions)
{
    constexpr std::size_t max_r = jrmwng::popcount_ranges::nCr_max_R(N);
    benchmark_sparse_representable_popcounts<N>(
        type_name, repetitions, std::make_index_sequence<max_r + 1>{});
}
#else
template <typename T>
void benchmark_type(std::string_view type_name, std::size_t repetitions)
{
    constexpr std::size_t n = std::numeric_limits<T>::digits;
    constexpr std::size_t max_r = jrmwng::popcount_ranges::nCr_max_R(n);
    benchmark_case<T, T{0}>(type_name, repetitions);
    benchmark_case<T, T{1}>(type_name, repetitions);
    benchmark_case<T, T{2}>(type_name, repetitions);
    if constexpr (max_r > 2)
        benchmark_case<T, T{max_r}>(type_name, repetitions);
    benchmark_case<T, T{n - 1}>(type_name, repetitions);
    benchmark_case<T, T{n - 2}>(type_name, repetitions);
    benchmark_case<T, T{n}>(type_name, repetitions);
}

template <std::size_t N>
void benchmark_sparse_type(std::string_view type_name, std::size_t repetitions)
{
    using namespace jrmwng::popcount_ranges::stdx;
    benchmark_case<uint_indices<N, 0>, uint_indices<N, 0>{}>(type_name, repetitions);
    benchmark_case<uint_indices<N, 1>, uint_indices<N, 1>{}>(type_name, repetitions);
    benchmark_case<uint_indices<N, 2>, uint_indices<N, 2>{}>(type_name, repetitions);
    benchmark_case<uint_complement_indices<N, 0>, uint_complement_indices<N, 0>{}>(type_name, repetitions);
    benchmark_case<uint_complement_indices<N, 1>, uint_complement_indices<N, 1>{}>(type_name, repetitions);
    benchmark_case<uint_complement_indices<N, 2>, uint_complement_indices<N, 2>{}>(type_name, repetitions);
}
#endif
}

int main(int argc, char const* argv[])
{
    bool const quick = argc > 1 && std::string_view(argv[1]) == "--quick";
    bool const medium = argc > 1 && std::string_view(argv[1]) == "--medium";
#ifdef POPCOUNT_RANGES_SAMPLING_EXPERIMENTAL
    bool const decompose = argc > 1 && std::string_view(argv[1]) == "--decompose";
#else
    bool const decompose = false;
#endif
    bool const full = argc <= 1 || std::string_view(argv[1]) == "--full";
    if (argc > 2 || (!quick && !medium && !decompose && !full))
    {
        std::cerr << "usage: sampling_benchmark [--quick|--medium|--full"
#ifdef POPCOUNT_RANGES_SAMPLING_EXPERIMENTAL
                     "|--decompose"
#endif
                     "]\n";
        return 2;
    }
#ifdef POPCOUNT_RANGES_SAMPLING_EXPERIMENTAL
    if (decompose)
    {
        std::cout << "compiler,type,N,R,lower_bound_threshold,phase,ns_per_sample,checksum\n";
        using uint8192 = jrmwng::popcount_ranges::stdx::uint<8192>;
        using sparse_uint8192_r1 = jrmwng::popcount_ranges::stdx::uint_indices<8192, 1>;
        using sparse_uint8192_r2 = jrmwng::popcount_ranges::stdx::uint_indices<8192, 2>;
        using sparse_uint8192_r3 = jrmwng::popcount_ranges::stdx::uint_indices<8192, 3>;
        using sparse_uint8192_r4 = jrmwng::popcount_ranges::stdx::uint_indices<8192, 4>;
        using sparse_uint8192_r5 = jrmwng::popcount_ranges::stdx::uint_indices<8192, 5>;
        using sparse_complement_uint8192_h1 =
            jrmwng::popcount_ranges::stdx::uint_complement_indices<8192, 1>;
        using sparse_complement_uint8192_h2 =
            jrmwng::popcount_ranges::stdx::uint_complement_indices<8192, 2>;
        constexpr std::size_t repetitions = 100'000u;
        constexpr std::size_t high_popcount_repetitions = 1'000u;
        if (!decompose_case<uint8192, uint8192{1}>("uint8192", repetitions) ||
            !decompose_case<uint8192, uint8192{2}>("uint8192", repetitions) ||
            !decompose_case<uint8192, uint8192{3}>("uint8192", repetitions) ||
            !decompose_case<uint8192, uint8192{4}>("uint8192", repetitions) ||
            !decompose_case<uint8192, uint8192{5}>("uint8192", repetitions) ||
            !decompose_case<uint8192, uint8192{8191}>("uint8192", high_popcount_repetitions) ||
            !decompose_case<uint8192, uint8192{8190}>("uint8192", high_popcount_repetitions) ||
            !decompose_case<sparse_uint8192_r1, sparse_uint8192_r1{}>("sparse_uint8192", repetitions) ||
            !decompose_case<sparse_uint8192_r2, sparse_uint8192_r2{}>("sparse_uint8192", repetitions) ||
            !decompose_case<sparse_uint8192_r3, sparse_uint8192_r3{}>("sparse_uint8192", repetitions) ||
            !decompose_case<sparse_uint8192_r4, sparse_uint8192_r4{}>("sparse_uint8192", repetitions) ||
            !decompose_case<sparse_uint8192_r5, sparse_uint8192_r5{}>("sparse_uint8192", repetitions) ||
            !decompose_case<sparse_complement_uint8192_h1, sparse_complement_uint8192_h1{}>(
                "sparse_uint8192", high_popcount_repetitions) ||
            !decompose_case<sparse_complement_uint8192_h2, sparse_complement_uint8192_h2{}>(
                "sparse_uint8192", high_popcount_repetitions))
            return 1;
        std::cerr << "sink=" << sink << '\n';
        return 0;
    }
#endif

    std::cout << "compiler,type,N,R,lower_bound_threshold,implementation,sample_ns_per_sample,mt19937_ns_per_sample,"
                 "non_mt19937_ns_per_sample,mt19937_calls_per_sample,checksum\n";

    std::size_t const repetitions = quick ? 1'000u : medium ? 10'000u : 100'000u;
    benchmark_type<std::uint8_t>("uint8", repetitions);
    benchmark_type<std::uint16_t>("uint16", repetitions);
    benchmark_type<std::uint32_t>("uint32", repetitions);
    benchmark_type<std::uint64_t>("uint64", repetitions);

    using uint128 = jrmwng::popcount_ranges::stdx::uint<128>;
    using uint256 = jrmwng::popcount_ranges::stdx::uint<256>;
    using uint512 = jrmwng::popcount_ranges::stdx::uint<512>;
    using uint1024 = jrmwng::popcount_ranges::stdx::uint<1024>;
    using uint2048 = jrmwng::popcount_ranges::stdx::uint<2048>;
    using uint4096 = jrmwng::popcount_ranges::stdx::uint<4096>;
    using uint8192 = jrmwng::popcount_ranges::stdx::uint<8192>;
    benchmark_type<uint128>("uint128", repetitions);
    benchmark_type<uint256>("uint256", repetitions);
#ifdef POPCOUNT_RANGES_SAMPLING_EXPERIMENTAL
    benchmark_type<uint512>("uint512", repetitions);
    benchmark_type<uint1024>("uint1024", repetitions);
    benchmark_type<uint2048>("uint2048", repetitions);
    benchmark_type<uint4096>("uint4096", repetitions);
#endif
    benchmark_type<uint8192>("uint8192", repetitions);
    benchmark_sparse_type<128>("sparse_uint128", repetitions);
    benchmark_sparse_type<256>("sparse_uint256", repetitions);
#ifdef POPCOUNT_RANGES_SAMPLING_EXPERIMENTAL
    benchmark_sparse_type<512>("sparse_uint512", repetitions);
    benchmark_sparse_type<1024>("sparse_uint1024", repetitions);
    benchmark_sparse_type<2048>("sparse_uint2048", repetitions);
    benchmark_sparse_type<4096>("sparse_uint4096", repetitions);
#endif
    benchmark_sparse_type<8192>("sparse_uint8192", repetitions);

    std::cerr << "sink=" << sink << '\n';
}
