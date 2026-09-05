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
        int popcount(const basic_simd_mask<Bytes, simd_abi_tuple<T, Tabis...>>& mask) noexcept;

        // ========== basic_simd_mask implementations ==========
        template <size_t Bytes, typename T, typename... Tabis>
        class basic_simd_mask<Bytes, simd_abi_tuple<T, Tabis...>>
        {
            friend int popcount<Bytes, T, Tabis...>(const basic_simd_mask<Bytes, simd_abi_tuple<T, Tabis...>>& mask) noexcept;

            using abi_type = simd_abi_tuple<T, Tabis...>;
            using tuple_type = std::tuple<basic_simd_mask<Bytes, Tabis>...>;

            template <size_t I, typename Tuple>
            static consteval size_t offset_of() {
                return ([]<size_t... Ks>(std::index_sequence<Ks...>) {
                    return (std::tuple_element_t<Ks, Tuple>::size + ... + 0u);
                })(std::make_index_sequence<I>{});
            }

            tuple_type m_tupleValue;
        public:
            static constexpr std::integral_constant<size_t, (basic_simd_mask<Bytes, Tabis>::size + ...)> size{};

            basic_simd_mask() = default;

            basic_simd_mask(const basic_simd_mask&) = default;
            basic_simd_mask& operator=(const basic_simd_mask&) = default;

            basic_simd_mask(basic_simd_mask&&) = default;
            basic_simd_mask& operator=(basic_simd_mask&&) = default;

            basic_simd_mask(std::tuple<basic_simd_mask<Bytes, Tabis>...> value) : m_tupleValue(value) {}
            explicit basic_simd_mask(bool v)
                : m_tupleValue(std::make_tuple(basic_simd_mask<Bytes, Tabis>(v)...))
            {}

            template <size_t N>
            basic_simd_mask(std::bitset<N> bs)
                : m_tupleValue
                (
                    [&]<size_t... Is>(std::index_sequence<Is...>) -> tuple_type
                    {
                        return tuple_type
                        (
                            std::tuple_element_t<Is, tuple_type>
                            (
                                // Extract relevant bits for this simd mask part.
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
                ((std::is_invocable_r_v<basic_simd_mask<Bytes, Tabis>, Tgenerator, basic_simd<T, Tabis>>) && ...)
            basic_simd_mask(Tgenerator&& gen)
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

            bool operator[](std::size_t i) const {
                size_t offset = 0;
                bool result = false;
                std::apply([&](auto const&... mask_parts) {
                    ((i < offset + basic_simd_mask<Bytes, std::decay_t<decltype(mask_parts)>>::size
                        ? result = mask_parts[i - offset]
                        : offset += basic_simd_mask<Bytes, std::decay_t<decltype(mask_parts)>>::size), ...);
                }, m_tupleValue);
                return result;
            }

            friend basic_simd_mask operator!(const basic_simd_mask& m) {
                return basic_simd_mask(
                    std::apply([&](auto const&... mask_parts) {
                        return std::make_tuple((!mask_parts)...);
                    }, m.m_tupleValue)
                );
            }

            friend basic_simd_mask operator&(const basic_simd_mask& a, const basic_simd_mask& b) {
                return basic_simd_mask(
                    std::apply([&](auto const&... a_parts) {
                        return std::apply([&](auto const&... b_parts) {
                            return std::make_tuple((a_parts & b_parts)...);
                        }, b.m_tupleValue);
                    }, a.m_tupleValue)
                );
            }

            friend basic_simd_mask operator|(const basic_simd_mask& a, const basic_simd_mask& b) {
                return basic_simd_mask(
                    std::apply([&](auto const&... a_parts) {
                        return std::apply([&](auto const&... b_parts) {
                            return std::make_tuple((a_parts | b_parts)...);
                        }, b.m_tupleValue);
                    }, a.m_tupleValue)
                );
            }

            friend basic_simd_mask operator^(const basic_simd_mask& a, const basic_simd_mask& b) {
                return basic_simd_mask(
                    std::apply([&](auto const&... a_parts) {
                        return std::apply([&](auto const&... b_parts) {
                            return std::make_tuple((a_parts ^ b_parts)...);
                        }, b.m_tupleValue);
                    }, a.m_tupleValue)
                );
            }

            basic_simd_mask& operator&=(const basic_simd_mask& other) {
                [&]<size_t... Is>(std::index_sequence<Is...>) {
                    ((std::get<Is>(m_tupleValue) &= std::get<Is>(other.m_tupleValue)), ...);
                }(std::make_index_sequence<sizeof...(Tabis)>{});
                return *this;
            }
            basic_simd_mask& operator|=(const basic_simd_mask& other) {
                [&]<size_t... Is>(std::index_sequence<Is...>) {
                    ((std::get<Is>(m_tupleValue) |= std::get<Is>(other.m_tupleValue)), ...);
                }(std::make_index_sequence<sizeof...(Tabis)>{});
                return *this;
            }
            basic_simd_mask& operator^=(const basic_simd_mask& other) {
                [&]<size_t... Is>(std::index_sequence<Is...>) {
                    ((std::get<Is>(m_tupleValue) ^= std::get<Is>(other.m_tupleValue)), ...);
                }(std::make_index_sequence<sizeof...(Tabis)>{});
                return *this;
            }
        };
        template <size_t Bytes, typename T, typename... Tabis>
        int popcount(const basic_simd_mask<Bytes, simd_abi_tuple<T, Tabis...>>& mask) noexcept {
            return std::apply([&](auto const&... mask_parts) {
                return (popcount(mask_parts) + ...);
            }, mask.m_tupleValue);
        }

        // ========== basic_simd implementations ==========

        template <typename T, typename... Tabis>
        class basic_simd<T, simd_abi_tuple<T, Tabis...>>
        {
            using mask_type = basic_simd_mask<sizeof(T), simd_abi_tuple<T, Tabis...>>;

            using tuple_type = std::tuple<basic_simd<T, Tabis>...>;
            tuple_type m_tupleValue;
        public:
            using value_type = T;
            using abi_type = simd_abi_tuple<T, Tabis...>;

            static constexpr std::integral_constant<size_t, (basic_simd<T, Tabis>::size + ...)> size{};

            basic_simd() = default;
            basic_simd(std::tuple<basic_simd<T, Tabis>...> tupleValue) : m_tupleValue(tupleValue) {}
            explicit basic_simd(T value)
                : m_tupleValue(std::make_tuple(basic_simd<T, Tabis>(value)...))
            {}

            template <typename... Flags>
            void copy_from(const T* mem, flags<Flags...>) {
                T const* ptr = mem;
                std::apply([&](auto&... simd_parts) {
                    ((simd_parts.copy_from(ptr, flags<Flags...>{}), ptr += std::decay_t<decltype(simd_parts)>::size), ...);
                }, m_tupleValue);
            }
            template <typename... Flags>
            void copy_to(T* mem, flags<Flags...>) const {
                T* ptr = mem;
                std::apply([&](auto const&... simd_parts) {
                    ((simd_parts.copy_to(ptr, flags<Flags...>{}), ptr += std::decay_t<decltype(simd_parts)>::size), ...);
                }, m_tupleValue);
            }

            T operator[](std::size_t i) const {
                size_t offset = 0;
                T result{};
                std::apply([&](auto const&... simd_parts) {
                    ((i < offset + std::decay_t<decltype(simd_parts)>::size
                        ? result = simd_parts[i - offset]
                        : offset += std::decay_t<decltype(simd_parts)>::size), ...);
                }, m_tupleValue);
                return result;
            }

            friend basic_simd operator+(const basic_simd& a, const basic_simd& b)
            {
                return basic_simd(
                    std::apply([&](auto const&... a_parts) {
                        return std::apply([&](auto const&... b_parts) {
                            return tuple_type((a_parts + b_parts)...);
                        }, b.m_tupleValue);
                    }, a.m_tupleValue)
                );
            }
            friend basic_simd operator-(const basic_simd& a, const basic_simd& b)
            {
                return basic_simd(
                    std::apply([&](auto const&... a_parts) {
                        return std::apply([&](auto const&... b_parts) {
                            return tuple_type((a_parts - b_parts)...);
                        }, b.m_tupleValue);
                    }, a.m_tupleValue)
                );
            }
            friend basic_simd operator*(const basic_simd& a, const basic_simd& b)
            requires requires { (std::declval<basic_simd<T, Tabis...>>() * std::declval<basic_simd<T, Tabis...>>()); }
            {
                return basic_simd(
                    std::apply([&](auto const&... a_parts) {
                        return std::apply([&](auto const&... b_parts) {
                            return tuple_type((a_parts * b_parts)...);
                        }, b.m_tupleValue);
                    }, a.m_tupleValue)
                );
            }
            friend basic_simd operator/(const basic_simd& a, const basic_simd& b)
            requires requires { (std::declval<basic_simd<T, Tabis...>>() / std::declval<basic_simd<T, Tabis...>>()); }
            {
                return basic_simd(
                    std::apply([&](auto const&... a_parts) {
                        return std::apply([&](auto const&... b_parts) {
                            return tuple_type((a_parts / b_parts)...);
                        }, b.m_tupleValue);
                    }, a.m_tupleValue)
                );
            }
            friend mask_type operator<(const basic_simd& a, const basic_simd& b)
            {
                return mask_type(
                    std::apply([&](auto const&... a_parts) {
                        return std::apply([&](auto const&... b_parts) {
                            return std::make_tuple((a_parts < b_parts)...);
                        }, b.m_tupleValue);
                    }, a.m_tupleValue)
                );
            }
            friend mask_type operator>(const basic_simd& a, const basic_simd& b)
            {
                return mask_type(
                    std::apply([&](auto const&... a_parts) {
                        return std::apply([&](auto const&... b_parts) {
                            return std::make_tuple((a_parts > b_parts)...);
                        }, b.m_tupleValue);
                    }, a.m_tupleValue)
                );
            }
            friend mask_type operator<=(const basic_simd& a, const basic_simd& b)
            {
                return mask_type(
                    std::apply([&](auto const&... a_parts) {
                        return std::apply([&](auto const&... b_parts) {
                            return std::make_tuple((a_parts <= b_parts)...);
                        }, b.m_tupleValue);
                    }, a.m_tupleValue)
                );
            }
            friend mask_type operator>=(const basic_simd& a, const basic_simd& b)
            {
                return mask_type(
                    std::apply([&](auto const&... a_parts) {
                        return std::apply([&](auto const&... b_parts) {
                            return std::make_tuple((a_parts >= b_parts)...);
                        }, b.m_tupleValue);
                    }, a.m_tupleValue)
                );
            }
            friend mask_type operator==(const basic_simd& a, const basic_simd& b)
            {
                return mask_type(
                    std::apply([&](auto const&... a_parts) {
                        return std::apply([&](auto const&... b_parts) {
                            return std::make_tuple((a_parts == b_parts)...);
                        }, b.m_tupleValue);
                    }, a.m_tupleValue)
                );
            }
            friend mask_type operator!=(const basic_simd& a, const basic_simd& b)
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