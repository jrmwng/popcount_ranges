#pragma once

#include <algorithm>
#include <ranges>
#include <numeric>
#include <execution>
#include <array>
#include <bitset>

#include <data_par/simd_range.hpp>

namespace jrmwng
{
    namespace popcount_ranges
    {
        template <typename T, typename Tpredicate>
        T * lower_bound(T * first, T * last, std::remove_cv_t<T> target, Tpredicate pred) noexcept(noexcept(pred(*first, target)))
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
                        using default_traits = data_par::simd_range_traits<type>;
                        using mask_type = std::bitset<default_traits::simd_width>;
                        using traits = data_par::simd_range_traits<
                            type, typename default_traits::simd_type::abi_type, mask_type>;
                        using range_type = typename traits::short_type;
                        using simd_type = typename traits::simd_type;

                        return it + []<size_t... Is>(auto simd_rng, auto target, auto pred, std::index_sequence<Is...>)->int {

                            // (chunk0, mask0), (chunk1, mask1), ...
                            std::tuple tuplePair(simd_rng[Is]...);

                            // (pred(chunk0, target), pred(chunk1, target), ...)
                            std::tuple tupleCompare(traits::mask_to_bitset(
                                pred(std::get<simd_type>(std::get<Is>(tuplePair)), target))...);

                            // (mask0 & pred(chunk0, target), mask1 & pred(chunk1, target), ...)
                            std::tuple tupleResult((std::get<mask_type>(
                                std::get<Is>(tuplePair)) & std::get<Is>(tupleCompare))...);
                            
                            // (reduce_count(mask0 & pred(chunk0, target)), reduce_count(mask1 & pred(chunk1, target)), ...)
                            std::tuple tuplePopcount(traits::reduce_count(std::get<Is>(tupleResult))...);

                            // (reduce_count(mask0 & pred(chunk0, target)) + reduce_count(mask1 & pred(chunk1, target)) + ...)
                            return (std::get<Is>(tuplePopcount) + ...);

                        }(range_type(it, next),
                          target, pred, std::make_index_sequence<(LANES + traits::simd_width - 1) / traits::simd_width>{});
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

        template <typename Trange>
        struct lower_bound_descent;

        template <typename T, std::size_t N>
        struct lower_bound_descent<std::array<T, N>>
        {
            static_assert(N > 0, "Array size must be greater than 0.");
            static_assert(sizeof(T) <= 8, "Type T must be at most 8 bytes.");

            #ifndef POPCOUNT_RANGES_LOWER_BOUND_THRESHOLD
            #define POPCOUNT_RANGES_LOWER_BOUND_THRESHOLD 4u
            #endif
            inline constexpr static std::size_t N0 = (64u * POPCOUNT_RANGES_LOWER_BOUND_THRESHOLD);
            inline constexpr static std::size_t N1 = (N0 / sizeof(T));
            inline constexpr static std::size_t N2 = ((N + N1 - 1) / N1);

            std::array<T, N> const &range;
            std::array<T, N2> chunks;

            std::conditional_t<(N2 > 1), lower_bound_descent<std::array<T, N2>>, std::array<T, N2>&> next;

            constexpr lower_bound_descent(std::array<T, N> const &r) noexcept
                : range(r), chunks{}, next(chunks)
            {
            }
            constexpr lower_bound_descent(lower_bound_descent const &) = delete;
            constexpr lower_bound_descent &operator=(lower_bound_descent const &) = delete;
            constexpr lower_bound_descent(lower_bound_descent &&) = delete;
            constexpr lower_bound_descent &operator=(lower_bound_descent &&) = delete;

            constexpr void update()
            {
                for (std::size_t i = 0, j = 1; j < N2; ++i, ++j)
                {
                    chunks[i] = range[j * N1 - 1u];
                }
                chunks[N2-1u] = range[N - 1u];
                if constexpr (N2 > 1)
                {
                    next.update();
                }
            }

            template <typename Tpredicate>
            std::size_t operator() (std::size_t first, std::size_t last, T target, Tpredicate pred) const noexcept(noexcept(lower_bound(range.data() + first, range.data() + last, target, pred)))
            {
                if constexpr (N2 > 1)
                if (last - first > N1) [[unlikely]]
                {
                    std::size_t const next_first = first / N1;
                    std::size_t const next_last = (last + N1 - 1) / N1;

                    std::size_t const next_chunk = next(next_first, next_last, target, pred);
                    if (next_chunk == next_last) [[unlikely]]
                        return last;

                    first = std::max(first, next_chunk * N1);    
                    last = std::min(last, next_chunk * N1 + N1);
                }    

                return static_cast<std::size_t>(lower_bound(range.data() + first, range.data() + last, target, pred) - range.data());
            }
        };
    }
}