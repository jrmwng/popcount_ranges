#pragma once

#include "simd.hpp"
#include "simd_tuple.hpp"

#include <immintrin.h>

#include <bitset>
#include <cstring>
#include <cstdint>
#include <tuple>
#include <bit>

namespace jrmwng
{
    namespace data_par
    {
        // AVX2 ABI tag
        template <typename T>
        struct simd_abi_avx2
        {
        };

        // int8_t traits
        template <>
        struct simd_abi_avx2<int8_t> {
            using vector_type = __m256i;
            using mask_vector_type = __m256i;
            static constexpr size_t lanes = 32;
            static constexpr int mask_bits = 32;
            static constexpr unsigned int all_mask = ~0u;
            
            static vector_type set1(int8_t v) { return _mm256_set1_epi8(v); }
            static vector_type setzero() { return _mm256_setzero_si256(); }
            static vector_type load(const int8_t* p) { return _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p)); }
            static vector_type load_aligned(const int8_t* p) { return _mm256_load_si256(reinterpret_cast<const __m256i*>(p)); }
            static void store(int8_t* p, vector_type v) { _mm256_storeu_si256(reinterpret_cast<__m256i*>(p), v); }
            static void store_aligned(int8_t* p, vector_type v) { _mm256_store_si256(reinterpret_cast<__m256i*>(p), v); }
            
            static vector_type add(vector_type a, vector_type b) { return _mm256_add_epi8(a, b); }
            static vector_type sub(vector_type a, vector_type b) { return _mm256_sub_epi8(a, b); }
            
            static mask_vector_type cmpgt(vector_type a, vector_type b) { return _mm256_cmpgt_epi8(a, b); }
            static mask_vector_type cmpeq(vector_type a, vector_type b) { return _mm256_cmpeq_epi8(a, b); }
            
            static int movemask(mask_vector_type m) { return _mm256_movemask_epi8(m); }
            static mask_vector_type mask_set1(bool v) { return v ? _mm256_set1_epi8(-1) : _mm256_setzero_si256(); }
        };

        // uint8_t traits
        template <>
        struct simd_abi_avx2<uint8_t> {
            using vector_type = __m256i;
            using mask_vector_type = __m256i;
            static constexpr size_t lanes = 32;
            static constexpr int mask_bits = 32;
            static constexpr unsigned int all_mask = ~0u;
            
            static vector_type set1(uint8_t v) { return _mm256_set1_epi8(v); }
            static vector_type setzero() { return _mm256_setzero_si256(); }
            static vector_type load(const uint8_t* p) { return _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p)); }
            static vector_type load_aligned(const uint8_t* p) { return _mm256_load_si256(reinterpret_cast<const __m256i*>(p)); }
            static void store(uint8_t* p, vector_type v) { _mm256_storeu_si256(reinterpret_cast<__m256i*>(p), v); }
            static void store_aligned(uint8_t* p, vector_type v) { _mm256_store_si256(reinterpret_cast<__m256i*>(p), v); }
            
            static vector_type add(vector_type a, vector_type b) { return _mm256_add_epi8(a, b); }
            static vector_type sub(vector_type a, vector_type b) { return _mm256_sub_epi8(a, b); }
            
            // Unsigned comparison using sign-bit flip
            static mask_vector_type cmpgt(vector_type a, vector_type b) {
                __m256i sign = _mm256_set1_epi8(-128);
                return _mm256_cmpgt_epi8(_mm256_xor_si256(a, sign), _mm256_xor_si256(b, sign));
            }
            static mask_vector_type cmpeq(vector_type a, vector_type b) { return _mm256_cmpeq_epi8(a, b); }
            
            static int movemask(mask_vector_type m) { return _mm256_movemask_epi8(m); }
            static mask_vector_type mask_set1(bool v) { return v ? _mm256_set1_epi8(-1) : _mm256_setzero_si256(); }
        };

        // int16_t traits
        template <>
        struct simd_abi_avx2<int16_t> {
            using vector_type = __m256i;
            using mask_vector_type = __m256i;
            static constexpr size_t lanes = 16;
            static constexpr int mask_bits = 16;
            static constexpr unsigned int all_mask = 0xFFFF;
            
            static vector_type set1(int16_t v) { return _mm256_set1_epi16(v); }
            static vector_type setzero() { return _mm256_setzero_si256(); }
            static vector_type load(const int16_t* p) { return _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p)); }
            static vector_type load_aligned(const int16_t* p) { return _mm256_load_si256(reinterpret_cast<const __m256i*>(p)); }
            static void store(int16_t* p, vector_type v) { _mm256_storeu_si256(reinterpret_cast<__m256i*>(p), v); }
            static void store_aligned(int16_t* p, vector_type v) { _mm256_store_si256(reinterpret_cast<__m256i*>(p), v); }
            
            static vector_type add(vector_type a, vector_type b) { return _mm256_add_epi16(a, b); }
            static vector_type sub(vector_type a, vector_type b) { return _mm256_sub_epi16(a, b); }
            static vector_type mul(vector_type a, vector_type b) { return _mm256_mullo_epi16(a, b); }
            
            static mask_vector_type cmpgt(vector_type a, vector_type b) { return _mm256_cmpgt_epi16(a, b); }
            static mask_vector_type cmpeq(vector_type a, vector_type b) { return _mm256_cmpeq_epi16(a, b); }
            
            static int movemask(mask_vector_type m) {
                #ifdef __BMI2__
                unsigned int mask32 = _mm256_movemask_epi8(m);
                return static_cast<int>(_pext_u32(mask32, 0xAAAAAAAAu));
                #else
                unsigned int mask16 = _mm256_movemask_epi8(_mm256_permute4x64_epi64(_mm256_packs_epi16(m, _mm256_setzero_si256()), 0xD8));
                return static_cast<int>(mask16);
                #endif
            }
            static mask_vector_type mask_set1(bool v) { return v ? _mm256_set1_epi16(-1) : _mm256_setzero_si256(); }
        };

        // uint16_t traits
        template <>
        struct simd_abi_avx2<uint16_t> {
            using vector_type = __m256i;
            using mask_vector_type = __m256i;
            static constexpr size_t lanes = 16;
            static constexpr int mask_bits = 16;
            static constexpr unsigned int all_mask = 0xFFFF;
            
            static vector_type set1(uint16_t v) { return _mm256_set1_epi16(v); }
            static vector_type setzero() { return _mm256_setzero_si256(); }
            static vector_type load(const uint16_t* p) { return _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p)); }
            static vector_type load_aligned(const uint16_t* p) { return _mm256_load_si256(reinterpret_cast<const __m256i*>(p)); }
            static void store(uint16_t* p, vector_type v) { _mm256_storeu_si256(reinterpret_cast<__m256i*>(p), v); }
            static void store_aligned(uint16_t* p, vector_type v) { _mm256_store_si256(reinterpret_cast<__m256i*>(p), v); }
            
            static vector_type add(vector_type a, vector_type b) { return _mm256_add_epi16(a, b); }
            static vector_type sub(vector_type a, vector_type b) { return _mm256_sub_epi16(a, b); }
            static vector_type mul(vector_type a, vector_type b) { return _mm256_mullo_epi16(a, b); }
            
            static mask_vector_type cmpgt(vector_type a, vector_type b) {
                __m256i sign = _mm256_set1_epi16(-32768);
                return _mm256_cmpgt_epi16(_mm256_xor_si256(a, sign), _mm256_xor_si256(b, sign));
            }
            static mask_vector_type cmpeq(vector_type a, vector_type b) { return _mm256_cmpeq_epi16(a, b); }
            
            static int movemask(mask_vector_type m) {
                #ifdef __BMI2__
                unsigned int mask32 = _mm256_movemask_epi8(m);
                return static_cast<int>(_pext_u32(mask32, 0xAAAAAAAAu));
                #else
                unsigned int mask16 = _mm256_movemask_epi8(_mm256_permute4x64_epi64(_mm256_packs_epi16(m, _mm256_setzero_si256()), 0xD8));
                return static_cast<int>(mask16);
                #endif
            }
            static mask_vector_type mask_set1(bool v) { return v ? _mm256_set1_epi16(-1) : _mm256_setzero_si256(); }
        };

        // int32_t traits
        template <>
        struct simd_abi_avx2<int32_t> {
            using vector_type = __m256i;
            using mask_vector_type = __m256;
            static constexpr size_t lanes = 8;
            static constexpr int mask_bits = 8;
            static constexpr unsigned int all_mask = 0xFF;
            
            static vector_type set1(int32_t v) { return _mm256_set1_epi32(v); }
            static vector_type setzero() { return _mm256_setzero_si256(); }
            static vector_type load(const int32_t* p) { return _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p)); }
            static vector_type load_aligned(const int32_t* p) { return _mm256_load_si256(reinterpret_cast<const __m256i*>(p)); }
            static void store(int32_t* p, vector_type v) { _mm256_storeu_si256(reinterpret_cast<__m256i*>(p), v); }
            static void store_aligned(int32_t* p, vector_type v) { _mm256_store_si256(reinterpret_cast<__m256i*>(p), v); }
            
            static vector_type add(vector_type a, vector_type b) { return _mm256_add_epi32(a, b); }
            static vector_type sub(vector_type a, vector_type b) { return _mm256_sub_epi32(a, b); }
            static vector_type mul(vector_type a, vector_type b) { return _mm256_mullo_epi32(a, b); }
            
            static mask_vector_type cmpgt(vector_type a, vector_type b) { return _mm256_castsi256_ps(_mm256_cmpgt_epi32(a, b)); }
            static mask_vector_type cmpeq(vector_type a, vector_type b) { return _mm256_castsi256_ps(_mm256_cmpeq_epi32(a, b)); }
            
            static int movemask(mask_vector_type m) { return _mm256_movemask_ps(m); }
            static mask_vector_type mask_set1(bool v) { return _mm256_castsi256_ps(v ? _mm256_set1_epi32(-1) : _mm256_setzero_si256()); }
        };

        // uint32_t traits
        template <>
        struct simd_abi_avx2<uint32_t> {
            using vector_type = __m256i;
            using mask_vector_type = __m256;
            static constexpr size_t lanes = 8;
            static constexpr int mask_bits = 8;
            static constexpr unsigned int all_mask = 0xFF;
            
            static vector_type set1(uint32_t v) { return _mm256_set1_epi32(v); }
            static vector_type setzero() { return _mm256_setzero_si256(); }
            static vector_type load(const uint32_t* p) { return _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p)); }
            static vector_type load_aligned(const uint32_t* p) { return _mm256_load_si256(reinterpret_cast<const __m256i*>(p)); }
            static void store(uint32_t* p, vector_type v) { _mm256_storeu_si256(reinterpret_cast<__m256i*>(p), v); }
            static void store_aligned(uint32_t* p, vector_type v) { _mm256_store_si256(reinterpret_cast<__m256i*>(p), v); }
            
            static vector_type add(vector_type a, vector_type b) { return _mm256_add_epi32(a, b); }
            static vector_type sub(vector_type a, vector_type b) { return _mm256_sub_epi32(a, b); }
            static vector_type mul(vector_type a, vector_type b) { return _mm256_mullo_epi32(a, b); }
            
            static mask_vector_type cmpgt(vector_type a, vector_type b) {
                __m256i sign = _mm256_set1_epi32(0x80000000);
                return _mm256_castsi256_ps(_mm256_cmpgt_epi32(_mm256_xor_si256(a, sign), _mm256_xor_si256(b, sign)));
            }
            static mask_vector_type cmpeq(vector_type a, vector_type b) { return _mm256_castsi256_ps(_mm256_cmpeq_epi32(a, b)); }
            
            static int movemask(mask_vector_type m) { return _mm256_movemask_ps(m); }
            static mask_vector_type mask_set1(bool v) { return _mm256_castsi256_ps(v ? _mm256_set1_epi32(-1) : _mm256_setzero_si256()); }
        };

        // int64_t traits
        template <>
        struct simd_abi_avx2<int64_t> {
            using vector_type = __m256i;
            using mask_vector_type = __m256d;
            static constexpr size_t lanes = 4;
            static constexpr int mask_bits = 4;
            static constexpr unsigned int all_mask = 0xF;
            
            static vector_type set1(int64_t v) { return _mm256_set1_epi64x(v); }
            static vector_type setzero() { return _mm256_setzero_si256(); }
            static vector_type load(const int64_t* p) { return _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p)); }
            static vector_type load_aligned(const int64_t* p) { return _mm256_load_si256(reinterpret_cast<const __m256i*>(p)); }
            static void store(int64_t* p, vector_type v) { _mm256_storeu_si256(reinterpret_cast<__m256i*>(p), v); }
            static void store_aligned(int64_t* p, vector_type v) { _mm256_store_si256(reinterpret_cast<__m256i*>(p), v); }
            
            static vector_type add(vector_type a, vector_type b) { return _mm256_add_epi64(a, b); }
            static vector_type sub(vector_type a, vector_type b) { return _mm256_sub_epi64(a, b); }
            
            static mask_vector_type cmpgt(vector_type a, vector_type b) { return _mm256_castsi256_pd(_mm256_cmpgt_epi64(a, b)); }
            static mask_vector_type cmpeq(vector_type a, vector_type b) { return _mm256_castsi256_pd(_mm256_cmpeq_epi64(a, b)); }
            
            static int movemask(mask_vector_type m) { return _mm256_movemask_pd(m); }
            static mask_vector_type mask_set1(bool v) { return _mm256_castsi256_pd(v ? _mm256_set1_epi64x(-1) : _mm256_setzero_si256()); }
        };

        // uint64_t traits
        template <>
        struct simd_abi_avx2<uint64_t> {
            using vector_type = __m256i;
            using mask_vector_type = __m256d;
            static constexpr size_t lanes = 4;
            static constexpr int mask_bits = 4;
            static constexpr unsigned int all_mask = 0xF;
            
            static vector_type set1(uint64_t v) { return _mm256_set1_epi64x(v); }
            static vector_type setzero() { return _mm256_setzero_si256(); }
            static vector_type load(const uint64_t* p) { return _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p)); }
            static vector_type load_aligned(const uint64_t* p) { return _mm256_load_si256(reinterpret_cast<const __m256i*>(p)); }
            static void store(uint64_t* p, vector_type v) { _mm256_storeu_si256(reinterpret_cast<__m256i*>(p), v); }
            static void store_aligned(uint64_t* p, vector_type v) { _mm256_store_si256(reinterpret_cast<__m256i*>(p), v); }
            
            static vector_type add(vector_type a, vector_type b) { return _mm256_add_epi64(a, b); }
            static vector_type sub(vector_type a, vector_type b) { return _mm256_sub_epi64(a, b); }
            
            static mask_vector_type cmpgt(vector_type a, vector_type b) {
                __m256i sign = _mm256_set1_epi64x(0x8000000000000000ULL);
                return _mm256_castsi256_pd(_mm256_cmpgt_epi64(_mm256_xor_si256(a, sign), _mm256_xor_si256(b, sign)));
            }
            static mask_vector_type cmpeq(vector_type a, vector_type b) { return _mm256_castsi256_pd(_mm256_cmpeq_epi64(a, b)); }
            
            static int movemask(mask_vector_type m) { return _mm256_movemask_pd(m); }
            static mask_vector_type mask_set1(bool v) { return _mm256_castsi256_pd(v ? _mm256_set1_epi64x(-1) : _mm256_setzero_si256()); }
        };

        // float traits
        template <>
        struct simd_abi_avx2<float> {
            using vector_type = __m256;
            using mask_vector_type = __m256;
            static constexpr size_t lanes = 8;
            static constexpr int mask_bits = 8;
            static constexpr unsigned int all_mask = 0xFF;
            
            static vector_type set1(float v) { return _mm256_set1_ps(v); }
            static vector_type setzero() { return _mm256_setzero_ps(); }
            static vector_type load(const float* p) { return _mm256_loadu_ps(p); }
            static vector_type load_aligned(const float* p) { return _mm256_load_ps(p); }
            static void store(float* p, vector_type v) { _mm256_storeu_ps(p, v); }
            static void store_aligned(float* p, vector_type v) { _mm256_store_ps(p, v); }
            
            static vector_type add(vector_type a, vector_type b) { return _mm256_add_ps(a, b); }
            static vector_type sub(vector_type a, vector_type b) { return _mm256_sub_ps(a, b); }
            static vector_type mul(vector_type a, vector_type b) { return _mm256_mul_ps(a, b); }
            static vector_type div(vector_type a, vector_type b) { return _mm256_div_ps(a, b); }
            
            static mask_vector_type cmpgt(vector_type a, vector_type b) { return _mm256_cmp_ps(a, b, _CMP_GT_OQ); }
            static mask_vector_type cmplt(vector_type a, vector_type b) { return _mm256_cmp_ps(a, b, _CMP_LT_OQ); }
            static mask_vector_type cmple(vector_type a, vector_type b) { return _mm256_cmp_ps(a, b, _CMP_LE_OQ); }
            static mask_vector_type cmpge(vector_type a, vector_type b) { return _mm256_cmp_ps(a, b, _CMP_GE_OQ); }
            static mask_vector_type cmpeq(vector_type a, vector_type b) { return _mm256_cmp_ps(a, b, _CMP_EQ_OQ); }
            static mask_vector_type cmpneq(vector_type a, vector_type b) { return _mm256_cmp_ps(a, b, _CMP_NEQ_OQ); }
            
            static int movemask(mask_vector_type m) { return _mm256_movemask_ps(m); }
            static mask_vector_type mask_set1(bool v) { return _mm256_castsi256_ps(v ? _mm256_set1_epi32(-1) : _mm256_setzero_si256()); }
        };

        // double traits
        template <>
        struct simd_abi_avx2<double> {
            using vector_type = __m256d;
            using mask_vector_type = __m256d;
            static constexpr size_t lanes = 4;
            static constexpr int mask_bits = 4;
            static constexpr unsigned int all_mask = 0xF;
            
            static vector_type set1(double v) { return _mm256_set1_pd(v); }
            static vector_type setzero() { return _mm256_setzero_pd(); }
            static vector_type load(const double* p) { return _mm256_loadu_pd(p); }
            static vector_type load_aligned(const double* p) { return _mm256_load_pd(p); }
            static void store(double* p, vector_type v) { _mm256_storeu_pd(p, v); }
            static void store_aligned(double* p, vector_type v) { _mm256_store_pd(p, v); }
            
            static vector_type add(vector_type a, vector_type b) { return _mm256_add_pd(a, b); }
            static vector_type sub(vector_type a, vector_type b) { return _mm256_sub_pd(a, b); }
            static vector_type mul(vector_type a, vector_type b) { return _mm256_mul_pd(a, b); }
            static vector_type div(vector_type a, vector_type b) { return _mm256_div_pd(a, b); }
            
            static mask_vector_type cmpgt(vector_type a, vector_type b) { return _mm256_cmp_pd(a, b, _CMP_GT_OQ); }
            static mask_vector_type cmplt(vector_type a, vector_type b) { return _mm256_cmp_pd(a, b, _CMP_LT_OQ); }
            static mask_vector_type cmple(vector_type a, vector_type b) { return _mm256_cmp_pd(a, b, _CMP_LE_OQ); }
            static mask_vector_type cmpge(vector_type a, vector_type b) { return _mm256_cmp_pd(a, b, _CMP_GE_OQ); }
            static mask_vector_type cmpeq(vector_type a, vector_type b) { return _mm256_cmp_pd(a, b, _CMP_EQ_OQ); }
            static mask_vector_type cmpneq(vector_type a, vector_type b) { return _mm256_cmp_pd(a, b, _CMP_NEQ_OQ); }
            
            static int movemask(mask_vector_type m) { return _mm256_movemask_pd(m); }
            static mask_vector_type mask_set1(bool v) { return _mm256_castsi256_pd(v ? _mm256_set1_epi64x(-1) : _mm256_setzero_si256()); }
        };

        // ========== Generic MASK implementation helpers ==========

        // Helper template to provide mask operations
        template <size_t Bytes, typename T>
        class alignas(32) basic_mask<Bytes, simd_abi_avx2<T>>
        {
            static_assert(Bytes == sizeof(T), "Size mismatch between Bytes and type T");

            using vector_type = typename simd_abi_avx2<T>::mask_vector_type;
            using tuple_type = std::tuple<int32_t, vector_type>;

            static constexpr int32_t nMASK = simd_abi_avx2<T>::all_mask;

            template <typename Tgenerator>
            static constexpr int32_t compute_mask_value(Tgenerator&& gen)
            {
                return [&]<size_t... Is>(std::index_sequence<Is...>)
                {
                    return ((gen(std::integral_constant<size_t, Is>{}) ? (1 << Is) : 0) | ...);
                }(std::make_index_sequence<size>{});
            }

            static auto mask_not(vector_type v) {
                if constexpr (std::is_same_v<vector_type, __m256i>) {
                    return _mm256_xor_si256(v, _mm256_set1_epi8(-1));
                } else if constexpr (std::is_same_v<vector_type, __m256>) {
                    return _mm256_xor_ps(v, _mm256_castsi256_ps(_mm256_set1_epi32(-1)));
                } else {
                    return _mm256_xor_pd(v, _mm256_castsi256_pd(_mm256_set1_epi64x(-1)));
                }
            }

            static auto mask_and(vector_type a, vector_type b) {
                if constexpr (std::is_same_v<vector_type, __m256i>) {
                    return _mm256_and_si256(a, b);
                } else if constexpr (std::is_same_v<vector_type, __m256>) {
                    return _mm256_and_ps(a, b);
                } else {
                    return _mm256_and_pd(a, b);
                }
            }

            static auto mask_or(vector_type a, vector_type b) {
                if constexpr (std::is_same_v<vector_type, __m256i>) {
                    return _mm256_or_si256(a, b);
                } else if constexpr (std::is_same_v<vector_type, __m256>) {
                    return _mm256_or_ps(a, b);
                } else {
                    return _mm256_or_pd(a, b);
                }
            }

            static auto mask_xor(vector_type a, vector_type b) {
                if constexpr (std::is_same_v<vector_type, __m256i>) {
                    return _mm256_xor_si256(a, b);
                } else if constexpr (std::is_same_v<vector_type, __m256>) {
                    return _mm256_xor_ps(a, b);
                } else {
                    return _mm256_xor_pd(a, b);
                }
            }

            template <typename Tgenerator, size_t... Is>
            static basic_mask mask_from_generator(Tgenerator&& gen, std::index_sequence<Is...>)
            {
                if constexpr (std::is_invocable_r_v<basic_mask, Tgenerator, basic_vec<T, simd_abi_avx2<T>>>)
                {
                    alignas(32) constexpr static T const atIOTA[ size ]{ static_cast<T>(Is)... };

                    return gen(unchecked_load<basic_vec<T, simd_abi_avx2<T>>>(atIOTA, size, flag_aligned));
                }
                else
                {
                    return basic_mask
                    (
                        [&]<size_t... Js>(std::index_sequence<Js...>)
                        {
                            if constexpr (Bytes == 1) {
                                return _mm256_setr_epi8((gen(std::integral_constant<size_t, Js>{}) ? -1 : 0)...);
                            }
                            else if constexpr (Bytes == 2) {
                                return _mm256_setr_epi16((gen(std::integral_constant<size_t, Js>{}) ? -1 : 0)...);
                            }
                            else if constexpr (Bytes == 4) {
                                return _mm256_castsi256_ps(_mm256_setr_epi32((gen(std::integral_constant<size_t, Js>{}) ? -1 : 0)...));
                            }
                            else if constexpr (Bytes == 8) {
                                return _mm256_castsi256_pd(_mm256_setr_epi64x((gen(std::integral_constant<size_t, Js>{}) ? -1 : 0)...));
                            }
                        }
                        (std::make_index_sequence<size>{}),
                        compute_mask_value(gen)
                    );
                }
            }

            tuple_type m_tupleValue;
        public:
            using value_type = bool;
            using abi_type = simd_abi_avx2<T>;
            static constexpr std::integral_constant<size_t, simd_abi_avx2<T>::lanes> size{};

            template <typename U>
            U get() const noexcept
            {
                return std::get<U>(m_tupleValue);
            }

            basic_mask() = default;

            basic_mask(const basic_mask&) = default;
            basic_mask& operator=(const basic_mask&) = default;

            basic_mask(basic_mask&&) = default;
            basic_mask& operator=(basic_mask&&) = default;

            basic_mask(tuple_type value) noexcept : m_tupleValue(std::move(value)) {}
            basic_mask(vector_type vec, int32_t mask) noexcept
                : m_tupleValue(mask, vec)
            {}

            explicit basic_mask(vector_type vec) noexcept
                : m_tupleValue(simd_abi_avx2<T>::movemask(vec), vec)
            {}

            explicit basic_mask(bool value) noexcept
                : m_tupleValue
                (
                    value ? nMASK : 0,
                    [value]()->vector_type {
                        if constexpr (Bytes == 1) {
                            return _mm256_set1_epi8(value ? -1 : 0);
                        } else if constexpr (Bytes == 2) {
                            return _mm256_set1_epi16(value ? -1 : 0);
                        } else if constexpr (Bytes == 4) {
                            return _mm256_castsi256_ps(_mm256_set1_epi32(value ? -1 : 0));
                        } else if constexpr (Bytes == 8) {
                            return _mm256_castsi256_pd(_mm256_set1_epi64x(value ? -1 : 0));
                        }
                    }()
                )
            {}

            template <size_t N>
            explicit basic_mask(std::bitset<N> const & bs) noexcept
                : m_tupleValue
                (
                    static_cast<int32_t>((bs.to_ulong()) & nMASK),
                    [&bs]()->vector_type
                    {
                        if constexpr (Bytes == 1)
                        {
                            __m128i const xmmBitset = _mm_cvtsi32_si128(static_cast<int>(bs.to_ulong()));
                            __m256i const ymmBroadcast = _mm256_broadcastd_epi32(xmmBitset);
                            #if 0
                            __m256i const shift_count = _mm256_setr_epi32(7,6,5,4,3,2,1,0);
                            __m256i const ymmShifted = _mm256_sllv_epi32(ymmBroadcast, shift_count);

                            __m256i const shuffle_index = _mm256_setr_epi8(0,4,8,12,16,20,24,28,1,5,9,13,17,21,25,29,2,6,10,14,18,22,26,30,3,7,11,15,19,23,27,31);
                            __m256i const ymmShuffle = _mm256_shuffle_epi8(ymmShifted, shuffle_index);
                            return ymmShuffle;
                            #else
                            __m256i const shuffle_index = _mm256_setr_epi8
                            (
                                0, 0, 0, 0, 0, 0, 0, 0,
                                1, 1, 1, 1, 1, 1, 1, 1,
                                2, 2, 2, 2, 2, 2, 2, 2,
                                3, 3, 3, 3, 3, 3, 3, 3
                            );
                            __m256i const ymmShuffle = _mm256_shuffle_epi8(ymmBroadcast, shuffle_index);
                            __m256i const mask = _mm256_set1_epi64x(0x8040201008040201ULL);
                            __m256i const ymmAnd = _mm256_and_si256(mask, ymmShuffle);
                            __m256i const ymmCmp = _mm256_cmpeq_epi8(mask, ymmAnd);
                            return ymmCmp;
                            #endif
                        }
                        else if constexpr (Bytes == 2)
                        {
                            __m256i const ymmBitset = _mm256_set1_epi16(static_cast<int16_t>(bs.to_ulong()));
                            #if defined(__AVX512BW__) && defined(__AVX512VL__)
                            __m256i const shift_count = _mm256_setr_epi16(15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0);
                            __m256i const ymmShifted = _mm256_sllv_epi16(ymmBitset, shift_count);
                            return ymmShifted;
                            #else
                            __m256i const mask = _mm256_setr_epi16(0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000, -0x8000);
                            __m256i const ymmAnd = _mm256_and_si256(mask, ymmBitset);
                            __m256i const ymmCmp = _mm256_cmpeq_epi16(mask, ymmAnd);
                            return ymmCmp;
                            #endif
                        }
                        else if constexpr (Bytes == 4)
                        {
                            __m256i const ymmBitset = _mm256_set1_epi32(static_cast<int32_t>(bs.to_ulong()));
                            #if 0
                            __m256i const shift_count = _mm256_setr_epi32(31,30,29,28,27,26,25,24);
                            __m256i const ymmShifted = _mm256_sllv_epi32(ymmBitset, shift_count);
                            return _mm256_castsi256_ps(ymmShifted);
                            #else
                            __m256i const mask = _mm256_setr_epi32(0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80);
                            __m256i const ymmAnd = _mm256_and_si256(mask, ymmBitset);
                            __m256i const ymmCmp = _mm256_cmpeq_epi32(mask, ymmAnd);
                            return _mm256_castsi256_ps(ymmCmp);
                            #endif
                        }
                        else if constexpr (Bytes == 8)
                        {
                            __m256i const ymmBitset = _mm256_set1_epi64x((bs.to_ulong()));
                            #if 0
                            __m256i const shift_count = _mm256_setr_epi64x(63,62,61,60);
                            __m256i const ymmShifted = _mm256_sllv_epi64(ymmBitset, shift_count);
                            return _mm256_castsi256_pd(ymmShifted);
                            #else
                            __m256i const mask = _mm256_setr_epi64x(0x1, 0x2, 0x4, 0x8);
                            __m256i const ymmAnd = _mm256_and_si256(mask, ymmBitset);
                            __m256i const ymmCmp = _mm256_cmpeq_epi64(mask, ymmAnd);
                            return _mm256_castsi256_pd(ymmCmp);
                            #endif
                        }
                    }
                    ()
                )
            {
                static_assert(N >= size, "Bitset size is smaller than SIMD mask size");
            }

            template <typename Tgenerator>
            requires
                std::is_invocable_r_v<bool, Tgenerator, std::integral_constant<size_t, 0>> or
                std::is_invocable_r_v<basic_mask, Tgenerator, basic_vec<T, simd_abi_avx2<T>>>
            explicit basic_mask(Tgenerator&& gen)
                : m_tupleValue(mask_from_generator(std::forward<Tgenerator>(gen), std::make_index_sequence<size>{}).m_tupleValue)
            {}


            bool operator[](std::size_t i) const noexcept { return ((std::get<int32_t>(m_tupleValue) >> i) & 1) != 0; }

            friend basic_mask operator!(const basic_mask& a) noexcept {
                return basic_mask(mask_not(std::get<vector_type>(a.m_tupleValue)), std::get<int32_t>(a.m_tupleValue) ^ nMASK);
            }
            friend basic_mask operator&(const basic_mask& a, const basic_mask& b) noexcept {
                return basic_mask(mask_and(std::get<vector_type>(a.m_tupleValue), std::get<vector_type>(b.m_tupleValue)), std::get<int32_t>(a.m_tupleValue) & std::get<int32_t>(b.m_tupleValue));
            }
            friend basic_mask operator|(const basic_mask& a, const basic_mask& b) noexcept {
                return basic_mask(mask_or(std::get<vector_type>(a.m_tupleValue), std::get<vector_type>(b.m_tupleValue)), std::get<int32_t>(a.m_tupleValue) | std::get<int32_t>(b.m_tupleValue));
            }
            friend basic_mask operator^(const basic_mask& a, const basic_mask& b) noexcept {
                return basic_mask(mask_xor(std::get<vector_type>(a.m_tupleValue), std::get<vector_type>(b.m_tupleValue)), std::get<int32_t>(a.m_tupleValue) ^ std::get<int32_t>(b.m_tupleValue));
            }

            basic_mask& operator&=(const basic_mask& other) noexcept {
                std::get<vector_type>(m_tupleValue) = mask_and(std::get<vector_type>(m_tupleValue), std::get<vector_type>(other.m_tupleValue));
                std::get<int32_t>(m_tupleValue) &= std::get<int32_t>(other.m_tupleValue);
                return *this;
            }
            basic_mask& operator|=(const basic_mask& other) noexcept {
                std::get<vector_type>(m_tupleValue) = mask_or(std::get<vector_type>(m_tupleValue), std::get<vector_type>(other.m_tupleValue));
                std::get<int32_t>(m_tupleValue) |= std::get<int32_t>(other.m_tupleValue);
                return *this;
            }
            basic_mask& operator^=(const basic_mask& other) noexcept {
                std::get<vector_type>(m_tupleValue) = mask_xor(std::get<vector_type>(m_tupleValue), std::get<vector_type>(other.m_tupleValue));
                std::get<int32_t>(m_tupleValue) ^= std::get<int32_t>(other.m_tupleValue);
                return *this;
            }
        };
        template <size_t Bytes, typename T>
        int reduce_count(const basic_mask<Bytes, simd_abi_avx2<T>>& mask) noexcept {
            return std::popcount(static_cast<uint32_t>(mask.template get<int32_t>()));
        }

        // ========== Generic SIMD implementation ==========

        template <typename T>
        class basic_vec<T, simd_abi_avx2<T>>
        {
            using traits = simd_abi_avx2<T>;
            using vector_type = typename traits::vector_type;

            template <typename G, size_t... Indices>
            static vector_type generate(G&& generator, std::index_sequence<Indices...>)
            {
                alignas(32) T values[traits::lanes]{static_cast<T>(generator(std::integral_constant<size_t, Indices>{}))...};
                return traits::load_aligned(values);
            }

            vector_type m_value;
        public:
            using value_type = T;
            using mask_type = basic_mask<sizeof(T), simd_abi_avx2<T>>;
            using abi_type = simd_abi_avx2<T>;

            static constexpr std::integral_constant<size_t, traits::lanes> const size{};

            basic_vec() noexcept = default;
            basic_vec(vector_type value) noexcept : m_value(value) {}
            basic_vec(T value) noexcept : m_value(traits::set1(value)) {}

            template <typename G>
            requires std::invocable<G, std::integral_constant<size_t, 0>>
            explicit basic_vec(G&& generator)
                : m_value(generate(std::forward<G>(generator), std::make_index_sequence<traits::lanes>{}))
            {}

            template <typename... Flags>
            void copy_from(const T* mem, flags<Flags...>) noexcept {
                if constexpr (std::disjunction_v<std::is_same<Flags, aligned_tag>...>) {
                    m_value = traits::load_aligned(mem);
                } else {
                    m_value = traits::load(mem);
                }
            }

            template <typename... Flags>
            void copy_to(T* mem, flags<Flags...>) const noexcept {
                if constexpr (std::disjunction_v<std::is_same<Flags, aligned_tag>...>) {
                    traits::store_aligned(mem, m_value);
                } else {
                    traits::store(mem, m_value);
                }
            }

            T operator[](std::size_t i) const noexcept {
                alignas(32) T tmp[traits::lanes];
                traits::store_aligned(tmp, m_value);
                return tmp[i];
            }

            friend basic_vec operator+(const basic_vec& a, const basic_vec& b) noexcept {
                return basic_vec(traits::add(a.m_value, b.m_value));
            }

            friend basic_vec operator-(const basic_vec& a, const basic_vec& b) noexcept {
                return basic_vec(traits::sub(a.m_value, b.m_value));
            }

            friend basic_vec operator*(const basic_vec& a, const basic_vec& b) noexcept
            requires requires { traits::mul(a.m_value, b.m_value); }
            {
                return basic_vec(traits::mul(a.m_value, b.m_value));
            }

            friend basic_vec operator/(const basic_vec& a, const basic_vec& b) noexcept
            requires requires { traits::div(a.m_value, b.m_value); }
            {
                return basic_vec(traits::div(a.m_value, b.m_value));
            }

            friend mask_type operator<(const basic_vec& a, const basic_vec& b) noexcept {
                if constexpr (requires { traits::cmplt(a.m_value, b.m_value); }) {
                    return mask_type(traits::cmplt(a.m_value, b.m_value));
                } else {
                    return mask_type(traits::cmpgt(b.m_value, a.m_value));
                }
            }

            friend mask_type operator>(const basic_vec& a, const basic_vec& b) noexcept {
                return mask_type(traits::cmpgt(a.m_value, b.m_value));
            }

            friend mask_type operator<=(const basic_vec& a, const basic_vec& b) noexcept {
                if constexpr (requires { traits::cmple(a.m_value, b.m_value); }) {
                    return mask_type(traits::cmple(a.m_value, b.m_value));
                } else {
                    return !(a > b);
                }
            }

            friend mask_type operator>=(const basic_vec& a, const basic_vec& b) noexcept {
                if constexpr (requires { traits::cmpge(a.m_value, b.m_value); }) {
                    return mask_type(traits::cmpge(a.m_value, b.m_value));
                } else {
                    return !(a < b);
                }
            }

            friend mask_type operator==(const basic_vec& a, const basic_vec& b) noexcept {
                return mask_type(traits::cmpeq(a.m_value, b.m_value));
            }

            friend mask_type operator!=(const basic_vec& a, const basic_vec& b) noexcept {
                if constexpr (requires { traits::cmpneq(a.m_value, b.m_value); }) {
                    return mask_type(traits::cmpneq(a.m_value, b.m_value));
                } else {
                    return !(a == b);
                }
            }
        };

        // ========== reduce_count implementations ==========

        // Define native_simd aliases if no higher-priority implementation has claimed them
        #if !defined(JRMWNG_NATIVE_SIMD_DEFINED)

            // Use AVX2 as native if compiler supports it
            template <typename T, typename Tabi = simd_abi_avx2<T>>
            class basic_vec;

            template <size_t Bytes, typename Tabi>
            class basic_mask;

            template <typename T, size_t N = simd_abi_avx2<T>::lanes>
            using vec = basic_vec<
                T,
                std::conditional_t<
                    (N <= simd_abi_avx2<T>::lanes),
                    simd_abi_avx2<T>,
                    decltype([]<size_t... Is>(std::index_sequence<Is...>) ->
                        simd_abi_tuple<T, simd_abi_avx2<std::conditional_t<(Is == Is), T, T>>...> {
                            return {};
                        }(std::make_index_sequence<
                            (1u + (N - 1u) / simd_abi_avx2<T>::lanes)
                        >{})
                    )>
                >;

            template <typename T, size_t N = vec<T>::size()>
            using mask = typename vec<T, N>::mask_type;
            #define JRMWNG_NATIVE_SIMD_DEFINED
        #endif

    }
}
