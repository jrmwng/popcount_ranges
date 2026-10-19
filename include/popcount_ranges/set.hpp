#pragma once

#include <stdexcept>
#include <iterator>
#include <bit>
#include <type_traits>
#include <limits>
#include <cstdint>
#include <cmath>

#include "ncr.hpp"
#include "uint.hpp"

namespace jrmwng
{
    namespace popcount_ranges
    {
        template <typename Tuint, std::size_t R>
        constexpr auto nCr_read(std::size_t n, std::integral_constant<std::size_t, R>) noexcept;

        template <typename Tuint, std::size_t R>
        constexpr decltype(auto) nCr_search(std::size_t n_first, std::size_t n_last, uintmax_t target, std::integral_constant<std::size_t, R>) noexcept;

        template <std::size_t N, std::size_t R>
        struct default_set_uint
        {
            using type = std::conditional_t<
                (N <= 8u), std::uint8_t,
                std::conditional_t<
                    (N <= 16u), std::uint16_t,
                    std::conditional_t<
                        (N <= 32u), std::uint32_t,
                        std::conditional_t<
                            (N <= 64u), std::uint64_t,
                            stdx::uint_indices<N, R>>>>>;
        };

        template <std::size_t N, std::size_t R>
        using default_set_uint_t = typename default_set_uint<N, R>::type;

        template <std::size_t N, std::size_t R, typename Tuint = default_set_uint_t<N, R>>
        class set;

        template <std::size_t N, typename Tuint, std::size_t R>
        struct set_complement
        {
            using type = set<N, N - R, Tuint>;
        };

        template <std::size_t N, std::size_t CurrentR, std::size_t R>
        struct set_complement<N, stdx::uint_indices<N, CurrentR>, R>
        {
            using type = set<N, N - R, stdx::uint_indices<N, N - R>>;
        };

        template <std::size_t N, std::size_t HoleCount, std::size_t R>
        struct set_complement<N, stdx::uint_complement_indices<N, HoleCount>, R>
        {
            using type = set<N, N - R, stdx::uint_indices<N, HoleCount>>;
        };

        // A set of unsigned integers having the same `std::popcount` result.
        template <std::size_t N, std::size_t R, typename Tuint>
        class set
        {
            inline constexpr static std::size_t zuPOPCOUNT = R;
            static_assert(stdx::is_unsigned_v<Tuint>, "set: Tuint must be an unsigned integer type");
            static_assert(N > 0, "set: N must be greater than zero");
            static_assert(N <= std::numeric_limits<Tuint>::digits,
                          "set: Tuint must have at least N value bits");
            static_assert(zuPOPCOUNT <= N, "set: R must be less than or equal to N");
            inline constexpr static bool IS_INDEX_UINT = requires(Tuint value) { value.indices; value.is_sentinel; };
            inline constexpr static bool IS_COMPLEMENT_INDEX = requires { Tuint::is_complement_index; };
            inline constexpr static std::size_t REGISTER_BITS = std::numeric_limits<Tuint>::digits;
            inline constexpr static Tuint LOGICAL_MASK = [] {
                if constexpr (IS_INDEX_UINT)
                    return Tuint{};
                else if constexpr (N == REGISTER_BITS)
                    return static_cast<Tuint>(~Tuint{});
                else
                    return static_cast<Tuint>((Tuint{1} << N) - Tuint{1});
            }();

            using complementary_set = typename set_complement<N, Tuint, zuPOPCOUNT>::type;
            friend complementary_set;

            constexpr static Tuint GospersHackRaw(Tuint value) noexcept
            {
                int const count = stdx::countr_zero(value);
                Tuint const shifted = value >> count;
                Tuint const raised = shifted + Tuint{1};
                return static_cast<Tuint>(((raised ^ shifted) >> 2) | (raised << count));
            }

            constexpr static Tuint LogicalNot(Tuint value) noexcept
            {
                if constexpr (IS_INDEX_UINT)
                    return Tuint{};
                else
                    return static_cast<Tuint>((~value) & LOGICAL_MASK);
            }

            constexpr static Tuint ReverseComplement(Tuint value) noexcept
            {
                if (value == LogicalNot(complementary_set::SENTINEL))
                    return complementary_set::SENTINEL;
                return LogicalNot(value);
            }

            constexpr static Tuint GospersHack(Tuint i0) noexcept
            {
                if constexpr (IS_INDEX_UINT)
                {
                    if (i0.is_sentinel)
                        return i0;
                    if constexpr (IS_COMPLEMENT_INDEX)
                    {
                        constexpr std::size_t HOLE_COUNT =
                            N - zuPOPCOUNT;
                        for (std::size_t i = 0; i < HOLE_COUNT; ++i)
                        {
                            if (static_cast<std::size_t>(i0.indices[i]) > i)
                            {
                                --i0.indices[i];
                                for (std::size_t j = 0; j < i; ++j)
                                    i0.indices[j] = static_cast<typename Tuint::index_type>(
                                        static_cast<std::size_t>(i0.indices[i]) - i + j);
                                return i0;
                            }
                        }
                    }
                    else
                    {
                        for (std::size_t i = 0; i < zuPOPCOUNT; ++i)
                        {
                            auto const limit = i + 1u < zuPOPCOUNT
                                ? static_cast<std::size_t>(i0.indices[i + 1u])
                                : N;
                            if (static_cast<std::size_t>(i0.indices[i]) + 1u < limit)
                            {
                                ++i0.indices[i];
                                for (std::size_t j = 0; j < i; ++j)
                                    i0.indices[j] = static_cast<typename Tuint::index_type>(j);
                                return i0;
                            }
                        }
                    }
                    return SENTINEL;
                }
                else if constexpr (zuPOPCOUNT == 0u || zuPOPCOUNT == N)
                    return SENTINEL;
                else
                {
                    return GospersHackRaw(i0);
                }
            }
            constexpr static Tuint Previous(Tuint value) noexcept
            {
                if constexpr (IS_INDEX_UINT)
                {
                    if (value.is_sentinel)
                    {
                        if constexpr (IS_COMPLEMENT_INDEX)
                        {
                            constexpr std::size_t HOLE_COUNT =
                                N - zuPOPCOUNT;
                            for (std::size_t i = 0; i < HOLE_COUNT; ++i)
                                value.indices[i] = static_cast<typename Tuint::index_type>(i);
                        }
                        else
                        {
                            for (std::size_t i = 0; i < zuPOPCOUNT; ++i)
                                value.indices[i] = static_cast<typename Tuint::index_type>(
                                    N - zuPOPCOUNT + i);
                        }
                        value.is_sentinel = false;
                        return value;
                    }
                    if (value == INITIAL)
                    {
                        value.is_sentinel = true;
                        return value;
                    }
                    if constexpr (IS_COMPLEMENT_INDEX)
                    {
                        constexpr std::size_t HOLE_COUNT =
                        N - zuPOPCOUNT;
                        for (std::size_t i = 0; i < HOLE_COUNT; ++i)
                        {
                            auto const limit = i + 1u < HOLE_COUNT
                                ? static_cast<std::size_t>(value.indices[i + 1u])
                                : N;
                            if (static_cast<std::size_t>(value.indices[i]) + 1u < limit)
                            {
                                ++value.indices[i];
                                for (std::size_t j = 0; j < i; ++j)
                                    value.indices[j] = static_cast<typename Tuint::index_type>(j);
                                return value;
                            }
                        }
                    }
                    else
                    {
                        for (std::size_t i = 0; i < zuPOPCOUNT; ++i)
                        {
                            if (static_cast<std::size_t>(value.indices[i]) > i)
                            {
                                --value.indices[i];
                                for (std::size_t j = 0; j < i; ++j)
                                    value.indices[j] = static_cast<typename Tuint::index_type>(
                                        static_cast<std::size_t>(value.indices[i]) - i + j);
                                return value;
                            }
                        }
                    }
                    value.is_sentinel = true;
                    return value;
                }
                else
                {
                    if (value == SENTINEL)
                        return static_cast<Tuint>((~complementary_set::INITIAL) & LOGICAL_MASK);
                    auto const complement = static_cast<Tuint>((~value) & LOGICAL_MASK);
                    return static_cast<Tuint>((~complementary_set::GospersHack(complement)) & LOGICAL_MASK);
                }
            }
            inline constexpr static Tuint INITIAL = [] {
                if constexpr (IS_INDEX_UINT)
                {
                    Tuint value{};
                    if constexpr (IS_COMPLEMENT_INDEX)
                    {
                        constexpr std::size_t HOLE_COUNT =
                            N - zuPOPCOUNT;
                        for (std::size_t i = 0; i < HOLE_COUNT; ++i)
                            value.indices[i] = static_cast<typename Tuint::index_type>(
                                N - HOLE_COUNT + i);
                    }
                    else
                    {
                        for (std::size_t i = 0; i < zuPOPCOUNT; ++i)
                            value.indices[i] = static_cast<typename Tuint::index_type>(i);
                    }
                    return value;
                }
                else if constexpr (zuPOPCOUNT == 0u)
                    return Tuint{};
                else if constexpr (zuPOPCOUNT == N)
                    return LOGICAL_MASK;
                else
                    return static_cast<Tuint>((Tuint{1} << zuPOPCOUNT) - Tuint{1});
            }();
            inline constexpr static Tuint SENTINEL = [] {
                if constexpr (IS_INDEX_UINT)
                {
                    Tuint value = INITIAL;
                    value.is_sentinel = true;
                    return value;
                }
                else if constexpr (zuPOPCOUNT == 0u)
                    return static_cast<Tuint>(~Tuint{});
                else if constexpr (zuPOPCOUNT == N)
                    return Tuint{};
                else
                {
                    constexpr Tuint largest =
                        static_cast<Tuint>(LOGICAL_MASK ^ ((Tuint{1} << (N - zuPOPCOUNT)) - Tuint{1}));
                    return GospersHackRaw(largest);
                }
            }();

            constexpr static std::uintmax_t rank(Tuint value) noexcept
            {
                constexpr auto TOTAL = nCr_read<Tuint>(
                    N, std::integral_constant<std::size_t, (zuPOPCOUNT < N - zuPOPCOUNT
                        ? zuPOPCOUNT : N - zuPOPCOUNT)>{});

                if (value == SENTINEL)
                    return TOTAL;
                else if (value == INITIAL)
                    return 0;
                else if constexpr (IS_INDEX_UINT)
                {
                    if constexpr (IS_COMPLEMENT_INDEX)
                    {
                        constexpr std::size_t HOLE_COUNT =
                            N - zuPOPCOUNT;
                        auto const complement_rank =
                            []<std::size_t... Js>(Tuint const& v, std::index_sequence<Js...>) {
                                return (std::uintmax_t{} + ... +
                                    nCr_read<Tuint>(v.indices[Js],
                                        std::integral_constant<std::size_t, Js + 1>{}));
                            }(value, std::make_index_sequence<HOLE_COUNT>{});
                        return TOTAL - 1u - complement_rank;
                    }
                    else
                    {
                        return []<std::size_t... Js>(Tuint const& v, std::index_sequence<Js...>) {
                            return (std::uintmax_t{} + ... +
                                nCr_read<Tuint>(v.indices[Js], std::integral_constant<std::size_t, Js + 1>{}));
                        }(value, std::make_index_sequence<zuPOPCOUNT>{});
                    }
                }
                else if constexpr (zuPOPCOUNT + zuPOPCOUNT > N)
                {
                    // Use the complementary set to compute the rank of the complement.
                    return TOTAL - 1u - complementary_set::rank(static_cast<Tuint>((~value) & LOGICAL_MASK));
                }
                else
                {
                    // Fold evaluates left-to-right via comma operator, so side effects
                    // on `v` (clearing the lowest set bit each step) are correctly sequenced.
                    //
                    // Step j:  pos = countr_zero(v)       -- position of lowest set bit (tzcnt)
                    //          result += C(pos, j+1)      -- combinadic contribution
                    //          v &= v - 1                 -- clear that bit
                    return []<std::size_t... Js>(std::uintmax_t result, Tuint v, std::index_sequence<Js...>) {
                        return ((result += 
                                nCr_read<Tuint>(stdx::countr_zero(v), std::integral_constant<std::size_t, Js + 1>{})
                            , v &= v - Tuint(1)), ...),
                            result;
                    }(0u, value, std::make_index_sequence<zuPOPCOUNT>{});
                }
            }
            constexpr static std::ptrdiff_t distance(Tuint first, Tuint last) noexcept
            {
                if (first == SENTINEL || last == SENTINEL || first == INITIAL || last == INITIAL)
                    return static_cast<std::ptrdiff_t>(rank(last)) - static_cast<std::ptrdiff_t>(rank(first));
                else if constexpr (IS_INDEX_UINT)
                {
                    return static_cast<std::ptrdiff_t>(rank(last)) - static_cast<std::ptrdiff_t>(rank(first));
                }
                else if constexpr (zuPOPCOUNT + zuPOPCOUNT > N)
                {
                    // Use the complementary set to compute the distance of the complement.
                    return static_cast<std::ptrdiff_t>(complementary_set::distance(
                        static_cast<Tuint>((~last) & LOGICAL_MASK),
                        static_cast<Tuint>((~first) & LOGICAL_MASK)));
                }
                else if (first == last)
                    return 0;
                else
                {
                    return []<std::size_t... Js>(std::uintmax_t r0, std::uintmax_t r1, Tuint v0, Tuint v1, std::index_sequence<Js...>) {
                        return ((r0 += nCr_read<Tuint>(stdx::countr_zero(v0), std::integral_constant<std::size_t, Js + 1>{}),
                                 r1 += nCr_read<Tuint>(stdx::countr_zero(v1), std::integral_constant<std::size_t, Js + 1>{}),
                                 v0 &= v0 - Tuint(1),
                                 v1 &= v1 - Tuint(1)), ...),
                               (r1 >= r0 ? static_cast<std::ptrdiff_t>(r1 - r0) : -static_cast<std::ptrdiff_t>(r0 - r1));
                    }(0u, 0u, first, last, std::make_index_sequence<zuPOPCOUNT>{});
                }
            }

            constexpr static Tuint unrank(std::uintmax_t rank) noexcept
            {
                constexpr auto TOTAL = nCr_read<Tuint>(
                    N, std::integral_constant<std::size_t, (zuPOPCOUNT < N - zuPOPCOUNT
                        ? zuPOPCOUNT : N - zuPOPCOUNT)>{});

                if (rank == 0)
                    return INITIAL;
                else if (rank == TOTAL)
                    return SENTINEL;
                else if constexpr (IS_COMPLEMENT_INDEX)
                {
                    constexpr std::size_t HOLE_COUNT =
                        N - zuPOPCOUNT;
                    Tuint result{};
                    [&]<std::size_t... Is>([[maybe_unused]] std::uintmax_t remaining, [[maybe_unused]] std::size_t n,
                                           std::index_sequence<Is...>) {
                        (([&] {
                            constexpr std::size_t position = HOLE_COUNT - Is - 1u;
                            auto const [found_n, found_nCr] = nCr_search<Tuint>(
                                position + 1u, n, remaining,
                                std::integral_constant<std::size_t, position + 1u>{});
                            n = found_n;
                            remaining -= found_nCr;
                            result.indices[position] = static_cast<typename Tuint::index_type>(found_n);
                        }()), ...);
                    }(TOTAL - 1u - rank, N + 1u,
                      std::make_index_sequence<HOLE_COUNT>{});
                    return result;
                }
                else if constexpr (IS_INDEX_UINT)
                {
                    Tuint result{};
                    [&]<std::size_t... Is>([[maybe_unused]] std::uintmax_t remaining, [[maybe_unused]] std::size_t n, std::index_sequence<Is...>) {
                        (([&] {
                            constexpr std::size_t position = zuPOPCOUNT - Is - 1u;
                            auto const [found_n, found_nCr] = nCr_search<Tuint>(
                                position + 1u, n, remaining, std::integral_constant<std::size_t, position + 1u>{});
                            n = found_n;
                            remaining -= found_nCr;
                            result.indices[position] = static_cast<typename Tuint::index_type>(found_n);
                        }()), ...);
                    }(rank, N + 1u, std::make_index_sequence<zuPOPCOUNT>{});
                    return result;
                }
                else if constexpr (zuPOPCOUNT + zuPOPCOUNT > N)
                {
                    return static_cast<Tuint>((~complementary_set::unrank(TOTAL - 1 - rank)) & LOGICAL_MASK);
                }
                else
                {
                    Tuint result{};
                    [&result]<std::size_t... Is>(std::uintmax_t remaining, std::size_t n, std::index_sequence<Is...>){
                        if constexpr (sizeof...(Is) == 0)
                            return;
                        else
                            (([&](auto count){
                                // count = R, R-1, ..., 1

                                // if constexpr (R == 2)
                                // {
                                //     auto const n_max = (3.0f + ::std::sqrt(::std::nextafter(8.0f * static_cast<float>(remaining) + 1.0f, std::numeric_limits<float>::max()))) * 0.5f;
                                //     n = ::std::min<size_t>(n, static_cast<std::size_t>(n_max));
                                // }

                                auto const [found_n, found_nCr] =
                                    nCr_search<Tuint>(count, n, remaining, count);
                                n = found_n;
                                remaining -= found_nCr;

                                if constexpr (stdx::is_uint<Tuint>::value)
                                    result.data[found_n / Tuint::B] |= 1ull << (found_n % Tuint::B);
                                else
                                    result |= static_cast<Tuint>(Tuint(1) << found_n);
                            }
                            (std::integral_constant<std::size_t, zuPOPCOUNT - Is>{})), ...);
                    }(rank, N + 1u, std::make_index_sequence<zuPOPCOUNT>{});
                    return result;
                }
            }

            Tuint m_first{INITIAL};
            Tuint m_last{SENTINEL};
        public:
            set() noexcept = default;
            #ifdef _DEBUG
            set(Tuint first, Tuint last)
            #else
            set(Tuint first, Tuint last) noexcept
            #endif
                : m_first(first), m_last(last)
            {
                #ifdef _DEBUG
                if (first != SENTINEL)
                {
                    if constexpr (!IS_INDEX_UINT)
                    {
                        if ((first & static_cast<Tuint>(~LOGICAL_MASK)) != 0)
                            throw std::invalid_argument("set: first exceeds the logical width");
                    }
                    if (stdx::popcount(first) != static_cast<int>(zuPOPCOUNT))
                        throw std::invalid_argument("set: first does not have the required reduce_count");
                }
                if (last != SENTINEL)
                {
                    if constexpr (!IS_INDEX_UINT)
                    {
                        if ((last & static_cast<Tuint>(~LOGICAL_MASK)) != 0)
                            throw std::invalid_argument("set: last exceeds the logical width");
                    }
                    if (stdx::popcount(last) != static_cast<int>(zuPOPCOUNT))
                        throw std::invalid_argument("set: last does not have the required reduce_count");
                    if (first == SENTINEL)
                        throw std::invalid_argument("set: last is not sentinel but first is");
                    if (first > last)
                        throw std::invalid_argument("set: first is greater than last");
                }
                #endif
            }

            class reverse_iterator;
            class iterator
            {
                friend class reverse_iterator;
                Tuint m_value;
                std::size_t m_rank;
            public:
                using iterator_category = std::random_access_iterator_tag;
                using value_type = Tuint;
                using difference_type = std::ptrdiff_t;
                using reference = value_type const &;
                
                constexpr iterator(Tuint value) noexcept : m_value(value), m_rank(rank(value)) {}

                iterator() = default;
                iterator(iterator const&) noexcept = default;
                iterator& operator=(iterator const&) noexcept = default;
                iterator(iterator &&) noexcept = default;
                iterator& operator=(iterator &&) noexcept = default;
                
                explicit constexpr iterator(reverse_iterator const&) noexcept;
                
                reference operator*() const noexcept { return m_value; }

                bool operator==(iterator const& other) const noexcept { return m_value == other.m_value; }
                bool operator!=(iterator const& other) const noexcept { return m_value != other.m_value; }
                
                #ifdef _DEBUG
                iterator& operator++()
                #else
                iterator& operator++() noexcept
                #endif
                {
                    #ifdef _DEBUG
                    if (m_value == SENTINEL)
                        throw std::out_of_range("set::iterator: incrementing past end");
                    #endif
                    m_value = GospersHack(m_value);
                    m_rank++;
                    return *this;
                }
                #ifdef _DEBUG
                iterator operator++ (int)
                #else
                iterator operator++ (int) noexcept
                #endif
                {
                    iterator it{m_value};
                    ++(*this);
                    return it;
                }
                #ifdef _DEBUG
                iterator& operator--()
                #else
                iterator& operator--() noexcept
                #endif
                {
                    #ifdef _DEBUG
                    if (m_value == INITIAL)
                        throw std::out_of_range("set::iterator: decrementing past begin");
                    #endif
                    m_value = Previous(m_value);
                    m_rank--;
                    return *this;
                }
                #ifdef _DEBUG
                iterator operator-- (int)
                #else
                iterator operator-- (int) noexcept
                #endif
                {
                    iterator it{m_value};
                    --(*this);
                    return it;
                }
                difference_type operator-(iterator const& other) const noexcept
                {
                    return distance(other.m_value, m_value);
                }

                #ifdef _DEBUG
                void advance(difference_type n)
                #else
                void advance(difference_type n) noexcept
                #endif
                {
                    constexpr auto LAST_RANK = rank(SENTINEL);
                    const     auto this_rank = m_rank;
                    const     auto next_rank = this_rank + n;

                    #ifdef _DEBUG
                    if (next_rank < 0 || next_rank > LAST_RANK)
                        throw std::out_of_range("set::iterator: advancing past valid range");
                    #endif
                    m_value = next_rank == LAST_RANK ? SENTINEL : unrank(next_rank);
                    m_rank = next_rank;
                }

                #ifdef _DEBUG
                iterator& operator+=(difference_type n) 
                #else
                iterator& operator+=(difference_type n) noexcept
                #endif
                {
                    if (n == 0)
                        return *this;
                    if (n == 1)
                        return ++(*this);
                    if (n == -1)
                        return --(*this);

                    advance(n);
                    return *this;
                }
                #ifdef _DEBUG
                iterator operator+(difference_type n) const
                #else
                iterator operator+(difference_type n) const noexcept
                #endif
                {
                    iterator it{*this};
                    it += n;
                    return it;
                }
                #ifdef _DEBUG
                iterator& operator-=(difference_type n)
                #else
                iterator& operator-=(difference_type n) noexcept
                #endif
                {
                    if (n == 0)
                        return *this;
                    if (n == 1)
                        return --(*this);
                    if (n == -1)
                        return ++(*this);
                    
                    advance(-n);
                    return *this;
                }
                #ifdef _DEBUG
                iterator operator-(difference_type n) const
                #else
                iterator operator-(difference_type n) const noexcept
                #endif
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

                constexpr reverse_iterator(Tuint value) noexcept : m_value(value) {}

                reverse_iterator() noexcept = default;
                reverse_iterator(reverse_iterator const&) noexcept = default;
                reverse_iterator& operator=(reverse_iterator const&) noexcept = default;
                reverse_iterator(reverse_iterator &&) noexcept = default;
                reverse_iterator& operator=(reverse_iterator &&) noexcept = default;

                explicit constexpr reverse_iterator(iterator const& it) noexcept
                    : m_value([&] {
                        if constexpr (IS_INDEX_UINT)
                            return Previous(it.m_value);
                        else
                            return it.m_value == SENTINEL
                                ? LogicalNot(complementary_set::INITIAL) // end() -> rbegin()
                                : LogicalNot(complementary_set::GospersHack(LogicalNot(it.m_value))); // ..., begin() -> rend()
                    }())
                {}

                reference operator*() const noexcept { return m_value; }

                bool operator==(reverse_iterator const& other) const noexcept { return m_value == other.m_value; }
                bool operator!=(reverse_iterator const& other) const noexcept { return m_value != other.m_value; }

                reverse_iterator& operator++() noexcept
                {
                    if constexpr (IS_INDEX_UINT)
                        m_value = Previous(m_value);
                    else
                        m_value = LogicalNot(complementary_set::GospersHack(LogicalNot(m_value)));
                    return *this;
                }
                reverse_iterator operator++ (int) noexcept
                {
                    reverse_iterator it{m_value};
                    if constexpr (IS_INDEX_UINT)
                        m_value = Previous(m_value);
                    else
                        m_value = LogicalNot(complementary_set::GospersHack(LogicalNot(m_value)));
                    return it;
                }

                difference_type operator-(reverse_iterator const& other) const noexcept
                {
                    if constexpr (IS_INDEX_UINT)
                    {
                        auto const this_rank = m_value.is_sentinel
                            ? difference_type{-1}
                            : static_cast<difference_type>(rank(m_value));
                        auto const other_rank = other.m_value.is_sentinel
                            ? difference_type{-1}
                            : static_cast<difference_type>(rank(other.m_value));
                        return other_rank - this_rank;
                    }
                    else
                    {
                        // The complement of `m_value` could be `complementary_set::SENTINEL`,
                        // therefore `complementary_set::distance` will handle that case correctly.
                        return complementary_set::distance(
                            ReverseComplement(other.m_value), ReverseComplement(m_value));
                    }
                }
            };
            
            iterator begin() const noexcept { return {m_first}; }
            iterator end() const noexcept { return {m_last}; }
            
            reverse_iterator rbegin() const noexcept { return reverse_iterator(end()); }
            reverse_iterator rend() const noexcept { return reverse_iterator(begin()); }

            std::size_t size() const noexcept
            {
                return distance(m_first, m_last);
            }
            Tuint operator[] (std::size_t index) const noexcept
            {
                return unrank(index);
            }
        };
        template <std::size_t N, std::size_t R, typename Tuint>
        constexpr set<N, R, Tuint>::iterator::iterator(reverse_iterator const& it) noexcept
            : m_value([&] {
                if constexpr (IS_INDEX_UINT)
                    return it.m_value.is_sentinel ? INITIAL : GospersHack(it.m_value);
                else
                    return it.m_value == LogicalNot(complementary_set::SENTINEL)
                        ? INITIAL // rend() -> begin()
                        : GospersHack(it.m_value); // ..., rbegin() -> end()
            }())
            , m_rank(rank(m_value))
        {}
    }
}
