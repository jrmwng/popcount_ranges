#include <popcount_ranges/set.hpp>

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <vector>

namespace
{
std::uint64_t volatile sink = 0;

template <typename Set, typename Operation>
void run_case(std::string_view type_name, std::size_t popcount, std::string_view direction,
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
        operation(current, distance);
        result += static_cast<std::uint64_t>(*current);
    }
    auto const elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - started).count();
    sink += result;

    std::cout << type_name << ',' << popcount << ',' << direction << ',' << distance << ','
              << implementation << ',' << std::fixed << std::setprecision(2)
              << elapsed / static_cast<double>(repetitions) << ',' << result << '\n';
}

template <typename T, T Popcount, std::size_t Total>
void benchmark_set(std::string_view type_name, bool quick)
{
    using Set = jrmwng::popcount_ranges::set<Popcount>;
    std::vector<std::size_t> distances{1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096};
    if (quick)
        distances = {1, 2, 4, 16, 64, 256};

    for (std::size_t distance : distances)
    {
        if (distance >= Total)
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
    std::cout << "type,popcount,direction,distance,implementation,ns_per_operation,checksum\n";
    benchmark_set<std::uint16_t, std::uint16_t{2}, 120>("uint16", quick);
    benchmark_set<std::uint16_t, std::uint16_t{8}, 12870>("uint16", quick);
    benchmark_set<std::uint32_t, std::uint32_t{2}, 496>("uint32", quick);
    benchmark_set<std::uint32_t, std::uint32_t{16}, 601080390>("uint32", quick);
    std::cerr << "sink=" << sink << '\n';
}