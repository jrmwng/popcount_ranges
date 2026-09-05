#pragma once

#include <cstddef>
#include <type_traits>
#include <ranges>

namespace jrmwng
{
    namespace data_par
    {
        // Forward declarations
        template <typename T, typename Abi> class basic_simd;
        template <size_t Bytes, typename Abi> class basic_simd_mask;

        // Forward declarations for basic_simd operators
        template <typename T, typename Abi> basic_simd<T, Abi>& operator+=(basic_simd<T, Abi>&, const basic_simd<T, Abi>&);
        template <typename T, typename Abi> basic_simd<T, Abi>& operator-=(basic_simd<T, Abi>&, const basic_simd<T, Abi>&);
        template <typename T, typename Abi> basic_simd<T, Abi>& operator*=(basic_simd<T, Abi>&, const basic_simd<T, Abi>&);
        template <typename T, typename Abi> basic_simd<T, Abi>& operator/=(basic_simd<T, Abi>&, const basic_simd<T, Abi>&);
        template <typename T, typename Abi> basic_simd<T, Abi>& operator%=(basic_simd<T, Abi>&, const basic_simd<T, Abi>&);
        template <typename T, typename Abi> basic_simd<T, Abi>& operator&=(basic_simd<T, Abi>&, const basic_simd<T, Abi>&);
        template <typename T, typename Abi> basic_simd<T, Abi>& operator|=(basic_simd<T, Abi>&, const basic_simd<T, Abi>&);
        template <typename T, typename Abi> basic_simd<T, Abi>& operator^=(basic_simd<T, Abi>&, const basic_simd<T, Abi>&);
        template <typename T, typename Abi> basic_simd<T, Abi>& operator<<=(basic_simd<T, Abi>&, const basic_simd<T, Abi>&);
        template <typename T, typename Abi> basic_simd<T, Abi>& operator>>=(basic_simd<T, Abi>&, const basic_simd<T, Abi>&);
        template <typename T, typename Abi> basic_simd<T, Abi>& operator<<=(basic_simd<T, Abi>&, int);
        template <typename T, typename Abi> basic_simd<T, Abi>& operator>>=(basic_simd<T, Abi>&, int);

        template <typename T, typename Abi> basic_simd<T, Abi> operator+(const basic_simd<T, Abi>&, const basic_simd<T, Abi>&);
        template <typename T, typename Abi> basic_simd<T, Abi> operator-(const basic_simd<T, Abi>&, const basic_simd<T, Abi>&);
        template <typename T, typename Abi> basic_simd<T, Abi> operator*(const basic_simd<T, Abi>&, const basic_simd<T, Abi>&);
        template <typename T, typename Abi> basic_simd<T, Abi> operator/(const basic_simd<T, Abi>&, const basic_simd<T, Abi>&);
        template <typename T, typename Abi> basic_simd<T, Abi> operator%(const basic_simd<T, Abi>&, const basic_simd<T, Abi>&);
        template <typename T, typename Abi> basic_simd<T, Abi> operator&(const basic_simd<T, Abi>&, const basic_simd<T, Abi>&);
        template <typename T, typename Abi> basic_simd<T, Abi> operator|(const basic_simd<T, Abi>&, const basic_simd<T, Abi>&);
        template <typename T, typename Abi> basic_simd<T, Abi> operator^(const basic_simd<T, Abi>&, const basic_simd<T, Abi>&);
        template <typename T, typename Abi> basic_simd<T, Abi> operator<<(const basic_simd<T, Abi>&, const basic_simd<T, Abi>&);
        template <typename T, typename Abi> basic_simd<T, Abi> operator>>(const basic_simd<T, Abi>&, const basic_simd<T, Abi>&);
        template <typename T, typename Abi> basic_simd<T, Abi> operator<<(const basic_simd<T, Abi>&, int);
        template <typename T, typename Abi> basic_simd<T, Abi> operator>>(const basic_simd<T, Abi>&, int);

        template <typename T, typename Abi> basic_simd_mask<sizeof(T), Abi> operator==(const basic_simd<T, Abi>&, const basic_simd<T, Abi>&);
        template <typename T, typename Abi> basic_simd_mask<sizeof(T), Abi> operator!=(const basic_simd<T, Abi>&, const basic_simd<T, Abi>&);
        template <typename T, typename Abi> basic_simd_mask<sizeof(T), Abi> operator<(const basic_simd<T, Abi>&, const basic_simd<T, Abi>&);
        template <typename T, typename Abi> basic_simd_mask<sizeof(T), Abi> operator<=(const basic_simd<T, Abi>&, const basic_simd<T, Abi>&);
        template <typename T, typename Abi> basic_simd_mask<sizeof(T), Abi> operator>(const basic_simd<T, Abi>&, const basic_simd<T, Abi>&);
        template <typename T, typename Abi> basic_simd_mask<sizeof(T), Abi> operator>=(const basic_simd<T, Abi>&, const basic_simd<T, Abi>&);

        // Forward declarations for basic_simd_mask operators
        template <size_t Bytes, typename Abi> basic_simd_mask<Bytes, Abi>& operator&=(basic_simd_mask<Bytes, Abi>&, const basic_simd_mask<Bytes, Abi>&);
        template <size_t Bytes, typename Abi> basic_simd_mask<Bytes, Abi>& operator|=(basic_simd_mask<Bytes, Abi>&, const basic_simd_mask<Bytes, Abi>&);
        template <size_t Bytes, typename Abi> basic_simd_mask<Bytes, Abi>& operator^=(basic_simd_mask<Bytes, Abi>&, const basic_simd_mask<Bytes, Abi>&);

        template <size_t Bytes, typename Abi> basic_simd_mask<Bytes, Abi> operator&&(const basic_simd_mask<Bytes, Abi>&, const basic_simd_mask<Bytes, Abi>&);
        template <size_t Bytes, typename Abi> basic_simd_mask<Bytes, Abi> operator||(const basic_simd_mask<Bytes, Abi>&, const basic_simd_mask<Bytes, Abi>&);
        template <size_t Bytes, typename Abi> basic_simd_mask<Bytes, Abi> operator&(const basic_simd_mask<Bytes, Abi>&, const basic_simd_mask<Bytes, Abi>&);
        template <size_t Bytes, typename Abi> basic_simd_mask<Bytes, Abi> operator|(const basic_simd_mask<Bytes, Abi>&, const basic_simd_mask<Bytes, Abi>&);
        template <size_t Bytes, typename Abi> basic_simd_mask<Bytes, Abi> operator^(const basic_simd_mask<Bytes, Abi>&, const basic_simd_mask<Bytes, Abi>&);

        template <size_t Bytes, typename Abi> basic_simd_mask<Bytes, Abi> operator==(const basic_simd_mask<Bytes, Abi>&, const basic_simd_mask<Bytes, Abi>&);
        template <size_t Bytes, typename Abi> basic_simd_mask<Bytes, Abi> operator!=(const basic_simd_mask<Bytes, Abi>&, const basic_simd_mask<Bytes, Abi>&);

        // ABI tag types
        template <typename T>
        struct simd_abi_scalar
        {
        };

        // Memory alignment flags for load/store operations
        struct aligned_tag {};
        template <std::size_t N>
        struct overaligned_tag { inline constexpr static std::size_t alignment = N; };

        template <class... Flags> struct flags {};
        
        inline constexpr flags<> flag_default{};
        inline constexpr flags<aligned_tag> flag_aligned{};

        template <std::size_t N>
        inline constexpr flags<overaligned_tag<N>> flag_overaligned{};

        template <class V, std::contiguous_iterator I, class... Flags>
        V unchecked_load(I first, std::iter_difference_t<I> n, flags<Flags...> f = {})
        {
            V v;
            if (n >= V::size())
            {
                v.copy_from(std::to_address(first), f);
            }
            else
            {
                using T = typename V::value_type;
                alignas(alignof(V)) T tmp[V::size()]{}; // Zero-initialize the temporary array
                for (std::iter_difference_t<I> i = 0; i < n; ++i)
                {
                    tmp[i] = *(first + i);
                }

                v.copy_from(tmp, flag_aligned);
            }
            return v;
        }

        // Reductions
        template <size_t Bytes, typename Abi>
        int popcount(const basic_simd_mask<Bytes, Abi>& mask) noexcept;

    }
}
