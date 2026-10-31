#include <popcount_ranges/lower_bound.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
volatile std::size_t sink = 0;

template <typename T, typename Predicate, typename Search>
std::pair<double, std::size_t> measure(
    std::vector<T> const& values, std::vector<T> const& queries,
    Predicate predicate, Search search, std::size_t repetitions)
{
    std::size_t result = 0;
    auto const started = std::chrono::steady_clock::now();
    for (std::size_t repetition = 0; repetition < repetitions; ++repetition)
        for (T query : queries)
            result += static_cast<std::size_t>(
                search(values, query, predicate));
    auto const elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - started).count();
    sink += result;
    return {elapsed / static_cast<double>(repetitions * queries.size()), result};
}

template <typename T, typename Predicate>
bool run_case(
    std::string_view type_name, std::string_view direction, std::size_t size,
    std::vector<T> const& values, std::vector<T> const& queries,
    Predicate predicate, std::size_t repetitions)
{
    auto custom_search = [](auto const& range, T query, auto pred)
    {
        return jrmwng::popcount_ranges::lower_bound(
            range.data(), range.data() + range.size(), query, pred) -
            range.data();
    };
    auto standard_search = [](auto const& range, T query, auto pred)
    {
        return std::lower_bound(
            range.begin(), range.end(), query, pred) - range.begin();
    };

    auto const [custom_time, custom_checksum] = measure(
        values, queries, predicate, custom_search, repetitions);
    auto const [standard_time, standard_checksum] = measure(
        values, queries, predicate, standard_search, repetitions);
    if (custom_checksum != standard_checksum)
    {
        std::cerr << "checksum mismatch for " << type_name << ' ' << direction
                  << " size=" << size << ": custom=" << custom_checksum
                  << ", std=" << standard_checksum << '\n';
        return false;
    }

    for (auto const& [implementation, elapsed] : {
             std::pair{"custom", custom_time},
             std::pair{"std", standard_time}})
    {
        std::size_t const checksum =
            implementation == std::string_view("custom")
                ? custom_checksum
                : standard_checksum;
        std::cout << type_name << ',' << direction << ',' << size << ','
                  << implementation << ',' << std::fixed
                  << std::setprecision(2) << elapsed << ',' << checksum << '\n';
    }
    return true;
}

template <typename T>
bool benchmark_type(std::string_view type_name, bool quick)
{
    constexpr std::size_t lanes = (sizeof(T) == 1 ? 64u : 128u) / sizeof(T);
    std::vector<std::size_t> sizes{1, lanes - 1, lanes, lanes + 1, 256, 4096, 65536};
    if (quick)
        sizes.resize(4);

    std::size_t const repetitions = quick ? 20u : 1000u;
    for (bool descending : {false, true})
    {
        for (std::size_t size : sizes)
        {
            std::vector<T> values(size);
            for (std::size_t index = 0; index < size; ++index)
            {
                std::size_t const value_index =
                    descending ? size - index - 1u : index;
                values[index] = static_cast<T>(size == 1
                    ? 0
                    : (value_index *
                           (static_cast<std::size_t>(
                                std::numeric_limits<T>::max()) /
                            (size - 1))) +
                          (value_index *
                           (static_cast<std::size_t>(
                                std::numeric_limits<T>::max()) %
                            (size - 1))) /
                              (size - 1));
            }

            std::vector<T> queries;
            for (std::size_t index : {
                     std::size_t{0}, size / 4, size / 2, (size * 3) / 4,
                     size - 1})
                queries.push_back(values[index]);
            queries.push_back(std::numeric_limits<T>::lowest());
            queries.push_back(std::numeric_limits<T>::max());

            bool const passed = descending
                ? run_case(
                      type_name, "descending", size, values, queries,
                      std::greater<>{}, repetitions)
                : run_case(
                      type_name, "ascending", size, values, queries,
                      std::less<>{}, repetitions);
            if (!passed)
                return false;
        }
    }
    return true;
}
}

int main(int argc, char const* argv[])
{
    bool const quick = argc > 1 && std::string_view(argv[1]) == "--quick";
    if (argc > 2 || (argc == 2 && !quick))
    {
        std::cerr << "usage: lower_bound_benchmark [--quick]\n";
        return 2;
    }

    std::cout << "type,direction,size,implementation,ns_per_query,checksum\n";
    bool const passed =
        benchmark_type<std::uint8_t>("uint8", quick) &&
        benchmark_type<std::uint16_t>("uint16", quick) &&
        benchmark_type<std::uint32_t>("uint32", quick) &&
        benchmark_type<std::uint64_t>("uint64", quick);
    std::cerr << "sink=" << sink << '\n';
    return passed ? 0 : 1;
}
