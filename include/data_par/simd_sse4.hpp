#pragma once

#include "simd.hpp"
#include "simd_tuple.hpp"

#include <smmintrin.h>

#include <bitset>
#include <cstring>
#include <cstdint>
#include <tuple>
#include <bit>

namespace jrmwng
{
    namespace data_par
    {
        // SSE4 ABI tag
        template <typename T>
        struct simd_abi_sse4
        {
        };

        // int8_t traits
        template <>
        struct simd_abi_sse4<int8_t> {
            using vector_type = __m128i;
            using mask_vector_type = __m128i;
            static constexpr size_t lanes = 16;
            static constexpr int mask_bits = 16;
            static constexpr unsigned int all_mask = 0xFFFF;
            
            static vector_type set1(int8_t v) { return _mm_set1_epi8(v); }
            static vector_type setzero() { return _mm_setzero_si128(); }
            static vector_type load(const int8_t* p) { return _mm_loadu_si128(reinterpret_cast<const __m128i*>(p)); }
            static vector_type load_aligned(const int8_t* p) { return _mm_load_si128(reinterpret_cast<const __m128i*>(p)); }
            static void store(int8_t* p, vector_type v) { _mm_storeu_si128(reinterpret_cast<__m128i*>(p), v); }
            static void store_aligned(int8_t* p, vector_type v) { _mm_store_si128(reinterpret_cast<__m128i*>(p), v); }
            
            static vector_type add(vector_type a, vector_type b) { return _mm_add_epi8(a, b); }
            static vector_type sub(vector_type a, vector_type b) { return _mm_sub_epi8(a, b); }
            
            static mask_vector_type cmpgt(vector_type a, vector_type b) { return _mm_cmpgt_epi8(a, b); }
            static mask_vector_type cmpeq(vector_type a, vector_type b) { return _mm_cmpeq_epi8(a, b); }
            
            static int movemask(mask_vector_type m) { return _mm_movemask_epi8(m); }
            static mask_vector_type mask_set1(bool v) { return v ? _mm_set1_epi8(-1) : _mm_setzero_si128(); }
        };

        // uint8_t traits
        template <>
        struct simd_abi_sse4<uint8_t> {
            using vector_type = __m128i;
            using mask_vector_type = __m128i;
            static constexpr size_t lanes = 16;
            static constexpr int mask_bits = 16;
            static constexpr unsigned int all_mask = 0xFFFF;
            
            static vector_type set1(uint8_t v) { return _mm_set1_epi8(v); }
            static vector_type setzero() { return _mm_setzero_si128(); }
            static vector_type load(const uint8_t* p) { return _mm_loadu_si128(reinterpret_cast<const __m128i*>(p)); }
            static vector_type load_aligned(const uint8_t* p) { return _mm_load_si128(reinterpret_cast<const __m128i*>(p)); }
            static void store(uint8_t* p, vector_type v) { _mm_storeu_si128(reinterpret_cast<__m128i*>(p), v); }
            static void store_aligned(uint8_t* p, vector_type v) { _mm_store_si128(reinterpret_cast<__m128i*>(p), v); }
            
            static vector_type add(vector_type a, vector_type b) { return _mm_add_epi8(a, b); }
            static vector_type sub(vector_type a, vector_type b) { return _mm_sub_epi8(a, b); }
            
            // Unsigned comparison using sign-bit flip
            static mask_vector_type cmpgt(vector_type a, vector_type b) {
                __m128i sign = _mm_set1_epi8(-128);
                return _mm_cmpgt_epi8(_mm_xor_si128(a, sign), _mm_xor_si128(b, sign));
            }
            static mask_vector_type cmpeq(vector_type a, vector_type b) { return _mm_cmpeq_epi8(a, b); }
            
            static int movemask(mask_vector_type m) { return _mm_movemask_epi8(m); }
            static mask_vector_type mask_set1(bool v) { return v ? _mm_set1_epi8(-1) : _mm_setzero_si128(); }
        };

        // int16_t traits
        template <>
        struct simd_abi_sse4<int16_t> {
            using vector_type = __m128i;
            using mask_vector_type = __m128i;
            static constexpr size_t lanes = 8;
            static constexpr int mask_bits = 8;
            static constexpr unsigned int all_mask = 0xFF;
            
            static vector_type set1(int16_t v) { return _mm_set1_epi16(v); }
            static vector_type setzero() { return _mm_setzero_si128(); }
            static vector_type load(const int16_t* p) { return _mm_loadu_si128(reinterpret_cast<const __m128i*>(p)); }
            static vector_type load_aligned(const int16_t* p) { return _mm_load_si128(reinterpret_cast<const __m128i*>(p)); }
            static void store(int16_t* p, vector_type v) { _mm_storeu_si128(reinterpret_cast<__m128i*>(p), v); }
            static void store_aligned(int16_t* p, vector_type v) { _mm_store_si128(reinterpret_cast<__m128i*>(p), v); }
            
            static vector_type add(vector_type a, vector_type b) { return _mm_add_epi16(a, b); }
            static vector_type sub(vector_type a, vector_type b) { return _mm_sub_epi16(a, b); }
            static vector_type mul(vector_type a, vector_type b) { return _mm_mullo_epi16(a, b); }
            
            static mask_vector_type cmpgt(vector_type a, vector_type b) { return _mm_cmpgt_epi16(a, b); }
            static mask_vector_type cmpeq(vector_type a, vector_type b) { return _mm_cmpeq_epi16(a, b); }
            
            static int movemask(mask_vector_type m) {
                // Pack 16-bit mask to 8-bit, then use movemask_epi8
                __m128i packed = _mm_packs_epi16(m, m);
                return _mm_movemask_epi8(packed) & 0xFF;
            }
            static mask_vector_type mask_set1(bool v) { return v ? _mm_set1_epi16(-1) : _mm_setzero_si128(); }
        };

        // uint16_t traits
        template <>
        struct simd_abi_sse4<uint16_t> {
            using vector_type = __m128i;
            using mask_vector_type = __m128i;
            static constexpr size_t lanes = 8;
            static constexpr int mask_bits = 8;
            static constexpr unsigned int all_mask = 0xFF;
            
            static vector_type set1(uint16_t v) { return _mm_set1_epi16(v); }
            static vector_type setzero() { return _mm_setzero_si128(); }
            static vector_type load(const uint16_t* p) { return _mm_loadu_si128(reinterpret_cast<const __m128i*>(p)); }
            static vector_type load_aligned(const uint16_t* p) { return _mm_load_si128(reinterpret_cast<const __m128i*>(p)); }
            static void store(uint16_t* p, vector_type v) { _mm_storeu_si128(reinterpret_cast<__m128i*>(p), v); }
            static void store_aligned(uint16_t* p, vector_type v) { _mm_store_si128(reinterpret_cast<__m128i*>(p), v); }
            
            static vector_type add(vector_type a, vector_type b) { return _mm_add_epi16(a, b); }
            static vector_type sub(vector_type a, vector_type b) { return _mm_sub_epi16(a, b); }
            static vector_type mul(vector_type a, vector_type b) { return _mm_mullo_epi16(a, b); }
            
            static mask_vector_type cmpgt(vector_type a, vector_type b) {
                __m128i sign = _mm_set1_epi16(-32768);
                return _mm_cmpgt_epi16(_mm_xor_si128(a, sign), _mm_xor_si128(b, sign));
            }
            static mask_vector_type cmpeq(vector_type a, vector_type b) { return _mm_cmpeq_epi16(a, b); }
            
            static int movemask(mask_vector_type m) {
                __m128i packed = _mm_packs_epi16(m, m);
                return _mm_movemask_epi8(packed) & 0xFF;
            }
            static mask_vector_type mask_set1(bool v) { return v ? _mm_set1_epi16(-1) : _mm_setzero_si128(); }
        };

        // int32_t traits
        template <>
        struct simd_abi_sse4<int32_t> {
            using vector_type = __m128i;
            using mask_vector_type = __m128;
            static constexpr size_t lanes = 4;
            static constexpr int mask_bits = 4;
            static constexpr unsigned int all_mask = 0xF;
            
            static vector_type set1(int32_t v) { return _mm_set1_epi32(v); }
            static vector_type setzero() { return _mm_setzero_si128(); }
            static vector_type load(const int32_t* p) { return _mm_loadu_si128(reinterpret_cast<const __m128i*>(p)); }
            static vector_type load_aligned(const int32_t* p) { return _mm_load_si128(reinterpret_cast<const __m128i*>(p)); }
            static void store(int32_t* p, vector_type v) { _mm_storeu_si128(reinterpret_cast<__m128i*>(p), v); }
            static void store_aligned(int32_t* p, vector_type v) { _mm_store_si128(reinterpret_cast<__m128i*>(p), v); }
            
            static vector_type add(vector_type a, vector_type b) { return _mm_add_epi32(a, b); }
            static vector_type sub(vector_type a, vector_type b) { return _mm_sub_epi32(a, b); }
            static vector_type mul(vector_type a, vector_type b) { return _mm_mullo_epi32(a, b); }
            
            static mask_vector_type cmpgt(vector_type a, vector_type b) { return _mm_castsi128_ps(_mm_cmpgt_epi32(a, b)); }
            static mask_vector_type cmpeq(vector_type a, vector_type b) { return _mm_castsi128_ps(_mm_cmpeq_epi32(a, b)); }
            
            static int movemask(mask_vector_type m) { return _mm_movemask_ps(m); }
            static mask_vector_type mask_set1(bool v) { return _mm_castsi128_ps(v ? _mm_set1_epi32(-1) : _mm_setzero_si128()); }
        };

        // uint32_t traits
        template <>
        struct simd_abi_sse4<uint32_t> {
            using vector_type = __m128i;
            using mask_vector_type = __m128;
            static constexpr size_t lanes = 4;
            static constexpr int mask_bits = 4;
            static constexpr unsigned int all_mask = 0xF;
            
            static vector_type set1(uint32_t v) { return _mm_set1_epi32(v); }
            static vector_type setzero() { return _mm_setzero_si128(); }
            static vector_type load(const uint32_t* p) { return _mm_loadu_si128(reinterpret_cast<const __m128i*>(p)); }
            static vector_type load_aligned(const uint32_t* p) { return _mm_load_si128(reinterpret_cast<const __m128i*>(p)); }
            static void store(uint32_t* p, vector_type v) { _mm_storeu_si128(reinterpret_cast<__m128i*>(p), v); }
            static void store_aligned(uint32_t* p, vector_type v) { _mm_store_si128(reinterpret_cast<__m128i*>(p), v); }
            
            static vector_type add(vector_type a, vector_type b) { return _mm_add_epi32(a, b); }
            static vector_type sub(vector_type a, vector_type b) { return _mm_sub_epi32(a, b); }
            static vector_type mul(vector_type a, vector_type b) { return _mm_mullo_epi32(a, b); }
            
            static mask_vector_type cmpgt(vector_type a, vector_type b) {
                __m128i sign = _mm_set1_epi32(0x80000000);
                return _mm_castsi128_ps(_mm_cmpgt_epi32(_mm_xor_si128(a, sign), _mm_xor_si128(b, sign)));
            }
            static mask_vector_type cmpeq(vector_type a, vector_type b) { return _mm_castsi128_ps(_mm_cmpeq_epi32(a, b)); }
            
            static int movemask(mask_vector_type m) { return _mm_movemask_ps(m); }
            static mask_vector_type mask_set1(bool v) { return _mm_castsi128_ps(v ? _mm_set1_epi32(-1) : _mm_setzero_si128()); }
        };

        // int64_t traits
        template <>
        struct simd_abi_sse4<int64_t> {
            using vector_type = __m128i;
            using mask_vector_type = __m128d;
            static constexpr size_t lanes = 2;
            static constexpr int mask_bits = 2;
            static constexpr unsigned int all_mask = 0x3;
            
            static vector_type set1(int64_t v) { return _mm_set1_epi64x(v); }
            static vector_type setzero() { return _mm_setzero_si128(); }
            static vector_type load(const int64_t* p) { return _mm_loadu_si128(reinterpret_cast<const __m128i*>(p)); }
            static vector_type load_aligned(const int64_t* p) { return _mm_load_si128(reinterpret_cast<const __m128i*>(p)); }
            static void store(int64_t* p, vector_type v) { _mm_storeu_si128(reinterpret_cast<__m128i*>(p), v); }
            static void store_aligned(int64_t* p, vector_type v) { _mm_store_si128(reinterpret_cast<__m128i*>(p), v); }
            
            static vector_type add(vector_type a, vector_type b) { return _mm_add_epi64(a, b); }
            static vector_type sub(vector_type a, vector_type b) { return _mm_sub_epi64(a, b); }
            
            static mask_vector_type cmpgt(vector_type a, vector_type b) { return _mm_castsi128_pd(_mm_cmpgt_epi64(a, b)); }
            static mask_vector_type cmpeq(vector_type a, vector_type b) { return _mm_castsi128_pd(_mm_cmpeq_epi64(a, b)); }
            
            static int movemask(mask_vector_type m) { return _mm_movemask_pd(m); }
            static mask_vector_type mask_set1(bool v) { return _mm_castsi128_pd(v ? _mm_set1_epi64x(-1) : _mm_setzero_si128()); }
        };

        // uint64_t traits
        template <>
        struct simd_abi_sse4<uint64_t> {
            using vector_type = __m128i;
            using mask_vector_type = __m128d;
            static constexpr size_t lanes = 2;
            static constexpr int mask_bits = 2;
            static constexpr unsigned int all_mask = 0x3;
            
            static vector_type set1(uint64_t v) { return _mm_set1_epi64x(v); }
            static vector_type setzero() { return _mm_setzero_si128(); }
            static vector_type load(const uint64_t* p) { return _mm_loadu_si128(reinterpret_cast<const __m128i*>(p)); }
            static vector_type load_aligned(const uint64_t* p) { return _mm_load_si128(reinterpret_cast<const __m128i*>(p)); }
            static void store(uint64_t* p, vector_type v) { _mm_storeu_si128(reinterpret_cast<__m128i*>(p), v); }
            static void store_aligned(uint64_t* p, vector_type v) { _mm_store_si128(reinterpret_cast<__m128i*>(p), v); }
            
            static vector_type add(vector_type a, vector_type b) { return _mm_add_epi64(a, b); }
            static vector_type sub(vector_type a, vector_type b) { return _mm_sub_epi64(a, b); }
            
            static mask_vector_type cmpgt(vector_type a, vector_type b) {
                __m128i sign = _mm_set1_epi64x(0x8000000000000000ULL);
                return _mm_castsi128_pd(_mm_cmpgt_epi64(_mm_xor_si128(a, sign), _mm_xor_si128(b, sign)));
            }
            static mask_vector_type cmpeq(vector_type a, vector_type b) { return _mm_castsi128_pd(_mm_cmpeq_epi64(a, b)); }
            
            static int movemask(mask_vector_type m) { return _mm_movemask_pd(m); }
            static mask_vector_type mask_set1(bool v) { return _mm_castsi128_pd(v ? _mm_set1_epi64x(-1) : _mm_setzero_si128()); }
        };

        // float traits
        template <>
        struct simd_abi_sse4<float> {
            using vector_type = __m128;
            using mask_vector_type = __m128;
            static constexpr size_t lanes = 4;
            static constexpr int mask_bits = 4;
            static constexpr unsigned int all_mask = 0xF;
            
            static vector_type set1(float v) { return _mm_set1_ps(v); }
            static vector_type setzero() { return _mm_setzero_ps(); }
            static vector_type load(const float* p) { return _mm_loadu_ps(p); }
            static vector_type load_aligned(const float* p) { return _mm_load_ps(p); }
            static void store(float* p, vector_type v) { _mm_storeu_ps(p, v); }
            static void store_aligned(float* p, vector_type v) { _mm_store_ps(p, v); }
            
            static vector_type add(vector_type a, vector_type b) { return _mm_add_ps(a, b); }
            static vector_type sub(vector_type a, vector_type b) { return _mm_sub_ps(a, b); }
            static vector_type mul(vector_type a, vector_type b) { return _mm_mul_ps(a, b); }
            static vector_type div(vector_type a, vector_type b) { return _mm_div_ps(a, b); }
            
            static mask_vector_type cmpgt(vector_type a, vector_type b) { return _mm_cmpgt_ps(a, b); }
            static mask_vector_type cmplt(vector_type a, vector_type b) { return _mm_cmplt_ps(a, b); }
            static mask_vector_type cmple(vector_type a, vector_type b) { return _mm_cmple_ps(a, b); }
            static mask_vector_type cmpge(vector_type a, vector_type b) { return _mm_cmpge_ps(a, b); }
            static mask_vector_type cmpeq(vector_type a, vector_type b) { return _mm_cmpeq_ps(a, b); }
            static mask_vector_type cmpneq(vector_type a, vector_type b) { return _mm_cmpneq_ps(a, b); }
            
            static int movemask(mask_vector_type m) { return _mm_movemask_ps(m); }
            static mask_vector_type mask_set1(bool v) { return _mm_castsi128_ps(v ? _mm_set1_epi32(-1) : _mm_setzero_si128()); }
        };

        // double traits
        template <>
        struct simd_abi_sse4<double> {
            using vector_type = __m128d;
            using mask_vector_type = __m128d;
            static constexpr size_t lanes = 2;
            static constexpr int mask_bits = 2;
            static constexpr unsigned int all_mask = 0x3;
            
            static vector_type set1(double v) { return _mm_set1_pd(v); }
            static vector_type setzero() { return _mm_setzero_pd(); }
            static vector_type load(const double* p) { return _mm_loadu_pd(p); }
            static vector_type load_aligned(const double* p) { return _mm_load_pd(p); }
            static void store(double* p, vector_type v) { _mm_storeu_pd(p, v); }
            static void store_aligned(double* p, vector_type v) { _mm_store_pd(p, v); }
            
            static vector_type add(vector_type a, vector_type b) { return _mm_add_pd(a, b); }
            static vector_type sub(vector_type a, vector_type b) { return _mm_sub_pd(a, b); }
            static vector_type mul(vector_type a, vector_type b) { return _mm_mul_pd(a, b); }
            static vector_type div(vector_type a, vector_type b) { return _mm_div_pd(a, b); }
            
            static mask_vector_type cmpgt(vector_type a, vector_type b) { return _mm_cmpgt_pd(a, b); }
            static mask_vector_type cmplt(vector_type a, vector_type b) { return _mm_cmplt_pd(a, b); }
            static mask_vector_type cmple(vector_type a, vector_type b) { return _mm_cmple_pd(a, b); }
            static mask_vector_type cmpge(vector_type a, vector_type b) { return _mm_cmpge_pd(a, b); }
            static mask_vector_type cmpeq(vector_type a, vector_type b) { return _mm_cmpeq_pd(a, b); }
            static mask_vector_type cmpneq(vector_type a, vector_type b) { return _mm_cmpneq_pd(a, b); }
            
            static int movemask(mask_vector_type m) { return _mm_movemask_pd(m); }
            static mask_vector_type mask_set1(bool v) { return _mm_castsi128_pd(v ? _mm_set1_epi64x(-1) : _mm_setzero_si128()); }
        };

        // ========== Generic MASK implementation helpers ==========

        // Helper template to provide mask operations
        template <size_t Bytes, typename T>
        class alignas(16) basic_mask<Bytes, simd_abi_sse4<T>>
        {
            static_assert(Bytes == sizeof(T), "Size mismatch in basic_mask for simd_abi_sse4");

            using vector_type = typename simd_abi_sse4<T>::mask_vector_type;
            struct tuple_type
            {
                int32_t mask{};
                vector_type vector{};
            };

            constexpr static int32_t const nMASK = simd_abi_sse4<T>::all_mask;

            static int32_t get_value(tuple_type const& value, int32_t*) noexcept { return value.mask; }
            static vector_type get_value(tuple_type const& value, vector_type*) noexcept { return value.vector; }


            template <typename Tgenerator>
            static constexpr int32_t compute_mask_value(Tgenerator&& gen)
            {
                return [&]<size_t... Is>(std::index_sequence<Is...>)
                {
                    return ((gen(std::integral_constant<size_t, Is>{}) ? (1 << Is) : 0) | ...);
                }(std::make_index_sequence<simd_abi_sse4<T>::lanes>{});
            }

            static vector_type mask_not(vector_type v) {
                if constexpr (Bytes <= 2) {
                    return _mm_xor_si128(v, _mm_set1_epi8(-1));
                } else if constexpr (Bytes == 4) {
                    return _mm_xor_ps(v, _mm_castsi128_ps(_mm_set1_epi32(-1)));
                } else {
                    return _mm_xor_pd(v, _mm_castsi128_pd(_mm_set1_epi64x(-1)));
                }
            }

            static vector_type mask_and(vector_type a, vector_type b) {
                if constexpr (Bytes <= 2) {
                    return _mm_and_si128(a, b);
                } else if constexpr (Bytes == 4) {
                    return _mm_and_ps(a, b);
                } else {
                    return _mm_and_pd(a, b);
                }
            }

            static vector_type mask_or(vector_type a, vector_type b) {
                if constexpr (Bytes <= 2) {
                    return _mm_or_si128(a, b);
                } else if constexpr (Bytes == 4) {
                    return _mm_or_ps(a, b);
                } else {
                    return _mm_or_pd(a, b);
                }
            }

            static vector_type mask_xor(vector_type a, vector_type b) {
                if constexpr (Bytes <= 2) {
                    return _mm_xor_si128(a, b);
                } else if constexpr (Bytes == 4) {
                    return _mm_xor_ps(a, b);
                } else {
                    return _mm_xor_pd(a, b);
                }
            }

            template <typename Tgenerator, size_t... Is>
            static basic_mask mask_from_generator(Tgenerator && gen, std::index_sequence<Is...>) {
                if constexpr (std::is_invocable_r_v<basic_mask, Tgenerator, basic_vec<T, simd_abi_sse4<T>>>)
                {
                    alignas(16) constexpr static T const atIOTA[size]{T(Is)...};

                    return gen(unchecked_load<basic_vec<T, simd_abi_sse4<T>>>(atIOTA, size, flag_aligned));
                }
                else if constexpr (std::is_invocable_r_v<bool, Tgenerator, std::integral_constant<size_t, 0>>)
                {
                    return basic_mask
                    (
                        [&]<size_t... Js>(std::index_sequence<Js...>)
                        {
                            if constexpr (Bytes == 1) {
                                return _mm_setr_epi8((gen(std::integral_constant<size_t, Js>{}) ? -1 : 0)...);
                            } else if constexpr (Bytes == 2) {
                                return _mm_setr_epi16((gen(std::integral_constant<size_t, Js>{}) ? -1 : 0)...);
                            } else if constexpr (Bytes == 4) {
                                return _mm_castsi128_ps(_mm_setr_epi32((gen(std::integral_constant<size_t, Js>{}) ? -1 : 0)...));
                            } else { // Bytes == 8
                                return _mm_castsi128_pd(_mm_set_epi64x((gen(std::integral_constant<size_t, 1u - Js>{}) ? -1 : 0)...));
                            }
                        }(std::index_sequence<Is...>{}),
                        compute_mask_value(gen)
                    );
                }
                else
                {
                    static_assert(false, "Invalid generator for basic_mask");
                }
            }

            tuple_type m_tupleValue;
        public:
            using value_type = bool;
            using abi_type = simd_abi_sse4<T>;
            static constexpr std::integral_constant<size_t, simd_abi_sse4<T>::lanes> const size{};

            template <typename U>
            U get() const noexcept
            {
                return get_value(m_tupleValue, static_cast<U*>(nullptr));
            }

            basic_mask() = default;

            basic_mask(const basic_mask&) = default;
            basic_mask& operator=(const basic_mask&) = default;

            basic_mask(basic_mask&&) = default;
            basic_mask& operator=(basic_mask&&) = default;

            basic_mask(tuple_type value) noexcept : m_tupleValue(std::move(value)) {}
            basic_mask(vector_type vecValue, int32_t maskValue) noexcept
                : m_tupleValue(maskValue, vecValue)
            {}

            explicit basic_mask(vector_type vecValue) noexcept
                : m_tupleValue(simd_abi_sse4<T>::movemask(vecValue), vecValue)
            {}

            explicit basic_mask(bool value) noexcept
                : m_tupleValue
                (
                    value ? nMASK : 0,
                    [value]()->vector_type
                    {
                        if constexpr (Bytes == 1) {
                            return value ? _mm_set1_epi8(-1) : _mm_setzero_si128();
                        } else if constexpr (Bytes == 2) {
                            return value ? _mm_set1_epi16(-1) : _mm_setzero_si128();
                        } else if constexpr (Bytes == 4) {
                            return _mm_castsi128_ps(_mm_set1_epi32(value ? -1 : 0));
                        } else { // Bytes == 8
                            return _mm_castsi128_pd(_mm_set1_epi64x(value ? -1 : 0));
                        }
                    }()
                )
            {}
            
            template <size_t N>
            explicit basic_mask(const std::bitset<N>& bs) noexcept
                : m_tupleValue
                (
                    static_cast<int32_t>((bs.to_ulong()) & nMASK),
                    [&bs]() -> vector_type
                    {
                        if constexpr (Bytes == 1)
                        {
                            #if 0 && defined(__AVX512BW__) && defined(__AVX512VL__)
                            __m128i const xmmBitset = _mm_set1_epi16(static_cast<int16_t>(bs.to_ulong()));

                            __m128i const shift_count = _mm_setr_epi16(7, 6, 5, 4, 3, 2, 1, 0);
                            __m128i const shifted = _mm_sllv_epi16(xmmBitset, shift_count);

                            __m128i const shuffle_index = _mm_setr_epi8(0, 2, 4, 6, 8, 10, 12, 14, 1, 3, 5, 7, 9, 11, 13, 15);
                            __m128i const xmmShuffle = _mm_shuffle_epi8(shifted, shuffle_index);
                            return xmmShuffle;
                            #else
                            __m128i const xmmBitset = _mm_cvtsi32_si128(static_cast<int32_t>(bs.to_ulong()));

                            __m128i const shuffle_index = _mm_setr_epi8(0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1);
                            __m128i const xmmShuffle = _mm_shuffle_epi8(xmmBitset, shuffle_index);

                            __m128i const mask = _mm_setr_epi8(0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01, 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01);
                            __m128i const xmmAnd = _mm_and_si128(xmmShuffle, mask);

                            __m128i const xmmCmp = _mm_cmpeq_epi8(xmmAnd, mask);
                            return xmmCmp;
                            #endif
                        }
                        else if constexpr (Bytes == 2)
                        {
                            __m128i const xmmBitset = _mm_set1_epi16(static_cast<int16_t>(bs.to_ulong()));

                            #if 0 && defined(__AVX512BW__) && defined(__AVX512VL__)
                            __m128i const shift_count = _mm_setr_epi16(7, 6, 5, 4, 3, 2, 1, 0);
                            __m128i const xmmShifted = _mm_sllv_epi16(xmmBitset, shift_count);
                            return xmmShifted;
                            #else
                            __m128i const mask = _mm_setr_epi16(0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80);

                            __m128i const xmmAnd = _mm_and_si128(xmmBitset, mask);
                            __m128i const xmmCmp = _mm_cmpeq_epi16(xmmAnd, mask);

                            return xmmCmp;
                            #endif
                        }
                        else if constexpr (Bytes == 4)
                        {
                            __m128i const xmmBitset = _mm_set1_epi32(static_cast<int32_t>(bs.to_ulong()));

                            #if 0 && defined(__AVX2__)
                            __m128i const shift_count = _mm_setr_epi32(31,30,29,28);
                            __m128i const shifted = _mm_sllv_epi32(xmmBitset, shift_count);
                            return _mm_castsi128_ps(shifted);
                            #else
                            __m128i const mask = _mm_setr_epi32(0x1, 0x2, 0x4, 0x8);

                            __m128i const xmmAnd = _mm_and_si128(xmmBitset, mask);
                            __m128i const xmmCmp = _mm_cmpeq_epi32(xmmAnd, mask);

                            return _mm_castsi128_ps(xmmCmp);
                            #endif
                        }
                        else if constexpr (Bytes == 8) // Bytes == 8
                        {
                            __m128i const xmmBitset = _mm_set1_epi64x((bs.to_ulong()));

                            #if 0 && defined(__AVX2__)
                            __m128i const shift_count = _mm_setr_epi64x(63,62);
                            __m128i const shifted = _mm_sllv_epi64(xmmBitset, shift_count);
                            return _mm_castsi128_pd(shifted);
                            #else
                            __m128i const mask = _mm_set_epi64x(0x2, 0x1);

                            __m128i const xmmAnd = _mm_and_si128(xmmBitset, mask);
                            __m128i const xmmCmp = _mm_cmpeq_epi64(xmmAnd, mask);

                            return _mm_castsi128_pd(xmmCmp);
                            #endif
                        }
                    }
                    ()
                )
            {
                static_assert(N >= size, "Bitset size is smaller than vec mask size");
            }

            template <typename Tgenerator>
            requires
                std::is_invocable_r_v<bool, Tgenerator, std::integral_constant<size_t, 0>> or
                std::is_invocable_r_v<basic_mask, Tgenerator, basic_vec<T, simd_abi_sse4<T>>>
            explicit basic_mask(Tgenerator&& gen)
                : m_tupleValue(mask_from_generator(std::forward<Tgenerator>(gen), std::make_index_sequence<size>{}).m_tupleValue)
            {}

            bool operator[](std::size_t i) const noexcept { return ((m_tupleValue.mask >> i) & 1) != 0; }

            friend basic_mask operator!(const basic_mask& a) noexcept {
                return basic_mask(mask_not(a.m_tupleValue.vector), a.m_tupleValue.mask ^ nMASK);
            }
            friend basic_mask operator&(const basic_mask& a, const basic_mask& b) noexcept {
                return basic_mask(mask_and(a.m_tupleValue.vector, b.m_tupleValue.vector), a.m_tupleValue.mask & b.m_tupleValue.mask);
            }
            friend basic_mask operator|(const basic_mask& a, const basic_mask& b) noexcept {
                return basic_mask(mask_or(a.m_tupleValue.vector, b.m_tupleValue.vector), a.m_tupleValue.mask | b.m_tupleValue.mask);
            }
            friend basic_mask operator^(const basic_mask& a, const basic_mask& b) noexcept {
                return basic_mask(mask_xor(a.m_tupleValue.vector, b.m_tupleValue.vector), a.m_tupleValue.mask ^ b.m_tupleValue.mask);
            }

            basic_mask& operator&=(const basic_mask& other) noexcept {
                m_tupleValue.vector = mask_and(m_tupleValue.vector, other.m_tupleValue.vector);
                m_tupleValue.mask &= other.m_tupleValue.mask;
                return *this;
            }
            basic_mask& operator|=(const basic_mask& other) noexcept {
                m_tupleValue.vector = mask_or(m_tupleValue.vector, other.m_tupleValue.vector);
                m_tupleValue.mask |= other.m_tupleValue.mask;
                return *this;
            }
            basic_mask& operator^=(const basic_mask& other) noexcept {
                m_tupleValue.vector = mask_xor(m_tupleValue.vector, other.m_tupleValue.vector);
                m_tupleValue.mask ^= other.m_tupleValue.mask;
                return *this;
            }
        };
        template <size_t Bytes, typename T>
        int reduce_count(const basic_mask<Bytes, simd_abi_sse4<T>>& mask) noexcept {
            return std::popcount(static_cast<uint32_t>(mask.template get<int32_t>()));
        }

        // ========== Generic SIMD implementation ==========

        template <typename T>
        class basic_vec<T, simd_abi_sse4<T>>
        {
            using traits = simd_abi_sse4<T>;
            using vector_type = typename traits::vector_type;

            template <typename G, size_t... Indices>
            static vector_type generate(G&& generator, std::index_sequence<Indices...>)
            {
                alignas(16) T values[traits::lanes]{static_cast<T>(generator(std::integral_constant<size_t, Indices>{}))...};
                return traits::load_aligned(values);
            }

            vector_type m_value;
        public:
            using value_type = T;
            using mask_type = basic_mask<sizeof(T), simd_abi_sse4<T>>;
            using abi_type = simd_abi_sse4<T>;

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
                alignas(16) T tmp[traits::lanes];
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

            // Use SSE4 as native if compiler supports it
            template <typename T, typename Tabi = simd_abi_sse4<T>>
            class basic_vec;

            template <size_t Bytes, typename Tabi>
            class basic_mask;

            template <typename T, size_t N = simd_abi_sse4<T>::lanes>
            using vec = basic_vec<
                T,
                std::conditional_t<
                    (N <= simd_abi_sse4<T>::lanes),
                    simd_abi_sse4<T>,
                    decltype([]<size_t... Is>(std::index_sequence<Is...>) ->
                        simd_abi_tuple<T, simd_abi_sse4<typename simd_repeated_type<Is, T>::type>...> {
                            return {};
                        }(std::make_index_sequence<
                            (1u + (N - 1u) / simd_abi_sse4<T>::lanes)
                        >())
                    )>
                >;

            template <typename T, size_t N = vec<T>::size()>
            using mask = typename vec<T, N>::mask_type;
                         
            #define JRMWNG_NATIVE_SIMD_DEFINED
        #endif

    }
}
