#include <popcount_set.hpp>

#include <bit>
#include <cstdint>
#include <iostream>

namespace
{
template <typename T, T Popcount>
bool check_exhaustive_set(std::size_t expected_size)
{
    using Set = jrmwng::popcount_ranges::popcount_set<Popcount>;
    Set set;
    std::size_t size = 0;
    T previous = 0;

    if (std::popcount(static_cast<T>(*set.end())) == static_cast<int>(Popcount))
        return false;

    for (auto value : set)
    {
        T current = static_cast<T>(value);
        if (size != 0 && current <= previous)
            return false;
        if (std::popcount(current) != static_cast<int>(Popcount))
            return false;
        previous = current;
        ++size;
    }

    return size == expected_size;
}

bool check_uint8_exhaustive_sets()
{
    if (!check_exhaustive_set<std::uint8_t, std::uint8_t{0}>(1))
        return false;
    if (!check_exhaustive_set<std::uint8_t, std::uint8_t{1}>(8))
        return false;
    if (!check_exhaustive_set<std::uint8_t, std::uint8_t{2}>(28))
        return false;
    if (!check_exhaustive_set<std::uint8_t, std::uint8_t{3}>(56))
        return false;
    if (!check_exhaustive_set<std::uint8_t, std::uint8_t{4}>(70))
        return false;
    if (!check_exhaustive_set<std::uint8_t, std::uint8_t{5}>(56))
        return false;
    if (!check_exhaustive_set<std::uint8_t, std::uint8_t{6}>(28))
        return false;
    if (!check_exhaustive_set<std::uint8_t, std::uint8_t{7}>(8))
        return false;
    if (!check_exhaustive_set<std::uint8_t, std::uint8_t{8}>(1))
        return false;
    return true;
}

bool check_uint16_exhaustive_sets()
{
    if (!check_exhaustive_set<std::uint16_t, std::uint16_t{0}>(1))
        return false;
    if (!check_exhaustive_set<std::uint16_t, std::uint16_t{1}>(16))
        return false;
    if (!check_exhaustive_set<std::uint16_t, std::uint16_t{2}>(120))
        return false;
    if (!check_exhaustive_set<std::uint16_t, std::uint16_t{3}>(560))
        return false;
    if (!check_exhaustive_set<std::uint16_t, std::uint16_t{4}>(1820))
        return false;
    if (!check_exhaustive_set<std::uint16_t, std::uint16_t{5}>(4368))
        return false;
    if (!check_exhaustive_set<std::uint16_t, std::uint16_t{6}>(8008))
        return false;
    if (!check_exhaustive_set<std::uint16_t, std::uint16_t{7}>(11440))
        return false;
    if (!check_exhaustive_set<std::uint16_t, std::uint16_t{8}>(12870))
        return false;
    if (!check_exhaustive_set<std::uint16_t, std::uint16_t{9}>(11440))
        return false;
    if (!check_exhaustive_set<std::uint16_t, std::uint16_t{10}>(8008))
        return false;
    if (!check_exhaustive_set<std::uint16_t, std::uint16_t{11}>(4368))
        return false;
    if (!check_exhaustive_set<std::uint16_t, std::uint16_t{12}>(1820))
        return false;
    if (!check_exhaustive_set<std::uint16_t, std::uint16_t{13}>(560))
        return false;
    if (!check_exhaustive_set<std::uint16_t, std::uint16_t{14}>(120))
        return false;
    if (!check_exhaustive_set<std::uint16_t, std::uint16_t{15}>(16))
        return false;
    if (!check_exhaustive_set<std::uint16_t, std::uint16_t{16}>(1))
        return false;
    return true;
}

bool check_uint32_exhaustive_sets()
{
    if (!check_exhaustive_set<std::uint32_t, std::uint32_t{0}>(1))
        return false;
    if (!check_exhaustive_set<std::uint32_t, std::uint32_t{1}>(32))
        return false;
    if (!check_exhaustive_set<std::uint32_t, std::uint32_t{2}>(496))
        return false;
    if (!check_exhaustive_set<std::uint32_t, std::uint32_t{30}>(496))
        return false;
    if (!check_exhaustive_set<std::uint32_t, std::uint32_t{31}>(32))
        return false;
    if (!check_exhaustive_set<std::uint32_t, std::uint32_t{32}>(1))
        return false;
    return true;
}

bool check_uint64_exhaustive_sets()
{
    if (!check_exhaustive_set<std::uint64_t, std::uint64_t{0}>(1))
        return false;
    if (!check_exhaustive_set<std::uint64_t, std::uint64_t{1}>(64))
        return false;
    if (!check_exhaustive_set<std::uint64_t, std::uint64_t{2}>(2016))
        return false;
    if (!check_exhaustive_set<std::uint64_t, std::uint64_t{62}>(2016))
        return false;
    if (!check_exhaustive_set<std::uint64_t, std::uint64_t{63}>(64))
        return false;
    if (!check_exhaustive_set<std::uint64_t, std::uint64_t{64}>(1))
        return false;
    return true;
}
}

int main()
{
    if (!check_uint8_exhaustive_sets())
    {
        std::cerr << "uint8_t exhaustive cases failed\n";
        return 1;
    }

    if (!check_uint16_exhaustive_sets())
    {
        std::cerr << "uint16_t exhaustive cases failed\n";
        return 2;
    }

    if (!check_uint32_exhaustive_sets())
    {
        std::cerr << "uint32_t exhaustive cases failed\n";
        return 3;
    }

    if (!check_uint64_exhaustive_sets())
    {
        std::cerr << "uint64_t exhaustive cases failed\n";
        return 4;
    }

    std::cout << "expanded popcount_set tests passed\n";
    return 0;
}
