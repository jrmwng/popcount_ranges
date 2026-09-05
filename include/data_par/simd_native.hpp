#pragma once

// This header selects the best available SIMD implementation based on
// compiler-defined preprocessor macros. It provides the native_simd alias
// that applications should use for portable, performance-optimized code.
//
// The selection priority is:
// 1. AVX-512 (__AVX512F__)
// 2. AVX2 (__AVX2__)
// 3. SSE4.1/4.2 (__SSE4_1__/__SSE4_2__)
// 4. ARM NEON (__ARM_NEON)
// 5. Scalar fallback (always available)
//
// Note: Each SIMD implementation header (simd_avx2.hpp, etc.) can also be
// included directly. When included alone, it will define native_simd based
// on compiler flags. When multiple implementations are included, the first
// included implementation that compiles will be selected.

#include "simd.hpp"

// Future SIMD implementations:

// #if defined(__AVX512F__)
// #include "simd_avx512.hpp"
// #endif

#if defined(__AVX2__) || defined(_M_AVX2)
#include "simd_avx2.hpp"
#endif

#if defined(__SSE4_2__) || defined(__SSE4_1__) || defined(__AVX2__) || defined(_M_X64)
#include "simd_sse4.hpp"
#endif

// #if defined(__SSE2__)
// #include "simd_sse2.hpp"
// #endif

// #if defined(__ARM_NEON)
// #include "simd_neon.hpp"
// #endif

// Include implementations in reverse priority order so higher priority
// implementations can override lower priority ones
#include "simd_scalar.hpp"  // Always available fallback
