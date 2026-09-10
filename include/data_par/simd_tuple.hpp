#pragma once

#include "simd.hpp"

#include <bitset>
#include <tuple>

namespace jrmwng
{
    namespace data_par
    {
        template <typename T, typename... Tabis>
        struct simd_abi_tuple
        {
            using type = std::tuple<Tabis...>;
        };

        template <size_t Bytes, typename T, typename... Tabis>
        int reduce_count(const basic_mask<Bytes, simd_abi_tuple<T, Tabis...>>& mask) noexcept;

        // ========== basic_mask implementations ==========
        template <size_t Bytes, typename T, typename... Tabis>
        class basic_mask<Bytes, simd_abi_tuple<T, Tabis...>>
        {
            friend int reduce_count<Bytes, T, Tabis...>(const basic_mask<Bytes, simd_abi_tuple<T, Tabis...>>& mask) noexcept;

            using tuple_type = std::tuple<basic_mask<Bytes, Tabis>...>;

            template <size_t I, typename Tuple>
            static consteval size_t offset_of() noexcept {
                return ([]<size_t... Ks>(std::index_sequence<Ks...>) {
                    return (std::tuple_element_t<Ks, Tuple>::size + ... + 0u);
                })(std::make_index_sequence<I>{});
            }

            tuple_type m_tupleValue;
        public:
            using value_type = bool;
            using abi_type = simd_abi_tuple<T, Tabis...>;
            static constexpr std::integral_constant<size_t, (basic_mask<Bytes, Tabis>::size + ...)> size{};

            basic_mask() noexcept = default;

            basic_mask(const basic_mask&) = default;
            basic_mask& operator=(const basic_mask&) = default;

            basic_mask(basic_mask&&) = default;
            basic_mask& operator=(basic_mask&&) = default;

            basic_mask(std::tuple<basic_mask<Bytes, Tabis>...> value) noexcept : m_tupleValue(value) {}
            explicit basic_mask(bool v) noexcept
                : m_tupleValue(std::make_tuple(basic_mask<Bytes, Tabis>(v)...))
            {}

            template <size_t N>
            basic_mask(std::bitset<N> bs)
                : m_tupleValue
                (
                    [&]<size_t... Is>(std::index_sequence<Is...>) -> tuple_type
                    {
                        return tuple_type
                        (
                            std::tuple_element_t<Is, tuple_type>
                            (
                                // Extract relevant bits for this vec mask part.
                                // Shift left to discard higher bits, then
                                // shift right to discard lower bits and align to LSB
                                // So that `to_ulong()` and `to_ullong()` can be used safely (at least for SSE4, AVX2 and AVX512 where N <= 64)
                                //
                                // In addition, modern compilers recognize this pattern and optimize accordingly
                                bs
                                << (N - std::tuple_element_t<Is, tuple_type>::size - offset_of<Is, tuple_type>())
                                >> (N - std::tuple_element_t<Is, tuple_type>::size)
                            )
                            ...
                        );
                    }
                    (std::index_sequence_for<Tabis...>{})
                )
            {
                static_assert(N >= size, "Bitset size is smaller than SIMD mask size");
            }

            template <typename Tgenerator>
            requires
                (std::is_invocable_r_v<bool, Tgenerator, std::integral_constant<size_t, 0>>) or
                ((std::is_invocable_r_v<basic_mask<Bytes, Tabis>, Tgenerator, basic_vec<T, Tabis>>) && ...)
            basic_mask(Tgenerator&& gen)
                : m_tupleValue
                (
                    [&]<size_t... Is>(std::index_sequence<Is...>) -> tuple_type
                    {
                        return tuple_type
                        (
                            std::tuple_element_t<Is, tuple_type>
                            (
                                [&](auto uIndex)
                                {
                                    return gen(uIndex + offset_of<Is, tuple_type>());
                                }
                            )...
                        );
                    }
                    (std::index_sequence_for<Tabis...>{})
                )
            {}

            bool operator[](std::size_t i) const noexcept {
                size_t offset = 0;
                bool result = false;
                std::apply([&](auto const&... mask_parts) {
                    ((i < offset + basic_mask<Bytes, std::decay_t<decltype(mask_parts)>>::size
                        ? result = mask_parts[i - offset]
                        : offset += basic_mask<Bytes, std::decay_t<decltype(mask_parts)>>::size), ...);
                }, m_tupleValue);
                return result;
            }

            friend basic_mask operator!(const basic_mask& m) noexcept {
                return basic_mask(
                    std::apply([&](auto const&... mask_parts) {
                        return std::make_tuple((!mask_parts)...);
                    }, m.m_tupleValue)
                );
            }

            friend basic_mask operator&(const basic_mask& a, const basic_mask& b) noexcept {
                return basic_mask(
                    std::apply([&](auto const&... a_parts) {
                        return std::apply([&](auto const&... b_parts) {
                            return std::make_tuple((a_parts & b_parts)...);
                        }, b.m_tupleValue);
                    }, a.m_tupleValue)
                );
            }

            friend basic_mask operator|(const basic_mask& a, const basic_mask& b) noexcept {
                return basic_mask(
                    std::apply([&](auto const&... a_parts) {
                        return std::apply([&](auto const&... b_parts) {
                            return std::make_tuple((a_parts | b_parts)...);
                        }, b.m_tupleValue);
                    }, a.m_tupleValue)
                );
            }

            friend basic_mask operator^(const basic_mask& a, const basic_mask& b) noexcept {
                return basic_mask(
                    std::apply([&](auto const&... a_parts) {
                        return std::apply([&](auto const&... b_parts) {
                            return std::make_tuple((a_parts ^ b_parts)...);
                        }, b.m_tupleValue);
                    }, a.m_tupleValue)
                );
            }

            basic_mask& operator&=(const basic_mask& other) noexcept {
                [&]<size_t... Is>(std::index_sequence<Is...>) {
                    ((std::get<Is>(m_tupleValue) &= std::get<Is>(other.m_tupleValue)), ...);
                }(std::make_index_sequence<sizeof...(Tabis)>{});
                return *this;
            }
            basic_mask& operator|=(const basic_mask& other) noexcept {
                [&]<size_t... Is>(std::index_sequence<Is...>) {
                    ((std::get<Is>(m_tupleValue) |= std::get<Is>(other.m_tupleValue)), ...);
                }(std::make_index_sequence<sizeof...(Tabis)>{});
                return *this;
            }
            basic_mask& operator^=(const basic_mask& other) noexcept {
                [&]<size_t... Is>(std::index_sequence<Is...>) {
                    ((std::get<Is>(m_tupleValue) ^= std::get<Is>(other.m_tupleValue)), ...);
                }(std::make_index_sequence<sizeof...(Tabis)>{});
                return *this;
            }
        };
        template <size_t Bytes, typename T, typename... Tabis>
        int reduce_count(const basic_mask<Bytes, simd_abi_tuple<T, Tabis...>>& mask) noexcept {
            return std::apply([&](auto const&... mask_parts) {
                return (reduce_count(mask_parts) + ...);
            }, mask.m_tupleValue);
        }

        // ========== basic_vec implementations ==========

        template <typename T, typename... Tabis>
        class basic_vec<T, simd_abi_tuple<T, Tabis...>>
        {
            using tuple_type = std::tuple<basic_vec<T, Tabis>...>;
            tuple_type m_tupleValue;
        public:
            using value_type = T;
            using mask_type = basic_mask<sizeof(T), simd_abi_tuple<T, Tabis...>>;
            using abi_type = simd_abi_tuple<T, Tabis...>;

            static constexpr std::integral_constant<size_t, (basic_vec<T, Tabis>::size + ...)> size{};

            basic_vec() noexcept = default;
            basic_vec(std::tuple<basic_vec<T, Tabis>...> tupleValue) noexcept : m_tupleValue(tupleValue) {}
            explicit basic_vec(T value) noexcept
                : m_tupleValue(std::make_tuple(basic_vec<T, Tabis>(value)...))
            {}

            template <typename... Flags>
            void copy_from(const T* mem, flags<Flags...>) noexcept {
                T const* ptr = mem;
                std::apply([&](auto&... simd_parts) {
                    ((simd_parts.copy_from(ptr, flags<Flags...>{}), ptr += std::decay_t<decltype(simd_parts)>::size), ...);
                }, m_tupleValue);
            }
            template <typename... Flags>
            void copy_to(T* mem, flags<Flags...>) const noexcept {
                T* ptr = mem;
                std::apply([&](auto const&... simd_parts) {
                    ((simd_parts.copy_to(ptr, flags<Flags...>{}), ptr += std::decay_t<decltype(simd_parts)>::size), ...);
                }, m_tupleValue);
            }

            T operator[](std::size_t i) const noexcept {
                size_t offset = 0;
                T result{};
                std::apply([&](auto const&... simd_parts) {
                    ((i < offset + std::decay_t<decltype(simd_parts)>::size
                        ? result = simd_parts[i - offset]
                        : offset += std::decay_t<decltype(simd_parts)>::size), ...);
                }, m_tupleValue);
                return result;
            }

            friend basic_vec operator+(const basic_vec& a, const basic_vec& b) noexcept
            {
                return basic_vec(
                    std::apply([&](auto const&... a_parts) {
                        return std::apply([&](auto const&... b_parts) {
                            return tuple_type((a_parts + b_parts)...);
                        }, b.m_tupleValue);
                    }, a.m_tupleValue)
                );
            }
            friend basic_vec operator-(const basic_vec& a, const basic_vec& b) noexcept
            {
                return basic_vec(
                    std::apply([&](auto const&... a_parts) {
                        return std::apply([&](auto const&... b_parts) {
                            return tuple_type((a_parts - b_parts)...);
                        }, b.m_tupleValue);
                    }, a.m_tupleValue)
                );
            }
            friend basic_vec operator*(const basic_vec& a, const basic_vec& b) noexcept
            requires requires { (std::declval<basic_vec<T, Tabis...>>() * std::declval<basic_vec<T, Tabis...>>()); }
            {
                return basic_vec(
                    std::apply([&](auto const&... a_parts) {
                        return std::apply([&](auto const&... b_parts) {
                            return tuple_type((a_parts * b_parts)...);
                        }, b.m_tupleValue);
                    }, a.m_tupleValue)
                );
            }
            friend basic_vec operator/(const basic_vec& a, const basic_vec& b) noexcept
            requires requires { (std::declval<basic_vec<T, Tabis...>>() / std::declval<basic_vec<T, Tabis...>>()); }
            {
                return basic_vec(
                    std::apply([&](auto const&... a_parts) {
                        return std::apply([&](auto const&... b_parts) {
                            return tuple_type((a_parts / b_parts)...);
                        }, b.m_tupleValue);
                    }, a.m_tupleValue)
                );
            }
            friend mask_type operator<(const basic_vec& a, const basic_vec& b) noexcept
            {
                return mask_type(
                    std::apply([&](auto const&... a_parts) {
                        return std::apply([&](auto const&... b_parts) {
                            return std::make_tuple((a_parts < b_parts)...);
                        }, b.m_tupleValue);
                    }, a.m_tupleValue)
                );
            }
            friend mask_type operator>(const basic_vec& a, const basic_vec& b) noexcept
            {
                return mask_type(
                    std::apply([&](auto const&... a_parts) {
                        return std::apply([&](auto const&... b_parts) {
                            return std::make_tuple((a_parts > b_parts)...);
                        }, b.m_tupleValue);
                    }, a.m_tupleValue)
                );
            }
            friend mask_type operator<=(const basic_vec& a, const basic_vec& b) noexcept
            {
                return mask_type(
                    std::apply([&](auto const&... a_parts) {
                        return std::apply([&](auto const&... b_parts) {
                            return std::make_tuple((a_parts <= b_parts)...);
                        }, b.m_tupleValue);
                    }, a.m_tupleValue)
                );
            }
            friend mask_type operator>=(const basic_vec& a, const basic_vec& b) noexcept
            {
                return mask_type(
                    std::apply([&](auto const&... a_parts) {
                        return std::apply([&](auto const&... b_parts) {
                            return std::make_tuple((a_parts >= b_parts)...);
                        }, b.m_tupleValue);
                    }, a.m_tupleValue)
                );
            }
            friend mask_type operator==(const basic_vec& a, const basic_vec& b) noexcept
            {
                return mask_type(
                    std::apply([&](auto const&... a_parts) {
                        return std::apply([&](auto const&... b_parts) {
                            return std::make_tuple((a_parts == b_parts)...);
                        }, b.m_tupleValue);
                    }, a.m_tupleValue)
                );
            }
            friend mask_type operator!=(const basic_vec& a, const basic_vec& b) noexcept
            {
                return mask_type(
                    std::apply([&](auto const&... a_parts) {
                        return std::apply([&](auto const&... b_parts) {
                            return std::make_tuple((a_parts != b_parts)...);
                        }, b.m_tupleValue);
                    }, a.m_tupleValue)
                );
            }

        };
    }
}