#include <popcount_ranges/uint.hpp>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <type_traits>
#include <vector>

namespace
{
template <std::size_t N>
using wide_uint = jrmwng::popcount_ranges::std::uint<N>;

template <std::size_t N>
wide_uint<N> reference_shift(wide_uint<N> const& value, std::size_t shift, bool left)
{
    wide_uint<N> result{};
    if (shift >= N)
        return result;

    for (std::size_t bit = 0; bit < N; ++bit)
    {
        auto const source_bit = left ? bit : bit + shift;
        auto const destination_bit = left ? bit + shift : bit;
        if (source_bit < N && destination_bit < N &&
            ((value.data[source_bit / 64] >> (source_bit % 64)) & 1u))
            result.data[destination_bit / 64] |= std::uint64_t{1} << (destination_bit % 64);
    }
    return result;
}

template <std::size_t N, bool Subtract>
wide_uint<N> reference_arithmetic(wide_uint<N> const& lhs, wide_uint<N> const& rhs)
{
    wide_uint<N> result{};
    bool carry_or_borrow = false;
    for (std::size_t bit = 0; bit < N; ++bit)
    {
        bool const a = ((lhs.data[bit / 64] >> (bit % 64)) & 1u) != 0;
        bool const b = ((rhs.data[bit / 64] >> (bit % 64)) & 1u) != 0;
        bool const output = a ^ b ^ carry_or_borrow;
        if (output)
            result.data[bit / 64] |= std::uint64_t{1} << (bit % 64);

        if constexpr (Subtract)
            carry_or_borrow = (!a && (b || carry_or_borrow)) || (b && carry_or_borrow);
        else
            carry_or_borrow = (a && b) || (carry_or_borrow && (a || b));
    }
    return result;
}

template <std::size_t N>
bool check_uint_operations()
{
    using uint_type = wide_uint<N>;
    constexpr auto words = N / 64;

    uint_type zero{};
    uint_type one{1};
    if (zero != uint_type{} || one.data[0] != 1)
    {
        std::cerr << N << "-bit construction failed\n";
        return false;
    }
    for (std::size_t i = 1; i < words; ++i)
        if (one.data[i] != 0)
        {
            std::cerr << N << "-bit zero extension failed\n";
            return false;
        }

    auto const maximum = std::numeric_limits<uint_type>::max();
    if (maximum != ~zero || std::numeric_limits<uint_type>::digits != static_cast<int>(N))
    {
        std::cerr << N << "-bit numeric_limits failed\n";
        return false;
    }
    if (std::numeric_limits<uint_type>::min() != zero || std::numeric_limits<uint_type>::lowest() != zero)
    {
        std::cerr << N << "-bit minimum failed\n";
        return false;
    }
    if (jrmwng::popcount_ranges::std::popcount(zero) != 0 ||
        jrmwng::popcount_ranges::std::popcount(maximum) != static_cast<int>(N))
    {
        std::cerr << N << "-bit popcount failed\n";
        return false;
    }
    if (-one != maximum || -maximum != one || maximum + one != zero || zero - one != maximum)
    {
        std::cerr << N << "-bit boundary arithmetic failed\n";
        return false;
    }
    uint_type low_with_high_word{};
    uint_type high_with_high_word{};
    low_with_high_word.data[0] = std::numeric_limits<std::uint64_t>::max();
    high_with_high_word.data[0] = low_with_high_word.data[0];
    high_with_high_word.data[words - 1] = 1;
    if (!(zero < one) || !(low_with_high_word < high_with_high_word) ||
        high_with_high_word == low_with_high_word || !(low_with_high_word != high_with_high_word))
    {
        std::cerr << N << "-bit comparison failed\n";
        return false;
    }
    int int_conversion = uint_type{42};
    std::size_t size_conversion = uint_type{42};
    if (!(high_with_high_word > low_with_high_word) ||
        !(low_with_high_word <= high_with_high_word) ||
        !(high_with_high_word >= low_with_high_word) ||
        int_conversion != 42 || size_conversion != 42)
    {
        std::cerr << N << "-bit comparison or conversion failed\n";
        return false;
    }

    uint_type carry_lhs{};
    uint_type carry_rhs{};
    carry_lhs.data[0] = std::numeric_limits<std::uint64_t>::max() - 1;
    for (std::size_t i = 1; i < words; ++i)
        carry_lhs.data[i] = std::numeric_limits<std::uint64_t>::max();
    carry_rhs.data[0] = 2;
    if (carry_lhs + carry_rhs != zero)
    {
        std::cerr << N << "-bit carry propagation failed\n";
        return false;
    }

    uint_type borrow_lhs{};
    uint_type borrow_rhs{};
    for (std::size_t i = 0; i < words; ++i)
        borrow_rhs.data[i] = std::numeric_limits<std::uint64_t>::max();
    borrow_rhs.data[0] -= 1;
    borrow_lhs.data[0] = 1;
    if (borrow_lhs - borrow_rhs != uint_type{3})
    {
        std::cerr << N << "-bit borrow propagation failed\n";
        return false;
    }

    uint_type a{};
    uint_type b{};
    for (std::size_t i = 0; i < words; ++i)
    {
        a.data[i] = 0x0123456789abcdefULL ^ (std::uint64_t{i} * 0x9e3779b97f4a7c15ULL);
        b.data[i] = 0xfedcba9876543210ULL ^ (std::uint64_t{i} * 0xd1b54a32d192ed03ULL);
    }

    if (a + b != reference_arithmetic<N, false>(a, b) ||
        a - b != reference_arithmetic<N, true>(a, b) ||
        -a != reference_arithmetic<N, true>(zero, a))
    {
        std::cerr << N << "-bit patterned arithmetic failed\n";
        return false;
    }
    if (((a & b) | (a ^ b)) != (a | b))
    {
        std::cerr << N << "-bit bitwise identity failed\n";
        return false;
    }
    auto and_assign = a;
    and_assign &= b;
    if (and_assign != (a & b))
    {
        std::cerr << N << "-bit and assignment failed\n";
        return false;
    }
    if ((a & b) != (b & a) || (a | b) != (b | a) || (a ^ b) != (b ^ a))
    {
        std::cerr << N << "-bit bitwise commutativity failed\n";
        return false;
    }
    if ((~a) != (maximum ^ a))
    {
        std::cerr << N << "-bit complement failed\n";
        return false;
    }

    for (std::size_t shift : {std::size_t{0}, std::size_t{1}, std::size_t{2}, std::size_t{31},
                              std::size_t{63}, std::size_t{64}, std::size_t{65}, std::size_t{127},
                              N - 1, N, N + 1})
    {
        if ((a << shift) != reference_shift(a, shift, true) ||
            (a >> shift) != reference_shift(a, shift, false))
        {
            std::cerr << N << "-bit boundary shift failed at " << shift << '\n';
            return false;
        }
    }

    std::mt19937_64 generator(0x5eed + N);
    for (std::size_t sample = 0; sample < 32; ++sample)
    {
        uint_type lhs{};
        uint_type rhs{};
        for (std::size_t i = 0; i < words; ++i)
        {
            lhs.data[i] = generator();
            rhs.data[i] = generator();
        }
        if (lhs + rhs != reference_arithmetic<N, false>(lhs, rhs) ||
            lhs - rhs != reference_arithmetic<N, true>(lhs, rhs))
        {
            std::cerr << N << "-bit randomized arithmetic failed at " << sample << '\n';
            return false;
        }
        for (std::size_t shift : {std::size_t{1}, std::size_t{63}, std::size_t{64},
                                  std::size_t{65}, N / 2, N - 1})
        {
            if ((lhs << shift) != reference_shift(lhs, shift, true) ||
                (lhs >> shift) != reference_shift(lhs, shift, false))
            {
                std::cerr << N << "-bit randomized shift failed at " << sample << ", shift " << shift << '\n';
                return false;
            }
        }
    }
    return true;
}

template <typename T>
bool check_native_set_bit_positions()
{
    T const value = static_cast<T>((T{1} << 0) | (T{1} << 3) |
                                   (T{1} << (std::numeric_limits<T>::digits - 1)));
    std::vector<std::size_t> positions;
    for (std::size_t position : jrmwng::popcount_ranges::std::set_bit_positions(value))
        positions.push_back(position);

    std::vector<std::size_t> const expected{0, 3, std::numeric_limits<T>::digits - 1};
    if (positions != expected)
    {
        std::cerr << "native set-bit position iteration failed\n";
        return false;
    }
    return true;
}

template <std::size_t N>
bool check_wide_set_bit_positions()
{
    using uint_type = wide_uint<N>;
    uint_type value{};
    std::vector<std::size_t> const expected{0, 1, 63, 64, N - 2, N - 1};
    for (std::size_t position : expected)
        value.data[position / 64] |= std::uint64_t{1} << (position % 64);

    std::vector<std::size_t> positions;
    for (std::size_t position : jrmwng::popcount_ranges::std::set_bit_positions(value))
        positions.push_back(position);

    if (positions != expected)
    {
        std::cerr << N << "-bit set-bit position iteration failed\n";
        return false;
    }

    std::vector<std::size_t> temporary_positions;
    for (std::size_t position : jrmwng::popcount_ranges::std::set_bit_positions(
             uint_type{1} | (uint_type{1} << (N - 1))))
        temporary_positions.push_back(position);
    if (temporary_positions != std::vector<std::size_t>{0, N - 1})
    {
        std::cerr << N << "-bit temporary set-bit position iteration failed\n";
        return false;
    }

    uint_type const zero{};
    auto const empty_positions = jrmwng::popcount_ranges::std::set_bit_positions(zero);
    if (empty_positions.begin() != empty_positions.end())
    {
        std::cerr << N << "-bit empty set-bit position iteration failed\n";
        return false;
    }
    return true;
}

}

int main()
{
    static_assert(std::is_trivially_copyable_v<wide_uint<128>>);
    static_assert(192 % 64 == 0 && !std::has_single_bit(std::size_t{192}));
    static_assert(320 % 64 == 0 && !std::has_single_bit(std::size_t{320}));
    static_assert(jrmwng::popcount_ranges::std::is_unsigned_v<wide_uint<128>>);
    static_assert(jrmwng::popcount_ranges::std::is_unsigned_v<wide_uint<128> const>);
    static_assert(jrmwng::popcount_ranges::std::is_unsigned_v<wide_uint<256>>);
    static_assert(jrmwng::popcount_ranges::std::is_unsigned_v<std::uint64_t>);
    static_assert(!jrmwng::popcount_ranges::std::is_unsigned_v<int>);
    static_assert(jrmwng::popcount_ranges::std::numeric_limits<wide_uint<128>>::digits == 128);
    static_assert(jrmwng::popcount_ranges::std::numeric_limits<wide_uint<192>>::digits == 192);
    static_assert(jrmwng::popcount_ranges::std::numeric_limits<wide_uint<256>>::digits == 256);
    static_assert(jrmwng::popcount_ranges::std::numeric_limits<wide_uint<320>>::digits == 320);
    static_assert(jrmwng::popcount_ranges::std::popcount(wide_uint<128>{3}) == 2);
    static_assert(jrmwng::popcount_ranges::std::countr_zero(wide_uint<128>{0}) == 128);
    static_assert(jrmwng::popcount_ranges::std::countr_zero(wide_uint<256>{1} << 128) == 128);
    static_assert(jrmwng::popcount_ranges::std::countr_zero(std::uint64_t{8}) == 3);
    static_assert(jrmwng::popcount_ranges::std::countr_one(~wide_uint<128>{}));
    static_assert(jrmwng::popcount_ranges::std::countl_zero(wide_uint<128>{1}) == 127);
    static_assert(jrmwng::popcount_ranges::std::countl_zero(wide_uint<192>{1}) == 191);
    static_assert(jrmwng::popcount_ranges::std::countl_one(~wide_uint<256>{}));
    static_assert(jrmwng::popcount_ranges::std::has_single_bit(wide_uint<256>{1} << 255));
    static_assert(jrmwng::popcount_ranges::std::has_single_bit(wide_uint<320>{1} << 319));
    static_assert(jrmwng::popcount_ranges::std::bit_width(wide_uint<128>{1} << 127) == 128);
    static_assert(jrmwng::popcount_ranges::std::bit_floor((wide_uint<256>{1} << 127) + wide_uint<256>{13}) ==
        (wide_uint<256>{1} << 127));
    static_assert((wide_uint<128>{1} << 64).data[1] == 1);

    if (!check_uint_operations<128>() || !check_uint_operations<192>() ||
        !check_uint_operations<256>() || !check_uint_operations<320>())
    {
        std::cerr << "uint operation tests failed\n";
        return 1;
    }
    if (!check_native_set_bit_positions<std::uint8_t>() ||
        !check_native_set_bit_positions<std::uint16_t>() ||
        !check_native_set_bit_positions<std::uint32_t>() ||
        !check_native_set_bit_positions<std::uint64_t>() ||
        !check_wide_set_bit_positions<128>() ||
        !check_wide_set_bit_positions<192>() ||
        !check_wide_set_bit_positions<256>() ||
        !check_wide_set_bit_positions<320>())
    {
        std::cerr << "set-bit position iterator tests failed\n";
        return 1;
    }

    std::cout << "uint tests passed\n";
    return 0;
}
