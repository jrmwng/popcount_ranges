#include <popcount_ranges/set.hpp>

#include "subset_sum_simd.hpp"

#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string_view>
#include <type_traits>

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

template <bool Branchless, typename T>
void record(result& found, std::uint32_t mask, T sum, T target)
{
    if constexpr (Branchless)
    {
        auto const match = static_cast<std::uint64_t>(sum == target);
        found.matches += match;
        found.mask_sum += std::uint64_t{mask} & (std::uint64_t{0} - match);
    }
    else if (sum == target)
    {
        ++found.matches;
        found.mask_sum += mask;
    }
}

template <std::size_t N, std::size_t R, bool Branchless, typename T>
NOINLINE result enumerate_set(subset_sum_simd<T, N> const& sum, subset_sum_result<T> target)
{
    result found;
    for (auto mask : pc::set<N, R>{})
        record<Branchless>(found, mask, sum(mask), target);
    return found;
}

template <std::size_t N, std::size_t R, bool Branchless, typename T>
NOINLINE result enumerate_masks(subset_sum_simd<T, N> const& sum, subset_sum_result<T> target)
{
    static_assert(N < 32);
    result found;
    for (std::uint32_t mask = 0; mask < (std::uint32_t{1} << N); ++mask)
    {
        if (std::popcount(mask) == R)
            record<Branchless>(found, mask, sum(mask), target);
    }
    return found;
}

template <std::size_t N, std::size_t R, typename T>
result scalar_reference(std::array<T, N> const& weights, subset_sum_result<T> target)
{
    result found;
    for (std::uint32_t mask = 0; mask < (std::uint32_t{1} << N); ++mask)
    {
        if (std::popcount(mask) != R)
            continue;
        subset_sum_result<T> sum = 0;
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

template <typename T, std::size_t N, std::size_t R>
bool run_case(std::size_t repetitions, std::string_view type_name, bool large)
{
    std::array<T, N> weights{};
    for (std::size_t i = 0; i < N; ++i)
    {
        if (!large)
            weights[i] = static_cast<T>((i * 13 + 7) % 31 + 1);
        else if constexpr (std::is_same_v<T, float>)
            weights[i] = std::ldexp(static_cast<float>(static_cast<int>((i * 13 + 7) % 31) - 15), 119);
        else
        {
            using U = std::make_unsigned_t<T>;
            U const bits = static_cast<U>(
                std::uint32_t{0x9e3779b9} * static_cast<std::uint32_t>(i + 1));
            weights[i] = i == 0 ? std::numeric_limits<T>::min() :
                         i == 1 ? std::numeric_limits<T>::max() : std::bit_cast<T>(bits);
        }
    }
    subset_sum_simd const sum(weights);
    using sum_type = subset_sum_result<T>;
    std::array<sum_type, 4> targets{};
    if (!large)
        targets = {static_cast<sum_type>(R * 8), static_cast<sum_type>(R * 12),
                   static_cast<sum_type>(R * 16), static_cast<sum_type>(R * 20)};
    else
        for (std::size_t t = 0; t < targets.size(); ++t)
            for (std::size_t i = 0; i < R; ++i)
                targets[t] += weights[(i + t * 3) % N];

    for (sum_type target : targets)
    {
        auto const expected = scalar_reference<N, R>(weights, target);
        if (enumerate_set<N, R, false>(sum, target) != expected ||
            enumerate_set<N, R, true>(sum, target) != expected ||
            enumerate_masks<N, R, false>(sum, target) != expected ||
            enumerate_masks<N, R, true>(sum, target) != expected)
        {
            std::cerr << "subset sum mismatch for type=" << type_name << " large=" << large
                      << " N=" << N << " R=" << R
                      << " target=" << target << '\n';
            return false;
        }
    }

    auto measure = [&](auto algorithm, std::string_view name, std::string_view handling)
    {
        result total;
        auto const start = std::chrono::steady_clock::now();
        for (std::size_t i = 0; i < repetitions; ++i)
        {
            auto const current = algorithm(sum, targets[i % targets.size()]);
            total.matches += current.matches;
            total.mask_sum += current.mask_sum;
        }
        auto const elapsed = std::chrono::duration<double, std::nano>(
            std::chrono::steady_clock::now() - start).count();
        std::cout << type_name << ',' << (large ? "large" : "small") << ',' << N << ',' << R
                  << ',' << name << ',' << handling << ',' << repetitions << ','
                  << std::fixed << std::setprecision(2) << elapsed / repetitions
                  << ',' << total.matches << ',' << total.mask_sum << '\n';
        return total;
    };
    auto const set_branch = measure(enumerate_set<N, R, false, T>, "set", "conditional");
    auto const set_mask = measure(enumerate_set<N, R, true, T>, "set", "masked");
    auto const brute_branch = measure(enumerate_masks<N, R, false, T>, "brute_force", "conditional");
    auto const brute_mask = measure(enumerate_masks<N, R, true, T>, "brute_force", "masked");
    if (set_branch != set_mask || set_branch != brute_branch || set_branch != brute_mask)
    {
        std::cerr << "timed subset sum checksum mismatch for type=" << type_name
                  << " large=" << large << " N=" << N << " R=" << R << '\n';
        return false;
    }
    return true;
}

template <typename T>
bool run_type(std::size_t repetitions, std::string_view name)
{
    for (bool large : {false, true})
        if (!run_case<T, 16, 4>(repetitions, name, large) ||
            !run_case<T, 20, 2>(repetitions, name, large) ||
            !run_case<T, 20, 10>(repetitions, name, large) ||
            !run_case<T, 20, 18>(repetitions, name, large))
            return false;
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
    std::cout << "type,range,N,R,implementation,match_handling,repetitions,ns_per_search,matches,mask_checksum\n";
    std::size_t const repetitions = quick ? 4 : 100;
    return run_type<std::uint8_t>(repetitions, "uint8") &&
           run_type<std::uint16_t>(repetitions, "uint16") &&
           run_type<std::uint32_t>(repetitions, "uint32") &&
           run_type<std::int8_t>(repetitions, "int8") &&
           run_type<std::int16_t>(repetitions, "int16") &&
           run_type<std::int32_t>(repetitions, "int32") &&
           run_type<float>(repetitions, "float") ? 0 : 1;
}
