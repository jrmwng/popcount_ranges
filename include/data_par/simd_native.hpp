#pragma once

// This header selects an available SIMD implementation based on compiler
// predefined macros. It provides the native SIMD aliases used by the library.
//
// The selection priority is:
// 1. AVX2 (__AVX2__ or _M_AVX2)
// 2. SSE4.1/4.2 (__SSE4_1__ or __SSE4_2__)
//
// Compilation fails when neither supported SIMD target is enabled.

#include "simd.hpp"

// Future SIMD implementations:

// #if defined(__AVX512F__)
// #include "simd_avx512.hpp"
// #endif

#if defined(__AVX2__) || defined(_M_AVX2)
#include "simd_avx2.hpp"
#endif

#if defined(__SSE4_2__) || defined(__SSE4_1__) || defined(__AVX2__) || defined(_M_AVX2)
#include "simd_sse4.hpp"
#endif

// #if defined(__SSE2__)
// #include "simd_sse2.hpp"
// #endif

// #if defined(__ARM_NEON)
// #include "simd_neon.hpp"
// #endif

#if !defined(__AVX2__) && !defined(_M_AVX2) && \
    !defined(__SSE4_1__) && !defined(__SSE4_2__)
#error "popcount_ranges requires an AVX2 or SSE4.1/4.2 SIMD target; scalar SIMD is not supported"
#endif
