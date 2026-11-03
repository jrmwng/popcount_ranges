#pragma once

#include <popcount_ranges/lower_bound.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__)
#include <sched.h>
#endif

#if defined(_MSC_VER)
#define POPCOUNT_RANGES_NOINLINE __declspec(noinline)
#else
#define POPCOUNT_RANGES_NOINLINE __attribute__((noinline))
#endif

#ifndef POPCOUNT_RANGES_BENCHMARK_COMPILER
#define POPCOUNT_RANGES_BENCHMARK_COMPILER "unknown"
#endif

namespace lower_bound_tuning
{
struct Options
{
    std::string_view stage;
    std::string_view selected_type;
    std::size_t type_bits;
    std::size_t selected_size;
    std::size_t forced_pitch;
    bool quick;
    std::uint32_t seed;
    std::size_t repetitions_override;
    std::size_t selected_processor;
    bool dirty_cache_mode;
};

class CacheFlusher
{
public:
    CacheFlusher(std::size_t bytes, std::size_t dirty_bytes)
        : buffer_(bytes), dirty_buffer_(dirty_bytes)
    {
    }

    void flush(volatile std::uint64_t& sink) const
    {
        auto const* data =
            reinterpret_cast<volatile std::uint8_t const*>(buffer_.data());
        std::uint64_t checksum = 0;
        for (std::size_t offset = 0; offset < buffer_.size(); offset += 64u)
            checksum += data[offset];
        sink += checksum;
    }

    void make_dirty(std::size_t repetition) const
    {
        auto* data =
            reinterpret_cast<volatile std::uint8_t*>(dirty_buffer_.data());
        for (std::size_t offset = 0; offset < dirty_buffer_.size(); offset += 64u)
            data[offset] = static_cast<std::uint8_t>(
                data[offset] + repetition + 1u);
    }

private:
    std::vector<std::uint8_t> buffer_;
    mutable std::vector<std::uint8_t> dirty_buffer_;
};
}

namespace
{
volatile std::uint64_t sink = 0;
std::uint32_t benchmark_seed = 0x5eedu;
std::size_t repetitions_override = 0;
std::size_t selected_processor = std::numeric_limits<std::size_t>::max();
std::size_t llc_bytes = 0;
std::size_t cache_flush_bytes = 0;
bool dirty_cache_mode = false;
using lower_bound_tuning::CacheFlusher;
#if defined(_WIN32)
WORD benchmark_processor_group = 0;
KAFFINITY benchmark_processor_mask = 0;
#elif defined(__linux__)
int benchmark_processor_index = -1;
#endif
std::size_t benchmark_processor_number = 0;

template <typename T>
constexpr std::string_view primitive_type_name()
{
    if constexpr (std::is_same_v<T, std::uint8_t>)
        return "uint8_t";
    else if constexpr (std::is_same_v<T, std::uint16_t>)
        return "uint16_t";
    else if constexpr (std::is_same_v<T, std::uint32_t>)
        return "uint32_t";
    else if constexpr (std::is_same_v<T, std::uint64_t>)
        return "uint64_t";
    else if constexpr (std::is_same_v<T, float>)
        return "float";
    else
        return "double";
}

std::size_t detected_llc_bytes()
{
#if defined(_WIN32)
    DWORD required_bytes = 0;
    GetLogicalProcessorInformationEx(
        RelationCache, nullptr, &required_bytes);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || required_bytes == 0)
        throw std::runtime_error(
            "could not query the Windows cache hierarchy");

    std::vector<std::byte> records(required_bytes);
    if (!GetLogicalProcessorInformationEx(
            RelationCache,
            reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(
                records.data()),
            &required_bytes))
        throw std::runtime_error(
            "could not read the Windows cache hierarchy");

    std::size_t largest_llc = 0;
    for (std::size_t offset = 0; offset < required_bytes;)
    {
        auto const* record =
            reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX const*>(
                records.data() + offset);
        if (record->Relationship == RelationCache &&
            record->Cache.Level >= 3)
        {
            GROUP_AFFINITY const& group_mask = record->Cache.GroupMask;
            if (group_mask.Group == benchmark_processor_group &&
                (group_mask.Mask & benchmark_processor_mask) != 0)
                largest_llc = std::max<std::size_t>(
                    largest_llc, record->Cache.CacheSize);
        }
        offset += record->Size;
    }
    if (largest_llc == 0)
        throw std::runtime_error(
            "Windows reported no level-3-or-higher cache");
    return largest_llc;
#elif defined(__linux__)
    namespace fs = std::filesystem;
    fs::path const cache_root(
        "/sys/devices/system/cpu/cpu" +
        std::to_string(benchmark_processor_index) + "/cache");
    std::size_t largest_llc = 0;
    for (fs::directory_entry const& entry :
         fs::directory_iterator(cache_root))
    {
        std::ifstream level_file(entry.path() / "level");
        std::ifstream type_file(entry.path() / "type");
        std::ifstream size_file(entry.path() / "size");
        int level = 0;
        std::string type;
        std::string size_text;
        if (!(level_file >> level) || !(type_file >> type) ||
            !(size_file >> size_text) || level < 3 || type != "Unified")
            continue;

        std::size_t multiplier = 1;
        char const suffix = size_text.back();
        if (suffix == 'K' || suffix == 'k')
            multiplier = 1024u;
        else if (suffix == 'M' || suffix == 'm')
            multiplier = 1024u * 1024u;
        auto const number_end =
            suffix == 'K' || suffix == 'k' || suffix == 'M' || suffix == 'm'
                ? size_text.data() + size_text.size() - 1u
                : size_text.data() + size_text.size();
        std::size_t size = 0;
        auto const [end, error] = std::from_chars(
            size_text.data(), number_end, size);
        if (error == std::errc{} && end == number_end &&
            size <= std::numeric_limits<std::size_t>::max() / multiplier)
            largest_llc = std::max(largest_llc, size * multiplier);
    }
    if (largest_llc == 0)
        throw std::runtime_error(
            "could not detect an LLC in the Linux cache sysfs");
    return largest_llc;
#else
    throw std::runtime_error(
        "automatic LLC detection is unsupported; specify "
        "--cache-flush-bytes and update the code for this platform");
#endif
}

#if defined(_WIN32)
class ThreadAffinity
{
public:
    ThreadAffinity()
    {
        PROCESSOR_NUMBER processor{};
        GetCurrentProcessorNumberEx(&processor);
        benchmark_processor_group = processor.Group;
        benchmark_processor_number =
            selected_processor == std::numeric_limits<std::size_t>::max()
                ? processor.Number
                : selected_processor;
        if (benchmark_processor_number >= sizeof(KAFFINITY) * 8u)
            throw std::runtime_error(
                "selected processor is outside its processor group");
        benchmark_processor_mask =
            KAFFINITY{1} << benchmark_processor_number;

        GROUP_AFFINITY requested{};
        requested.Group = benchmark_processor_group;
        requested.Mask = benchmark_processor_mask;
        if (!SetThreadGroupAffinity(
                GetCurrentThread(), &requested, &previous_))
            throw std::runtime_error(
                "could not pin benchmark thread to its LLC");
        active_ = true;
    }

    ~ThreadAffinity()
    {
        if (active_)
            SetThreadGroupAffinity(GetCurrentThread(), &previous_, nullptr);
    }

    ThreadAffinity(ThreadAffinity const&) = delete;
    ThreadAffinity& operator=(ThreadAffinity const&) = delete;

private:
    GROUP_AFFINITY previous_{};
    bool active_ = false;
};
#elif defined(__linux__)
class ThreadAffinity
{
public:
    ThreadAffinity()
    {
        if (sched_getaffinity(0, sizeof(previous_), &previous_) != 0)
            throw std::runtime_error(
                "could not read benchmark thread affinity");
        if (selected_processor != std::numeric_limits<std::size_t>::max() &&
            selected_processor >= CPU_SETSIZE)
            throw std::runtime_error(
                "selected processor is outside the supported CPU set");
        benchmark_processor_index = selected_processor ==
                std::numeric_limits<std::size_t>::max()
            ? sched_getcpu()
            : static_cast<int>(selected_processor);
        if (benchmark_processor_index < 0)
            throw std::runtime_error(
                "could not determine benchmark processor");
        if (benchmark_processor_index >= CPU_SETSIZE)
            throw std::runtime_error(
                "selected processor is outside the supported CPU set");
        benchmark_processor_number =
            static_cast<std::size_t>(benchmark_processor_index);

        cpu_set_t requested;
        CPU_ZERO(&requested);
        CPU_SET(benchmark_processor_index, &requested);
        if (sched_setaffinity(0, sizeof(requested), &requested) != 0)
            throw std::runtime_error(
                "could not pin benchmark thread to its LLC");
        active_ = true;
    }

    ~ThreadAffinity()
    {
        if (active_)
            sched_setaffinity(0, sizeof(previous_), &previous_);
    }

    ThreadAffinity(ThreadAffinity const&) = delete;
    ThreadAffinity& operator=(ThreadAffinity const&) = delete;

private:
    cpu_set_t previous_{};
    bool active_ = false;
};
#endif

CacheFlusher* cache_flusher = nullptr;

struct Result
{
    std::string_view stage;
    std::string_view strategy;
    std::string_view type;
    std::size_t type_bits;
    std::size_t size;
    std::size_t query_count;
    std::size_t query_passes;
    std::size_t threshold;
    std::size_t pitch;
    double custom_ns_per_query;
    double standard_ns_per_query;
    double custom_ns_per_sample;
    double standard_ns_per_sample;
    std::uint64_t checksum;
};

struct Measurement
{
    double ns_per_query;
    double ns_per_sample;
    std::uint64_t checksum;
    std::size_t query_passes;
};

template <std::size_t Pitch>
struct pitch_traits
{
    inline constexpr static
        jrmwng::popcount_ranges::binary_search_algorithm
            BINARY_SEARCH_ALGORITHM =
                jrmwng::popcount_ranges::binary_search_algorithm::none;
    inline constexpr static std::size_t SEQUENTIAL_SEARCH_PITCH = Pitch;

    template <typename T>
    static constexpr auto lookup_params(T const*, T const*) noexcept
    {
        return std::pair{
            std::integral_constant<std::size_t, Pitch>{},
            std::integral_constant<std::size_t, Pitch * 2>{}};
    }
};

template <std::size_t Threshold, std::size_t Pitch>
struct branchless_traits
{
    inline constexpr static
        jrmwng::popcount_ranges::binary_search_algorithm
            BINARY_SEARCH_ALGORITHM =
                jrmwng::popcount_ranges::binary_search_algorithm::branchless;
    inline constexpr static std::size_t BINARY_SEARCH_THRESHOLD = Threshold;
    inline constexpr static std::size_t SEQUENTIAL_SEARCH_PITCH = Pitch;

    template <typename T>
    static constexpr auto lookup_params(T const*, T const*) noexcept
    {
        return std::pair{
            std::integral_constant<std::size_t, Pitch>{},
            std::integral_constant<std::size_t, Threshold>{}};
    }
};

template <std::size_t Threshold, std::size_t Pitch>
struct branched_traits
{
    inline constexpr static
        jrmwng::popcount_ranges::binary_search_algorithm
            BINARY_SEARCH_ALGORITHM =
                jrmwng::popcount_ranges::binary_search_algorithm::branched;
    inline constexpr static std::size_t BINARY_SEARCH_THRESHOLD = Threshold;
    inline constexpr static std::size_t SEQUENTIAL_SEARCH_PITCH = Pitch;

    template <typename T>
    static constexpr auto lookup_params(T const*, T const*) noexcept
    {
        return std::pair{
            std::integral_constant<std::size_t, Pitch>{},
            std::integral_constant<std::size_t, Threshold>{}};
    }
};

template <typename T, typename Traits>
POPCOUNT_RANGES_NOINLINE std::size_t custom_search(
    std::vector<T> const& values, T query, Traits = {})
{
    return static_cast<std::size_t>(
        jrmwng::popcount_ranges::lower_bound(
            values.data(), values.data() + values.size(), query,
            std::less<>{}, Traits{}) -
        values.data());
}

template <typename T>
POPCOUNT_RANGES_NOINLINE std::size_t standard_search(
    std::vector<T> const& values, T query)
{
    return static_cast<std::size_t>(
        std::lower_bound(
            values.data(), values.data() + values.size(), query,
            std::less<>{}) -
        values.data());
}

template <typename T, typename Search>
Measurement measure(
    std::vector<T> const& values, std::vector<T> const& queries,
    Search search, std::size_t repetitions)
{
    std::size_t const query_passes = std::max<std::size_t>(
        1u, (16'384u + queries.size() - 1u) / queries.size());
    std::size_t const measured_query_count =
        queries.size() * query_passes;

    std::uint64_t warmup = 0;
    for (T query : queries)
        warmup += search(values, query);
    sink += warmup;

    std::vector<double> samples;
    samples.reserve(repetitions);
    std::uint64_t checksum = 0;
    for (std::size_t repetition = 0; repetition < repetitions; ++repetition)
    {
        std::uint64_t result = 0;
        cache_flusher->flush(sink);
        if (dirty_cache_mode)
            cache_flusher->make_dirty(repetition);
        auto const started = std::chrono::steady_clock::now();
        for (std::size_t pass = 0; pass < query_passes; ++pass)
            for (T query : queries)
                result += search(values, query);
        samples.push_back(std::chrono::duration<double, std::nano>(
                              std::chrono::steady_clock::now() - started)
                              .count());
        checksum = result;
        sink += result;
    }

    std::sort(samples.begin(), samples.end());
    double const median_ns_per_sample = samples[samples.size() / 2u];
    return {median_ns_per_sample /
                static_cast<double>(measured_query_count),
            median_ns_per_sample, checksum, query_passes};
}

template <typename T, typename Traits>
Result measure_configuration(
    std::vector<T> const& typed_values,
    std::vector<T> const& typed_queries, std::size_t repetitions,
    std::string_view stage, std::string_view strategy,
    std::size_t threshold, std::size_t pitch, std::size_t run_index)
{
    auto custom = [&](auto const& range, T query)
    {
        return custom_search(range, query, Traits{});
    };
    auto standard = [](auto const& range, T query)
    {
        return standard_search(range, query);
    };

    Measurement custom_result{};
    Measurement standard_result{};
    if (((benchmark_seed + static_cast<std::uint32_t>(run_index)) & 1u) == 0)
    {
        standard_result = measure(
            typed_values, typed_queries, standard, repetitions);
        custom_result = measure(
            typed_values, typed_queries, custom, repetitions);
    }
    else
    {
        custom_result = measure(
            typed_values, typed_queries, custom, repetitions);
        standard_result = measure(
            typed_values, typed_queries, standard, repetitions);
    }

    if (custom_result.checksum != standard_result.checksum)
    {
        std::cerr << "checksum mismatch: strategy=" << strategy
                  << ", threshold=" << threshold << ", pitch=" << pitch
                  << ", N=" << typed_values.size() << '\n';
        return {stage, strategy, primitive_type_name<T>(),
                sizeof(T) * 8u, typed_values.size(),
                typed_queries.size(), custom_result.query_passes,
                threshold, pitch, -1.0, -1.0,
                -1.0, -1.0, 0};
    }

    return {stage, strategy, primitive_type_name<T>(),
            sizeof(T) * 8u, typed_values.size(),
            typed_queries.size(), custom_result.query_passes,
            threshold, pitch,
            custom_result.ns_per_query, standard_result.ns_per_query,
            custom_result.ns_per_sample, standard_result.ns_per_sample,
            custom_result.checksum};
}

void print_result(Result const& result)
{
    std::cout << result.stage << ',' << benchmark_seed << ','
#if defined(_WIN32)
              << benchmark_processor_group << ','
#else
              << 0 << ','
#endif
              << benchmark_processor_number << ','
              << llc_bytes << ',' << cache_flush_bytes << ','
              << result.type << ',' << result.type_bits << ','
              << result.size << ','
              << result.query_count << ',' << result.query_passes << ','
              << result.strategy << ',';
    if (result.threshold != 0)
        std::cout << result.threshold;
    std::cout << ',' << result.pitch << ',' << std::fixed
              << std::setprecision(3) << result.custom_ns_per_query << ','
              << result.standard_ns_per_query << ','
              << result.custom_ns_per_sample << ','
              << result.standard_ns_per_sample << ',' << result.checksum
              << '\n';
}

bool valid(Result const& result)
{
    return result.custom_ns_per_query >= 0.0 &&
           result.standard_ns_per_query >= 0.0;
}

std::vector<std::size_t> sizes_for_type(std::size_t selected_size)
{
    constexpr std::array candidate_sizes{
        std::size_t{8}, std::size_t{16}, std::size_t{32},
        std::size_t{64}, std::size_t{128}, std::size_t{256},
        std::size_t{512}, std::size_t{1024}, std::size_t{2048},
        std::size_t{4096}, std::size_t{8196}, std::size_t{16384},
        std::size_t{32768}, std::size_t{65536}, std::size_t{131072},
        std::size_t{262144}};
    if (selected_size != 0)
        return {selected_size};

    return {candidate_sizes.begin(), candidate_sizes.end()};
}

template <typename T, std::size_t Pitch>
Result run_pitch(
    std::vector<T> const& values, std::vector<T> const& queries,
    std::size_t repetitions, std::size_t run_index)
{
    return measure_configuration<T, pitch_traits<Pitch>>(
        values, queries, repetitions, "pitch", "linear", 0, Pitch,
        run_index);
}

template <typename T, std::size_t Threshold, std::size_t Pitch>
Result run_branchless(
    std::vector<T> const& values, std::vector<T> const& queries,
    std::size_t repetitions, std::size_t run_index)
{
    return measure_configuration<T, branchless_traits<Threshold, Pitch>>(
        values, queries, repetitions, "threshold", "branchless",
        Threshold, Pitch, run_index);
}

template <typename T, std::size_t Threshold, std::size_t Pitch>
Result run_branched(
    std::vector<T> const& values, std::vector<T> const& queries,
    std::size_t repetitions, std::size_t run_index)
{
    return measure_configuration<T, branched_traits<Threshold, Pitch>>(
        values, queries, repetitions, "threshold", "branched",
        Threshold, Pitch, run_index);
}

template <typename Function>
bool for_each_pitch(Function&& function)
{
    if ((benchmark_seed & 1u) == 0)
        return function.template operator()<1024>() &&
               function.template operator()<512>() &&
               function.template operator()<256>() &&
               function.template operator()<128>() &&
               function.template operator()<64>() &&
               function.template operator()<32>() &&
               function.template operator()<16>() &&
               function.template operator()<8>() &&
               function.template operator()<4>() &&
               function.template operator()<2>() &&
               function.template operator()<1>();

    return function.template operator()<1>() &&
           function.template operator()<2>() &&
           function.template operator()<4>() &&
           function.template operator()<8>() &&
           function.template operator()<16>() &&
           function.template operator()<32>() &&
           function.template operator()<64>() &&
           function.template operator()<128>() &&
           function.template operator()<256>() &&
           function.template operator()<512>() &&
           function.template operator()<1024>();
}

template <std::size_t Pitch, typename Function>
bool for_each_threshold(Function&& function)
{
    return function.template operator()<1, Pitch>() &&
           function.template operator()<2, Pitch>() &&
           function.template operator()<4, Pitch>() &&
           function.template operator()<8, Pitch>() &&
           function.template operator()<16, Pitch>() &&
           function.template operator()<32, Pitch>() &&
           function.template operator()<64, Pitch>() &&
           function.template operator()<128, Pitch>() &&
           function.template operator()<256, Pitch>() &&
           function.template operator()<512, Pitch>() &&
           function.template operator()<1024, Pitch>() &&
           function.template operator()<2048, Pitch>() &&
           function.template operator()<4096, Pitch>() &&
           function.template operator()<8192, Pitch>() &&
           function.template operator()<16384, Pitch>() &&
           function.template operator()<32768, Pitch>() &&
           function.template operator()<65536, Pitch>() &&
           function.template operator()<131072, Pitch>() &&
           function.template operator()<262144, Pitch>() &&
           function.template operator()<524288, Pitch>() &&
           function.template operator()<1048576, Pitch>();
}

template <typename T>
Result run_selected_pitch(
    std::vector<T> const& values, std::vector<T> const& queries,
    std::size_t repetitions, std::size_t run_index, std::size_t pitch)
{
    if (pitch == 1) return run_pitch<T, 1>(values, queries, repetitions, run_index);
    if (pitch == 2) return run_pitch<T, 2>(values, queries, repetitions, run_index);
    if (pitch == 4) return run_pitch<T, 4>(values, queries, repetitions, run_index);
    if (pitch == 8) return run_pitch<T, 8>(values, queries, repetitions, run_index);
    if (pitch == 16) return run_pitch<T, 16>(values, queries, repetitions, run_index);
    if (pitch == 32) return run_pitch<T, 32>(values, queries, repetitions, run_index);
    if (pitch == 64) return run_pitch<T, 64>(values, queries, repetitions, run_index);
    if (pitch == 128) return run_pitch<T, 128>(values, queries, repetitions, run_index);
    if (pitch == 256) return run_pitch<T, 256>(values, queries, repetitions, run_index);
    if (pitch == 512) return run_pitch<T, 512>(values, queries, repetitions, run_index);
    return run_pitch<T, 1024>(values, queries, repetitions, run_index);
}

template <typename T>
bool benchmark_type(
    std::string_view stage, bool quick, std::size_t selected_size,
    std::size_t forced_pitch)
{
    std::size_t run_index = 0;
    for (std::size_t size : sizes_for_type(selected_size))
    {
        std::vector<T> values(size);
        if constexpr (std::is_floating_point_v<T>)
        {
            for (std::size_t index = 0; index < size; ++index)
                values[index] = static_cast<T>(index);
        }
        else
        {
            auto const maximum =
                static_cast<std::uint64_t>(std::numeric_limits<T>::max());
            if (size - 1u <= maximum)
            {
                for (std::size_t index = 0; index < size; ++index)
                    values[index] = static_cast<T>(index);
            }
            else
            {
                auto const denominator =
                    static_cast<std::uint64_t>(size - 1u);
                auto const quotient = maximum / denominator;
                auto const remainder = maximum % denominator;
                for (std::size_t index = 0; index < size; ++index)
                {
                    auto const position =
                        static_cast<std::uint64_t>(index);
                    values[index] = static_cast<T>(
                        quotient * position +
                        remainder * position / denominator);
                }
            }
        }

        std::vector<std::size_t> distinct_positions;
        distinct_positions.reserve(size);
        for (std::size_t index = 0; index < size; ++index)
            if (index == 0 || values[index - 1] != values[index])
                distinct_positions.push_back(index);

        std::size_t const query_count = std::min(
            quick ? std::size_t{64} : std::size_t{16'384},
            distinct_positions.size());
        std::vector<T> queries;
        queries.reserve(query_count);
        for (std::size_t query_index = 0;
             query_index < query_count; ++query_index)
        {
            std::size_t const position = query_count == 1
                ? distinct_positions.size() / 2u
                : query_index * (distinct_positions.size() - 1u) /
                      (query_count - 1u);
            queries.push_back(values[distinct_positions[position]]);
        }

        std::size_t const repetitions = repetitions_override != 0
            ? repetitions_override
            : quick ? 2u : 4u;
        std::size_t best_pitch =
            forced_pitch == 0 ? 1 : forced_pitch;
        double best_pitch_time = std::numeric_limits<double>::infinity();
        bool passed = true;

        if (stage == "default")
        {
            auto const [pitch, threshold] =
                jrmwng::popcount_ranges::lower_bound_traits<T>::lookup_params(
                    values.data(), values.data() + values.size());
            Result const result = measure_configuration<
                T, jrmwng::popcount_ranges::lower_bound_traits<T>>(
                values, queries, repetitions, "default", "default",
                threshold, pitch, run_index++);
            if (!valid(result))
                return false;
            print_result(result);
            continue;
        }

        if (stage == "pitch" && forced_pitch != 0)
        {
            Result const result = run_selected_pitch<T>(
                values, queries, repetitions, run_index++, forced_pitch);
            if (!valid(result))
                return false;
            print_result(result);
            continue;
        }

        if (stage == "pitch" || stage == "all" ||
            (stage == "threshold" && forced_pitch == 0))
        {
            auto run = [&]<std::size_t Pitch>()
            {
                Result const result = run_pitch<T, Pitch>(
                    values, queries, repetitions, run_index++);
                if (!valid(result))
                    return false;
                if (stage != "threshold")
                    print_result(result);
                if (forced_pitch == 0 &&
                    result.custom_ns_per_query < best_pitch_time)
                {
                    best_pitch = Pitch;
                    best_pitch_time = result.custom_ns_per_query;
                }
                return true;
            };
            passed = for_each_pitch(run);
        }

        if (!passed)
            return false;
        if (stage == "pitch")
            continue;

        auto run_thresholds = [&]<std::size_t Pitch>()
        {
            auto run_branchless = [&]<std::size_t Threshold, std::size_t P>()
            {
                if constexpr (Threshold < P)
                    return true;
                else
                {
                    if (Threshold > values.size())
                        return true;
                    Result const result = ::run_branchless<T, Threshold, P>(
                        values, queries, repetitions, run_index++);
                    if (!valid(result))
                        return false;
                    print_result(result);
                    return true;
                }
            };
            auto run_branched = [&]<std::size_t Threshold, std::size_t P>()
            {
                if constexpr (Threshold < P)
                    return true;
                else
                {
                    if (Threshold > values.size())
                        return true;
                    Result const result = ::run_branched<T, Threshold, P>(
                        values, queries, repetitions, run_index++);
                    if (!valid(result))
                        return false;
                    print_result(result);
                    return true;
                }
            };

            Result const linear = measure_configuration<
                T, pitch_traits<Pitch>>(
                values, queries, repetitions, "threshold", "linear",
                0, Pitch, run_index++);
            if (!valid(linear))
                return false;
            print_result(linear);

            if ((benchmark_seed & 1u) == 0)
                return for_each_threshold<Pitch>(run_branched) &&
                       for_each_threshold<Pitch>(run_branchless);
            return for_each_threshold<Pitch>(run_branchless) &&
                   for_each_threshold<Pitch>(run_branched);
        };

        if (best_pitch == 1)
            passed = run_thresholds.template operator()<1>();
        else if (best_pitch == 2)
            passed = run_thresholds.template operator()<2>();
        else if (best_pitch == 4)
            passed = run_thresholds.template operator()<4>();
        else if (best_pitch == 8)
            passed = run_thresholds.template operator()<8>();
        else if (best_pitch == 16)
            passed = run_thresholds.template operator()<16>();
        else if (best_pitch == 32)
            passed = run_thresholds.template operator()<32>();
        else if (best_pitch == 64)
            passed = run_thresholds.template operator()<64>();
        else if (best_pitch == 128)
            passed = run_thresholds.template operator()<128>();
        else if (best_pitch == 256)
            passed = run_thresholds.template operator()<256>();
        else if (best_pitch == 512)
            passed = run_thresholds.template operator()<512>();
        else
            passed = run_thresholds.template operator()<1024>();
        if (!passed)
            return false;
    }
    return true;
}
}

#if defined(POPCOUNT_RANGES_TUNING_SHARD_TYPE)
bool POPCOUNT_RANGES_TUNING_SHARD_FUNCTION(
    lower_bound_tuning::Options const& options,
    lower_bound_tuning::CacheFlusher& flusher,
    std::size_t processor_group, std::size_t processor,
    std::size_t detected_llc_bytes, std::size_t eviction_buffer_bytes)
{
    benchmark_seed = options.seed;
    repetitions_override = options.repetitions_override;
    selected_processor = options.selected_processor;
    dirty_cache_mode = options.dirty_cache_mode;
    llc_bytes = detected_llc_bytes;
    cache_flush_bytes = eviction_buffer_bytes;
    benchmark_processor_number = processor;
#if defined(_WIN32)
    benchmark_processor_group = static_cast<WORD>(processor_group);
#else
    (void)processor_group;
#endif
    cache_flusher = &flusher;

    return benchmark_type<POPCOUNT_RANGES_TUNING_SHARD_TYPE>(
        options.stage, options.quick, options.selected_size,
        options.forced_pitch);
}
#else
bool run_lower_bound_tuning_uint8(
    lower_bound_tuning::Options const&, lower_bound_tuning::CacheFlusher&,
    std::size_t, std::size_t, std::size_t, std::size_t);
bool run_lower_bound_tuning_uint16(
    lower_bound_tuning::Options const&, lower_bound_tuning::CacheFlusher&,
    std::size_t, std::size_t, std::size_t, std::size_t);
bool run_lower_bound_tuning_uint32(
    lower_bound_tuning::Options const&, lower_bound_tuning::CacheFlusher&,
    std::size_t, std::size_t, std::size_t, std::size_t);
bool run_lower_bound_tuning_uint64(
    lower_bound_tuning::Options const&, lower_bound_tuning::CacheFlusher&,
    std::size_t, std::size_t, std::size_t, std::size_t);
bool run_lower_bound_tuning_float(
    lower_bound_tuning::Options const&, lower_bound_tuning::CacheFlusher&,
    std::size_t, std::size_t, std::size_t, std::size_t);
bool run_lower_bound_tuning_double(
    lower_bound_tuning::Options const&, lower_bound_tuning::CacheFlusher&,
    std::size_t, std::size_t, std::size_t, std::size_t);

int main(int argc, char const* argv[])
{
    std::string_view stage = "all";
    std::string_view selected_type;
    std::size_t type_bits = 0;
    std::size_t selected_size = 0;
    std::size_t forced_pitch = 0;
    bool quick = false;

    for (int index = 1; index < argc; ++index)
    {
        std::string_view const argument(argv[index]);
        if (argument == "--quick")
        {
            quick = true;
        }
        else if ((argument == "--stage" || argument == "--type" ||
                  argument == "--type-bits" || argument == "--size" ||
                  argument == "--seed" ||
                  argument == "--pitch" || argument == "--repetitions" ||
                  argument == "--cache-flush-bytes" ||
                  argument == "--processor") &&
                 index + 1 < argc)
        {
            std::string_view const value(argv[++index]);
            if (argument == "--stage")
            {
                stage = value;
                if (stage != "all" && stage != "pitch" &&
                    stage != "threshold" && stage != "default")
                {
                    std::cerr << "invalid stage: " << value
                              << " (expected all, pitch, threshold, or default)\n";
                    return 2;
                }
                continue;
            }
            if (argument == "--type")
            {
                selected_type = value;
                continue;
            }

            std::size_t parsed = 0;
            auto const [end, error] = std::from_chars(
                value.data(), value.data() + value.size(), parsed);
            if (error != std::errc{} || end != value.data() + value.size())
            {
                std::cerr << "invalid value for " << argument << ": "
                          << value << '\n';
                return 2;
            }
            if (argument == "--size" && parsed == 0)
            {
                std::cerr << "size must be greater than zero\n";
                return 2;
            }
            if (argument == "--type-bits")
                type_bits = parsed;
            else if (argument == "--size")
                selected_size = parsed;
            else if (argument == "--pitch")
                forced_pitch = parsed;
            else if (argument == "--repetitions")
            {
                if (parsed == 0 || parsed > 100'000u)
                {
                    std::cerr << "repetitions must be in 1..100000\n";
                    return 2;
                }
                repetitions_override = parsed;
            }
            else if (argument == "--cache-flush-bytes")
            {
                if (parsed == 0)
                {
                    std::cerr << "cache-flush-bytes must be greater than zero\n";
                    return 2;
                }
                cache_flush_bytes = parsed;
            }
            else if (argument == "--processor")
                selected_processor = parsed;
            else
            {
                if (parsed > std::numeric_limits<std::uint32_t>::max())
                {
                    std::cerr << "seed must fit in uint32_t\n";
                    return 2;
                }
                benchmark_seed = static_cast<std::uint32_t>(parsed);
            }
        }
        else if (argument == "--dirty-cache")
        {
            dirty_cache_mode = true;
        }
        else
        {
            std::cerr << "usage: lower_bound_tuning "
                         "[--stage all|pitch|threshold|default] "
                         "[--type uint8_t|uint16_t|uint32_t|uint64_t|float|double] "
                         "[--type-bits 8|16|32|64] [--size N] [--pitch N] "
                         "[--seed N] "
                         "[--repetitions N] [--cache-flush-bytes N] "
                         "[--processor N] [--dirty-cache] "
                         "[--quick]\n";
            return 2;
        }
    }

    if (!selected_type.empty() &&
        selected_type != "uint8_t" && selected_type != "uint16_t" &&
        selected_type != "uint32_t" && selected_type != "uint64_t" &&
        selected_type != "float" && selected_type != "double")
    {
        std::cerr << "unknown primitive type: " << selected_type << '\n';
        return 2;
    }
    if (!selected_type.empty() && type_bits != 0)
    {
        std::cerr << "use either --type or --type-bits, not both\n";
        return 2;
    }
    if (type_bits != 0 && type_bits != 8 && type_bits != 16 &&
        type_bits != 32 && type_bits != 64)
    {
        std::cerr << "type width must be 8, 16, 32, or 64\n";
        return 2;
    }
    if (forced_pitch != 0 &&
        forced_pitch != 1 && forced_pitch != 2 && forced_pitch != 4 &&
        forced_pitch != 8 && forced_pitch != 16 && forced_pitch != 32 &&
        forced_pitch != 64 && forced_pitch != 128 && forced_pitch != 256 &&
        forced_pitch != 512 && forced_pitch != 1024)
    {
        std::cerr << "pitch must be one of 1, 2, 4, 8, 16, 32, 64, 128, "
                     "256, 512, 1024\n";
        return 2;
    }
#if defined(_WIN32) || defined(__linux__)
    std::unique_ptr<ThreadAffinity> thread_affinity;
    try
    {
        thread_affinity = std::make_unique<ThreadAffinity>();
        llc_bytes = detected_llc_bytes();
    }
    catch (std::runtime_error const& error)
    {
        std::cerr << "LLC detection failed: " << error.what() << '\n';
        return 2;
    }
    if (cache_flush_bytes == 0)
    {
        if (llc_bytes > std::numeric_limits<std::size_t>::max() / 2u)
        {
            std::cerr << "detected LLC size is too large\n";
            return 2;
        }
        cache_flush_bytes = llc_bytes * 2u;
    }
    else if (cache_flush_bytes <= llc_bytes)
    {
        std::cerr << "--cache-flush-bytes must exceed the detected LLC size ("
                  << llc_bytes << " bytes)\n";
        return 2;
    }
#else
    if (cache_flush_bytes == 0)
    {
        std::cerr << "LLC size detection is unsupported on this platform; "
                     "specify --cache-flush-bytes larger than the LLC\n";
        return 2;
    }
#endif
    CacheFlusher flusher(cache_flush_bytes, llc_bytes);
    std::cerr << "LLC=" << llc_bytes << " bytes; eviction buffer="
              << cache_flush_bytes << " bytes";
    if (dirty_cache_mode)
        std::cerr << "; LLC-sized dirty-write buffer=" << llc_bytes
                  << " bytes";
#if defined(_WIN32)
    std::cerr << "; pinned to processor group " << benchmark_processor_group
              << ", processor " << benchmark_processor_number;
#elif defined(__linux__)
    std::cerr << "; pinned to processor " << benchmark_processor_number;
#endif
    std::cerr << '\n';

    std::cout << "stage,seed,processor_group,processor,llc_bytes,"
                 "cache_flush_bytes,type,type_bits,N,"
                 "unique_query_count,query_passes,strategy,threshold,"
                 "sequential_pitch,"
                 "custom_ns_per_query,std_ns_per_query,"
                 "custom_ns_per_sample,std_ns_per_sample,checksum\n";
    lower_bound_tuning::Options const options{
        stage, selected_type, type_bits, selected_size, forced_pitch, quick,
        benchmark_seed, repetitions_override, selected_processor,
        dirty_cache_mode};
#if defined(_WIN32)
    std::size_t const processor_group = benchmark_processor_group;
#else
    std::size_t constexpr processor_group = 0;
#endif
    bool passed = true;
    if (selected_type == "uint8_t" ||
        (selected_type.empty() && (type_bits == 0 || type_bits == 8)))
        passed = run_lower_bound_tuning_uint8(
            options, flusher, processor_group, benchmark_processor_number,
            llc_bytes, cache_flush_bytes) && passed;
    if (selected_type == "uint16_t" ||
        (selected_type.empty() && (type_bits == 0 || type_bits == 16)))
        passed = run_lower_bound_tuning_uint16(
            options, flusher, processor_group, benchmark_processor_number,
            llc_bytes, cache_flush_bytes) && passed;
    if (selected_type == "uint32_t" ||
        (selected_type.empty() && (type_bits == 0 || type_bits == 32)))
        passed = run_lower_bound_tuning_uint32(
            options, flusher, processor_group, benchmark_processor_number,
            llc_bytes, cache_flush_bytes) && passed;
    if (selected_type == "uint64_t" ||
        (selected_type.empty() && (type_bits == 0 || type_bits == 64)))
        passed = run_lower_bound_tuning_uint64(
            options, flusher, processor_group, benchmark_processor_number,
            llc_bytes, cache_flush_bytes) && passed;
    if (selected_type == "float" ||
        (selected_type.empty() && type_bits == 0))
        passed = run_lower_bound_tuning_float(
            options, flusher, processor_group, benchmark_processor_number,
            llc_bytes, cache_flush_bytes) && passed;
    if (selected_type == "double" ||
        (selected_type.empty() && type_bits == 0))
        passed = run_lower_bound_tuning_double(
            options, flusher, processor_group, benchmark_processor_number,
            llc_bytes, cache_flush_bytes) && passed;

    if (!passed)
        return 1;
    return 0;
}
#endif
