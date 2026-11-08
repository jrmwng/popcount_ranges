#include <popcount_ranges/set.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <string_view>
#include <vector>

namespace pc = jrmwng::popcount_ranges;

template <typename T>
std::uint64_t checksum(T const& value)
{
    std::uint64_t result = 0;
    for (std::size_t position : pc::stdx::set_bit_positions(value))
        result = result * 131 + position + 1;
    return result;
}

template <typename Set>
bool run_case(std::string_view name, std::size_t n, std::size_t r, bool quick)
{
    Set set;
    auto const count = std::min<std::size_t>(set.size(), quick ? 128 : 1024);
    auto const first = (set.size() - count) / 2;
    auto begin = set.begin() + static_cast<std::ptrdiff_t>(first);
    auto end = begin + static_cast<std::ptrdiff_t>(count);
    std::vector<typename Set::iterator::value_type> reference;
    for (auto it = begin; it != end; ++it)
        reference.push_back(*it);
    if (reference.size() != count)
        return false;
    for (std::size_t i = 0; i < count; ++i)
        if (set[first + i] != reference[i])
        {
            std::cerr << name << ": rank lookup mismatch at " << first + i << '\n';
            return false;
        }
    auto reverse = end;
    for (std::size_t i = count; i > 0; --i)
        if (*--reverse != reference[i - 1])
        {
            std::cerr << name << ": reverse traversal mismatch\n";
            return false;
        }

    std::vector<std::size_t> ranks(count);
    std::mt19937 engine(0x5eedu);
    std::uniform_int_distribution<std::size_t> distribution(0, set.size() - 1);
    for (auto& rank : ranks)
        rank = distribution(engine);

    std::size_t const repetitions = quick ? 2 : 20;
    auto measure = [&](std::string_view operation, auto work)
    {
        std::uint64_t total = 0;
        auto const started = std::chrono::steady_clock::now();
        for (std::size_t rep = 0; rep < repetitions; ++rep)
            total += work();
        auto const elapsed = std::chrono::duration<double, std::nano>(
            std::chrono::steady_clock::now() - started).count();
        std::cout << name << ',' << n << ',' << r << ',' << operation << ','
                  << count << ',' << repetitions << ',' << std::fixed << std::setprecision(2)
                  << elapsed / (count * repetitions) << ',' << total << '\n';
        return total;
    };
    auto const forward = measure("forward", [&]
    {
        std::uint64_t sum = 0;
        for (auto it = begin; it != end; ++it)
            sum += checksum(*it);
        return sum;
    });
    auto const backward = measure("backward", [&]
    {
        std::uint64_t sum = 0;
        for (auto it = end; it != begin;)
            sum += checksum(*--it);
        return sum;
    });
    auto const indexed = measure("sequential_index", [&]
    {
        std::uint64_t sum = 0;
        for (std::size_t i = 0; i < count; ++i)
            sum += checksum(set[first + i]);
        return sum;
    });
    measure("random_index", [&]
    {
        std::uint64_t sum = 0;
        for (auto rank : ranks)
            sum += checksum(set[rank]);
        return sum;
    });
    if (forward != backward || forward != indexed)
    {
        std::cerr << name << ": operation checksum mismatch\n";
        return false;
    }
    return true;
}

int main(int argc, char const* argv[])
{
    bool const quick = argc == 2 && std::string_view(argv[1]) == "--quick";
    if (argc > 2 || (argc == 2 && !quick))
    {
        std::cerr << "usage: set_operations_benchmark [--quick]\n";
        return 2;
    }
    std::cout << "representation,N,R,operation,values,repetitions,ns_per_value,checksum\n";
    return run_case<pc::set<16, 4>>("native", 16, 4, quick) &&
           run_case<pc::set<64, 2>>("native", 64, 2, quick) &&
           run_case<pc::set<128, 2>>("sparse", 128, 2, quick) &&
           run_case<pc::set<128, 2, pc::stdx::uint<128>>>("dense", 128, 2, quick) &&
           run_case<pc::set<128, 126, pc::stdx::uint_complement_indices<128, 2>>>(
               "complement", 128, 126, quick) ? 0 : 1;
}
