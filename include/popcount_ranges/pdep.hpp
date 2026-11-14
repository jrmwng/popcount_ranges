#pragma once

#include <concepts>
#include <cstdint>
#include <type_traits>

#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
#if defined(_MSC_VER) || (defined(__clang__) && defined(_WIN32))
#include <intrin.h>
#endif
#endif

namespace jrmwng::popcount_ranges::stdx
{
    namespace detail
    {
        template <std::unsigned_integral T>
            requires (!std::same_as<T, bool> && sizeof(T) <= sizeof(std::uint64_t))
        constexpr T pdep_software(T value, T mask) noexcept
        {
            T result = 0;
            for (T destination = 1; mask != 0; destination = static_cast<T>(destination << 1))
            {
                if ((mask & destination) != 0)
                {
                    if ((value & 1) != 0)
                        result |= destination;
                    value = static_cast<T>(value >> 1);
                    mask &= static_cast<T>(mask - 1);
                }
            }
            return result;
        }

#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)
        inline bool bmi2_available() noexcept
        {
#if defined(_MSC_VER)
            static bool const available = []
            {
                int registers[4]{};
                __cpuidex(registers, 0, 0);
                if (registers[0] < 7)
                    return false;
                __cpuidex(registers, 7, 0);
                return (registers[1] & (1 << 8)) != 0;
            }();
            return available;
#else
            static bool const available = []
            {
                __builtin_cpu_init();
                return __builtin_cpu_supports("bmi2");
            }();
            return available;
#endif
        }

#if defined(_MSC_VER) && !defined(__clang__)
        __declspec(noinline) inline std::uint32_t pdep_bmi2(
            std::uint32_t value, std::uint32_t mask) noexcept
        {
            return _pdep_u32(value, mask);
        }
        __declspec(noinline) inline std::uint64_t pdep_bmi2(
            std::uint64_t value, std::uint64_t mask) noexcept
        {
            return _pdep_u64(value, mask);
        }
#else
        __attribute__((target("bmi2"), noinline))
        inline std::uint32_t pdep_bmi2(std::uint32_t value, std::uint32_t mask) noexcept
        {
            return _pdep_u32(value, mask);
        }
        __attribute__((target("bmi2"), noinline))
        inline std::uint64_t pdep_bmi2(std::uint64_t value, std::uint64_t mask) noexcept
        {
            return _pdep_u64(value, mask);
        }
#endif
#endif
    }

    template <std::unsigned_integral T>
        requires (!std::same_as<T, bool> && sizeof(T) <= sizeof(std::uint64_t))
    constexpr T pdep(T value, T mask) noexcept
    {
        if (std::is_constant_evaluated()) [[unlikely]]
            return detail::pdep_software(value, mask);

#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)
        if (detail::bmi2_available()) [[likely]]
        {
            if constexpr (sizeof(T) <= sizeof(std::uint32_t))
                return static_cast<T>(detail::pdep_bmi2(
                    static_cast<std::uint32_t>(value), static_cast<std::uint32_t>(mask)));
            else
                return static_cast<T>(detail::pdep_bmi2(
                    static_cast<std::uint64_t>(value), static_cast<std::uint64_t>(mask)));
        }
#endif
        return detail::pdep_software(value, mask);
    }
}
