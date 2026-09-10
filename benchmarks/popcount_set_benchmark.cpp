#include <popcount_ranges/set.hpp>

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string_view>
#include <vector>

namespace
{
std::uint64_t volatile sink = 0;

template <typename T>
inline void do_not_optimize(T& value) noexcept
{
#if defined(__clang__) || defined(__GNUC__)
    asm volatile("" : "+m"(value) : : "memory");
#else
    static_cast<void>(value);
#endif
}

template <typename Set, typename Operation>
void run_case(std::string_view type_name, std::size_t reduce_count, std::string_view direction,
              std::size_t distance, std::size_t repetitions, Operation operation,
              std::string_view implementation)
{
    using iterator = typename Set::iterator;
    Set set;
    auto const start = direction == "increment" ? set.begin() : set.end();
    std::uint64_t result = 0;

    auto const started = std::chrono::steady_clock::now();
    for (std::size_t repetition = 0; repetition < repetitions; ++repetition)
    {
        iterator current = start;
        do_not_optimize(current);
        operation(current, distance);
        result += static_cast<std::uint64_t>(*current);
    }
    auto const elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - started).count();
    sink += result;

    std::cout << type_name << ',' << reduce_count << ',' << direction << ',' << distance << ','
              << implementation << ',' << std::fixed << std::setprecision(2)
              << elapsed / static_cast<double>(repetitions) << ',' << result << '\n';
}

template <typename T, T Popcount>
void benchmark_set(std::string_view type_name, bool quick)
{
    using Set = jrmwng::popcount_ranges::set<Popcount>;
    constexpr auto total = static_cast<std::size_t>(jrmwng::popcount_ranges::nCr_read<T>(
        std::numeric_limits<T>::digits, std::integral_constant<std::size_t, Popcount>{}));
    std::vector<std::size_t> distances{1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096};
    if (quick)
        distances = {1, 2, 4, 16, 64, 256};

    for (std::size_t distance : distances)
    {
        if (distance >= total)
            continue;
        auto const repetitions = quick ? 1000u : 10000u;
        run_case<Set>(type_name, Popcount, "increment", distance, repetitions,
            [](auto& iterator, std::size_t offset) { for (std::size_t i = 0; i < offset; ++i) ++iterator; },
            "repeated_gosper");
        run_case<Set>(type_name, Popcount, "increment", distance, repetitions,
            [](auto& iterator, std::size_t offset) { iterator.advance(static_cast<std::ptrdiff_t>(offset)); },
            "unrank_advance");
        run_case<Set>(type_name, Popcount, "decrement", distance, repetitions,
            [](auto& iterator, std::size_t offset) { for (std::size_t i = 0; i < offset; ++i) --iterator; },
            "repeated_gosper");
        run_case<Set>(type_name, Popcount, "decrement", distance, repetitions,
            [](auto& iterator, std::size_t offset) { iterator.advance(-static_cast<std::ptrdiff_t>(offset)); },
            "unrank_advance");
    }
}
}

int main(int argc, char const* argv[])
{
    bool const quick = argc > 1 && std::string_view(argv[1]) == "--quick";
    std::cout << "type,reduce_count,direction,distance,implementation,ns_per_operation,checksum\n";
    benchmark_set<std::uint8_t, std::uint8_t{1}>("uint8", quick);
    benchmark_set<std::uint8_t, std::uint8_t{2}>("uint8", quick);
    benchmark_set<std::uint8_t, std::uint8_t{3}>("uint8", quick);
    benchmark_set<std::uint8_t, std::uint8_t{4}>("uint8", quick);
    benchmark_set<std::uint16_t, std::uint16_t{1}>("uint16", quick);
    benchmark_set<std::uint16_t, std::uint16_t{2}>("uint16", quick);
    benchmark_set<std::uint16_t, std::uint16_t{3}>("uint16", quick);
    benchmark_set<std::uint16_t, std::uint16_t{8}>("uint16", quick);
    benchmark_set<std::uint32_t, std::uint32_t{1}>("uint32", quick);
    benchmark_set<std::uint32_t, std::uint32_t{2}>("uint32", quick);
    benchmark_set<std::uint32_t, std::uint32_t{3}>("uint32", quick);
    benchmark_set<std::uint32_t, std::uint32_t{16}>("uint32", quick);
    std::cerr << "sink=" << sink << '\n';
}