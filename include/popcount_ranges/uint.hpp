#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace jrmwng::popcount_ranges::stdx
{
    template <size_t N, size_t R>
    struct uint_indices
    {
        static_assert(N > 0, "N must be greater than zero");
        static_assert(R <= N, "R must be less than or equal to N");

        using index_type = std::conditional_t<
            N - 1u <= std::numeric_limits<std::uint8_t>::max(), std::uint8_t,
            std::conditional_t<
                N - 1u <= std::numeric_limits<std::uint16_t>::max(), std::uint16_t,
                std::conditional_t<
                    N - 1u <= std::numeric_limits<std::uint32_t>::max(), std::uint32_t,
                    size_t>>>;

        std::array<index_type, R> indices{};
        bool is_sentinel{};

        constexpr uint_indices() noexcept
        {
            for (size_t i = 0; i < R; ++i)
                indices[i] = static_cast<index_type>(i);
        }

        constexpr explicit uint_indices(std::array<index_type, R> positions) noexcept
            : indices(positions)
        {}

        constexpr operator size_t() const noexcept
        {
            return R;
        }

        constexpr bool operator==(uint_indices const& other) const noexcept
        {
            if (is_sentinel || other.is_sentinel)
                return is_sentinel == other.is_sentinel;
            return indices == other.indices;
        }

        constexpr bool operator<(uint_indices const& other) const noexcept
        {
            if (is_sentinel != other.is_sentinel)
                return !is_sentinel;
            if (is_sentinel)
                return false;
            for (size_t i = R; i-- > 0;)
            {
                if (indices[i] != other.indices[i])
                    return indices[i] < other.indices[i];
            }
            return false;
        }

        constexpr bool operator!=(uint_indices const& other) const noexcept { return !(*this == other); }
        constexpr bool operator>(uint_indices const& other) const noexcept { return other < *this; }
        constexpr bool operator<=(uint_indices const& other) const noexcept { return !(other < *this); }
        constexpr bool operator>=(uint_indices const& other) const noexcept { return !(*this < other); }
    };

    template <size_t N, size_t R>
    struct uint_complement_indices
    {
        static_assert(N > 0, "N must be greater than zero");
        static_assert(R <= N, "R must be less than or equal to N");

        inline constexpr static bool is_complement_index = true;
        using index_type = typename uint_indices<N, R>::index_type;

        std::array<index_type, R> indices{};
        bool is_sentinel{};

        constexpr uint_complement_indices() noexcept
        {
            for (size_t i = 0; i < R; ++i)
                indices[i] = static_cast<index_type>(N - R + i);
        }

        constexpr explicit uint_complement_indices(std::array<index_type, R> positions) noexcept
            : indices(positions)
        {}

        constexpr operator size_t() const noexcept
        {
            return N - R;
        }

        constexpr bool operator==(uint_complement_indices const& other) const noexcept
        {
            if (is_sentinel || other.is_sentinel)
                return is_sentinel == other.is_sentinel;
            return indices == other.indices;
        }

        constexpr bool operator<(uint_complement_indices const& other) const noexcept
        {
            if (is_sentinel != other.is_sentinel)
                return !is_sentinel;
            if (is_sentinel)
                return false;
            for (size_t i = R; i-- > 0;)
            {
                if (indices[i] != other.indices[i])
                    return indices[i] > other.indices[i];
            }
            return false;
        }

        constexpr bool operator!=(uint_complement_indices const& other) const noexcept { return !(*this == other); }
        constexpr bool operator>(uint_complement_indices const& other) const noexcept { return other < *this; }
        constexpr bool operator<=(uint_complement_indices const& other) const noexcept { return !(other < *this); }
        constexpr bool operator>=(uint_complement_indices const& other) const noexcept { return !(*this < other); }
    };

    template <size_t N>
    struct uint
    {
        static_assert(N % 64u == 0, "N must be a multiple of 64");
        static_assert(N > 64u, "N must be greater than 64");

        using bits_type = std::uint64_t;
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
            requires std::is_integral_v<T>
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
            requires std::is_integral_v<T>
        constexpr uint operator>>(T shift) const noexcept
        {
            return *this >> static_cast<size_t>(shift);
        }
    };
}

#include <type_traits>
namespace jrmwng::popcount_ranges::stdx
{
    template <typename T>
    struct is_uint : std::false_type
    {};

    template <size_t N>
    struct is_uint<uint<N>> : std::true_type
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
    struct is_unsigned : std::is_unsigned<T>
    {};

    template <size_t N>
    struct is_unsigned<uint<N>> : std::true_type
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

    template <size_t N, size_t R>
    struct is_unsigned<uint_indices<N, R>> : std::true_type
    {};

    template <size_t N, size_t R>
    struct is_unsigned<uint_complement_indices<N, R>> : std::true_type
    {};

    template <size_t N, size_t R>
    struct is_unsigned<uint_indices<N, R> const> : is_unsigned<uint_indices<N, R>>
    {};

    template <size_t N, size_t R>
    struct is_unsigned<uint_indices<N, R> volatile> : is_unsigned<uint_indices<N, R>>
    {};

    template <size_t N, size_t R>
    struct is_unsigned<uint_indices<N, R> const volatile> : is_unsigned<uint_indices<N, R>>
    {};

    template <size_t N, size_t R>
    struct is_unsigned<uint_complement_indices<N, R> const> : is_unsigned<uint_complement_indices<N, R>>
    {};

    template <size_t N, size_t R>
    struct is_unsigned<uint_complement_indices<N, R> volatile> : is_unsigned<uint_complement_indices<N, R>>
    {};

    template <size_t N, size_t R>
    struct is_unsigned<uint_complement_indices<N, R> const volatile> : is_unsigned<uint_complement_indices<N, R>>
    {};

    template <typename T>
    inline constexpr bool is_unsigned_v = is_unsigned<T>::value;
}

#include <bit>
namespace jrmwng::popcount_ranges::stdx
{
    template <size_t N>
    constexpr int popcount(uint<N> value) noexcept
    {
        int count = 0;
        for (size_t i = 0; i < uint<N>::M; ++i)
            count += std::popcount(value.data[i]);
        return count;
    }

    template <size_t N, size_t R>
    constexpr int popcount(uint_indices<N, R>) noexcept
    {
        return static_cast<int>(R);
    }

    template <size_t N, size_t R>
    constexpr int popcount(uint_complement_indices<N, R>) noexcept
    {
        return static_cast<int>(N - R);
    }

    template <typename T>
        requires std::is_unsigned_v<T>
    constexpr int popcount(T value) noexcept
    {
        return std::popcount(value);
    }

    template <size_t N>
    constexpr int countr_zero(uint<N> value) noexcept
    {
        for (size_t i = 0; i < uint<N>::M; ++i) [[likely]]
            if (value.data[i] != 0) [[unlikely]]
                return static_cast<int>(i * uint<N>::B + std::countr_zero(value.data[i]));
        return static_cast<int>(N);
    }

    template <size_t N, size_t R>
    constexpr int countr_zero(uint_indices<N, R> value) noexcept
    {
        return value.is_sentinel || R == 0 ? static_cast<int>(N) : static_cast<int>(value.indices[0]);
    }

    template <size_t N, size_t R>
    constexpr int countr_zero(uint_complement_indices<N, R> value) noexcept
    {
        if (value.is_sentinel)
            return static_cast<int>(N);

        size_t position = 0;
        for (size_t i = 0; i < R && value.indices[i] == position; ++i)
            ++position;
        return static_cast<int>(position);
    }

    template <typename T>
        requires std::is_unsigned_v<T>
    constexpr int countr_zero(T value) noexcept
    {
        return std::countr_zero(value);
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
                return static_cast<int>((uint<N>::M - i - 1) * uint<N>::B + std::countl_zero(value.data[i]));
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

    template <typename T>
    class set_bit_positions_view
    {
        T m_value;

    public:
        class iterator
        {
            T m_remaining;

        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = size_t;
            using difference_type = ptrdiff_t;
            using reference = value_type;

            constexpr explicit iterator(T remaining) noexcept
                : m_remaining(remaining)
            {}

            constexpr reference operator*() const noexcept
            {
                return std::countr_zero(m_remaining);
            }

            constexpr iterator& operator++() noexcept
            {
                m_remaining &= static_cast<T>(m_remaining - 1u);
                return *this;
            }

            constexpr iterator operator++(int) noexcept
            {
                iterator result{*this};
                ++(*this);
                return result;
            }

            constexpr bool operator==(iterator const& other) const noexcept = default;
        };

        constexpr explicit set_bit_positions_view(T value) noexcept
            : m_value(value)
        {}

        constexpr iterator begin() const noexcept
        {
            return iterator{m_value};
        }

        constexpr iterator end() const noexcept
        {
            return iterator{T{}};
        }
    };

    template <size_t N>
    class set_bit_positions_view<uint<N>>
    {
        uint<N> m_value;

    public:
        class iterator
        {
            uint<N> const* m_value{};
            size_t m_word{uint<N>::M};
            typename uint<N>::bits_type m_remaining{};

            constexpr void skip_empty_words() noexcept
            {
                while (m_word < uint<N>::M && m_remaining == 0)
                {
                    ++m_word;
                    if (m_word < uint<N>::M)
                        m_remaining = m_value->data[m_word];
                }
            }

        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = size_t;
            using difference_type = ptrdiff_t;
            using reference = value_type;

            constexpr iterator(uint<N> const& value, size_t word) noexcept
                : m_value(&value)
                , m_word(word)
                , m_remaining(word < uint<N>::M ? value.data[word] : 0)
            {
                skip_empty_words();
            }

            constexpr reference operator*() const noexcept
            {
                return m_word * uint<N>::B + std::countr_zero(m_remaining);
            }

            constexpr iterator& operator++() noexcept
            {
                m_remaining &= m_remaining - 1u;
                skip_empty_words();
                return *this;
            }

            constexpr iterator operator++(int) noexcept
            {
                iterator result{*this};
                ++(*this);
                return result;
            }

            constexpr bool operator==(iterator const& other) const noexcept
            {
                return m_value == other.m_value &&
                       m_word == other.m_word &&
                       m_remaining == other.m_remaining;
            }
        };

        constexpr explicit set_bit_positions_view(uint<N> value) noexcept
            : m_value(value)
        {}

        constexpr iterator begin() const noexcept
        {
            return iterator{m_value, 0};
        }

        constexpr iterator end() const noexcept
        {
            return iterator{m_value, uint<N>::M};
        }
    };

    template <size_t N, size_t R>
    class set_bit_positions_view<uint_indices<N, R>>
    {
        uint_indices<N, R> m_value;

    public:
        class iterator
        {
            uint_indices<N, R> const* m_value{};
            size_t m_index{};

        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = size_t;
            using difference_type = ptrdiff_t;
            using reference = value_type;

            constexpr iterator(uint_indices<N, R> const& value, size_t index) noexcept
                : m_value(&value)
                , m_index(value.is_sentinel ? R : index)
            {}

            constexpr reference operator*() const noexcept
            {
                return m_value->indices[m_index];
            }

            constexpr iterator& operator++() noexcept
            {
                ++m_index;
                return *this;
            }

            constexpr iterator operator++(int) noexcept
            {
                iterator result{*this};
                ++(*this);
                return result;
            }

            constexpr bool operator==(iterator const& other) const noexcept
            {
                return m_value == other.m_value && m_index == other.m_index;
            }
        };

        constexpr explicit set_bit_positions_view(uint_indices<N, R> value) noexcept
            : m_value(value)
        {}

        constexpr iterator begin() const noexcept
        {
            return iterator{m_value, 0};
        }

        constexpr iterator end() const noexcept
        {
            return iterator{m_value, R};
        }
    };

    template <size_t N, size_t R>
    class set_bit_positions_view<uint_complement_indices<N, R>>
    {
        uint_complement_indices<N, R> m_value;

    public:
        class iterator
        {
            uint_complement_indices<N, R> const* m_value{};
            size_t m_position{};
            size_t m_hole_index{};
            size_t m_next_hole{N + 1u};

            constexpr void skip_holes() noexcept
            {
                while (m_position == m_next_hole) [[unlikely]]
                {
                    ++m_position;
                    ++m_hole_index;
                    m_next_hole = m_hole_index < R
                        ? static_cast<size_t>(m_value->indices[m_hole_index])
                        : N + 1u;
                }
            }

        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = size_t;
            using difference_type = ptrdiff_t;
            using reference = value_type;

            constexpr iterator(uint_complement_indices<N, R> const& value,
                               size_t position) noexcept
                : m_value(&value)
                , m_position(value.is_sentinel ? N : position)
                , m_next_hole([&] {
                    if constexpr (R == 0)
                        return N + 1u;
                    else
                        return value.is_sentinel
                            ? N + 1u
                            : static_cast<size_t>(value.indices[0]);
                }())
            {
                skip_holes();
            }

            constexpr reference operator*() const noexcept
            {
                return m_position;
            }

            constexpr iterator& operator++() noexcept
            {
                ++m_position;
                skip_holes();
                return *this;
            }

            constexpr iterator operator++(int) noexcept
            {
                iterator result{*this};
                ++(*this);
                return result;
            }

            constexpr bool operator==(iterator const& other) const noexcept
            {
                return m_value == other.m_value && m_position == other.m_position;
            }
        };

        constexpr explicit set_bit_positions_view(uint_complement_indices<N, R> value) noexcept
            : m_value(value)
        {}

        constexpr iterator begin() const noexcept
        {
            return iterator{m_value, 0};
        }

        constexpr iterator end() const noexcept
        {
            return iterator{m_value, N};
        }
    };

    template <typename T>
        requires is_unsigned_v<T>
    constexpr auto set_bit_positions(T value) noexcept
    {
        return set_bit_positions_view<T>{value};
    }
}

#include <limits>
namespace std
{
    template <size_t N, size_t R>
    struct numeric_limits<jrmwng::popcount_ranges::stdx::uint_indices<N, R>>
    {
        static constexpr bool is_specialized = true;
        static constexpr bool is_signed = false;
        static constexpr bool is_integer = true;
        static constexpr bool is_exact = true;
        static constexpr bool has_infinity = false;
        static constexpr bool has_quiet_NaN = false;
        static constexpr bool has_signaling_NaN = false;
        static constexpr int max_digits10 = 0;
        static constexpr bool is_bounded = true;
        static constexpr bool is_modulo = true;
        static constexpr int radix = 2;
        static constexpr int digits = static_cast<int>(N);
        static constexpr int digits10 = N * 301 / 1000;
        using value_type = jrmwng::popcount_ranges::stdx::uint_indices<N, R>;
        static constexpr value_type min() noexcept { return {}; }
        static constexpr value_type lowest() noexcept { return {}; }
        static constexpr value_type max() noexcept
        {
            value_type value{};
            for (size_t i = 0; i < R; ++i)
                value.indices[i] = static_cast<typename value_type::index_type>(N - R + i);
            return value;
        }
    };

    template <size_t N, size_t R>
    struct numeric_limits<jrmwng::popcount_ranges::stdx::uint_complement_indices<N, R>>
    {
        static constexpr bool is_specialized = true;
        static constexpr bool is_signed = false;
        static constexpr bool is_integer = true;
        static constexpr bool is_exact = true;
        static constexpr bool has_infinity = false;
        static constexpr bool has_quiet_NaN = false;
        static constexpr bool has_signaling_NaN = false;
        static constexpr int max_digits10 = 0;
        static constexpr bool is_bounded = true;
        static constexpr bool is_modulo = true;
        static constexpr int radix = 2;
        static constexpr int digits = static_cast<int>(N);
        static constexpr int digits10 = N * 301 / 1000;
        using value_type = jrmwng::popcount_ranges::stdx::uint_complement_indices<N, R>;
        static constexpr value_type min() noexcept { return {}; }
        static constexpr value_type lowest() noexcept { return {}; }
        static constexpr value_type max() noexcept
        {
            value_type value{};
            for (size_t i = 0; i < R; ++i)
                value.indices[i] = static_cast<typename value_type::index_type>(i);
            return value;
        }
    };

    template <size_t N>
    struct numeric_limits<jrmwng::popcount_ranges::stdx::uint<N>>
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
        static constexpr jrmwng::popcount_ranges::stdx::uint<N> min() noexcept { return jrmwng::popcount_ranges::stdx::uint<N>{0}; }
        static constexpr jrmwng::popcount_ranges::stdx::uint<N> lowest() noexcept { return jrmwng::popcount_ranges::stdx::uint<N>{0}; }
        static constexpr jrmwng::popcount_ranges::stdx::uint<N> max() noexcept { return ~jrmwng::popcount_ranges::stdx::uint<N>{0}; }
    };
}