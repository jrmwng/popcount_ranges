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
        template <typename Tuint, size_t R>
        constexpr auto nCr_read(size_t n, std::integral_constant<size_t, R>) noexcept;

        template <typename Tuint, size_t R>
        constexpr size_t nCr_search(size_t n_first, size_t n_last, uintmax_t target, std::integral_constant<size_t, R> = {}) noexcept;

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

            constexpr static std::uintmax_t rank(Tuint value) noexcept
            {
                constexpr auto TOTAL = nCr_read<Tuint>(std::numeric_limits<Tuint>::digits, std::integral_constant<size_t, uPOPCOUNT>{});

                if (value == SENTINEL)
                    return TOTAL;
                else if (value == INITIAL)
                    return 0;
                else if constexpr (uPOPCOUNT + uPOPCOUNT > std::numeric_limits<Tuint>::digits)
                {
                    // Use the complementary set to compute the rank of the complement.
                    return TOTAL - 1u - complementary_set::rank(~value);
                }
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
                                nCr_read<Tuint>(std::countr_zero(v), std::integral_constant<size_t, Js + 1>{})
                            , v &= v - Tuint(1)), ...),
                            result;
                    }(0u, value, std::make_index_sequence<uPOPCOUNT>{});
                }
            }
            constexpr static std::ptrdiff_t distance(Tuint first, Tuint last) noexcept
            {
                if (first == SENTINEL || last == SENTINEL || first == INITIAL || last == INITIAL)
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
                        return ((r0 += nCr_read<Tuint>(std::countr_zero(v0), std::integral_constant<size_t, Js + 1>{}),
                                 r1 += nCr_read<Tuint>(std::countr_zero(v1), std::integral_constant<size_t, Js + 1>{}),
                                 v0 &= v0 - Tuint(1),
                                 v1 &= v1 - Tuint(1)), ...),
                               (r1 >= r0 ? static_cast<std::ptrdiff_t>(r1 - r0) : -static_cast<std::ptrdiff_t>(r0 - r1));
                    }(0u, 0u, first, last, std::make_index_sequence<uPOPCOUNT>{});
                }
            }

            constexpr static Tuint unrank(std::uintmax_t rank) noexcept
            {
                constexpr auto TOTAL = nCr_read<Tuint>(std::numeric_limits<Tuint>::digits, std::integral_constant<size_t, uPOPCOUNT>{});

                if (rank == 0)
                    return INITIAL;
                else if (rank == TOTAL)
                    return SENTINEL;
                else if constexpr (uPOPCOUNT + uPOPCOUNT > std::numeric_limits<Tuint>::digits)    
                {
                    return static_cast<Tuint>(~complementary_set::unrank(TOTAL - 1 - rank));
                }
                else
                {
                    Tuint result = 0;
                    std::uintmax_t remaining = rank;
                    size_t n = std::numeric_limits<Tuint>::digits + 1u;

                    [&]<size_t... Is>(std::index_sequence<Is...>) {
                        (([&]() {
                            constexpr std::integral_constant<size_t, uPOPCOUNT - Is> R{}; // R = uPOPCOUNT, uPOPCOUNT-1, ..., 1

                            if (remaining == 0)
                            {
                                // All remaining bits go at the lowest positions
                                result |= static_cast<Tuint>(Tuint(1) << (R - 1u));
                            }
                            else
                            {
                                n = nCr_search<Tuint>(std::max<size_t>(R, std::bit_width(remaining) - 1u), n, remaining, R);

                                remaining -= nCr_read<Tuint>(n, R);
                                result |= static_cast<Tuint>(Tuint(1) << n);
                            }
                        })(), ...);
                    }(std::make_index_sequence<uPOPCOUNT>{});

                    return result;
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
                size_t m_rank;
            public:
                using iterator_category = std::random_access_iterator_tag;
                using value_type = Tuint;
                using difference_type = std::ptrdiff_t;
                using reference = value_type const &;
                
                constexpr iterator(Tuint value) : m_value(value), m_rank(rank(value)) {}

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
                    if (m_value == SENTINEL)
                        throw std::out_of_range("set::iterator: incrementing past end");
                    m_value = GospersHack(m_value);
                    m_rank++;
                    return *this;
                }
                iterator operator++ (int)
                {
                    iterator it{m_value};
                    ++(*this);
                    return it;
                }
                iterator& operator--()
                {
                    if (m_value == INITIAL)
                        throw std::out_of_range("set::iterator: decrementing past begin");
                    if (m_value == SENTINEL)
                        m_value = static_cast<Tuint>(~complementary_set::INITIAL);
                    else
                        m_value = ~complementary_set::GospersHack(~m_value);
                    m_rank--;
                    return *this;
                }
                iterator operator-- (int)
                {
                    iterator it{m_value};
                    --(*this);
                    return it;
                }
                difference_type operator-(iterator const& other) const
                {
                    return distance(other.m_value, m_value);
                }

                iterator& operator+=(difference_type n)
                {
                    if (n == 0)
                        return *this;
                    if (n == 1)
                        return ++(*this);
                    if (n == -1)
                        return --(*this);

                    constexpr auto LAST_RANK = rank(SENTINEL);
                    const     auto this_rank = m_rank;
                    const     auto next_rank = (n < 0 ? this_rank - static_cast<std::uintmax_t>(-n) : this_rank + static_cast<std::uintmax_t>(n));
                    if (n < 0 && static_cast<std::uintmax_t>(-n) > this_rank)
                        throw std::out_of_range("set::iterator: decrementing past begin");
                    if (n > 0 && static_cast<std::uintmax_t>(n) > LAST_RANK - this_rank)
                        throw std::out_of_range("set::iterator: incrementing past end");

                    m_value = next_rank == LAST_RANK ? SENTINEL : unrank(next_rank);
                    m_rank = next_rank;
                    return *this;
                }
                iterator operator+(difference_type n) const
                {
                    iterator it{*this};
                    it += n;
                    return it;
                }
                iterator& operator-=(difference_type n)
                {
                    if (n == 0)
                        return *this;
                    if (n == 1)
                        return --(*this);
                    if (n == -1)
                        return ++(*this);
                    
                    constexpr auto LAST_RANK = rank(SENTINEL);
                    const     auto this_rank = m_rank;
                    const     auto next_rank = (n < 0 ? this_rank + static_cast<std::uintmax_t>(-n) : this_rank - static_cast<std::uintmax_t>(n));
                    if (n < 0 && static_cast<std::uintmax_t>(-n) > LAST_RANK - this_rank)
                        throw std::out_of_range("set::iterator: incrementing past end");
                    if (n > 0 && static_cast<std::uintmax_t>(n) > this_rank)
                        throw std::out_of_range("set::iterator: decrementing past begin");

                    m_value = next_rank == LAST_RANK ? SENTINEL : unrank(next_rank);
                    m_rank = next_rank;
                    return *this;
                }
                iterator operator-(difference_type n) const
                {
                    iterator it{*this};
                    it -= n;
                    return it;
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
            , m_rank(rank(m_value))
        {}
    }
}
