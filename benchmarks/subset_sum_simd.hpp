#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <immintrin.h>
#include <type_traits>

template <typename T>
using subset_sum_result = std::conditional_t<std::is_floating_point_v<T>, float,
    std::conditional_t<std::is_signed_v<T>, std::int64_t, std::uint64_t>>;

template <typename T, std::size_t N>
class subset_sum_simd
{
    static_assert(N <= 32);
    static_assert(std::is_same_v<T, float> || (std::is_integral_v<T> && sizeof(T) <= 4));
    std::array<T, ((N + 7) / 8) * 8> weights_{};

public:
    using result_type = subset_sum_result<T>;

    explicit subset_sum_simd(std::array<T, N> const& weights) noexcept
    {
        for (std::size_t i = 0; i < N; ++i)
            weights_[i] = weights[i];
    }

    result_type operator()(std::uint32_t mask) const noexcept
    {
        __m256i const broadcast = _mm256_set1_epi32(std::bit_cast<std::int32_t>(mask));
        __m256i integer_total = _mm256_setzero_si256();
        __m256 float_total = _mm256_setzero_ps();
        for (std::size_t offset = 0; offset < N; offset += 8)
        {
            __m256i const shifts = _mm256_setr_epi32(
                static_cast<int>(offset), static_cast<int>(offset + 1),
                static_cast<int>(offset + 2), static_cast<int>(offset + 3),
                static_cast<int>(offset + 4), static_cast<int>(offset + 5),
                static_cast<int>(offset + 6), static_cast<int>(offset + 7));
            __m256i const selected = _mm256_cmpeq_epi32(
                _mm256_and_si256(_mm256_srlv_epi32(broadcast, shifts), _mm256_set1_epi32(1)),
                _mm256_set1_epi32(1));
            if constexpr (std::is_same_v<T, float>)
            {
                __m256 const values = _mm256_loadu_ps(weights_.data() + offset);
                float_total = _mm256_add_ps(float_total, _mm256_and_ps(values, _mm256_castsi256_ps(selected)));
            }
            else
            {
                __m256i values;
                if constexpr (sizeof(T) == 1)
                {
                    __m128i const packed = _mm_loadl_epi64(
                        reinterpret_cast<__m128i const*>(weights_.data() + offset));
                    if constexpr (std::is_signed_v<T>)
                        values = _mm256_cvtepi8_epi32(packed);
                    else
                        values = _mm256_cvtepu8_epi32(packed);
                }
                else if constexpr (sizeof(T) == 2)
                {
                    __m128i const packed = _mm_loadu_si128(
                        reinterpret_cast<__m128i const*>(weights_.data() + offset));
                    if constexpr (std::is_signed_v<T>)
                        values = _mm256_cvtepi16_epi32(packed);
                    else
                        values = _mm256_cvtepu16_epi32(packed);
                }
                else
                    values = _mm256_loadu_si256(
                        reinterpret_cast<__m256i const*>(weights_.data() + offset));
                values = _mm256_and_si256(values, selected);
                __m128i const low = _mm256_castsi256_si128(values);
                __m128i const high = _mm256_extracti128_si256(values, 1);
                if constexpr (std::is_signed_v<T>)
                {
                    integer_total = _mm256_add_epi64(integer_total, _mm256_cvtepi32_epi64(low));
                    integer_total = _mm256_add_epi64(integer_total, _mm256_cvtepi32_epi64(high));
                }
                else
                {
                    integer_total = _mm256_add_epi64(integer_total, _mm256_cvtepu32_epi64(low));
                    integer_total = _mm256_add_epi64(integer_total, _mm256_cvtepu32_epi64(high));
                }
            }
        }
        if constexpr (std::is_same_v<T, float>)
        {
            __m128 total = _mm_add_ps(_mm256_castps256_ps128(float_total),
                                       _mm256_extractf128_ps(float_total, 1));
            total = _mm_hadd_ps(total, total);
            total = _mm_hadd_ps(total, total);
            return _mm_cvtss_f32(total);
        }
        else
        {
            __m128i const halves = _mm_add_epi64(
                _mm256_castsi256_si128(integer_total), _mm256_extracti128_si256(integer_total, 1));
            __m128i const total = _mm_add_epi64(halves, _mm_unpackhi_epi64(halves, halves));
            return static_cast<result_type>(_mm_cvtsi128_si64(total));
        }
    }
};

template <std::size_t N>
class subset_sum_simd<std::uint8_t, N>
{
    static_assert(N <= 32);
    __m256i weights_;

public:
    using result_type = subset_sum_result<std::uint8_t>;

    explicit subset_sum_simd(std::array<std::uint8_t, N> const& weights) noexcept
    {
        std::array<std::uint8_t, 32> padded{};
        for (std::size_t i = 0; i < N; ++i)
            padded[i] = weights[i];
        weights_ = _mm256_loadu_si256(reinterpret_cast<__m256i const*>(padded.data()));
    }

    result_type operator()(std::uint32_t mask) const noexcept
    {
        __m256i const bytes = _mm256_shuffle_epi8(
            _mm256_set1_epi32(std::bit_cast<std::int32_t>(mask)),
            _mm256_setr_epi8(
                0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,
                2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3));
        __m256i const bits = _mm256_setr_epi8(
            1,2,4,8,16,32,64,-128,1,2,4,8,16,32,64,-128,
            1,2,4,8,16,32,64,-128,1,2,4,8,16,32,64,-128);
        __m256i const absent = _mm256_cmpeq_epi8(
            _mm256_and_si256(bytes, bits), _mm256_setzero_si256());
        __m256i const selected = _mm256_andnot_si256(absent, weights_);
        __m256i const partial = _mm256_sad_epu8(selected, _mm256_setzero_si256());
        __m128i const halves = _mm_add_epi64(
            _mm256_castsi256_si128(partial), _mm256_extracti128_si256(partial, 1));
        __m128i const total = _mm_add_epi64(halves, _mm_unpackhi_epi64(halves, halves));
        return static_cast<result_type>(_mm_cvtsi128_si64(total));
    }
};
