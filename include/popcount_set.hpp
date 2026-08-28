#pragma once

#include <stdexcept>
#include <iterator>
#include <bit>
#include <type_traits>
#include <limits>

namespace jrmwng
{
    namespace popcount_ranges
    {
        // A set of unsigned integers having the same `std::popcount` result.
        template <auto uPOPCOUNT>
        class popcount_set
        {
            using Tuint = decltype(uPOPCOUNT);
            static_assert(std::is_unsigned_v<Tuint>, "popcount_set: Tuint must be an unsigned integer type");
            static_assert(uPOPCOUNT <= std::numeric_limits<Tuint>::digits, "popcount_set: uPOPCOUNT must be less than or equal to the number of bits in Tuint");

            using complementary_set = popcount_set<Tuint{std::numeric_limits<Tuint>::digits - uPOPCOUNT}>;
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
            // static_assert(GospersHack(~complementary_set::GospersHack(~INITIAL)) == (Tuint)(uPOPCOUNT == std::numeric_limits<Tuint>::digits ? ~Tuint{} : ~(static_cast<Tuint>(~Tuint{}) << uPOPCOUNT)));

            inline constexpr static Tuint SENTINEL = GospersHack(uPOPCOUNT == std::numeric_limits<Tuint>::digits ? ~Tuint{} : ~(static_cast<Tuint>(~Tuint{}) >> uPOPCOUNT));
            // static_assert((Tuint)(~complementary_set::GospersHack(~SENTINEL)) == (Tuint)(uPOPCOUNT == std::numeric_limits<Tuint>::digits ? ~Tuint{} : ~(static_cast<Tuint>(~Tuint{}) >> uPOPCOUNT)));

            constexpr static std::ptrdiff_t rank(Tuint value)
            {
                if (value == SENTINEL)
                    return -1;
                std::ptrdiff_t rank = 0;
                for (Tuint i = INITIAL; i != value; i = GospersHack(i))
                    ++rank;
                return rank;
            }

            Tuint m_first{INITIAL};
            Tuint m_last{SENTINEL};
        public:
            popcount_set() = default;
            popcount_set(Tuint first, Tuint last) : m_first(first), m_last(last)
            {
                if (first != SENTINEL)
                {
                    if (std::popcount(first) != static_cast<int>(uPOPCOUNT))
                        throw std::invalid_argument("popcount_set: first does not have the required popcount");
                }
                if (last != SENTINEL)
                {
                    if (std::popcount(last) != static_cast<int>(uPOPCOUNT))
                        throw std::invalid_argument("popcount_set: last does not have the required popcount");
                    if (first == SENTINEL)
                        throw std::invalid_argument("popcount_set: last is not sentinel but first is");
                    if (first > last)
                        throw std::invalid_argument("popcount_set: first is greater than last");
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
                        ? ~complementary_set::INITIAL // end() -> rbegin()
                        : ~complementary_set::GospersHack(~(it.m_value)))
                {}

                reference operator*() const { return m_value; }

                bool operator==(reverse_iterator const& other) const { return m_value == other.m_value; }
                bool operator!=(reverse_iterator const& other) const { return m_value != other.m_value; }

                reverse_iterator& operator++()
                {
                    m_value = ~complementary_set::GospersHack(~m_value);
                    return *this;
                }
                reverse_iterator operator++ (int)
                {
                    reverse_iterator it{m_value};
                    m_value = ~complementary_set::GospersHack(~m_value);
                    return it;
                }
            };
            
            iterator begin() const { return {m_first}; }
            iterator end() const { return {m_last}; }
            
            reverse_iterator rbegin() const { return reverse_iterator(end()); }
            reverse_iterator rend() const { return reverse_iterator(begin()); }
        };
        template <auto uPOPCOUNT>
        constexpr popcount_set<uPOPCOUNT>::iterator::iterator(reverse_iterator const& it)
            : m_value(it.m_value == (Tuint)~complementary_set::SENTINEL
                ? INITIAL // rend() -> begin()
                : GospersHack(it.m_value))
        {}
    }
}
