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

#include <data_par/simd_range.hpp>

namespace jrmwng
{
    namespace popcount_ranges
    {
        enum class binary_search_algorithm
        {
            none,
            branchless,
            branched,
        };

        template <typename T>
        struct lower_bound_traits
        {
            using type = std::remove_cv_t<T>;

            inline constexpr static binary_search_algorithm BINARY_SEARCH_ALGORITHM = binary_search_algorithm::none;
            inline constexpr static std::size_t BINARY_SEARCH_THRESHOLD = []
            {
                if constexpr (std::is_same_v<type, std::uint8_t>)
                    return std::size_t{2048};
                else if constexpr (std::is_same_v<type, std::uint16_t>)
                    return std::size_t{2048};
                else if constexpr (std::is_same_v<type, std::uint32_t>)
                    return std::size_t{1024};
                else if constexpr (std::is_same_v<type, std::uint64_t>)
                    return std::size_t{1024};
                else if constexpr (std::is_same_v<type, float>)
                    return std::size_t{1024};
                else if constexpr (std::is_same_v<type, double>)
                    return std::size_t{1024};
                else
                    return std::size_t{32u * 64u / sizeof(T)};
            }();

            inline constexpr static std::array<std::size_t, 14> BINARY_SEARCH_THRESHOLDS = []
            {
                if constexpr (std::is_same_v<type, std::uint8_t>)
                    return std::array<std::size_t, 14>{32, 32, 64, 128, 256, 512, 1024, 1024, 2048, 4096, 4096, 4096, 8192, 8192};
                else if constexpr (std::is_same_v<type, std::uint16_t>)
                    return std::array<std::size_t, 14>{16, 32, 64, 128, 256, 512, 512, 1024, 1024, 4096, 2048, 4096, 4096, 4096};
                else if constexpr (std::is_same_v<type, std::uint32_t>)
                    return std::array<std::size_t, 14>{16, 32, 64, 128, 256, 256, 1024, 1024, 1024, 1024, 2048, 4096, 4096, 4096};
                else if constexpr (std::is_same_v<type, std::uint64_t>)
                    return std::array<std::size_t, 14>{16, 32, 64, 128, 256, 128, 256, 512, 512, 1024, 2048, 2048, 2048, 16384};
                else if constexpr (std::is_same_v<type, float>)
                    return std::array<std::size_t, 14>{16, 32, 64, 128, 256, 512, 512, 512, 1024, 1024, 2048, 4096, 2048, 4096};
                else if constexpr (std::is_same_v<type, double>)
                    return std::array<std::size_t, 14>{16, 32, 64, 128, 128, 256, 256, 512, 512, 1024, 2048, 2048, 2048, 4096};
                else
                {
                    std::array<std::size_t, 14> thresholds{};
                    thresholds.fill(BINARY_SEARCH_THRESHOLD);
                    return thresholds;
                }
            }();

            inline constexpr static std::array<std::size_t, 14> SEQUENTIAL_SEARCH_PITCHES = []
            {
                if constexpr (std::is_same_v<type, std::uint8_t>)
                    return std::array<std::size_t, 14>{32, 32, 32, 32, 64, 128, 128, 128, 256, 256, 512, 512, 1024, 1024};
                else if constexpr (std::is_same_v<type, std::uint16_t>)
                    return std::array<std::size_t, 14>{16, 16, 16, 32, 64, 64, 64, 128, 128, 256, 256, 512, 512, 1024};
                else if constexpr (std::is_same_v<type, std::uint32_t>)
                    return std::array<std::size_t, 14>{8, 8, 16, 32, 32, 32, 64, 64, 128, 128, 256, 512, 512, 512};
                else if constexpr (std::is_same_v<type, std::uint64_t>)
                    return std::array<std::size_t, 14>{4, 8, 16, 16, 32, 32, 32, 64, 64, 128, 256, 256, 256, 1024};
                else if constexpr (std::is_same_v<type, float>)
                    return std::array<std::size_t, 14>{8, 8, 16, 32, 32, 64, 64, 64, 128, 128, 256, 512, 512, 512};
                else if constexpr (std::is_same_v<type, double>)
                    return std::array<std::size_t, 14>{4, 8, 16, 16, 16, 32, 32, 64, 64, 128, 256, 256, 256, 1024};
                else
                {
                    std::array<std::size_t, 14> pitches{};
                    pitches.fill(128u / sizeof(T));
                    return pitches;
                }
            }();

            inline constexpr static std::size_t SEQUENTIAL_SEARCH_PITCH = 128u / sizeof(T);

            static constexpr std::size_t range_bucket_index(std::size_t value) noexcept
            {
                if (value <= 16)
                    return 0;
                if (value <= 8196 && value > 4096)
                    return 9;

                std::size_t const index = static_cast<std::size_t>(std::bit_width(value - 1)) - 4;
                return std::min(index, std::size_t{13});
            }

            static constexpr std::size_t binary_search_threshold(std::size_t range_size) noexcept
            {
                return BINARY_SEARCH_THRESHOLDS[range_bucket_index(range_size)];
            }

            static constexpr std::size_t sequential_search_pitch(std::size_t binary_search_threshold) noexcept
            {
                return SEQUENTIAL_SEARCH_PITCHES[range_bucket_index(binary_search_threshold)];
            }
        };
        template <typename T, typename Tpredicate, typename Ttraits = lower_bound_traits<T>>
        T * lower_bound(T * first, T * last, std::remove_cv_t<T> target, Tpredicate pred, Ttraits = Ttraits{}) noexcept(noexcept(pred(*first, target)))
        {
            #ifdef POPCOUNT_RANGES_LOWER_BOUND_STD
            return std::lower_bound(first, last, target, pred);
            #else
            using type = std::remove_cv_t<T>;

            if constexpr (std::is_same_v<Tpredicate, std::greater<type>> || std::is_same_v<Tpredicate, std::ranges::greater>)
                return lower_bound(first, last, target, std::greater<>{}, Ttraits{});
            else if constexpr (std::is_same_v<Tpredicate, std::less<type>> || std::is_same_v<Tpredicate, std::ranges::less>)
                return lower_bound(first, last, target, std::less<>{}, Ttraits{});
            else
            {
                if (first == last) [[unlikely]] // empty domain
                    return last;

                if (!pred(*first, target)) [[unlikely]] // Elements from `first` satisfy the condition
                    return first;
                if (pred(*(last - 1), target)) [[unlikely]] // Elements before `last` do not satisfy the condition
                    return last;

                std::size_t const binary_search_threshold = [&]
                {
                    if constexpr (std::is_same_v<Ttraits, lower_bound_traits<T>>)
                        return Ttraits::binary_search_threshold(static_cast<std::size_t>(last - first));
                    else if constexpr (requires { Ttraits::BINARY_SEARCH_THRESHOLD; })
                        return Ttraits::BINARY_SEARCH_THRESHOLD;
                    else if constexpr (requires { Ttraits::binary_search_threshold(std::size_t{}); })
                        return Ttraits::binary_search_threshold(static_cast<std::size_t>(last - first));
                    else if constexpr (requires { Ttraits::BINARY_SEARCH_THRESHOLD; })
                        return Ttraits::BINARY_SEARCH_THRESHOLD;
                    else
                        return std::size_t{};
                }();

                std::size_t const sequential_search_pitch = [&]
                {
                    if constexpr (!std::is_same_v<Ttraits, lower_bound_traits<T>> && requires { Ttraits::SEQUENTIAL_SEARCH_PITCH; })
                        return Ttraits::SEQUENTIAL_SEARCH_PITCH;
                    else if constexpr (requires { Ttraits::sequential_search_pitch(std::size_t{}); })
                        return Ttraits::sequential_search_pitch(binary_search_threshold);
                    else
                        return Ttraits::SEQUENTIAL_SEARCH_PITCH;
                }();

                // 1. Coarse-grained binary search using branchless or branched approach depending on traits

                if constexpr (Ttraits::BINARY_SEARCH_ALGORITHM != binary_search_algorithm::none)
                {
                    if constexpr (Ttraits::BINARY_SEARCH_ALGORITHM == binary_search_algorithm::branchless)
                    {
                        std::ptrdiff_t count = last - first;
                        while (count > binary_search_threshold) [[likely]]
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
                    else if constexpr (Ttraits::BINARY_SEARCH_ALGORITHM == binary_search_algorithm::branched)
                    {
                        while (last - first > binary_search_threshold) [[likely]]
                        {
                            T * const mid = first + (last - first) / 2;
                            if (pred(*mid, target))
                                first = mid + 1;
                            else
                                last = mid;
                        }
                    }
                }

                // 2. Coarse-grained sequential search using the last element of each chunk
 
                for (T * it = first; it != last; ) [[likely]]
                {
                    T * const next = std::min(it + sequential_search_pitch, last);

                    if (!pred(next[-1], target)) [[unlikely]]
                    {
                        // 3. Fine-grained SIMD-based search within the identified chunk in a single pass (fully unrolled)

                        using Tsimd_traits = data_par::simd_range_traits<type, data_par::simd_bitset_mask<type>>;
                        using mask_type = typename Tsimd_traits::mask_type;
                        using range_type = typename Tsimd_traits::short_type;
                        using simd_type = typename Tsimd_traits::simd_type;

                        auto const simd_find = [&]<std::size_t Pitch>() -> T *
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

                            }(range_type(it, next), target, pred, std::make_index_sequence<(Pitch + Tsimd_traits::simd_width - 1) / Tsimd_traits::simd_width>{});
                        };

                        if constexpr (requires
                        {
                            Ttraits::sequential_search_pitch(std::size_t{});
                        })
                        {
                            switch (sequential_search_pitch)
                            {
                            case 1: return simd_find.template operator()<1>();
                            case 2: return simd_find.template operator()<2>();
                            case 4: return simd_find.template operator()<4>();
                            case 8: return simd_find.template operator()<8>();
                            case 16: return simd_find.template operator()<16>();
                            case 32: return simd_find.template operator()<32>();
                            case 64: return simd_find.template operator()<64>();
                            case 128: return simd_find.template operator()<128>();
                            case 256: return simd_find.template operator()<256>();
                            case 512: return simd_find.template operator()<512>();
                            case 1024: return simd_find.template operator()<1024>();
                            default: std::terminate();
                            }
                        }
                        else
                            return simd_find.template operator()<
                                Ttraits::SEQUENTIAL_SEARCH_PITCH>();
                    }

                    it = next;
                }
                return last;
            }
            #endif
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