#pragma once

#include "simd.hpp"
#include "simd_tuple.hpp"

#include <array>
#include <algorithm>
#include <bitset>

namespace jrmwng
{
    namespace data_par
    {
        // Scalar implementation of basic_simd for portability
        // This provides a fallback when SIMD instructions are not available

        // basic_simd<T, simd_abi_scalar> specialization
        template <typename T>
        class basic_simd<T, simd_abi_scalar<T>>
        {
            using reference = T&;
            using mask_type = basic_simd_mask<sizeof(T), simd_abi_scalar<T>>;

            T value;
        public:
            using value_type = T;
            using abi_type = simd_abi_scalar<T>;

            static constexpr std::integral_constant<size_t, 1> size{};

            // Constructors
            basic_simd() = default;
            basic_simd(T val) : value(val) {}

            // Broadcast constructor
            template <typename U>
            requires std::convertible_to<U, T> && (!std::invocable<U, std::integral_constant<std::size_t, 0>>)
            explicit basic_simd(U&& x) : value(static_cast<T>(std::forward<U>(x))) {}

            // Generator constructor
            template <typename G>
            requires std::invocable<G, std::integral_constant<std::size_t, 0>>
            explicit basic_simd(G&& gen) : value(gen(std::integral_constant<std::size_t, 0>{})) {}

            // Load from memory
            template <typename U, typename Flags>
            void copy_from(const U* mem, Flags) { value = static_cast<T>(*mem); }

            // Store to memory
            template <typename U, typename Flags>
            void copy_to(U* mem, Flags) const { *mem = static_cast<U>(value); }

            // Element access
            T operator[](std::size_t i) const { return value; }
            reference operator[](std::size_t i) { return value; }

            // Unary operators
            basic_simd operator+() const { return *this; }
            basic_simd operator-() const { return basic_simd(-value); }

            // Compound assignment operators
            basic_simd& operator+=(const basic_simd& other) { value += other.value; return *this; }
            basic_simd& operator-=(const basic_simd& other) { value -= other.value; return *this; }
            basic_simd& operator*=(const basic_simd& other) { value *= other.value; return *this; }
            basic_simd& operator/=(const basic_simd& other) { value /= other.value; return *this; }

            // Binary arithmetic operators for scalar simd
            friend basic_simd<T, simd_abi_scalar<T>> operator+(const basic_simd<T, simd_abi_scalar<T>>& a, const basic_simd<T, simd_abi_scalar<T>>& b) {
                return basic_simd<T, simd_abi_scalar<T>>(a.value + b.value);
            }

            friend basic_simd<T, simd_abi_scalar<T>> operator-(const basic_simd<T, simd_abi_scalar<T>>& a, const basic_simd<T, simd_abi_scalar<T>>& b) {
                return basic_simd<T, simd_abi_scalar<T>>(a.value - b.value);
            }

            friend basic_simd<T, simd_abi_scalar<T>> operator*(const basic_simd<T, simd_abi_scalar<T>>& a, const basic_simd<T, simd_abi_scalar<T>>& b) {
                return basic_simd<T, simd_abi_scalar<T>>(a.value * b.value);
            }

            friend basic_simd<T, simd_abi_scalar<T>> operator/(const basic_simd<T, simd_abi_scalar<T>>& a, const basic_simd<T, simd_abi_scalar<T>>& b) {
                return basic_simd<T, simd_abi_scalar<T>>(a.value / b.value);
            }

            // Comparison operators for scalar simd
            friend basic_simd_mask<sizeof(T), simd_abi_scalar<T>> operator==(const basic_simd<T, simd_abi_scalar<T>>& a, const basic_simd<T, simd_abi_scalar<T>>& b) {
                return basic_simd_mask<sizeof(T), simd_abi_scalar<T>>(a.value == b.value);
            }

            friend basic_simd_mask<sizeof(T), simd_abi_scalar<T>> operator!=(const basic_simd<T, simd_abi_scalar<T>>& a, const basic_simd<T, simd_abi_scalar<T>>& b) {
                return basic_simd_mask<sizeof(T), simd_abi_scalar<T>>(a.value != b.value);
            }

            friend basic_simd_mask<sizeof(T), simd_abi_scalar<T>> operator<(const basic_simd<T, simd_abi_scalar<T>>& a, const basic_simd<T, simd_abi_scalar<T>>& b) {
                return basic_simd_mask<sizeof(T), simd_abi_scalar<T>>(a.value < b.value);
            }

            friend basic_simd_mask<sizeof(T), simd_abi_scalar<T>> operator<=(const basic_simd<T, simd_abi_scalar<T>>& a, const basic_simd<T, simd_abi_scalar<T>>& b) {
                return basic_simd_mask<sizeof(T), simd_abi_scalar<T>>(a.value <= b.value);
            }

            friend basic_simd_mask<sizeof(T), simd_abi_scalar<T>> operator>(const basic_simd<T, simd_abi_scalar<T>>& a, const basic_simd<T, simd_abi_scalar<T>>& b) {
                return basic_simd_mask<sizeof(T), simd_abi_scalar<T>>(a.value > b.value);
            }

            friend basic_simd_mask<sizeof(T), simd_abi_scalar<T>> operator>=(const basic_simd<T, simd_abi_scalar<T>>& a, const basic_simd<T, simd_abi_scalar<T>>& b) {
                return basic_simd_mask<sizeof(T), simd_abi_scalar<T>>(a.value >= b.value);
            }

        };

        template <size_t Bytes, typename T>
        int popcount(const basic_simd_mask<Bytes, simd_abi_scalar<T>>& m) noexcept;

        // basic_simd_mask<T, simd_abi_scalar> specialization
        template <size_t Bytes, typename T>
        class basic_simd_mask<Bytes, simd_abi_scalar<T>>
        {
            friend int popcount<Bytes, T>(const basic_simd_mask<Bytes, simd_abi_scalar<T>>& m) noexcept;

            using value_type = bool;
            using reference = bool&;

            bool value;
        public:
            using abi_type = simd_abi_scalar<T>;

            static constexpr std::integral_constant<size_t, 1> const size{};

            // Constructors
            basic_simd_mask() = default;
            explicit basic_simd_mask(bool b) : value(b) {}

            template <size_t N>
            explicit basic_simd_mask(const std::bitset<N>& bs) noexcept : value(bs.test(0)) {}

            template <typename Tgenerator>
            requires std::is_invocable_r_v<bool, Tgenerator, std::integral_constant<size_t, 0>>
            explicit basic_simd_mask(Tgenerator&& gen) noexcept : value(gen(std::integral_constant<size_t, 0>{})) {}

            // Element access
            bool operator[](std::size_t i) const { return value; }
            reference operator[](std::size_t i) { return value; }

            // Logical operators
            basic_simd_mask operator!() const { return basic_simd_mask(!value); }
            basic_simd_mask operator&&(const basic_simd_mask& other) const { return basic_simd_mask(value && other.value); }
            basic_simd_mask operator||(const basic_simd_mask& other) const { return basic_simd_mask(value || other.value); }

            basic_simd_mask operator&(const basic_simd_mask& other) const { return basic_simd_mask(value & other.value); }
            basic_simd_mask operator|(const basic_simd_mask& other) const { return basic_simd_mask(value | other.value); }
            basic_simd_mask operator^(const basic_simd_mask& other) const { return basic_simd_mask(value != other.value); }

            basic_simd_mask& operator&=(const basic_simd_mask& other) { value &= other.value; return *this; }
            basic_simd_mask& operator|=(const basic_simd_mask& other) { value |= other.value; return *this; }
            basic_simd_mask& operator^=(const basic_simd_mask& other) { value ^= other.value; return *this; }
        };

        // Reductions for scalar basic_simd_mask
        template <size_t Bytes, typename T>
        int popcount(const basic_simd_mask<Bytes, simd_abi_scalar<T>>& m) noexcept {
            return m.value ? 1 : 0;
        }

        // Define native_simd aliases if no higher-priority implementation has claimed them
        #if !defined(JRMWNG_NATIVE_SIMD_DEFINED)

            // Use Scalar as native if compiler supports it
            template <typename T, typename Tabi = simd_abi_scalar<T>>
            class basic_simd;

            template <size_t Bytes, typename Tabi>
            class basic_simd_mask;

            template <typename T, size_t N = 1>
            using simd = basic_simd<
                T,
                std::conditional_t<
                    (N <= 1),
                    simd_abi_scalar<T>,
                    decltype([]<size_t... Is>(std::index_sequence<Is...>) ->
                        simd_abi_tuple<T, simd_abi_scalar<std::conditional_t<(Is == Is), T, T>>...> {
                            return {};
                        }(std::make_index_sequence<N>{})
                    )>
                >;
            
            #define JRMWNG_NATIVE_SIMD_DEFINED
        #endif
    }
}
