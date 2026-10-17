#pragma once

#include <cstdint>
#include <limits>
#include <type_traits>
#include <tuple>
#include <utility>
#include <algorithm>
#include <iterator>
#include <numeric>
#include <ranges>
#include <vector>

#include "lower_bound.hpp"

namespace jrmwng
{
    namespace popcount_ranges
    {
        constexpr std::tuple<uintmax_t,bool> nCr(size_t const N, size_t const R) noexcept
        {
            if (R > N - R)
                return nCr(N, N - R);

            if (R == 0 || R == N)
                return {1,false};
            if (R == 1)
                return {N,false};
            if (R > N)
                return {0,false};

            std::vector<uintmax_t> terms(R);
            std::iota(terms.begin(), terms.end(), N - R + 1);

            for (uintmax_t denom : std::views::iota(uintmax_t{2}, uintmax_t{R + 1}) | std::views::reverse)
            {
                for (auto itNum = terms.begin(), endNum = terms.end(); itNum != endNum; )
                {
                    uintmax_t const gcd = std::gcd(*itNum, denom);
                    if (gcd > 1)
                    {
                        *itNum /= gcd;
                        denom /= gcd;

                        if (denom == 1)
                            break;
                        if (*itNum == 1)
                        {
                            *itNum = terms.back();
                            terms.pop_back();
                            endNum = terms.end(); // update end iterator after pop_back
                            continue;
                        }
                    }
                    ++itNum;
                }
            }

            bool overflowed = false;
            uintmax_t result = 1;
            for (uintmax_t num : terms)
            {
                if (result > std::numeric_limits<uintmax_t>::max() / num)
                {
                    overflowed = true;
                    break;
                }
                result *= num;
            }
            return {result,overflowed};
        }

        constexpr size_t nCr_max_R(size_t N) noexcept
        {
            size_t R = 0;
            for (size_t candidate = 1; candidate <= N / 2u; ++candidate)
            {
                auto const [value, overflowed] = nCr(N, candidate);
                if (overflowed)
                    break;
                R = candidate;
            }
            return R;
        }

        template <size_t MAX_N, size_t R>
        class nCr_row
        {
            static_assert(R <= MAX_N, "nCr_row: R must be less than or equal to MAX_N");
            static_assert(std::get<bool>(nCr(MAX_N, R)) == false, "nCr_row: overflow occurred in nCr(MAX_N, R)");

            template <uintmax_t Max>
            using min_uint_t = std::conditional_t<
                Max <= std::numeric_limits<uint8_t>::max(),  uint8_t,
                std::conditional_t<
                    Max <= std::numeric_limits<uint16_t>::max(), uint16_t,
                    std::conditional_t<
                        Max <= std::numeric_limits<uint32_t>::max(), uint32_t,
                        uintmax_t>>>;
            using Tvalue = min_uint_t<std::get<uintmax_t>(nCr(MAX_N, R))>;
            std::array<Tvalue, MAX_N - R + 2> m_row; // C(MAX_N, R), C(MAX_N-1, R), ..., C(R+1,R)=R+1, C(R, R)=1, C(R-1, R)=0
            lower_bound_descent<std::array<Tvalue, MAX_N - R + 2>> m_lower_bound_descent = m_row;
        public:
            constexpr void update() noexcept
            {
                m_lower_bound_descent.update();
            }

            using value_type = Tvalue;
            constexpr auto & operator[](size_t n) noexcept
            {
                return m_row[MAX_N - n]; // reversed order for reverse iteration in search()
            }
            constexpr auto const & operator[](size_t n) const noexcept
            {
                return m_row[MAX_N - n]; // reversed order for reverse iteration in search()
            }
            constexpr std::tuple<size_t, value_type> search(size_t n_first, size_t n_last, uintmax_t target) const noexcept
            {
                std::size_t const found = m_lower_bound_descent(MAX_N - (n_last - 1u), std::size(m_row) - 1u, target, std::greater<>{});
                return {MAX_N - found, m_row[found]};
            }
        };
        template <size_t MAX_N>
        class nCr_row<MAX_N, 0>
        {
        public:
            using value_type = std::integral_constant<size_t, 1>;
            constexpr value_type operator[](size_t) const noexcept
            {
                return {};
            }
            constexpr std::tuple<size_t, value_type> search(size_t first_n, size_t last_n, uintmax_t target) const noexcept
            {
                return {(target >= 1u ? last_n : first_n) - 1u, value_type{}};
            }
        };
        template <size_t MAX_N>
        class nCr_row<MAX_N, 1>
        {
            inline constexpr static auto VIEW = std::views::iota(size_t{1}, size_t{MAX_N + 1});
        public:
            using value_type = size_t;
            constexpr value_type operator[](size_t n) const noexcept
            {
                return n;
            }
            constexpr std::tuple<size_t, value_type> search(size_t first_n, size_t last_n, uintmax_t target) const noexcept
            {
                size_t const value = static_cast<size_t>(std::clamp(target, first_n - 1u, last_n - 1u));
                return {value, value};
            }
        };
        template <size_t MAX_N, size_t MAX_R>
        class nCr_table
        {
            static_assert(MAX_R <= MAX_N, "nCr_table: MAX_R must be less than or equal to MAX_N");
            static_assert(false == std::get<bool>(nCr(MAX_N, MAX_R)), "nCr_table: nCr value is not representable in uintmax_t");
            using row_tuple = decltype([]<size_t... Is>(std::index_sequence<Is...>)
            {
                return std::tuple<nCr_row<MAX_N, Is>...>{};
            }
            (std::make_index_sequence<MAX_R + 1>{}));
            row_tuple m_tupleRow;
        public:
            constexpr nCr_table() noexcept
            {
                if constexpr (sizeof(row_tuple) <= 4096)
                {
                    ([&]<size_t... R2s>(std::index_sequence<R2s...>){ // R2s = 0, 1, ..., MAX_R-2
                        [&]<size_t... Rs>(std::index_sequence<Rs...>){
                            ((std::get<Rs>(m_tupleRow)[Rs - 1u] = 0), ...);
                            ((std::get<Rs>(m_tupleRow)[Rs] = 1), ...);
                            for (size_t n = 3; n <= MAX_N; ++n)
                            {
                                (((n >= Rs + 1) && (std::get<Rs>(m_tupleRow)[n] = static_cast<nCr_row<MAX_N, Rs>::value_type>(std::get<Rs-1>(m_tupleRow)[n-1] + std::get<Rs>(m_tupleRow)[n-1]))), ...);
                            }
                        }(std::index_sequence<(R2s + 2)...>{});
                    }(std::make_index_sequence<MAX_R - 1>{}));
                }
                else
                {
                    ([&, this]<size_t... R2s>(std::index_sequence<R2s...>){ // R2s = 0, 1, ..., MAX_R-2
                        ([&, this]<size_t R>(std::integral_constant<size_t, R>){ // R = 2, 3, ..., MAX_R
                            std::get<R>(m_tupleRow)[R - 1u] = 0;
                            std::get<R>(m_tupleRow)[R] = 1;
                            for (size_t n = R + 1; n <= MAX_N; ++n)
                            {
                                std::get<R>(m_tupleRow)[n] = static_cast<nCr_row<MAX_N, R>::value_type>(std::get<R-1>(m_tupleRow)[n-1] + std::get<R>(m_tupleRow)[n-1]);
                            }
                        }(std::integral_constant<size_t, R2s + 2>{}), ...);
                    }(std::make_index_sequence<MAX_R - 1>{}));
                }
                [&, this]<size_t... R2s>(std::index_sequence<R2s...>){ // R2s = 0, 1, ..., MAX_R-2
                    ([&, this]<size_t R>(std::integral_constant<size_t, R>){
                        std::get<R>(m_tupleRow).update();
                    }(std::integral_constant<size_t, R2s + 2>{}), ...);
                }(std::make_index_sequence<MAX_R - 1>{});
            }

            template <size_t R>
            constexpr auto operator() (size_t n, std::integral_constant<size_t, R>) const noexcept
            {
                return std::get<R>(m_tupleRow)[n];
            }
            template <size_t R>
            constexpr decltype(auto) search(size_t n_first, size_t n_last, uintmax_t target, std::integral_constant<size_t, R> = {}) const noexcept
            {
                return std::get<R>(m_tupleRow).search(n_first, n_last, target);
            }
        };
        template <typename Tuint>
        inline constexpr nCr_table<std::numeric_limits<Tuint>::digits, std::min<std::size_t>(std::numeric_limits<Tuint>::digits / 2u, nCr_max_R(std::numeric_limits<Tuint>::digits))> nCr_LUT{};

        template <typename Tuint, size_t R>
        constexpr auto nCr_read(size_t n, std::integral_constant<size_t, R>) noexcept
        {
            constexpr size_t N = std::numeric_limits<Tuint>::digits;
            static_assert(R <= N, "nCr_read: R must be less than or equal to N");
            static_assert(std::min<size_t>(R, N-R) <= nCr_max_R(N), "nCr_read: R is too large");
            
            if constexpr (R > N / 2u)
            {
                return nCr_LUT<Tuint>(n, std::integral_constant<size_t, N - R>{});
            }
            else
            {
                return nCr_LUT<Tuint>(n, std::integral_constant<size_t, R>{});
            }
        }
        template <typename Tuint, size_t R>
        constexpr decltype(auto) nCr_search(size_t n_first, size_t n_last, uintmax_t target, std::integral_constant<size_t, R>) noexcept
        {
            constexpr size_t N = std::numeric_limits<Tuint>::digits;
            static_assert(R <= N, "nCr_search: R must be less than or equal to N");
            static_assert(std::min<size_t>(R, N-R) <= nCr_max_R(N), "nCr_search: R is too large");

            if constexpr (R > N / 2u)
            {
                return nCr_LUT<Tuint>.search(n_first, n_last, target, std::integral_constant<size_t, N - R>{});
            }
            else
            {
                return nCr_LUT<Tuint>.search(n_first, n_last, target, std::integral_constant<size_t, R>{});
            }
        }
    }
}
