#include <popcount_set.hpp>

#include <array>
#include <bit>
#include <cstdint>
#include <iostream>
#include <vector>

namespace
{
template <typename T, T Popcount>
std::vector<T> collect_first(std::size_t limit)
{
    using Set = jrmwng::popcount_ranges::popcount_set<Popcount>;
    std::vector<T> values;

    for (auto value : Set{})
    {
        values.push_back(static_cast<T>(value));
        if (values.size() == limit)
            break;
    }

    return values;
}

template <typename T, T Popcount>
bool check_exact_prefix(std::initializer_list<T> expected)
{
    auto actual = collect_first<T, Popcount>(expected.size());
    auto expected_vector = std::vector<T>(expected.begin(), expected.end());

    if (actual != expected_vector)
        return false;

    for (auto value : actual)
    {
        if (std::popcount(static_cast<unsigned long long>(value)) != static_cast<int>(Popcount))
            return false;
    }

    return true;
}

template <typename T, T Popcount>
bool check_single_value(T expected)
{
    auto values = collect_first<T, Popcount>(2);
    return values.size() == 1 && values.front() == expected;
}

bool check_zero_popcount_cases()
{
    if (!check_single_value<std::uint8_t, std::uint8_t{0}>(std::uint8_t{0}))
        return false;
    if (!check_single_value<std::uint16_t, std::uint16_t{0}>(std::uint16_t{0}))
        return false;
    if (!check_single_value<std::uint32_t, std::uint32_t{0}>(std::uint32_t{0}))
        return false;
    if (!check_single_value<std::uint64_t, std::uint64_t{0}>(std::uint64_t{0}))
        return false;
    return true;
}

bool check_one_popcount_cases()
{
    if (!check_exact_prefix<std::uint8_t, std::uint8_t{1}>({1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u}))
        return false;
    if (!check_exact_prefix<std::uint16_t, std::uint16_t{1}>({1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u, 256u, 512u}))
        return false;
    if (!check_exact_prefix<std::uint32_t, std::uint32_t{1}>({1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u, 256u, 512u}))
        return false;
    if (!check_exact_prefix<std::uint64_t, std::uint64_t{1}>({1ull, 2ull, 4ull, 8ull, 16ull, 32ull, 64ull, 128ull, 256ull, 512ull}))
        return false;
    return true;
}

bool check_two_popcount_cases()
{
    if (!check_exact_prefix<std::uint8_t, std::uint8_t{2}>({3u, 5u, 6u, 9u, 10u, 12u, 17u, 18u, 20u, 24u}))
        return false;
    if (!check_exact_prefix<std::uint16_t, std::uint16_t{2}>({3u, 5u, 6u, 9u, 10u, 12u, 17u, 18u, 20u, 24u}))
        return false;
    if (!check_exact_prefix<std::uint32_t, std::uint32_t{2}>({3u, 5u, 6u, 9u, 10u, 12u, 17u, 18u, 20u, 24u}))
        return false;
    if (!check_exact_prefix<std::uint64_t, std::uint64_t{2}>({3ull, 5ull, 6ull, 9ull, 10ull, 12ull, 17ull, 18ull, 20ull, 24ull}))
        return false;
    return true;
}

bool check_uint8_edge_cases()
{
    if (!check_exact_prefix<std::uint8_t, std::uint8_t{6}>({63u, 95u, 111u, 119u, 123u, 125u, 126u}))
        return false;
    if (!check_exact_prefix<std::uint8_t, std::uint8_t{7}>({127u, 191u, 223u, 239u, 247u, 251u, 253u, 254u}))
        return false;
    if (!check_single_value<std::uint8_t, std::uint8_t{8}>(std::uint8_t{255}))
        return false;
    return true;
}

bool check_uint16_edge_cases()
{
    if (!check_exact_prefix<std::uint16_t, std::uint16_t{14}>({16383u, 24575u, 28671u, 30719u, 31743u, 32255u, 32511u, 32639u, 32703u, 32735u}))
        return false;
    if (!check_exact_prefix<std::uint16_t, std::uint16_t{15}>({32767u, 49151u, 57343u, 61439u, 63487u, 64511u, 65023u, 65279u, 65407u, 65471u}))
        return false;
    if (!check_single_value<std::uint16_t, std::uint16_t{16}>(std::uint16_t{65535}))
        return false;
    return true;
}

bool check_uint32_edge_cases()
{
    // if (!check_exact_prefix<std::uint32_t, std::uint32_t{30}>({0x3fffffffU, 0x5fffffffU, 0x6fffffffU, 0x9fffffffU, 0xafffffffU, 0xbfffffffU, 0xdfffffffU, 0xefffffffU}))
    //     return false;
    if (!check_exact_prefix<std::uint32_t, std::uint32_t{31}>({0x7fffffffU, 0xbfffffffU, 0xdfffffffU, 0xefffffffU, 0xf7ffffffU, 0xfbffffffU, 0xfdffffffU, 0xfeffffffU}))
        return false;
    if (!check_single_value<std::uint32_t, std::uint32_t{32}>(std::uint32_t{0xffffffffU}))
        return false;
    return true;
}

bool check_uint64_edge_cases()
{
    // if (!check_exact_prefix<std::uint64_t, std::uint64_t{62}>({0x3fffffffffffffffULL, 0x5fffffffffffffffULL, 0x6fffffffffffffffULL, 0x9fffffffffffffffULL, 0xafffffffffffffffULL, 0xbfffffffffffffffULL, 0xdfffffffffffffffULL, 0xefffffffffffffffULL}))
    //     return false;
    if (!check_exact_prefix<std::uint64_t, std::uint64_t{63}>({0x7fffffffffffffffULL, 0xbfffffffffffffffULL, 0xdfffffffffffffffULL, 0xefffffffffffffffULL, 0xf7ffffffffffffffULL, 0xfbffffffffffffffULL, 0xfdffffffffffffffULL, 0xfeffffffffffffffULL}))
        return false;
    if (!check_single_value<std::uint64_t, std::uint64_t{64}>(std::uint64_t{0xffffffffffffffffULL}))
        return false;
    return true;
}
}

int main()
{
    if (!check_zero_popcount_cases())
    {
        std::cerr << "zero-popcount cases failed\n";
        return 1;
    }

    if (!check_one_popcount_cases())
    {
        std::cerr << "one-popcount cases failed\n";
        return 2;
    }

    if (!check_two_popcount_cases())
    {
        std::cerr << "two-popcount cases failed\n";
        return 3;
    }

    if (!check_uint8_edge_cases())
    {
        std::cerr << "uint8_t edge cases failed\n";
        return 4;
    }

    if (!check_uint16_edge_cases())
    {
        std::cerr << "uint16_t edge cases failed\n";
        return 5;
    }

    if (!check_uint32_edge_cases())
    {
        std::cerr << "uint32_t edge cases failed\n";
        return 6;
    }

    if (!check_uint64_edge_cases())
    {
        std::cerr << "uint64_t edge cases failed\n";
        return 7;
    }

    std::cout << "expanded popcount_set tests passed\n";
    return 0;
}
