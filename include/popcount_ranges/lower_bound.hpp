#pragma once

#include <algorithm>
#include <ranges>
#include <numeric>
#include <execution>

#include <data_par/simd_native.hpp>
#include <data_par/simd_range.hpp>

namespace jrmwng
{
    namespace popcount_ranges
    {
        template <typename T, typename Tpredicate>
        T * lower_bound(T * first, T * last, std::remove_cv_t<T> target, Tpredicate pred) noexcept
        {
            using type = std::remove_cv_t<T>;

            if constexpr (std::is_same_v<Tpredicate, std::greater<type>> || std::is_same_v<Tpredicate, std::ranges::greater>)
                return lower_bound(first, last, target, std::greater<>{});
            else if constexpr (std::is_same_v<Tpredicate, std::less<type>> || std::is_same_v<Tpredicate, std::ranges::less>)
                return lower_bound(first, last, target, std::less<>{});
            else
            {
                if (first == last)
                    return last;

                constexpr size_t LANES = (sizeof(T) == 1 ? 64u : 128u) / sizeof(T);

                if (!pred(*first, target))
                    return first;

                for (T * it = first; it != last; it += LANES) [[likely]]
                {
                    T * const next = std::min(it + LANES, last);

                    if (!pred(next[-1], target)) [[unlikely]]
                    {
                        #if 0
                        for (auto const &[simd_chunk, mask] : data_par::simd_short_range<type>(it, next)) [[likely]]
                        {
                            int const count = data_par::popcount(mask & pred(simd_chunk, target));
                            if (count == 0) [[unlikely]]
                                break;
                            it += count;
                        }
                        return it;
                        #else
                        constexpr auto LAMBDA = [](this auto&& self, auto itSimd, auto itEnd, type target, Tpredicate pred, auto OFFSET)
                        {
                            constexpr auto SIMDWIDTH = data_par::simd_range_traits<type>::simd_width;

                            if constexpr (OFFSET.value + SIMDWIDTH < LANES)
                            {
                                if (itSimd != itEnd) [[likely]]
                                {
                                    auto const [chunk, mask] = *itSimd;
                                    auto const count1 = self(std::move(++itSimd), itEnd, target, pred, std::integral_constant<size_t, OFFSET.value + SIMDWIDTH>{});
                                    auto const count0 = data_par::popcount(mask & (pred(chunk, target)));
                                    return count0 + count1;
                                }
                                else
                                    return 0;
                            }
                            else
                            {
                                if (itSimd != itEnd) [[likely]]
                                {
                                    auto const [chunk, mask] = *itSimd;
                                    return data_par::popcount(mask & (pred(chunk, target)));
                                }
                                else
                                    return 0;
                            }
                        };
                        data_par::simd_short_range<type> simd_rng(it, next);
                        return it + LAMBDA(simd_rng.begin(), simd_rng.end(), target, pred, std::integral_constant<size_t, 0>{});
                        #endif
                    }
                }
                return last;
            }
        }
        template <typename T, typename U, typename Tpredicate>
        std::enable_if_t<(std::is_integral_v<T> == std::is_integral_v<U> && std::is_signed_v<T> == std::is_signed_v<U> && sizeof(T) < sizeof(U)), T *> lower_bound(T * first, T * last, U target, Tpredicate pred) noexcept
        {
            if constexpr (std::is_same_v<Tpredicate, std::greater<>> || std::is_same_v<Tpredicate, std::greater<T>> || std::is_same_v<Tpredicate, std::ranges::greater>)
            {
                if (target < std::numeric_limits<T>::min())
                    return last;
                else
                    return lower_bound(first, last, static_cast<T>(std::min<U>(target, static_cast<U>(std::numeric_limits<T>::max()))), std::greater<>{});
            }
            else if constexpr (std::is_same_v<Tpredicate, std::less<>> || std::is_same_v<Tpredicate, std::less<T>> || std::is_same_v<Tpredicate, std::ranges::less>)
            {
                if (target > std::numeric_limits<T>::max())
                    return last;
                else
                    return lower_bound(first, last, static_cast<T>(std::max<U>(target, static_cast<U>(std::numeric_limits<T>::min()))), std::less<>{});
            }
            else
            {
                return std::lower_bound(first, last, target, pred);
            }
        }
    }
}