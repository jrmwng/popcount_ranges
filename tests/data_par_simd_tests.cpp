#include <data_par/simd_native.hpp>
#include <data_par/simd_range.hpp>

#include <concepts>
#include <cstddef>
#include <array>
#include <bitset>
#include <iostream>
#include <span>
#include <type_traits>

namespace
{
struct throwing_generator
{
    bool operator()(std::integral_constant<std::size_t, 0>) const
    {
        throw 0;
    }
};

template <typename T>
bool check_vec_and_mask()
{
    using vec_type = jrmwng::data_par::vec<T>;
    using mask_type = jrmwng::data_par::mask<T>;

    static_assert(std::same_as<typename vec_type::value_type, T>);
    static_assert(std::same_as<typename vec_type::mask_type, mask_type>);
    static_assert(std::same_as<typename mask_type::value_type, bool>);
    static_assert(vec_type::size() == mask_type::size());
    static_assert(noexcept(std::declval<vec_type const&>()[0]));
    static_assert(noexcept(std::declval<vec_type const&>() + std::declval<vec_type const&>()));
    static_assert(noexcept(std::declval<vec_type const&>() < std::declval<vec_type const&>()));
    static_assert(noexcept(std::declval<mask_type const&>()[0]));
    static_assert(noexcept(jrmwng::data_par::reduce_count(std::declval<mask_type const&>())));
    static_assert(noexcept(jrmwng::data_par::all_of(std::declval<mask_type const&>())));
    static_assert(noexcept(jrmwng::data_par::any_of(std::declval<mask_type const&>())));
    static_assert(noexcept(jrmwng::data_par::none_of(std::declval<mask_type const&>())));
    static_assert(!noexcept(mask_type(std::declval<throwing_generator>())));

    using short_range_type = jrmwng::data_par::simd_short_range<T>;
    static_assert(noexcept(std::declval<short_range_type const&>().begin()));
    static_assert(noexcept(std::declval<short_range_type const&>().end()));
    static_assert(noexcept(std::declval<short_range_type const&>().size()));

    vec_type const values([](auto index)
    {
        return static_cast<T>(index);
    });
    vec_type const limit(static_cast<T>(vec_type::size() / 2));
    mask_type const below_limit = values < limit;

    if (jrmwng::data_par::reduce_count(below_limit) != static_cast<int>(vec_type::size() / 2))
        return false;
    if (!jrmwng::data_par::all_of(mask_type(true)) ||
        !jrmwng::data_par::any_of(below_limit) ||
        !jrmwng::data_par::none_of(mask_type(false)))
        return false;
    if (jrmwng::data_par::reduce_min_index(below_limit) != 0 ||
        jrmwng::data_par::reduce_max_index(below_limit) != static_cast<std::ptrdiff_t>(vec_type::size() / 2 - 1))
        return false;

    for (std::size_t index = 0; index < vec_type::size(); ++index)
    {
        if (values[index] != static_cast<T>(index))
            return false;
        if (below_limit[index] != (index < vec_type::size() / 2))
            return false;
    }

    return true;
}

template <typename T>
bool check_range_mask()
{
    using traits = jrmwng::data_par::simd_range_traits<T>;
    using simd_type = typename traits::simd_type;
    using mask_type = typename traits::mask_type;

    auto const valid_bits = traits::make_bitset(0, traits::simd_width / 2);
    mask_type const valid_mask = traits::make_mask(valid_bits);
    if (jrmwng::data_par::reduce_count(valid_mask) !=
        static_cast<int>(traits::simd_width / 2))
        return false;

    simd_type const values([](auto index)
    {
        return static_cast<T>(index);
    });
    auto const comparison = values < simd_type(static_cast<T>(traits::simd_width / 2));
    auto const combined = valid_mask & comparison;
    if (jrmwng::data_par::reduce_count(combined) !=
        static_cast<int>(traits::simd_width / 2))
        return false;

    auto const complement = !valid_mask;
    return jrmwng::data_par::reduce_count(valid_mask | complement) ==
               static_cast<int>(traits::simd_width) &&
           jrmwng::data_par::reduce_count(valid_mask ^ complement) ==
               static_cast<int>(traits::simd_width);
}

template <typename T>
bool check_bitset_range_mask()
{
    using traits = jrmwng::data_par::simd_range_traits<T>;
    using abi_type = typename traits::simd_type::abi_type;
    using bitset_type = std::bitset<traits::simd_width>;
    using short_range_type =
        jrmwng::data_par::simd_short_range<T, abi_type, bitset_type>;
    using long_range_type =
        jrmwng::data_par::simd_range<T, abi_type, bitset_type>;
    using native_mask_type = typename traits::simd_type::mask_type;
    using native_short_range_type =
        jrmwng::data_par::simd_short_range<T, abi_type, native_mask_type>;
    using default_short_range_type = jrmwng::data_par::simd_short_range<T>;
    using default_long_range_type = jrmwng::data_par::simd_range<T>;

    static_assert(std::same_as<typename short_range_type::mask_type, bitset_type>);
    static_assert(std::same_as<typename long_range_type::mask_type, bitset_type>);
    static_assert(std::same_as<typename native_short_range_type::mask_type, native_mask_type>);

    constexpr std::size_t count = traits::simd_width + 2;
    std::array<T, count + 2> values{};
    auto const *first = values.data() + 1;
    auto const *last = first + count;

    short_range_type const short_range(first, last);
    std::size_t short_valid_count = 0;
    for (auto const &chunk : short_range)
        short_valid_count += std::get<1>(chunk).count();
    if (short_valid_count != count)
        return false;

    native_short_range_type const native_short_range(first, last);
    std::size_t native_short_valid_count = 0;
    for (auto const &chunk : native_short_range)
    {
        auto const &mask = std::get<1>(chunk);
        for (std::size_t lane = 0; lane < traits::simd_width; ++lane)
            native_short_valid_count += mask[lane] ? 1u : 0u;
    }
    if (native_short_valid_count != count)
        return false;

    default_short_range_type const default_short_range(first, last);
    std::size_t default_short_valid_count = 0;
    for (auto const &chunk : default_short_range)
        default_short_valid_count +=
            jrmwng::data_par::reduce_count(std::get<1>(chunk));
    if (default_short_valid_count != count)
        return false;

    long_range_type const long_range(std::span<T const>(first, count));
    std::size_t long_valid_count = 0;
    for (auto const &chunk : long_range)
        long_valid_count += chunk.second.count();
    if (long_valid_count != count)
        return false;

    default_long_range_type const default_long_range(std::span<T const>(first, count));
    std::size_t default_long_valid_count = 0;
    for (auto const &chunk : default_long_range)
        default_long_valid_count +=
            jrmwng::data_par::reduce_count(chunk.second);
    return default_long_valid_count == count;
}
}

int main()
{
    bool const passed = check_vec_and_mask<int>() && check_vec_and_mask<float>() &&
                        check_range_mask<std::int32_t>() &&
                        check_range_mask<std::uint64_t>() &&
                        check_bitset_range_mask<std::int32_t>() &&
                        check_bitset_range_mask<std::uint64_t>();
    if (!passed)
    {
        std::cerr << "data_par SIMD API test failed\n";
        return 1;
    }

    std::cout << "data_par SIMD API test passed\n";
    return 0;
}
