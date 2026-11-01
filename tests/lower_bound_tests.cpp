#include <popcount_ranges/lower_bound.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <random>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
template <
    typename T,
    typename Predicate,
    typename Traits = jrmwng::popcount_ranges::lower_bound_traits<T>>
bool check_case(
    std::vector<T> const& values, T target, Predicate predicate,
    char const* name, Traits traits = {})
{
    auto const expected = std::lower_bound(values.begin(), values.end(), target, predicate);
    auto const actual = jrmwng::popcount_ranges::lower_bound(
        values.data(), values.data() + values.size(), target, predicate, traits);
    auto const expected_index = values.empty() ? 0 : expected - values.begin();
    auto const actual_index = values.empty() ? 0 : actual - values.data();

    if (values.empty() ? actual != values.data() : actual_index != expected_index)
    {
        std::cerr << name << ": target=" << static_cast<long long>(target)
                  << ", expected=" << expected_index << ", actual=" << actual_index << '\n';
        return false;
    }

    auto const count = std::count_if(values.begin(), values.end(), [&](T value)
    {
        return predicate(value, target);
    });
    if (count != expected_index)
    {
        std::cerr << name << ": oracle count mismatch\n";
        return false;
    }
    return true;
}

template <typename T>
std::vector<T> make_ascending(std::size_t size)
{
    std::vector<T> values(size);
    auto const start = [&]
    {
        if constexpr (std::is_signed_v<T>)
            return static_cast<T>(-static_cast<long long>(size / 3));
        else
            return T{};
    }();
    for (std::size_t index = 0; index < size; ++index)
        values[index] = static_cast<T>(start + static_cast<T>(index / 3));
    return values;
}

template <typename T>
bool check_width(char const* name)
{
    constexpr std::size_t lanes = (sizeof(T) == 1 ? 64u : 128u) / sizeof(T);
    std::array<std::size_t, 11> sizes{
        0, 1, lanes - 1, lanes, lanes + 1, 2 * lanes - 1,
        2 * lanes, 2 * lanes + 1, 3 * lanes + 3, 64, 257};

    for (std::size_t size : sizes)
    {
        auto ascending = make_ascending<T>(size);
        auto descending = ascending;
        std::reverse(descending.begin(), descending.end());

        std::array<T, 7> targets{
            std::numeric_limits<T>::lowest(),
            static_cast<T>(-1),
            static_cast<T>(0),
            static_cast<T>(1),
            static_cast<T>(size / 3),
            std::numeric_limits<T>::max(),
            static_cast<T>(ascending.empty() ? 0 : ascending.back())};

        for (T target : targets)
        {
            if (!check_case(ascending, target, std::less<>{}, name) ||
                !check_case(descending, target, std::greater<>{}, name))
                return false;

            auto const less_lambda = [](auto value, auto query) { return value < query; };
            auto const greater_lambda = [](auto value, auto query) { return value > query; };
            if (!check_case(ascending, target, less_lambda, name) ||
                !check_case(descending, target, greater_lambda, name))
                return false;
        }
    }
    return true;
}

template <typename T>
bool check_random(char const* name)
{
    std::mt19937_64 generator(0x5eeda11u + sizeof(T));
    std::uniform_int_distribution<std::uint64_t> distribution;
    std::vector<T> values(4097);
    for (T& value : values)
        value = static_cast<T>(distribution(generator));
    std::sort(values.begin(), values.end());

    for (std::size_t index = 0; index < 1000; ++index)
    {
        T target = static_cast<T>(distribution(generator));
        if (!check_case(values, target, std::less<>{}, name))
            return false;
    }
    return true;
}

template <typename T, typename U>
bool check_wider_target()
{
    std::vector<T> values{-100, -10, 0, 10, 100};
    auto descending = values;
    std::reverse(descending.begin(), descending.end());
    for (U target : {static_cast<U>(-1000), static_cast<U>(-10), static_cast<U>(5), static_cast<U>(1000)})
    {
        auto const ascending_actual = jrmwng::popcount_ranges::lower_bound(
            values.data(), values.data() + values.size(), target, std::less<>{});
        auto const ascending_expected = std::lower_bound(
            values.begin(), values.end(), target, std::less<>{});
        auto const descending_actual = jrmwng::popcount_ranges::lower_bound(
            descending.data(), descending.data() + descending.size(),
            target, std::greater<>{});
        auto const descending_expected = std::lower_bound(
            descending.begin(), descending.end(), target, std::greater<>{});
        if (ascending_actual != values.data() + (ascending_expected - values.begin()) ||
            descending_actual != descending.data() +
                (descending_expected - descending.begin()))
            return false;
    }
    return true;
}

template <typename T>
struct branchless_traits : jrmwng::popcount_ranges::lower_bound_traits<T>
{
    inline constexpr static jrmwng::popcount_ranges::binary_search_algorithm
        BINARY_SEARCH_ALGORITHM =
            jrmwng::popcount_ranges::binary_search_algorithm::branchless;
    inline constexpr static std::size_t BINARY_SEARCH_THRESHOLD = 16;
};

template <typename T>
struct branched_traits
{
    inline constexpr static jrmwng::popcount_ranges::binary_search_algorithm
        BINARY_SEARCH_ALGORITHM =
            jrmwng::popcount_ranges::binary_search_algorithm::branched;
    inline constexpr static std::size_t BINARY_SEARCH_THRESHOLD = 16;
    inline constexpr static std::size_t SEQUENTIAL_SEARCH_PITCH =
        128u / sizeof(T);
};

template <typename T>
struct threshold_one_branchless_traits
{
    inline constexpr static jrmwng::popcount_ranges::binary_search_algorithm
        BINARY_SEARCH_ALGORITHM =
            jrmwng::popcount_ranges::binary_search_algorithm::branchless;
    inline constexpr static std::size_t BINARY_SEARCH_THRESHOLD = 1;
    inline constexpr static std::size_t SEQUENTIAL_SEARCH_PITCH =
        128u / sizeof(T);
};

template <typename T>
struct threshold_one_branched_traits
{
    inline constexpr static jrmwng::popcount_ranges::binary_search_algorithm
        BINARY_SEARCH_ALGORITHM =
            jrmwng::popcount_ranges::binary_search_algorithm::branched;
    inline constexpr static std::size_t BINARY_SEARCH_THRESHOLD = 1;
    inline constexpr static std::size_t SEQUENTIAL_SEARCH_PITCH =
        128u / sizeof(T);
};

template <std::size_t Pitch>
struct pitch_only_traits
{
    inline constexpr static jrmwng::popcount_ranges::binary_search_algorithm
        BINARY_SEARCH_ALGORITHM =
            jrmwng::popcount_ranges::binary_search_algorithm::none;
    inline constexpr static std::size_t SEQUENTIAL_SEARCH_PITCH = Pitch;
};

template <typename Traits>
bool check_custom_traits(char const* name, Traits traits)
{
    auto ascending = make_ascending<std::int32_t>(4097);
    auto descending = ascending;
    std::reverse(descending.begin(), descending.end());

    for (std::int32_t target : {
             std::numeric_limits<std::int32_t>::lowest(),
             -1, 0, 1, 512, 1024,
             std::numeric_limits<std::int32_t>::max()})
    {
        if (!check_case(ascending, target, std::less<>{}, name, traits) ||
            !check_case(descending, target, std::greater<>{}, name, traits))
            return false;
    }
    return true;
}

bool check_threshold_one_end_position()
{
    std::vector<std::uint32_t> values{1, 3, 5, 7};
    auto const target = std::numeric_limits<std::uint32_t>::max();
    auto const expected = values.data() + values.size();
    auto const branchless = jrmwng::popcount_ranges::lower_bound(
        values.data(), expected, target, std::less<>{},
        threshold_one_branchless_traits<std::uint32_t>{});
    auto const branched = jrmwng::popcount_ranges::lower_bound(
        values.data(), expected, target, std::less<>{},
        threshold_one_branched_traits<std::uint32_t>{});
    return branchless == expected && branched == expected;
}

template <typename T, std::size_t Pitch>
bool check_pitch_variant(char const* name)
{
    auto ascending = make_ascending<T>(257);
    auto descending = ascending;
    std::reverse(descending.begin(), descending.end());
    for (T target : {
             std::numeric_limits<T>::lowest(), static_cast<T>(0),
             static_cast<T>(1), std::numeric_limits<T>::max()})
        if (!check_case(
                ascending, target, std::less<>{}, name,
                pitch_only_traits<Pitch>{}) ||
            !check_case(
                descending, target, std::greater<>{}, name,
                pitch_only_traits<Pitch>{}))
            return false;
    return true;
}

template <typename T>
bool check_custom_pitches(char const* name)
{
    return check_pitch_variant<T, 1>(name) &&
           check_pitch_variant<T, 4>(name) &&
           check_pitch_variant<T, 16>(name) &&
           check_pitch_variant<T, 128>(name);
}
}

int main()
{
    bool passed = true;
    passed = passed && check_width<std::int8_t>("int8_t");
    passed = passed && check_width<std::uint8_t>("uint8_t");
    passed = passed && check_width<std::int16_t>("int16_t");
    passed = passed && check_width<std::uint16_t>("uint16_t");
    passed = passed && check_width<std::int32_t>("int32_t");
    passed = passed && check_width<std::uint32_t>("uint32_t");
    passed = passed && check_width<std::int64_t>("int64_t");
    passed = passed && check_width<std::uint64_t>("uint64_t");
    passed = passed && check_random<std::int32_t>("random int32_t");
    passed = passed && check_random<std::uint64_t>("random uint64_t");
    passed = passed && check_wider_target<std::int8_t, std::int16_t>();
    passed = passed && check_wider_target<std::int16_t, std::int32_t>();
    passed = passed && check_wider_target<std::int32_t, std::int64_t>();
    passed = passed && check_custom_traits(
        "custom branchless traits", branchless_traits<std::int32_t>{});
    passed = passed && check_custom_traits(
        "custom branched traits", branched_traits<std::int32_t>{});
    passed = passed && check_threshold_one_end_position();
    passed = passed && check_custom_pitches<std::uint8_t>("uint8_t pitches");
    passed = passed && check_custom_pitches<std::uint16_t>("uint16_t pitches");
    passed = passed && check_custom_pitches<std::uint32_t>("uint32_t pitches");
    passed = passed && check_custom_pitches<std::uint64_t>("uint64_t pitches");

    if (!passed)
        return 1;
    std::cout << "lower_bound tests passed\n";
    return 0;
}