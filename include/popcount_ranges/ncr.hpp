#pragma once

#include <cstdint>
#include <limits>
#include <type_traits>
#include <tuple>
#include <utility>

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
    }
}
