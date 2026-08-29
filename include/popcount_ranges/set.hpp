#pragma once

#include <stdexcept>
#include <iterator>
#include <bit>
#include <type_traits>
#include <limits>
#include <cstdint>

namespace jrmwng
{
    namespace popcount_ranges
    {
        constexpr uintmax_t nCr(size_t n, size_t r) noexcept {
            if (r > n) return 0;
            if (r == 0 || r == n) return 1;
            if (r > n - r) r = n - r;
            uintmax_t result = 1;
            for (uintmax_t i = 0; i < r; ++i) {
                result *= (n - i);
                result /= (i + 1);
            }
            return result;
        }

        template <size_t MAX_N, size_t R>
        class nCr_row
        {
            static_assert(R <= MAX_N, "nCr_row: R must be less than or equal to MAX_N");

            template <uintmax_t Max>
            using min_uint_t = std::conditional_t<
                Max <= std::numeric_limits<uint8_t>::max(),  uint8_t,
                std::conditional_t<
                    Max <= std::numeric_limits<uint16_t>::max(), uint16_t,
                    std::conditional_t<
                        Max <= std::numeric_limits<uint32_t>::max(), uint32_t,
                        uintmax_t>>>;
            using value_type = min_uint_t<nCr(MAX_N, R)>;
            value_type m_row[MAX_N - R + 2]{};
        public:
            constexpr auto & operator[](size_t n) noexcept
            {
                return m_row[n-(R-1)];
            }
            constexpr auto operator[](size_t n) const noexcept
            {
                return m_row[n-(R-1)];
            }
        };
        template <size_t MAX_N>
        class nCr_row<MAX_N, 0>
        {
        public:
            constexpr std::integral_constant<size_t, 1> operator[](size_t) const noexcept
            {
                return {};
            }
        };
        template <size_t MAX_N>
        class nCr_row<MAX_N, 1>
        {
        public:
            constexpr size_t operator[](size_t n) const noexcept
            {
                return n;
            }
        };
        template <size_t MAX_N, size_t MAX_R>
        class nCr_table
        {
            static_assert(MAX_R <= MAX_N, "nCr_table: MAX_R must be less than or equal to MAX_N");
            using row_tuple = decltype([]<size_t... Is>(std::index_sequence<Is...>)
            {
                return std::make_tuple(nCr_row<MAX_N, Is>{}...);
            }
            (std::make_index_sequence<MAX_R + 1>{}));
            row_tuple m_tupleRow;
        public:
            constexpr nCr_table() noexcept
            {
                ([&, this]<size_t... R2s>(std::index_sequence<R2s...>){
                    ([&, this]<size_t R>(std::integral_constant<size_t, R>)
                    {
                        std::get<R>(m_tupleRow)[R] = 1;
                        for (size_t n = R + 1; n <= MAX_N; ++n)
                        {
                            std::get<R>(m_tupleRow)[n] = std::get<R-1>(m_tupleRow)[n-1] + std::get<R>(m_tupleRow)[n-1];
                        }
                    }
                    (std::integral_constant<size_t, R2s + 2>{}), ...);
                }(std::make_index_sequence<MAX_R - 1>{}));
            }

            template <size_t R>
            constexpr auto operator() (size_t n, std::integral_constant<size_t, R>) const noexcept
            {
                return std::get<R>(m_tupleRow)[n];
            }
        };
        template <typename Tuint>
        inline constexpr nCr_table<std::numeric_limits<Tuint>::digits, std::numeric_limits<Tuint>::digits / 2u> nCr_LUT{};
        template <typename Tuint, size_t R>
        constexpr auto nCr(size_t n, std::integral_constant<size_t, R>) noexcept
        {
            constexpr size_t N = std::numeric_limits<Tuint>::digits;
            if constexpr (R > N / 2u)
            {
                return nCr_LUT<Tuint>(n, std::integral_constant<size_t, N - R>{});
            }
            else
            {
                return nCr_LUT<Tuint>(n, std::integral_constant<size_t, R>{});
            }
        }


        // A set of unsigned integers having the same `std::popcount` result.
        template <auto uPOPCOUNT>
        class set
        {
            using Tuint = decltype(uPOPCOUNT);
            static_assert(std::is_unsigned_v<Tuint>, "set: Tuint must be an unsigned integer type");
            static_assert(uPOPCOUNT <= std::numeric_limits<Tuint>::digits, "set: uPOPCOUNT must be less than or equal to the number of bits in Tuint");

            using complementary_set = set<Tuint{std::numeric_limits<Tuint>::digits - uPOPCOUNT}>;
            friend complementary_set;

            constexpr static Tuint GospersHack(Tuint i0)
            {
                if constexpr (uPOPCOUNT == 0)
                    return ~i0; // either 0 or ~0
                else if constexpr (uPOPCOUNT == std::numeric_limits<Tuint>::digits)
                    return ~i0; // either 0 or ~0
                else if constexpr (uPOPCOUNT == 1)
                    return (i0 << 1);
                else if constexpr (uPOPCOUNT + 1u == std::numeric_limits<Tuint>::digits)
                    return ~(static_cast<Tuint>(~i0) >> 1);
                else
                {
                    // Gosper's hack: https://graphics.stanford.edu/~seander/bithacks.html#NextBitPermutation
                    int const cnt = std::countr_zero(i0);
                    Tuint const i1 = i0 >> cnt;
                    Tuint const r1 = i1 + Tuint(1);
                    return ((r1 ^ i1) >> 2) | (r1 << cnt);
                }
            }
            inline constexpr static Tuint INITIAL = (Tuint)(uPOPCOUNT == std::numeric_limits<Tuint>::digits ? ~Tuint{} : ~(static_cast<Tuint>(~Tuint{}) << uPOPCOUNT));
            inline constexpr static Tuint SENTINEL = GospersHack(uPOPCOUNT == std::numeric_limits<Tuint>::digits ? ~Tuint{} : ~(static_cast<Tuint>(~Tuint{}) >> uPOPCOUNT));

            constexpr static std::uintmax_t rank(Tuint value)
            {
                if (value == SENTINEL)
                    return nCr<Tuint>(std::numeric_limits<Tuint>::digits, std::integral_constant<size_t, uPOPCOUNT>{});
                else if constexpr (uPOPCOUNT + uPOPCOUNT > std::numeric_limits<Tuint>::digits)
                {
                    // Use the complementary set to compute the rank of the complement.
                    return nCr<Tuint>(std::numeric_limits<Tuint>::digits, std::integral_constant<size_t, uPOPCOUNT>{})
                        - 1u - complementary_set::rank(~value);
                }
                else if constexpr (uPOPCOUNT == 0)
                    return 0;
                else if constexpr (uPOPCOUNT == std::numeric_limits<Tuint>::digits)
                    return 0;
                else
                {
                    // Fold evaluates left-to-right via comma operator, so side effects
                    // on `v` (clearing the lowest set bit each step) are correctly sequenced.
                    //
                    // Step j:  pos = countr_zero(v)       -- position of lowest set bit (tzcnt)
                    //          result += C(pos, j+1)      -- combinadic contribution
                    //          v &= v - 1                 -- clear that bit
                    return []<size_t... Js>(std::uintmax_t result, Tuint v, std::index_sequence<Js...>) {
                        return ((result += 
                                nCr<Tuint>(std::countr_zero(v), std::integral_constant<size_t, Js + 1>{})
                            , v &= v - Tuint(1)), ...),
                            result;
                    }(0u, value, std::make_index_sequence<uPOPCOUNT>{});
                }
            }
            constexpr static std::ptrdiff_t distance(Tuint first, Tuint last)
            {
                if (first == SENTINEL || last == SENTINEL)
                    return static_cast<std::ptrdiff_t>(rank(last)) - static_cast<std::ptrdiff_t>(rank(first));
                else if constexpr (uPOPCOUNT + uPOPCOUNT > std::numeric_limits<Tuint>::digits)
                {
                    // Use the complementary set to compute the distance of the complement.
                    return static_cast<std::ptrdiff_t>(complementary_set::distance(~last, ~first));
                }
                else if (first == last)
                    return 0;
                else
                {
                    return []<size_t... Js>(std::uintmax_t r0, std::uintmax_t r1, Tuint v0, Tuint v1, std::index_sequence<Js...>) {
                        return ((r0 += nCr<Tuint>(std::countr_zero(v0), std::integral_constant<size_t, Js + 1>{}),
                                 r1 += nCr<Tuint>(std::countr_zero(v1), std::integral_constant<size_t, Js + 1>{}),
                                 v0 &= v0 - Tuint(1),
                                 v1 &= v1 - Tuint(1)), ...),
                               (r1 >= r0 ? static_cast<std::ptrdiff_t>(r1 - r0) : -static_cast<std::ptrdiff_t>(r0 - r1));
                    }(0u, 0u, first, last, std::make_index_sequence<uPOPCOUNT>{});
                }
            }

            Tuint m_first{INITIAL};
            Tuint m_last{SENTINEL};
        public:
            set() = default;
            set(Tuint first, Tuint last) : m_first(first), m_last(last)
            {
                if (first != SENTINEL)
                {
                    if (std::popcount(first) != static_cast<int>(uPOPCOUNT))
                        throw std::invalid_argument("set: first does not have the required popcount");
                }
                if (last != SENTINEL)
                {
                    if (std::popcount(last) != static_cast<int>(uPOPCOUNT))
                        throw std::invalid_argument("set: last does not have the required popcount");
                    if (first == SENTINEL)
                        throw std::invalid_argument("set: last is not sentinel but first is");
                    if (first > last)
                        throw std::invalid_argument("set: first is greater than last");
                }
            }

            class reverse_iterator;
            class iterator
            {
                friend class reverse_iterator;
                Tuint m_value;
            public:
                using iterator_category = std::forward_iterator_tag;
                using value_type = Tuint;
                using difference_type = std::ptrdiff_t;
                using reference = value_type const &;
                
                constexpr iterator(Tuint value) : m_value(value) {}

                iterator() = default;
                iterator(iterator const&) = default;
                iterator& operator=(iterator const&) = default;
                iterator(iterator &&) = default;
                iterator& operator=(iterator &&) = default;
                
                explicit constexpr iterator(reverse_iterator const&);
                
                reference operator*() const { return m_value; }

                bool operator==(iterator const& other) const { return m_value == other.m_value; }
                bool operator!=(iterator const& other) const { return m_value != other.m_value; }
                
                iterator& operator++()
                {
                    m_value = GospersHack(m_value);
                    return *this;
                }
                iterator operator++ (int)
                {
                    iterator it{m_value};
                    m_value = GospersHack(m_value);
                    return it;
                }
                difference_type operator-(iterator const& other) const
                {
                    return distance(other.m_value, m_value);
                }
            };
            class reverse_iterator
            {
                friend class iterator;
                Tuint m_value;
            public:
                using iterator_category = std::forward_iterator_tag;
                using value_type = Tuint;
                using difference_type = std::ptrdiff_t;
                using reference = value_type const &;

                constexpr reverse_iterator(Tuint value) : m_value(value) {}

                reverse_iterator() = default;
                reverse_iterator(reverse_iterator const&) = default;
                reverse_iterator& operator=(reverse_iterator const&) = default;
                reverse_iterator(reverse_iterator &&) = default;
                reverse_iterator& operator=(reverse_iterator &&) = default;

                explicit constexpr reverse_iterator(iterator const& it)
                    : m_value(it.m_value == SENTINEL
                        ? static_cast<Tuint>(~complementary_set::INITIAL) // end() -> rbegin()
                        : static_cast<Tuint>(~complementary_set::GospersHack(~(it.m_value)))) // ..., begin() -> rend()
                {}

                reference operator*() const { return m_value; }

                bool operator==(reverse_iterator const& other) const { return m_value == other.m_value; }
                bool operator!=(reverse_iterator const& other) const { return m_value != other.m_value; }

                reverse_iterator& operator++()
                {
                    m_value = static_cast<Tuint>(~complementary_set::GospersHack(~m_value));
                    return *this;
                }
                reverse_iterator operator++ (int)
                {
                    reverse_iterator it{m_value};
                    m_value = static_cast<Tuint>(~complementary_set::GospersHack(~m_value));
                    return it;
                }

                difference_type operator-(reverse_iterator const& other) const
                {
                    // `~m_value` could be `complementary_set::SENTINEL` but `SENTINEL`,
                    // therefore `complementary_set::distance` will handle that case correctly.
                    return complementary_set::distance(~other.m_value, ~m_value);
                }
            };
            
            iterator begin() const { return {m_first}; }
            iterator end() const { return {m_last}; }
            
            reverse_iterator rbegin() const { return reverse_iterator(end()); }
            reverse_iterator rend() const { return reverse_iterator(begin()); }
        };
        template <auto uPOPCOUNT>
        constexpr set<uPOPCOUNT>::iterator::iterator(reverse_iterator const& it)
            : m_value(it.m_value == (Tuint)~complementary_set::SENTINEL
                ? INITIAL // rend() -> begin()
                : GospersHack(it.m_value)) // ..., rbegin() -> end()
        {}
    }
}
