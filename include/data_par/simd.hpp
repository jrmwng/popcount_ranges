#pragma once

#include <cstddef>
#include <type_traits>
#include <ranges>

namespace jrmwng
{
    namespace data_par
    {
        // Forward declarations
        template <typename T, typename Abi> class basic_vec;
        template <size_t Bytes, typename Abi> class basic_mask;

        // Forward declarations for basic_vec operators
        template <typename T, typename Abi> basic_vec<T, Abi>& operator+=(basic_vec<T, Abi>&, const basic_vec<T, Abi>&) noexcept;
        template <typename T, typename Abi> basic_vec<T, Abi>& operator-=(basic_vec<T, Abi>&, const basic_vec<T, Abi>&) noexcept;
        template <typename T, typename Abi> basic_vec<T, Abi>& operator*=(basic_vec<T, Abi>&, const basic_vec<T, Abi>&) noexcept;
        template <typename T, typename Abi> basic_vec<T, Abi>& operator/=(basic_vec<T, Abi>&, const basic_vec<T, Abi>&) noexcept;
        template <typename T, typename Abi> basic_vec<T, Abi>& operator%=(basic_vec<T, Abi>&, const basic_vec<T, Abi>&) noexcept;
        template <typename T, typename Abi> basic_vec<T, Abi>& operator&=(basic_vec<T, Abi>&, const basic_vec<T, Abi>&) noexcept;
        template <typename T, typename Abi> basic_vec<T, Abi>& operator|=(basic_vec<T, Abi>&, const basic_vec<T, Abi>&) noexcept;
        template <typename T, typename Abi> basic_vec<T, Abi>& operator^=(basic_vec<T, Abi>&, const basic_vec<T, Abi>&) noexcept;
        template <typename T, typename Abi> basic_vec<T, Abi>& operator<<=(basic_vec<T, Abi>&, const basic_vec<T, Abi>&) noexcept;
        template <typename T, typename Abi> basic_vec<T, Abi>& operator>>=(basic_vec<T, Abi>&, const basic_vec<T, Abi>&) noexcept;
        template <typename T, typename Abi> basic_vec<T, Abi>& operator<<=(basic_vec<T, Abi>&, int) noexcept;
        template <typename T, typename Abi> basic_vec<T, Abi>& operator>>=(basic_vec<T, Abi>&, int) noexcept;

        template <typename T, typename Abi> basic_vec<T, Abi> operator+(const basic_vec<T, Abi>&, const basic_vec<T, Abi>&) noexcept;
        template <typename T, typename Abi> basic_vec<T, Abi> operator-(const basic_vec<T, Abi>&, const basic_vec<T, Abi>&) noexcept;
        template <typename T, typename Abi> basic_vec<T, Abi> operator*(const basic_vec<T, Abi>&, const basic_vec<T, Abi>&) noexcept;
        template <typename T, typename Abi> basic_vec<T, Abi> operator/(const basic_vec<T, Abi>&, const basic_vec<T, Abi>&) noexcept;
        template <typename T, typename Abi> basic_vec<T, Abi> operator%(const basic_vec<T, Abi>&, const basic_vec<T, Abi>&) noexcept;
        template <typename T, typename Abi> basic_vec<T, Abi> operator&(const basic_vec<T, Abi>&, const basic_vec<T, Abi>&) noexcept;
        template <typename T, typename Abi> basic_vec<T, Abi> operator|(const basic_vec<T, Abi>&, const basic_vec<T, Abi>&) noexcept;
        template <typename T, typename Abi> basic_vec<T, Abi> operator^(const basic_vec<T, Abi>&, const basic_vec<T, Abi>&) noexcept;
        template <typename T, typename Abi> basic_vec<T, Abi> operator<<(const basic_vec<T, Abi>&, const basic_vec<T, Abi>&) noexcept;
        template <typename T, typename Abi> basic_vec<T, Abi> operator>>(const basic_vec<T, Abi>&, const basic_vec<T, Abi>&) noexcept;
        template <typename T, typename Abi> basic_vec<T, Abi> operator<<(const basic_vec<T, Abi>&, int) noexcept;
        template <typename T, typename Abi> basic_vec<T, Abi> operator>>(const basic_vec<T, Abi>&, int) noexcept;

        template <typename T, typename Abi> basic_mask<sizeof(T), Abi> operator==(const basic_vec<T, Abi>&, const basic_vec<T, Abi>&) noexcept;
        template <typename T, typename Abi> basic_mask<sizeof(T), Abi> operator!=(const basic_vec<T, Abi>&, const basic_vec<T, Abi>&) noexcept;
        template <typename T, typename Abi> basic_mask<sizeof(T), Abi> operator<(const basic_vec<T, Abi>&, const basic_vec<T, Abi>&) noexcept;
        template <typename T, typename Abi> basic_mask<sizeof(T), Abi> operator<=(const basic_vec<T, Abi>&, const basic_vec<T, Abi>&) noexcept;
        template <typename T, typename Abi> basic_mask<sizeof(T), Abi> operator>(const basic_vec<T, Abi>&, const basic_vec<T, Abi>&) noexcept;
        template <typename T, typename Abi> basic_mask<sizeof(T), Abi> operator>=(const basic_vec<T, Abi>&, const basic_vec<T, Abi>&) noexcept;

        // Forward declarations for basic_mask operators
        template <size_t Bytes, typename Abi> basic_mask<Bytes, Abi>& operator&=(basic_mask<Bytes, Abi>&, const basic_mask<Bytes, Abi>&) noexcept;
        template <size_t Bytes, typename Abi> basic_mask<Bytes, Abi>& operator|=(basic_mask<Bytes, Abi>&, const basic_mask<Bytes, Abi>&) noexcept;
        template <size_t Bytes, typename Abi> basic_mask<Bytes, Abi>& operator^=(basic_mask<Bytes, Abi>&, const basic_mask<Bytes, Abi>&) noexcept;

        template <size_t Bytes, typename Abi> basic_mask<Bytes, Abi> operator&&(const basic_mask<Bytes, Abi>&, const basic_mask<Bytes, Abi>&) noexcept;
        template <size_t Bytes, typename Abi> basic_mask<Bytes, Abi> operator||(const basic_mask<Bytes, Abi>&, const basic_mask<Bytes, Abi>&) noexcept;
        template <size_t Bytes, typename Abi> basic_mask<Bytes, Abi> operator&(const basic_mask<Bytes, Abi>&, const basic_mask<Bytes, Abi>&) noexcept;
        template <size_t Bytes, typename Abi> basic_mask<Bytes, Abi> operator|(const basic_mask<Bytes, Abi>&, const basic_mask<Bytes, Abi>&) noexcept;
        template <size_t Bytes, typename Abi> basic_mask<Bytes, Abi> operator^(const basic_mask<Bytes, Abi>&, const basic_mask<Bytes, Abi>&) noexcept;

        template <size_t Bytes, typename Abi> basic_mask<Bytes, Abi> operator==(const basic_mask<Bytes, Abi>&, const basic_mask<Bytes, Abi>&) noexcept;
        template <size_t Bytes, typename Abi> basic_mask<Bytes, Abi> operator!=(const basic_mask<Bytes, Abi>&, const basic_mask<Bytes, Abi>&) noexcept;

        // Memory alignment flags for load/store operations
        struct convert_tag {};
        struct aligned_tag {};
        template <std::size_t N>
        struct overaligned_tag { inline constexpr static std::size_t alignment = N; };

        template <class... Flags> struct flags {};
        
        inline constexpr flags<> flag_default{};
        inline constexpr flags<convert_tag> flag_convert{};
        inline constexpr flags<aligned_tag> flag_aligned{};

        template <std::size_t N>
        inline constexpr flags<overaligned_tag<N>> flag_overaligned{};

        template <class V, std::contiguous_iterator I, class... Flags>
        V unchecked_load(I first, std::size_t n, flags<Flags...> f = {}) noexcept(noexcept(std::declval<V&>().copy_from(std::to_address(first), f)))
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
                for (std::size_t i = 0; i < n; ++i)
                {
                    tmp[i] = *(first + i);
                }

                v.copy_from(tmp, flag_aligned);
            }
            return v;
        }

        // Reductions
        template <size_t Bytes, typename Abi>
        int reduce_count(const basic_mask<Bytes, Abi>& mask) noexcept;

        template <size_t Bytes, typename Abi>
        bool all_of(const basic_mask<Bytes, Abi>& mask) noexcept
        {
            return reduce_count(mask) == basic_mask<Bytes, Abi>::size();
        }

        template <size_t Bytes, typename Abi>
        bool any_of(const basic_mask<Bytes, Abi>& mask) noexcept
        {
            return reduce_count(mask) != 0;
        }

        template <size_t Bytes, typename Abi>
        bool none_of(const basic_mask<Bytes, Abi>& mask) noexcept
        {
            return !any_of(mask);
        }

        template <size_t Bytes, typename Abi>
        std::ptrdiff_t reduce_min_index(const basic_mask<Bytes, Abi>& mask) noexcept
        {
            for (std::size_t index = 0; index < basic_mask<Bytes, Abi>::size(); ++index)
            {
                if (mask[index])
                    return static_cast<std::ptrdiff_t>(index);
            }
            return static_cast<std::ptrdiff_t>(basic_mask<Bytes, Abi>::size());
        }

        template <size_t Bytes, typename Abi>
        std::ptrdiff_t reduce_max_index(const basic_mask<Bytes, Abi>& mask) noexcept
        {
            for (std::size_t index = basic_mask<Bytes, Abi>::size(); index != 0; --index)
            {
                if (mask[index - 1])
                    return static_cast<std::ptrdiff_t>(index - 1);
            }
            return static_cast<std::ptrdiff_t>(basic_mask<Bytes, Abi>::size());
        }

    }
}
