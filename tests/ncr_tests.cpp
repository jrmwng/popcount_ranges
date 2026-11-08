#include <popcount_ranges/ncr.hpp>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <tuple>
#include <type_traits>

namespace pc = jrmwng::popcount_ranges;

std::uintmax_t choose(std::size_t n, std::size_t r)
{
    if (r > n)
        return 0;
    std::uintmax_t result = 1;
    for (std::size_t i = 1; i <= r; ++i)
        result = result * (n - i + 1) / i;
    return result;
}

template <std::size_t R>
bool check_row(pc::nCr_table<16, 4> const& table)
{
    for (std::size_t n = R - 1; n <= 16; ++n)
    {
        if (table(n, std::integral_constant<std::size_t, R>{}) != choose(n, R))
            return false;
    }
    for (std::uintmax_t target = 0; target <= choose(16, R) + 1; ++target)
    {
        std::size_t expected = R - 1;
        while (expected < 16 && choose(expected + 1, R) <= target)
            ++expected;
        auto const [found, value] = table.search(R, 17, target,
                                                 std::integral_constant<std::size_t, R>{});
        if (found != expected || value != choose(expected, R))
            return false;
    }
    for (std::size_t first = R; first < 16; ++first)
    {
        std::size_t const last = first + 2;
        for (std::uintmax_t target : {std::uintmax_t{0}, choose(first, R), choose(16, R)})
        {
            std::size_t expected = first - 1;
            while (expected + 1 < last && choose(expected + 1, R) <= target)
                ++expected;
            auto const [found, value] = table.search(first, last, target,
                                                     std::integral_constant<std::size_t, R>{});
            if (found != expected || value != choose(expected, R))
                return false;
        }
    }
    return true;
}

int main()
{
    for (std::size_t n = 0; n <= 30; ++n)
        for (std::size_t r = 0; r <= n; ++r)
        {
            auto const [value, overflow] = pc::nCr(n, r);
            if (overflow || value != choose(n, r))
            {
                std::cerr << "nCr mismatch at " << n << ',' << r << '\n';
                return 1;
            }
        }

    auto const [largest, fits] = pc::nCr(67, 33);
    auto const [ignored, overflows] = pc::nCr(68, 34);
    static_cast<void>(ignored);
    if (fits || largest != UINT64_C(14226520737620288370) || !overflows ||
        pc::nCr_max_R(67) != 33)
    {
        std::cerr << "nCr overflow boundary failed\n";
        return 1;
    }

    pc::nCr_table<16, 4> const table;
    auto const [zero_below, zero_value] = table.search(1, 17, 0,
                                                       std::integral_constant<std::size_t, 0>{});
    auto const [zero_above, ignored_one] = table.search(1, 17, 1,
                                                        std::integral_constant<std::size_t, 0>{});
    auto const [one_middle, one_value] = table.search(3, 10, 5,
                                                     std::integral_constant<std::size_t, 1>{});
    if (!check_row<2>(table) || !check_row<3>(table) || !check_row<4>(table) ||
        zero_below != 0 || zero_value != 1 || zero_above != 16 || ignored_one != 1 ||
        one_middle != 5 || one_value != 5 ||
        pc::nCr_read<std::uint16_t>(16, std::integral_constant<std::size_t, 14>{}) != 120)
    {
        std::cerr << "nCr table or search failed\n";
        return 1;
    }
    std::cout << "nCr tests passed\n";
}
