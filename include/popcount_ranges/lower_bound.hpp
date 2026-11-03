#pragma once

#include <algorithm>
#include <bit>
#include <cstddef>
#include <ranges>
#include <numeric>
#include <execution>
#include <array>
#include <bitset>
#include <cstdint>
#include <exception>
#include <type_traits>
#include <utility>

#include <data_par/simd_range.hpp>

namespace jrmwng
{
    namespace popcount_ranges
    {
        enum class binary_search_algorithm
        {
            none,
            // `std::lower_bound` algorithm
            std,
            // Clang version: compiles to branchless assembly codes
            branchless,
            // MSVC version: compiles to branched assembly codes
            branched,
        };

        template <typename T>
        struct lower_bound_traits
        {
            using type = std::remove_cv_t<T>;

            inline constexpr static binary_search_algorithm BINARY_SEARCH_ALGORITHM = []()
            {
                if constexpr (sizeof(T) == 1)
                    return binary_search_algorithm::branched;
                else if constexpr (sizeof(T) == 2)
                    return binary_search_algorithm::branchless;
                else
                    return binary_search_algorithm::std;
            }();

            // Lookup parameters for the lower bound algorithm
            // Depending on the traits implementation, the lookup parameters may be compile-time constants or run-time variables
            //
            // Compile-time constants: std::integral_constant<std::uint*_t, ...>
            // Run-time variables: std::uint*_t
            template <typename U>
            constexpr static auto lookup_params(U *first, U *last) noexcept
            {
                static_assert(sizeof(T) == sizeof(U));
                (void)first;
                (void)last;

                constexpr std::integral_constant<std::uint_fast16_t, 64u / sizeof(T)> SEQUENTIAL_SEARCH_PITCH{};
                constexpr std::integral_constant<std::uint_fast16_t, 128u / sizeof(T)> BINARY_SEARCH_THRESHOLD{};

                return std::tuple
                (
                    SEQUENTIAL_SEARCH_PITCH,
                    BINARY_SEARCH_THRESHOLD
                );
            }
        };
        template <typename T, typename Tpredicate, typename Ttraits = lower_bound_traits<T>>
        T * lower_bound(T * first, T * last, std::remove_cv_t<T> target, Tpredicate pred, Ttraits = Ttraits{}) noexcept(noexcept(pred(*first, target)))
        {
            if constexpr (requires { Ttraits::BINARY_SEARCH_ALGORITHM; })
            if constexpr (Ttraits::BINARY_SEARCH_ALGORITHM == binary_search_algorithm::std)
                return std::lower_bound(first, last, target, pred);

            using type = std::remove_cv_t<T>;

            if constexpr (std::is_same_v<Tpredicate, std::greater<type>> || std::is_same_v<Tpredicate, std::ranges::greater>)
                return lower_bound(first, last, target, std::greater<>{}, Ttraits{});
            else if constexpr (std::is_same_v<Tpredicate, std::less<type>> || std::is_same_v<Tpredicate, std::ranges::less>)
                return lower_bound(first, last, target, std::less<>{}, Ttraits{});
            else
            {
                if (first == last) [[unlikely]] // empty domain
                    return last;

                auto const [SEQUENTIAL_SEARCH_PITCH, BINARY_SEARCH_THRESHOLD] = Ttraits::lookup_params(first, last);

                // 1. Coarse-grained binary search using branchless or branched approach depending on traits

                if (last - first > BINARY_SEARCH_THRESHOLD) [[unlikely]]
                {
                    if (!pred(*first, target)) [[unlikely]] // Elements from `first` satisfy the condition
                        return first;
                    if (pred(*(last - 1), target)) [[unlikely]] // Elements before `last` do not satisfy the condition
                        return last;

                    if constexpr (requires { Ttraits::BINARY_SEARCH_ALGORITHM; })
                    {
                        if constexpr (Ttraits::BINARY_SEARCH_ALGORITHM == binary_search_algorithm::branchless)
                        {
                            std::ptrdiff_t count = last - first;
                            while (count > BINARY_SEARCH_THRESHOLD) [[likely]]
                            {
                                std::ptrdiff_t mid = count / 2;
                                if (pred(first[mid], target))
                                {
                                    first += mid + 1;
                                    count -= mid + 1;
                                }
                                else
                                {
                                    count = mid;
                                }
                            }
                            last = first + count;
                        }
                        if constexpr (Ttraits::BINARY_SEARCH_ALGORITHM == binary_search_algorithm::branched)
                        {
                            while (last - first > BINARY_SEARCH_THRESHOLD) [[likely]]
                            {
                                T * const mid = first + (last - first) / 2;
                                if (pred(*mid, target))
                                    first = mid + 1;
                                else
                                    last = mid;
                            }
                        }
                    }
                }

                // 2. Coarse-grained sequential search using the last element of each chunk
 
                for (T * it = first; it != last; ) [[likely]]
                {
                    T * const next = std::min(it + SEQUENTIAL_SEARCH_PITCH, last);

                    if (!pred(next[-1], target)) [[unlikely]]
                    {
                        using Tsimd_traits = data_par::simd_range_traits<type, data_par::simd_bitset_mask<type>>;
                        using range_type = typename Tsimd_traits::short_type;
                        using simd_type = typename Tsimd_traits::simd_type;
                        using mask_type = typename Tsimd_traits::mask_type;

                        // 3. Fine-grained SIMD-based search within the identified chunk in a single pass (fully unrolled)

                        if constexpr (std::is_integral_v<std::remove_cvref_t<decltype(SEQUENTIAL_SEARCH_PITCH)>>)
                        {
                            range_type const simd_range(it, next);

                            int offset = 0;
                            for (std::size_t i = 0; i < simd_range.size(); ++i) [[likely]]
                            {
                                auto const [vec, msk] = simd_range[i];

                                offset += Tsimd_traits::reduce_count(msk & Tsimd_traits::mask_to_bitset(pred(vec, target)));
                            }
                            return it + offset;
                        }
                        else
                        {
                            return it + []<std::size_t... Is>(auto simd_rng, auto target, auto pred, std::index_sequence<Is...>) -> int
                            {
                                // (chunk0, mask0), (chunk1, mask1), ...
                                auto const tuplePair = std::make_tuple(simd_rng[Is]...);

                                // (pred(chunk0, target), pred(chunk1, target), ...)
                                std::tuple const tupleCompare(Tsimd_traits::mask_to_bitset(pred(std::get<simd_type>(std::get<Is>(tuplePair)), target))...);

                                // (mask0 & pred(chunk0, target), mask1 & pred(chunk1, target), ...)
                                std::tuple const tupleResult((std::get<mask_type>(std::get<Is>(tuplePair)) & std::get<Is>(tupleCompare))...);

                                // (reduce_count(mask0 & pred(chunk0, target)), reduce_count(mask1 & pred(chunk1, target)), ...)
                                std::tuple const tuplePopcount(Tsimd_traits::reduce_count(std::get<Is>(tupleResult))...);

                                // (reduce_count(mask0 & pred(chunk0, target)) + reduce_count(mask1 & pred(chunk1, target)) + ...)
                                return (std::get<Is>(tuplePopcount) + ...);

                            }(range_type(it, next), target, pred, std::make_index_sequence<(SEQUENTIAL_SEARCH_PITCH + Tsimd_traits::simd_width - 1) / Tsimd_traits::simd_width>{});
                        }
                    }

                    it = next;
                }
                return last;
            }
        }
        template <typename T, typename U, typename Tpredicate, typename Ttraits = lower_bound_traits<T>>
        std::enable_if_t<(std::is_integral_v<T> == std::is_integral_v<U> && std::is_signed_v<T> == std::is_signed_v<U> && sizeof(T) < sizeof(U)), T *> lower_bound(T * first, T * last, U target, Tpredicate pred, Ttraits = lower_bound_traits<T>{}) noexcept
        {
            if constexpr (std::is_same_v<Tpredicate, std::greater<>> || std::is_same_v<Tpredicate, std::greater<T>> || std::is_same_v<Tpredicate, std::ranges::greater>)
            {
                if (target < std::numeric_limits<T>::min())
                    return last;
                else
                    return lower_bound(first, last, static_cast<T>(std::min<U>(target, static_cast<U>(std::numeric_limits<T>::max()))), std::greater<>{}, Ttraits{});
            }
            else if constexpr (std::is_same_v<Tpredicate, std::less<>> || std::is_same_v<Tpredicate, std::less<T>> || std::is_same_v<Tpredicate, std::ranges::less>)
            {
                if (target > std::numeric_limits<T>::max())
                    return last;
                else
                    return lower_bound(first, last, static_cast<T>(std::max<U>(target, static_cast<U>(std::numeric_limits<T>::min()))), std::less<>{}, Ttraits{});
            }
            else
            {
                return std::lower_bound(first, last, target, pred);
            }
        }

        template <typename Trange, std::size_t THRESHOLD = 32u>
        struct lower_bound_descent;

        template <typename T, std::size_t N, std::size_t THRESHOLD>
        struct lower_bound_descent<std::array<T, N>, THRESHOLD>
        {
            static_assert(THRESHOLD > 0, "THRESHOLD must be greater than 0.");
            static_assert(N > 0, "Array size must be greater than 0.");
            static_assert(sizeof(T) <= 8, "Type T must be at most 8 bytes.");

            inline constexpr static std::size_t N0 = (64u * THRESHOLD);
            inline constexpr static std::size_t N1 = (N0 / sizeof(T));
            inline constexpr static std::size_t N2 = ((N + N1 - 1) / N1);

            std::array<T, N> const &range;
            std::array<T, N2> chunks;

            std::conditional_t<(N2 > 1), lower_bound_descent<std::array<T, N2>, THRESHOLD>, std::array<T, N2>&> next;

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
                    chunks[i] = range[j * N1 - 1u];
                chunks[N2-1u] = range[N - 1u];

                if constexpr (N2 > 1)
                    next.update();
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