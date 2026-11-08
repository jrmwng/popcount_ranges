#include <popcount_ranges/set.hpp>

#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string_view>

#if defined(_MSC_VER)
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE __attribute__((noinline))
#endif

namespace pc = jrmwng::popcount_ranges;

struct result
{
    std::uint64_t matches{};
    std::uint64_t mask_sum{};
    bool operator==(result const&) const = default;
};

template <std::size_t N, std::size_t R>
NOINLINE result enumerate_set(std::array<int, N> const& weights, int target)
{
    result found;
    for (auto mask : pc::set<N, R>{})
    {
        int sum = 0;
        for (auto position : pc::stdx::set_bit_positions(mask))
            sum += weights[position];
        if (sum == target)
        {
            ++found.matches;
            found.mask_sum += mask;
        }
    }
    return found;
}

template <std::size_t N, std::size_t R>
NOINLINE result enumerate_masks(std::array<int, N> const& weights, int target)
{
    result found;
    for (std::uint32_t mask = 0; mask < (std::uint32_t{1} << N); ++mask)
    {
        if (std::popcount(mask) != R)
            continue;
        int sum = 0;
        for (std::size_t bit = 0; bit < N; ++bit)
            if ((mask & (std::uint32_t{1} << bit)) != 0)
                sum += weights[bit];
        if (sum == target)
        {
            ++found.matches;
            found.mask_sum += mask;
        }
    }
    return found;
}

template <std::size_t N, std::size_t R>
bool run_case(std::size_t repetitions)
{
    std::array<int, N> weights{};
    for (std::size_t i = 0; i < N; ++i)
        weights[i] = static_cast<int>((i * 13 + 7) % 31 + 1);
    std::array<int, 4> const targets{
        static_cast<int>(R * 8), static_cast<int>(R * 12),
        static_cast<int>(R * 16), static_cast<int>(R * 20)};
    for (int target : targets)
    {
        if (enumerate_set<N, R>(weights, target) != enumerate_masks<N, R>(weights, target))
        {
            std::cerr << "subset sum mismatch for N=" << N << " R=" << R
                      << " target=" << target << '\n';
            return false;
        }
    }

    auto measure = [&](auto algorithm, std::string_view name)
    {
        result total;
        auto const start = std::chrono::steady_clock::now();
        for (std::size_t i = 0; i < repetitions; ++i)
        {
            auto const current = algorithm(weights, targets[i % targets.size()]);
            total.matches += current.matches;
            total.mask_sum += current.mask_sum;
        }
        auto const elapsed = std::chrono::duration<double, std::nano>(
            std::chrono::steady_clock::now() - start).count();
        std::cout << N << ',' << R << ',' << name << ',' << repetitions << ','
                  << std::fixed << std::setprecision(2) << elapsed / repetitions
                  << ',' << total.matches << ',' << total.mask_sum << '\n';
        return total;
    };
    auto const set_result = measure(enumerate_set<N, R>, "set");
    auto const mask_result = measure(enumerate_masks<N, R>, "brute_force");
    if (set_result != mask_result)
    {
        std::cerr << "timed subset sum checksum mismatch for N=" << N << " R=" << R << '\n';
        return false;
    }
    return true;
}

int main(int argc, char const* argv[])
{
    bool const quick = argc == 2 && std::string_view(argv[1]) == "--quick";
    if (argc > 2 || (argc == 2 && !quick))
    {
        std::cerr << "usage: subset_sum_benchmark [--quick]\n";
        return 2;
    }
    std::cout << "N,R,implementation,repetitions,ns_per_search,matches,mask_checksum\n";
    std::size_t const repetitions = quick ? 4 : 100;
    return run_case<16, 4>(repetitions) && run_case<20, 2>(repetitions) &&
           run_case<20, 10>(repetitions) && run_case<20, 18>(repetitions) ? 0 : 1;
}
