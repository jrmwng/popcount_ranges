#include <popcount_ranges/set.hpp>
#include <popcount_ranges/ncr.hpp>
#include <popcount_ranges/uint.hpp>

#include <bit>
#include <concepts>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace
{
#ifdef _DEBUG
template <typename Function>
bool throws_out_of_range(Function&& function)
{
    try
    {
        function();
    }
    catch (std::out_of_range const&)
    {
        return true;
    }
    catch (...)
    {
    }
    return false;
}

template <typename Function>
bool throws_invalid_argument(Function&& function)
{
    try
    {
        function();
    }
    catch (std::invalid_argument const&)
    {
        return true;
    }
    catch (...)
    {
    }
    return false;
}

bool check_constructor_exceptions()
{
    using Set = jrmwng::popcount_ranges::set<std::uint8_t{2}>;
    auto const sentinel = *Set().end();

    // 1. first does not have required reduce_count
    if (!throws_invalid_argument([] { Set s(static_cast<std::uint8_t>(0), static_cast<std::uint8_t>(3)); }))
    {
        std::cerr << "constructor exception check failed: invalid first reduce_count\n";
        return false;
    }

    // 2. last does not have required reduce_count
    if (!throws_invalid_argument([] { Set s(static_cast<std::uint8_t>(3), static_cast<std::uint8_t>(0)); }))
    {
        std::cerr << "constructor exception check failed: invalid last reduce_count\n";
        return false;
    }

    // 3. first is sentinel but last is not
    if (!throws_invalid_argument([sentinel] { Set s(sentinel, static_cast<std::uint8_t>(3)); }))
    {
        std::cerr << "constructor exception check failed: first is sentinel but last is not\n";
        return false;
    }

    // 4. first > last
    if (!throws_invalid_argument([] { Set s(static_cast<std::uint8_t>(5), static_cast<std::uint8_t>(3)); }))
    {
        std::cerr << "constructor exception check failed: first > last\n";
        return false;
    }

    return true;
}

bool check_iterator_exception_bounds()
{
    using Set = jrmwng::popcount_ranges::set<std::uint8_t{2}>;
    Set set;

    // 1. Incrementing past end
    auto end_it = set.end();
    if (!throws_out_of_range([&end_it] { ++end_it; }))
    {
        std::cerr << "iterator exception check failed: ++end\n";
        return false;
    }
    end_it = set.end();
    if (!throws_out_of_range([&end_it] { end_it++; }))
    {
        std::cerr << "iterator exception check failed: end++\n";
        return false;
    }

    // 2. Decrementing past begin
    auto begin_it = set.begin();
    if (!throws_out_of_range([&begin_it] { --begin_it; }))
    {
        std::cerr << "iterator exception check failed: --begin\n";
        return false;
    }
    begin_it = set.begin();
    if (!throws_out_of_range([&begin_it] { begin_it--; }))
    {
        std::cerr << "iterator exception check failed: begin--\n";
        return false;
    }

    // 3. Out-of-bounds += / -= / + / -
    if (!throws_out_of_range([&set] { auto it = set.begin(); it += -1; }))
        return false;
    if (!throws_out_of_range([&set] { auto it = set.end(); it += 1; }))
        return false;
    if (!throws_out_of_range([&set] { auto it = set.begin(); it -= 1; }))
        return false;
    if (!throws_out_of_range([&set] { auto it = set.end(); it -= -1; }))
        return false;

    return true;
}
#endif

template <typename T, T Popcount>
bool check_iterator_operators()
{
    using Set = jrmwng::popcount_ranges::set<Popcount>;
    using difference_type = typename Set::iterator::difference_type;

    Set set;
    std::vector<T> expected;
    std::vector<typename Set::iterator> positions;
    for (auto iterator = set.begin(); iterator != set.end(); ++iterator)
    {
        expected.push_back(*iterator);
        positions.push_back(iterator);
    }
    positions.push_back(set.end());

    auto const size = static_cast<difference_type>(expected.size());
    auto const fail = [](char const* operation, difference_type index, difference_type offset = 0)
    {
        std::cerr << "iterator operator failure: " << operation << ", index=" << index << ", offset=" << offset << "\n";
        return false;
    };
    auto const iterator_at = [&positions](difference_type index)
    {
        return positions[static_cast<std::size_t>(index)];
    };

    for (difference_type index = 1; index < size; ++index)
    {
        auto prefix = iterator_at(index);
        if (*--prefix != expected[static_cast<std::size_t>(index - 1)])
            return fail("prefix decrement", index);

        auto postfix = iterator_at(index);
        auto const original = postfix--;
        if (*original != expected[static_cast<std::size_t>(index)] ||
            *postfix != expected[static_cast<std::size_t>(index - 1)])
            return fail("postfix decrement", index);
    }

    auto from_end = set.end();
    if (*--from_end != expected.back())
        return fail("decrement end", size);
#ifdef _DEBUG
    if (!throws_out_of_range([&set] { --set.begin(); }))
        return fail("decrement begin", 0);
#endif

    for (difference_type index = 0; index <= size; ++index)
    {
        auto const iterator = iterator_at(index);
        for (difference_type offset : {difference_type{0}, difference_type{1}, difference_type{-1}, difference_type{2}, difference_type{-2}})
        {
            auto const target = index + offset;
            auto const is_valid = target >= 0 && target <= size;

            auto plus_assign = iterator;
            if (is_valid)
            {
                plus_assign += offset;
                if (plus_assign != iterator_at(target))
                    return fail("plus assign", index, offset);
            }
#ifdef _DEBUG
            else if (!throws_out_of_range([&plus_assign, offset] { plus_assign += offset; }))
                return fail("plus assign bounds", index, offset);
#endif

            auto minus_assign = iterator;
            if (is_valid)
            {
                minus_assign -= -offset;
                if (minus_assign != iterator_at(target))
                    return fail("minus assign", index, offset);
            }
#ifdef _DEBUG
            else if (!throws_out_of_range([&minus_assign, offset] { minus_assign -= -offset; }))
                return fail("minus assign bounds", index, offset);
#endif

            if (is_valid)
            {
                if (iterator + offset != iterator_at(target) || iterator != iterator_at(index))
                    return fail("plus", index, offset);
                if (iterator - -offset != iterator_at(target) || iterator != iterator_at(index))
                    return fail("minus", index, offset);
            }
#ifdef _DEBUG
            else
            {
                if (!throws_out_of_range([iterator, offset] { static_cast<void>(iterator + offset); }))
                    return fail("plus bounds", index, offset);
                if (!throws_out_of_range([iterator, offset] { static_cast<void>(iterator - -offset); }))
                    return fail("minus bounds", index, offset);
            }
#endif
        }
    }

    return true;
}

template <typename T, T Popcount>
bool check_exhaustive_set(std::size_t expected_size)
{
    using Set = jrmwng::popcount_ranges::set<Popcount>;
    Set set;
    std::size_t size = 0;
    T previous = 0;

    if (std::popcount(static_cast<T>(*set.end())) == static_cast<int>(Popcount))
        return false;

    auto const expected_dist = static_cast<std::ptrdiff_t>(expected_size);

    // Check full set distance for iterator and reverse_iterator
    if (std::distance(set.begin(), set.end()) != expected_dist)
    {
        std::cerr << "Failed: std::distance(begin, end) != expected_dist for Popcount=" << (int)Popcount << "\n";
        return false;
    }
    if ((set.end() - set.begin()) != expected_dist)
    {
        std::cerr << "Failed: (end - begin) != expected_dist for Popcount=" << (int)Popcount << "\n";
        return false;
    }
    if ((set.begin() - set.end()) != -expected_dist)
    {
        std::cerr << "Failed: (begin - end) != -expected_dist for Popcount=" << (int)Popcount << "\n";
        return false;
    }

    if (std::distance(set.rbegin(), set.rend()) != expected_dist)
    {
        std::cerr << "Failed: std::distance(rbegin, rend) != expected_dist for Popcount=" << (int)Popcount << "\n";
        return false;
    }
    if ((set.rend() - set.rbegin()) != expected_dist)
    {
        std::cerr << "Failed: (rend - rbegin) != expected_dist for Popcount=" << (int)Popcount << "\n";
        return false;
    }
    if ((set.rbegin() - set.rend()) != -expected_dist)
    {
        std::cerr << "Failed: (rbegin - rend) != -expected_dist for Popcount=" << (int)Popcount << "\n";
        return false;
    }

    // Check self-distance
    if (std::distance(set.begin(), set.begin()) != 0 || (set.begin() - set.begin()) != 0)
        return false;
    if (std::distance(set.end(), set.end()) != 0 || (set.end() - set.end()) != 0)
        return false;
    if (std::distance(set.rbegin(), set.rbegin()) != 0 || (set.rbegin() - set.rbegin()) != 0)
        return false;
    if (std::distance(set.rend(), set.rend()) != 0 || (set.rend() - set.rend()) != 0)
        return false;

    // Check distances during forward iteration
    {
        std::size_t idx = 0;
        for (auto it = set.begin(); it != set.end(); ++it, ++idx)
        {
            auto const cur_idx = static_cast<std::ptrdiff_t>(idx);
            auto const rem_idx = static_cast<std::ptrdiff_t>(expected_size - idx);

            if (std::distance(set.begin(), it) != cur_idx)
                return false;
            if ((it - set.begin()) != cur_idx)
                return false;
            if ((set.begin() - it) != -cur_idx)
                return false;

            if (std::distance(it, set.end()) != rem_idx)
                return false;
            if ((set.end() - it) != rem_idx)
                return false;
            if ((it - set.end()) != -rem_idx)
                return false;
        }
    }

    // Check distances during reverse iteration
    {
        std::size_t idx = 0;
        for (auto rit = set.rbegin(); rit != set.rend(); ++rit, ++idx)
        {
            auto const cur_idx = static_cast<std::ptrdiff_t>(idx);
            auto const rem_idx = static_cast<std::ptrdiff_t>(expected_size - idx);

            if (std::distance(set.rbegin(), rit) != cur_idx)
                return false;
            if ((rit - set.rbegin()) != cur_idx)
                return false;
            if ((set.rbegin() - rit) != -cur_idx)
                return false;

            if (std::distance(rit, set.rend()) != rem_idx)
                return false;
            if ((set.rend() - rit) != rem_idx)
                return false;
            if ((rit - set.rend()) != -rem_idx)
                return false;
        }
    }

    for (auto value : set)
    {
        T current = static_cast<T>(value);
        if (size != 0 && current <= previous)
            return false;
        if (std::popcount(current) != static_cast<int>(Popcount))
            return false;
        previous = current;
        ++size;
    }

    return size == expected_size;
}

template <typename T, T Popcount>
bool check_range_and_jump_operations()
{
    using Set = jrmwng::popcount_ranges::set<Popcount>;
    using difference_type = typename Set::iterator::difference_type;

    static_assert(std::bidirectional_iterator<typename Set::iterator>);
    static_assert(std::forward_iterator<typename Set::reverse_iterator>);

    Set full;
    std::vector<T> values;
    for (auto iterator = full.begin(); iterator != full.end(); ++iterator)
        values.push_back(*iterator);

    if (values.empty())
        return false;

    auto const first = values[values.size() / 4];
    auto const last = values[(values.size() * 3) / 4];
    Set bounded(first, last);
    if (*bounded.begin() != first || *bounded.end() != last)
        return false;

    auto const bounded_size = static_cast<difference_type>(values.size() / 2);
    if (bounded.end() - bounded.begin() != bounded_size)
        return false;

    if (typename Set::iterator{bounded.rend()} != bounded.begin() ||
        typename Set::iterator{bounded.rbegin()} != bounded.end())
        return false;
    if (typename Set::reverse_iterator{bounded.begin()} != bounded.rend() ||
        typename Set::reverse_iterator{bounded.end()} != bounded.rbegin())
        return false;

    auto const check_offset = [&](difference_type start_rank, difference_type offset)
    {
        auto jumped = full.begin() + start_rank;
        auto advanced = jumped;
        auto repeated = jumped;
        auto const steps = offset >= 0 ? offset : -offset;
        if (offset >= 0)
        {
            for (difference_type step = 0; step < steps; ++step)
                ++repeated;
        }
        else
        {
            for (difference_type step = 0; step < steps; ++step)
                --repeated;
        }
        jumped += offset;
        advanced.advance(offset);
        return jumped == repeated && advanced == repeated;
    };

    auto const size = static_cast<difference_type>(values.size());
    for (difference_type start_rank : {difference_type{0}, size / 3, size - 1})
    {
        for (difference_type offset : {difference_type{0}, difference_type{1}, difference_type{-1},
                                      difference_type{2}, difference_type{-2}})
        {
            if (start_rank + offset >= 0 && start_rank + offset < size &&
                !check_offset(start_rank, offset))
                return false;
        }
    }

    auto const singleton_value = values[values.size() / 2];
    Set singleton(singleton_value, singleton_value);
    if (singleton.begin() != singleton.end() || singleton.end() - singleton.begin() != 0)
        return false;

    return true;
}

template <std::size_t N, std::size_t K>
bool check_wide_uint_set()
{
    using T = jrmwng::popcount_ranges::std::uint<N>;
    using Set = jrmwng::popcount_ranges::set<T{K}>;
    constexpr std::size_t r = K < N - K ? K : N - K;
    constexpr std::size_t expected_size = [] {
        std::size_t result = 1;
        for (std::size_t i = 1; i <= r; ++i)
            result = result * (N - i + 1) / i;
        return result;
    }();

    Set values;
    if (values.size() != expected_size ||
        values.end() - values.begin() != static_cast<std::ptrdiff_t>(expected_size))
    {
        std::cerr << "wide set size failed for N=" << N << ", K=" << K << '\n';
        return false;
    }

    std::size_t index = 0;
    T previous{};
    for (T value : values)
    {
        if (jrmwng::popcount_ranges::std::popcount(value) != static_cast<int>(K) ||
            (index != 0 && !(previous < value)))
        {
            std::cerr << "wide set iteration failed for N=" << N << ", K=" << K
                      << ", index=" << index << '\n';
            return false;
        }
        previous = value;
        ++index;
    }
    if (index != expected_size)
        return false;

    for (std::size_t rank : {std::size_t{0}, expected_size / 3, expected_size / 2,
                             (expected_size * 2) / 3, expected_size - 1})
    {
        auto const expected = values[rank];
        if (*(values.begin() + static_cast<std::ptrdiff_t>(rank)) != expected)
            return false;
    }

    if (expected_size > 2)
    {
        auto const first_rank = expected_size / 3;
        auto const last_rank = (expected_size * 2) / 3;
        auto const first = values[first_rank];
        auto const last = values[last_rank];
        Set bounded{first, last};
        if (*bounded.begin() != first || *bounded.end() != last ||
            bounded.size() != last_rank - first_rank)
            return false;
    }

    return true;
}

bool check_uint8_iterator_operators()
{
    if (!check_iterator_operators<std::uint8_t, std::uint8_t{0}>())
        return false;
    if (!check_iterator_operators<std::uint8_t, std::uint8_t{1}>())
        return false;
    if (!check_iterator_operators<std::uint8_t, std::uint8_t{2}>())
        return false;
    if (!check_iterator_operators<std::uint8_t, std::uint8_t{3}>())
        return false;
    if (!check_iterator_operators<std::uint8_t, std::uint8_t{4}>())
        return false;
    if (!check_iterator_operators<std::uint8_t, std::uint8_t{5}>())
        return false;
    if (!check_iterator_operators<std::uint8_t, std::uint8_t{6}>())
        return false;
    if (!check_iterator_operators<std::uint8_t, std::uint8_t{7}>())
        return false;
    if (!check_iterator_operators<std::uint8_t, std::uint8_t{8}>())
        return false;
    return true;
}

bool check_uint8_exhaustive_sets()
{
    if (!check_range_and_jump_operations<std::uint8_t, std::uint8_t{3}>())
    {
        std::cerr << "range and jump operations failed for uint8_t\n";
        return false;
    }

    if (!check_uint8_iterator_operators())
    {
        std::cerr << "iterator operators failed for uint8_t\n";
        return false;
    }

    if (!check_exhaustive_set<std::uint8_t, std::uint8_t{0}>(1))
        return false;
    if (!check_exhaustive_set<std::uint8_t, std::uint8_t{1}>(8))
        return false;
    if (!check_exhaustive_set<std::uint8_t, std::uint8_t{2}>(28))
        return false;
    if (!check_exhaustive_set<std::uint8_t, std::uint8_t{3}>(56))
        return false;
    if (!check_exhaustive_set<std::uint8_t, std::uint8_t{4}>(70))
        return false;
    if (!check_exhaustive_set<std::uint8_t, std::uint8_t{5}>(56))
        return false;
    if (!check_exhaustive_set<std::uint8_t, std::uint8_t{6}>(28))
        return false;
    if (!check_exhaustive_set<std::uint8_t, std::uint8_t{7}>(8))
        return false;
    if (!check_exhaustive_set<std::uint8_t, std::uint8_t{8}>(1))
        return false;
    return true;
}

bool check_uint16_exhaustive_sets()
{
    if (!check_exhaustive_set<std::uint16_t, std::uint16_t{0}>(1))
        return false;
    if (!check_exhaustive_set<std::uint16_t, std::uint16_t{1}>(16))
        return false;
    if (!check_exhaustive_set<std::uint16_t, std::uint16_t{2}>(120))
        return false;
    if (!check_exhaustive_set<std::uint16_t, std::uint16_t{3}>(560))
        return false;
    if (!check_exhaustive_set<std::uint16_t, std::uint16_t{4}>(1820))
        return false;
    if (!check_exhaustive_set<std::uint16_t, std::uint16_t{5}>(4368))
        return false;
    if (!check_exhaustive_set<std::uint16_t, std::uint16_t{6}>(8008))
        return false;
    if (!check_exhaustive_set<std::uint16_t, std::uint16_t{7}>(11440))
        return false;
    if (!check_exhaustive_set<std::uint16_t, std::uint16_t{8}>(12870))
        return false;
    if (!check_exhaustive_set<std::uint16_t, std::uint16_t{9}>(11440))
        return false;
    if (!check_exhaustive_set<std::uint16_t, std::uint16_t{10}>(8008))
        return false;
    if (!check_exhaustive_set<std::uint16_t, std::uint16_t{11}>(4368))
        return false;
    if (!check_exhaustive_set<std::uint16_t, std::uint16_t{12}>(1820))
        return false;
    if (!check_exhaustive_set<std::uint16_t, std::uint16_t{13}>(560))
        return false;
    if (!check_exhaustive_set<std::uint16_t, std::uint16_t{14}>(120))
        return false;
    if (!check_exhaustive_set<std::uint16_t, std::uint16_t{15}>(16))
        return false;
    if (!check_exhaustive_set<std::uint16_t, std::uint16_t{16}>(1))
        return false;
    return true;
}

bool check_uint32_exhaustive_sets()
{
    if (!check_exhaustive_set<std::uint32_t, std::uint32_t{0}>(1))
        return false;
    if (!check_exhaustive_set<std::uint32_t, std::uint32_t{1}>(32))
        return false;
    if (!check_exhaustive_set<std::uint32_t, std::uint32_t{2}>(496))
        return false;
    if (!check_exhaustive_set<std::uint32_t, std::uint32_t{30}>(496))
        return false;
    if (!check_exhaustive_set<std::uint32_t, std::uint32_t{31}>(32))
        return false;
    if (!check_exhaustive_set<std::uint32_t, std::uint32_t{32}>(1))
        return false;
    return true;
}

bool check_uint64_exhaustive_sets()
{
    if (!check_exhaustive_set<std::uint64_t, std::uint64_t{0}>(1))
        return false;
    if (!check_exhaustive_set<std::uint64_t, std::uint64_t{1}>(64))
        return false;
    if (!check_exhaustive_set<std::uint64_t, std::uint64_t{2}>(2016))
        return false;
    if (!check_exhaustive_set<std::uint64_t, std::uint64_t{62}>(2016))
        return false;
    if (!check_exhaustive_set<std::uint64_t, std::uint64_t{63}>(64))
        return false;
    if (!check_exhaustive_set<std::uint64_t, std::uint64_t{64}>(1))
        return false;
    return true;
}
}

int main()
{
#ifdef _DEBUG
    std::cout << "Running debug exception tests..." << std::endl;

    std::cout << "Testing constructor exceptions..." << std::endl;
    if (!check_constructor_exceptions())
    {
        std::cerr << "constructor exception tests failed\n";
        return 1;
    }

    std::cout << "Testing iterator exception bounds..." << std::endl;
    if (!check_iterator_exception_bounds())
    {
        std::cerr << "iterator exception bounds tests failed\n";
        return 2;
    }

    std::cout << "debug exception tests passed\n";
    return 0;
#else
    std::cout << "Running release exhaustive set tests..." << std::endl;

    if (!check_uint8_exhaustive_sets())
    {
        std::cerr << "uint8_t exhaustive cases failed\n";
        return 1;
    }

    if (!check_uint16_exhaustive_sets())
    {
        std::cerr << "uint16_t exhaustive cases failed\n";
        return 2;
    }

    if (!check_uint32_exhaustive_sets())
    {
        std::cerr << "uint32_t exhaustive cases failed\n";
        return 3;
    }

    if (!check_uint64_exhaustive_sets())
    {
        std::cerr << "uint64_t exhaustive cases failed\n";
        return 4;
    }

    if (!check_wide_uint_set<128, 0>() || !check_wide_uint_set<128, 1>() ||
        !check_wide_uint_set<128, 2>() || !check_wide_uint_set<128, 126>() ||
        !check_wide_uint_set<128, 127>() || !check_wide_uint_set<128, 128>() ||
        !check_wide_uint_set<192, 0>() || !check_wide_uint_set<192, 1>() ||
        !check_wide_uint_set<192, 2>() || !check_wide_uint_set<192, 190>() ||
        !check_wide_uint_set<192, 191>() || !check_wide_uint_set<192, 192>() ||
        !check_wide_uint_set<256, 0>() || !check_wide_uint_set<256, 1>() ||
        !check_wide_uint_set<256, 2>() || !check_wide_uint_set<256, 254>() ||
        !check_wide_uint_set<256, 255>() || !check_wide_uint_set<256, 256>())
    {
        std::cerr << "wide uint set tests failed\n";
        return 5;
    }

    std::cout << "expanded set tests passed\n";
    return 0;
#endif
}
