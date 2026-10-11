#pragma once

#include <cstddef>
#include <cstdint>
namespace jrmwng::popcount_ranges::std
{
    using uintmax_t = ::std::uintmax_t;
    using size_t = ::std::size_t;
    using ptrdiff_t = ::std::ptrdiff_t;
}

namespace jrmwng::popcount_ranges::std
{
    template <size_t N>
    struct uint
    {
        static_assert(N % 64u == 0, "N must be a multiple of 64");
        static_assert(N > 64u, "N must be greater than 64");

        using bits_type = ::std::uint64_t;
        inline constexpr static size_t B = sizeof(bits_type) * 8u;
        inline constexpr static size_t M = N / B;

        bits_type data[M]{};

        constexpr uint() noexcept = default;

        constexpr uint(uint const &) noexcept = default;
        constexpr uint & operator=(uint const &) noexcept = default;

        constexpr uint(uint &&) noexcept = default;
        constexpr uint & operator=(uint &&) noexcept = default;

        constexpr uint(bits_type value) noexcept
        {
            data[0] = value;
            for (size_t i = 1; i < M; ++i)
            {
                data[i] = 0;
            }
        }

        constexpr bool operator==(uint const & other) const noexcept
        {
            for (size_t i = 0; i < M; ++i)
            {
                if (data[i] != other.data[i]) [[unlikely]] return false;
            }
            return true;
        }
        constexpr bool operator!=(uint const & other) const noexcept
        {
            return !(*this == other);
        }
        constexpr bool operator<(uint const & other) const noexcept
        {
            for (size_t i = M; i-- > 0;) [[likely]]
            {
                if (data[i] < other.data[i]) [[unlikely]] return true;
                if (data[i] > other.data[i]) [[unlikely]] return false;
            }
            return false;
        }
        constexpr bool operator>(uint const & other) const noexcept
        {
            return other < *this;
        }
        constexpr bool operator<=(uint const & other) const noexcept
        {
            return !(other < *this);
        }
        constexpr bool operator>=(uint const & other) const noexcept
        {
            return !(*this < other);
        }

        constexpr operator int() const noexcept
        {
            return static_cast<int>(data[0]);
        }
        constexpr operator size_t() const noexcept
        {
            return static_cast<size_t>(data[0]);
        }

        constexpr uint& operator+=(uint const & other) noexcept = delete;
        constexpr uint operator+ (uint const & other) const noexcept
        {
            uint result;
            bits_type carry = 0;
            for (size_t i = 0; i < M; ++i)
            {
                result.data[i] = this->data[i] + other.data[i] + carry;
                carry = (result.data[i] < this->data[i] || (carry && result.data[i] == this->data[i])) ? 1 : 0;
            }
            return result;
        }
        constexpr uint& operator-=(uint const & other) noexcept = delete;
        constexpr uint operator- (uint const & other) const noexcept
        {
            uint result;
            bits_type borrow = 0;
            for (size_t i = 0; i < M; ++i)
            {
                result.data[i] = this->data[i] - other.data[i] - borrow;
                borrow = (result.data[i] > this->data[i] || (borrow && result.data[i] == this->data[i])) ? 1 : 0;
            }
            return result;
        }
        constexpr uint operator-() const noexcept
        {
            return (~*this) + uint{1};
        }
        constexpr uint operator~() const noexcept
        {
            uint result;
            for (size_t i = 0; i < M; ++i)
            {
                result.data[i] = ~this->data[i];
            }
            return result;
        }
        constexpr uint & operator&=(uint const & other) noexcept
        {
            for (size_t i = 0; i < M; ++i)
                data[i] &= other.data[i];
            return *this;
        }
        constexpr uint operator&(uint const & other) const noexcept
        {
            uint result;
            for (size_t i = 0; i < M; ++i)
                result.data[i] = this->data[i] & other.data[i];
            return result;
        }

        constexpr uint& operator|=(uint const & other) noexcept = delete;
        constexpr uint operator|(uint const & other) const noexcept
        {
            uint result;
            for (size_t i = 0; i < M; ++i)
                result.data[i] = this->data[i] | other.data[i];
            return result;
        }

        constexpr uint& operator^=(uint const & other) noexcept = delete;
        constexpr uint operator^(uint const & other) const noexcept
        {
            uint result;
            for (size_t i = 0; i < M; ++i)
                result.data[i] = this->data[i] ^ other.data[i];
            return result;
        }

        constexpr uint& operator<<=(size_t shift) noexcept = delete;
        constexpr uint operator<<(size_t shift) const noexcept
        {
            uint result;
            if (shift >= N) [[unlikely]]
            {
                for (size_t i = 0; i < M; ++i)
                {
                    result.data[i] = 0;
                }
                return result;
            }
            size_t whole_words = shift / B;
            size_t bit_shift = shift % B;
            for (size_t i = M; i-- > 0;)
            {
                if (i >= whole_words)
                {
                    result.data[i] = this->data[i - whole_words] << bit_shift;
                    if (bit_shift && i - whole_words > 0)
                    {
                        result.data[i] |= this->data[i - whole_words - 1] >> (B - bit_shift);
                    }
                }
                else
                {
                    result.data[i] = 0;
                }
            }
            return result;
        }
        template <typename T>
            requires ::std::is_integral_v<T>
        constexpr uint operator<<(T shift) const noexcept
        {
            return *this << static_cast<size_t>(shift);
        }

        constexpr uint& operator>>=(size_t shift) noexcept = delete;
        constexpr uint operator>>(size_t shift) const noexcept
        {
            uint result;
            if (shift >= N) [[unlikely]]
            {
                for (size_t i = 0; i < M; ++i)
                {
                    result.data[i] = 0;
                }
                return result;
            }
            size_t whole_words = shift / B;
            size_t bit_shift = shift % B;
            for (size_t i = 0; i < M; ++i)
            {
                if (i + whole_words < M)
                {
                    result.data[i] = this->data[i + whole_words] >> bit_shift;
                    if (bit_shift && i + whole_words + 1 < M)
                    {
                        result.data[i] |= this->data[i + whole_words + 1] << (B - bit_shift);
                    }
                }
                else
                {
                    result.data[i] = 0;
                }
            }
            return result;
        }
        template <typename T>
            requires ::std::is_integral_v<T>
        constexpr uint operator>>(T shift) const noexcept
        {
            return *this >> static_cast<size_t>(shift);
        }
    };
}

#include <type_traits>
namespace jrmwng::popcount_ranges::std
{
    template <typename T>
    struct is_uint : ::std::false_type
    {};

    template <size_t N>
    struct is_uint<uint<N>> : ::std::true_type
    {};

    template <size_t N>
    struct is_uint<uint<N> const> : is_uint<uint<N>>
    {};

    template <size_t N>
    struct is_uint<uint<N> volatile> : is_uint<uint<N>>
    {};

    template <size_t N>
    struct is_uint<uint<N> const volatile> : is_uint<uint<N>>
    {};

    template <typename T>
    struct is_unsigned : ::std::is_unsigned<T>
    {};

    template <size_t N>
    struct is_unsigned<uint<N>> : ::std::true_type
    {};

    template <size_t N>
    struct is_unsigned<uint<N> const> : is_unsigned<uint<N>>
    {};

    template <size_t N>
    struct is_unsigned<uint<N> volatile> : is_unsigned<uint<N>>
    {};

    template <size_t N>
    struct is_unsigned<uint<N> const volatile> : is_unsigned<uint<N>>
    {};

    template <typename T>
    inline constexpr bool is_unsigned_v = is_unsigned<T>::value;
}

#include <bit>
namespace jrmwng::popcount_ranges::std
{
    template <size_t N>
    constexpr int popcount(uint<N> value) noexcept
    {
        int count = 0;
        for (size_t i = 0; i < uint<N>::M; ++i)
            count += ::std::popcount(value.data[i]);
        return count;
    }

    template <typename T>
        requires std::is_unsigned_v<T>
    constexpr int popcount(T value) noexcept
    {
        return ::std::popcount(value);
    }

    template <size_t N>
    constexpr int countr_zero(uint<N> value) noexcept
    {
        for (size_t i = 0; i < uint<N>::M; ++i) [[likely]]
            if (value.data[i] != 0) [[unlikely]]
                return static_cast<int>(i * uint<N>::B + ::std::countr_zero(value.data[i]));
        return static_cast<int>(N);
    }

    template <typename T>
        requires std::is_unsigned_v<T>
    constexpr int countr_zero(T value) noexcept
    {
        return ::std::countr_zero(value);
    }

    template <size_t N>
    constexpr int countr_one(uint<N> value) noexcept
    {
        return countr_zero(~value);
    }

    template <size_t N>
    constexpr int countl_zero(uint<N> value) noexcept
    {
        for (size_t i = uint<N>::M; i-- > 0;) [[likely]]
            if (value.data[i] != 0) [[unlikely]]
                return static_cast<int>((uint<N>::M - i - 1) * uint<N>::B + ::std::countl_zero(value.data[i]));
        return static_cast<int>(N);
    }

    template <size_t N>
    constexpr int countl_one(uint<N> value) noexcept
    {
        return countl_zero(~value);
    }

    template <size_t N>
    constexpr bool has_single_bit(uint<N> value) noexcept
    {
        return popcount(value) == 1;
    }

    template <size_t N>
    constexpr int bit_width(uint<N> value) noexcept
    {
        return static_cast<int>(N) - countl_zero(value);
    }

    template <size_t N>
    constexpr uint<N> bit_floor(uint<N> value) noexcept
    {
        auto const width = bit_width(value);
        return width == 0 ? uint<N>{} : uint<N>{1} << (width - 1);
    }
}

#include <iterator>
namespace jrmwng::popcount_ranges::std
{
    using random_access_iterator_tag = ::std::random_access_iterator_tag;
    using forward_iterator_tag = ::std::forward_iterator_tag;
}

#include <utility>
namespace jrmwng::popcount_ranges::std
{
    template <size_t I, typename Ttuple>
    constexpr decltype(auto) get(Ttuple && tuple) noexcept
    {
        return ::std::get<I>(::std::forward<Ttuple>(tuple));
    }
    template <typename T, typename Ttuple>
    constexpr decltype(auto) get(Ttuple && tuple) noexcept
    {
        return ::std::get<T>(::std::forward<Ttuple>(tuple));
    }

    template <typename T, T VALUE>
    using integral_constant = ::std::integral_constant<T, VALUE>;
    template <size_t N>
    using make_index_sequence = ::std::make_index_sequence<N>;
    template <size_t... Is>
    using index_sequence = ::std::index_sequence<Is...>;
}

#include <limits>
namespace jrmwng::popcount_ranges::std
{
    template <typename T>
    using numeric_limits = ::std::numeric_limits<T>;
}
namespace std
{
    template <size_t N>
    struct numeric_limits<jrmwng::popcount_ranges::std::uint<N>>
    {
        static constexpr bool is_specialized = true;
        static constexpr bool is_signed = false;
        static constexpr bool is_integer = true;
        static constexpr bool is_exact = true;
        static constexpr bool has_infinity = false;
        static constexpr bool has_quiet_NaN = false;
        static constexpr bool has_signaling_NaN = false;
        // static constexpr float_denorm_style has_denorm = std::denorm_absent;
        static constexpr bool is_bounded = true;
        static constexpr bool is_modulo = true;
        static constexpr int digits = N;
        static constexpr int digits10 = N * 301 / 1000; // approximate log10(2)
        static constexpr int max_digits10 = 0;
        static constexpr int radix = 2;
        static constexpr jrmwng::popcount_ranges::std::uint<N> min() noexcept { return jrmwng::popcount_ranges::std::uint<N>{0}; }
        static constexpr jrmwng::popcount_ranges::std::uint<N> lowest() noexcept { return jrmwng::popcount_ranges::std::uint<N>{0}; }
        static constexpr jrmwng::popcount_ranges::std::uint<N> max() noexcept { return ~jrmwng::popcount_ranges::std::uint<N>{0}; }
    };
}