#include <popcount_ranges/set.hpp>

#include "../benchmarks/subset_sum_simd.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <type_traits>
#include <vector>

namespace pc = jrmwng::popcount_ranges;

template <typename T, std::size_t N>
bool check_simd_sums(std::array<T, N> const& weights, std::uint32_t mask)
{
    subset_sum_simd const sum(weights);
    subset_sum_result<T> expected = 0;
    for (std::size_t bit = 0; bit < N; ++bit)
        if ((mask & (std::uint32_t{1} << bit)) != 0)
            expected += weights[bit];
    if (sum(mask) != expected)
    {
        std::cerr << "SIMD weighted sum failed for N=" << N << " mask=" << mask << '\n';
        return false;
    }
    return true;
}

template <typename T>
bool check_simd_type()
{
    std::array<T, 32> weights{};
    for (std::size_t i = 0; i < weights.size(); ++i)
    {
        if constexpr (std::is_same_v<T, float>)
            weights[i] = std::ldexp(static_cast<float>(static_cast<int>(i % 31) - 15), 119);
        else
        {
            using U = std::make_unsigned_t<T>;
            U const bits = static_cast<U>(std::uint32_t{0x9e3779b9} * static_cast<std::uint32_t>(i + 1));
            weights[i] = std::bit_cast<T>(bits);
        }
    }
    if constexpr (!std::is_same_v<T, float>)
    {
        weights[0] = std::numeric_limits<T>::min();
        weights[1] = std::numeric_limits<T>::max();
    }
    for (std::size_t i = 0; i < 32; ++i)
        if (!check_simd_sums(weights, std::uint32_t{1} << i))
            return false;
    if (!check_simd_sums(weights, 0) || !check_simd_sums(weights, UINT32_MAX))
        return false;
    std::mt19937 engine(0x5eedu);
    for (int i = 0; i < 10000; ++i)
        if (!check_simd_sums(weights, static_cast<std::uint32_t>(engine())))
            return false;
    std::array<T, 8> small{};
    for (std::size_t i = 0; i < small.size(); ++i)
        small[i] = weights[i];
    for (std::uint32_t mask = 0; mask < 256; ++mask)
        if (!check_simd_sums(small, mask))
            return false;
    std::array<T, 19> partial{};
    for (std::size_t i = 0; i < partial.size(); ++i)
        partial[i] = weights[i];
    if (!check_simd_sums(partial, UINT32_MAX) || !check_simd_sums(std::array<T, 0>{}, UINT32_MAX))
        return false;
    if constexpr (std::is_same_v<T, float>)
    {
        weights.fill(std::ldexp(15.0f, 119));
        if (!check_simd_sums(weights, UINT32_MAX))
            return false;
        weights.fill(std::ldexp(-15.0f, 119));
    }
    else
    {
        weights.fill(std::numeric_limits<T>::max());
        if (!check_simd_sums(weights, UINT32_MAX))
            return false;
        weights.fill(std::numeric_limits<T>::min());
    }
    return check_simd_sums(weights, UINT32_MAX);
}

bool check_simd_weighted_sums()
{
    if (!check_simd_type<std::uint8_t>() || !check_simd_type<std::uint16_t>() ||
        !check_simd_type<std::uint32_t>() || !check_simd_type<std::int8_t>() ||
        !check_simd_type<std::int16_t>() || !check_simd_type<std::int32_t>() ||
        !check_simd_type<float>())
        return false;
    std::array<std::uint8_t, 8> const small{0, 1, 2, 31, 127, 128, 254, 255};
    for (std::uint32_t mask = 0; mask < 256; ++mask)
        if (!check_simd_sums(small, mask))
            return false;
    std::array<std::uint8_t, 32> weights{};
    for (std::size_t i = 0; i < weights.size(); ++i)
        weights[i] = static_cast<std::uint8_t>((i * 59u + 17u) % 256u);
    for (std::size_t i = 0; i < 32; ++i)
        if (!check_simd_sums(weights, std::uint32_t{1} << i))
            return false;
    if (!check_simd_sums(weights, 0) || !check_simd_sums(weights, UINT32_MAX))
        return false;
    std::mt19937 engine(0x5eedu);
    for (int i = 0; i < 10000; ++i)
        if (!check_simd_sums(weights, static_cast<std::uint32_t>(engine())))
            return false;
    std::array<std::uint8_t, 20> const max_weights{
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255};
    if (!check_simd_sums(max_weights, UINT32_MAX))
        return false;

    std::array<std::uint8_t, 20> benchmark_weights{};
    for (std::size_t i = 0; i < benchmark_weights.size(); ++i)
        benchmark_weights[i] = static_cast<std::uint8_t>((i * 13 + 7) % 31 + 1);
    subset_sum_simd const sum(benchmark_weights);
    for (std::uint32_t mask = 0; mask < (std::uint32_t{1} << 20); ++mask)
    {
        int expected = 0;
        for (std::size_t bit = 0; bit < benchmark_weights.size(); ++bit)
            if ((mask & (std::uint32_t{1} << bit)) != 0)
                expected += benchmark_weights[bit];
        if (sum(mask) != expected)
        {
            std::cerr << "SIMD benchmark sum failed for mask=" << mask << '\n';
            return false;
        }
    }
    return true;
}

template <std::size_t N, std::size_t R>
bool check_subset_sum(std::array<int, N> const& weights, int target)
{
    using Set = pc::set<N, R>;
    using value_type = typename Set::iterator::value_type;
    std::vector<value_type> expected;
    for (std::uint32_t mask = 0; mask < (std::uint32_t{1} << N); ++mask)
    {
        if (std::popcount(mask) != R)
            continue;
        int sum = 0;
        for (std::size_t i = 0; i < N; ++i)
            if ((mask & (std::uint32_t{1} << i)) != 0)
                sum += weights[i];
        if (sum == target)
            expected.push_back(static_cast<value_type>(mask));
    }

    Set set;
    std::vector<value_type> actual;
    // Rank intervals allow workers to process disjoint portions of the range.
    for (std::size_t shard = 0; shard < 7; ++shard)
    {
        auto const first = set.size() * shard / 7;
        auto const last = set.size() * (shard + 1) / 7;
        auto it = set.begin() + static_cast<std::ptrdiff_t>(first);
        auto const end = set.begin() + static_cast<std::ptrdiff_t>(last);
        for (; it != end; ++it)
        {
            int sum = 0;
            for (std::size_t position : pc::stdx::set_bit_positions(*it))
                sum += weights[position];
            if (sum == target)
                actual.push_back(*it);
        }
    }
    if (actual != expected)
    {
        std::cerr << "subset sum mismatch for N=" << N << " R=" << R
                  << " target=" << target << '\n';
        return false;
    }
    return true;
}

bool check_sampling()
{
    pc::set<8, 3> choices;
    std::vector<std::uint8_t> expected;
    for (unsigned mask = 0; mask < 256; ++mask)
        if (std::popcount(mask) == 3)
            expected.push_back(static_cast<std::uint8_t>(mask));
    if (choices.size() != expected.size())
        return false;
    for (std::size_t rank = 0; rank < expected.size(); ++rank)
        if (choices[rank] != expected[rank])
            return false;

    std::mt19937 engine(0x5eedu);
    std::uniform_int_distribution<std::size_t> rank(0, choices.size() - 1);
    for (std::size_t i = 0; i < 1000; ++i)
    {
        auto const index = rank(engine);
        auto const sample = choices[index];
        std::vector<std::size_t> positions;
        for (auto position : pc::stdx::set_bit_positions(sample))
            positions.push_back(position);
        std::vector<std::size_t> reference;
        for (std::size_t bit = 0; bit < 8; ++bit)
            if ((expected[index] & (1u << bit)) != 0)
                reference.push_back(bit);
        if (positions != reference)
            return false;
    }
    return true;
}

bool check_sparse_pairs()
{
    pc::set<70, 2> choices;
    std::vector<std::array<std::size_t, 2>> actual;
    for (auto value : choices)
    {
        std::array<std::size_t, 2> positions{};
        std::size_t count = 0;
        for (auto position : pc::stdx::set_bit_positions(value))
        {
            if (count == positions.size())
                return false;
            positions[count++] = position;
        }
        if (count != 2)
            return false;
        if (positions[0] + positions[1] == 70)
            actual.push_back(positions);
    }
    std::vector<std::array<std::size_t, 2>> expected;
    for (std::size_t high = 1; high < 70; ++high)
        for (std::size_t low = 0; low < high; ++low)
            if (low + high == 70)
                expected.push_back({low, high});
    return actual == expected;
}

int main()
{
    std::array<int, 8> const weights{0, 1, 1, 2, 3, 5, 8, 13};
    if (!check_simd_weighted_sums() || !check_sampling() || !check_sparse_pairs() ||
        !check_subset_sum<8, 0>(weights, 0) ||
        !check_subset_sum<8, 0>(weights, 1) ||
        !check_subset_sum<8, 1>(weights, 1) ||
        !check_subset_sum<8, 3>(weights, 6) ||
        !check_subset_sum<8, 4>(weights, 100) ||
        !check_subset_sum<8, 7>(weights, 20) ||
        !check_subset_sum<8, 8>(weights, 33))
    {
        std::cerr << "set application tests failed\n";
        return 1;
    }
    std::cout << "set application tests passed\n";
}
