#include <popcount_ranges/lower_bound.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <limits>
#include <iostream>
#include <string_view>
#include <type_traits>
#include <vector>

namespace
{
volatile std::size_t sink = 0;

template <typename T, typename Predicate>
void run_case(std::string_view type_name, std::string_view direction, std::size_t size,
              std::vector<T> const& values, std::vector<T> const& queries,
              Predicate predicate, std::string_view implementation,
              std::size_t repetitions)
{
    auto const start = std::chrono::steady_clock::now();
    std::size_t result = 0;
    for (std::size_t repetition = 0; repetition < repetitions; ++repetition)
    {
        for (T query : queries)
        {
            auto const first = values.data();
            auto const last = first + values.size();
            if (implementation == "vec")
                result += static_cast<std::size_t>(jrmwng::popcount_ranges::lower_bound(first, last, query, predicate) - first);
            else if (implementation == "std")
                result += static_cast<std::size_t>(std::lower_bound(values.begin(), values.end(), query, predicate) - values.begin());
            else if (implementation == "linear")
                result += static_cast<std::size_t>(std::find_if(values.begin(), values.end(), [&](T value) { return !predicate(value, query); }) - values.begin());
            else
                result += static_cast<std::size_t>(std::count_if(values.begin(), values.end(), [&](T value) { return predicate(value, query); }));
        }
    }
    sink += result;
    auto const elapsed = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start).count();
    auto const operations = static_cast<double>(repetitions * queries.size());
    std::cout << type_name << ',' << direction << ',' << size << ',' << implementation << ','
              << std::fixed << std::setprecision(2) << elapsed / operations << ',' << result << '\n';
}

template <typename T>
void benchmark_type(std::string_view type_name, bool quick)
{
    constexpr std::size_t lanes = (sizeof(T) == 1 ? 64u : 128u) / sizeof(T);
    std::vector<std::size_t> sizes{1, lanes - 1, lanes, lanes + 1, 256, 4096, 65536};
    if (quick)
        sizes.resize(4);

    for (bool descending : {false, true})
    {
        for (std::size_t size : sizes)
        {
            std::vector<T> values(size);
            for (std::size_t index = 0; index < size; ++index)
            {
                std::size_t const value_index = descending ? size - index - 1u : index;
                values[index] = static_cast<T>(size == 1
                    ? 0
                    : (value_index * (static_cast<std::size_t>(std::numeric_limits<T>::max()) / (size - 1)))
                        + (value_index * (static_cast<std::size_t>(std::numeric_limits<T>::max()) % (size - 1))) / (size - 1));
            }

            std::vector<T> queries;
            for (std::size_t index : {std::size_t{0}, size / 4, size / 2, (size * 3) / 4, size - 1})
                queries.push_back(values[index]);
            queries.push_back(std::numeric_limits<T>::lowest());
            queries.push_back(std::numeric_limits<T>::max());

            auto run_direction = [&](auto predicate, std::string_view direction)
            {
                std::size_t const repetitions = quick ? 20 : 1000;
                for (std::string_view implementation : {"vec", "std", "linear", "count_if"})
                    run_case(type_name, direction, size, values, queries, predicate, implementation, repetitions);
            };

            if (descending)
                run_direction(std::greater<>{}, "descending");
            else
                run_direction(std::less<>{}, "ascending");
        }
    }
}
}

int main(int argc, char const* argv[])
{
    bool const quick = argc > 1 && std::string_view(argv[1]) == "--quick";
    std::cout << "type,direction,size,implementation,ns_per_query,checksum\n";
    benchmark_type<std::uint8_t>("uint8", quick);
    benchmark_type<std::uint16_t>("uint16", quick);
    benchmark_type<std::uint32_t>("uint32", quick);
    benchmark_type<std::uint64_t>("uint64", quick);
    std::cerr << "sink=" << sink << '\n';
}