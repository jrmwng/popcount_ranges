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
                if (first == last) [[unlikely]]
                    return last;

                constexpr size_t LANES = (sizeof(T) == 1 ? 64u : 128u) / sizeof(T);

                if (!pred(*first, target)) [[unlikely]]
                    return first;

                for (T * it = first; it != last; ) [[likely]]
                {
                    T * const next = std::min(it + LANES, last);

                    if (!pred(next[-1], target)) [[unlikely]]
                    {
                        using traits = data_par::simd_range_traits<type>;

                        return it + []<size_t... Is>(auto simd_rng, auto target, auto pred, std::index_sequence<Is...>)->int {

                            // (chunk0, mask0), (chunk1, mask1), ...
                            std::tuple tuplePair(simd_rng[Is]...);

                            // (pred(chunk0, target), pred(chunk1, target), ...)
                            std::tuple tupleCompare(pred(std::get<typename traits::simd_type>(std::get<Is>(tuplePair)), target)...);

                            // (mask0 & pred(chunk0, target), mask1 & pred(chunk1, target), ...)
                            std::tuple tupleResult((std::get<typename traits::mask_type>(std::get<Is>(tuplePair)) & std::get<Is>(tupleCompare))...);
                            
                            // (reduce_count(mask0 & pred(chunk0, target)), reduce_count(mask1 & pred(chunk1, target)), ...)
                            std::tuple tuplePopcount(data_par::reduce_count(std::get<Is>(tupleResult))...);

                            // (reduce_count(mask0 & pred(chunk0, target)) + reduce_count(mask1 & pred(chunk1, target)) + ...)
                            return (std::get<Is>(tuplePopcount) + ...);

                        }(data_par::simd_short_range<type>(it, next), target, pred, std::make_index_sequence<(LANES + traits::simd_width - 1) / traits::simd_width>{});
                    }

                    it = next;
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