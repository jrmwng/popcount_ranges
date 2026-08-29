#include <popcount_ranges/set.hpp>
#include <popcount_ranges/ncr.hpp>

#include <bit>
#include <cstdint>
#include <iostream>
#include <iterator>

namespace
{
template <typename T, T Popcount>
bool check_exhaustive_set(std::size_t expected_size)
{
    using Set = jrmwng::popcount_ranges::set<Popcount>;
    Set set;
    std::size_t size = 0;
    T previous = 0;

    if (std::popcount(static_cast<T>(*set.end())) == static_cast<int>(Popcount))
        return false;

    auto const expected_dist = static_cast<std::ptrdiff_t>(expected_size);

    // Check full set distance for iterator and reverse_iterator
    if (std::distance(set.begin(), set.end()) != expected_dist)
    {
        std::cerr << "Failed: std::distance(begin, end) != expected_dist for Popcount=" << (int)Popcount << "\n";
        return false;
    }
    if ((set.end() - set.begin()) != expected_dist)
    {
        std::cerr << "Failed: (end - begin) != expected_dist for Popcount=" << (int)Popcount << "\n";
        return false;
    }
    if ((set.begin() - set.end()) != -expected_dist)
    {
        std::cerr << "Failed: (begin - end) != -expected_dist for Popcount=" << (int)Popcount << "\n";
        return false;
    }

    if (std::distance(set.rbegin(), set.rend()) != expected_dist)
    {
        std::cerr << "Failed: std::distance(rbegin, rend) != expected_dist for Popcount=" << (int)Popcount << "\n";
        return false;
    }
    if ((set.rend() - set.rbegin()) != expected_dist)
    {
        std::cerr << "Failed: (rend - rbegin) != expected_dist for Popcount=" << (int)Popcount << "\n";
        return false;
    }
    if ((set.rbegin() - set.rend()) != -expected_dist)
    {
        std::cerr << "Failed: (rbegin - rend) != -expected_dist for Popcount=" << (int)Popcount << "\n";
        return false;
    }

    // Check self-distance
    if (std::distance(set.begin(), set.begin()) != 0 || (set.begin() - set.begin()) != 0)
        return false;
    if (std::distance(set.end(), set.end()) != 0 || (set.end() - set.end()) != 0)
        return false;
    if (std::distance(set.rbegin(), set.rbegin()) != 0 || (set.rbegin() - set.rbegin()) != 0)
        return false;
    if (std::distance(set.rend(), set.rend()) != 0 || (set.rend() - set.rend()) != 0)
        return false;

    // Check distances during forward iteration
    {
        std::size_t idx = 0;
        for (auto it = set.begin(); it != set.end(); ++it, ++idx)
        {
            auto const cur_idx = static_cast<std::ptrdiff_t>(idx);
            auto const rem_idx = static_cast<std::ptrdiff_t>(expected_size - idx);

            if (std::distance(set.begin(), it) != cur_idx)
                return false;
            if ((it - set.begin()) != cur_idx)
                return false;
            if ((set.begin() - it) != -cur_idx)
                return false;

            if (std::distance(it, set.end()) != rem_idx)
                return false;
            if ((set.end() - it) != rem_idx)
                return false;
            if ((it - set.end()) != -rem_idx)
                return false;
        }
    }

    // Check distances during reverse iteration
    {
        std::size_t idx = 0;
        for (auto rit = set.rbegin(); rit != set.rend(); ++rit, ++idx)
        {
            auto const cur_idx = static_cast<std::ptrdiff_t>(idx);
            auto const rem_idx = static_cast<std::ptrdiff_t>(expected_size - idx);

            if (std::distance(set.rbegin(), rit) != cur_idx)
                return false;
            if ((rit - set.rbegin()) != cur_idx)
                return false;
            if ((set.rbegin() - rit) != -cur_idx)
                return false;

            if (std::distance(rit, set.rend()) != rem_idx)
                return false;
            if ((set.rend() - rit) != rem_idx)
                return false;
            if ((rit - set.rend()) != -rem_idx)
                return false;
        }
    }

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

    std::cout << "expanded set tests passed\n";
    return 0;
}
