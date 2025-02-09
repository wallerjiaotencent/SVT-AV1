/*
* Copyright(c) 2019 Intel Corporation
* SPDX - License - Identifier: BSD - 2 - Clause - Patent
*/

#ifndef EbPictureOperators_Inline_AVX2_h
#define EbPictureOperators_Inline_AVX2_h

#include <immintrin.h>
#include "EbDefinitions.h"
#include "EbMemory_AVX2.h"
#include "EbPictureOperators_SSE2.h"

#ifdef __cplusplus
extern "C" {
#endif

    SIMD_INLINE void residual_kernel4_avx2(
        const uint8_t *input, const uint32_t input_stride, const uint8_t *pred,
        const uint32_t pred_stride, int16_t *residual,
        const uint32_t residual_stride, const uint32_t area_height)
    {
        const __m256i zero = _mm256_setzero_si256();
        uint32_t y = area_height;

        do {
            const __m256i in = load_u8_4x4_avx2(input, input_stride);
            const __m256i pr = load_u8_4x4_avx2(pred, pred_stride);
            const __m256i in_lo = _mm256_unpacklo_epi8(in, zero);
            const __m256i pr_lo = _mm256_unpacklo_epi8(pr, zero);
            const __m256i re_lo = _mm256_sub_epi16(in_lo, pr_lo);
            const __m128i r0 = _mm256_castsi256_si128(re_lo);
            const __m128i r1 = _mm256_extracti128_si256(re_lo, 1);

            store_s16_4x2_sse2(r0, residual + 0 * residual_stride, residual_stride);
            store_s16_4x2_sse2(r1, residual + 2 * residual_stride, residual_stride);

            input += 4 * input_stride;
            pred += 4 * pred_stride;
            residual += 4 * residual_stride;
            y -= 4;
        } while (y);
    }

    SIMD_INLINE void residual_kernel8_avx2(
        const uint8_t *input, const uint32_t input_stride, const uint8_t *pred,
        const uint32_t pred_stride, int16_t *residual,
        const uint32_t residual_stride, const uint32_t area_height)
    {
        const __m256i zero = _mm256_setzero_si256();
        uint32_t y = area_height;

        do {
            const __m256i in = load_u8_8x4_avx2(input, input_stride);
            const __m256i pr = load_u8_8x4_avx2(pred, pred_stride);
            const __m256i in_lo = _mm256_unpacklo_epi8(in, zero);
            const __m256i in_hi = _mm256_unpackhi_epi8(in, zero);
            const __m256i pr_lo = _mm256_unpacklo_epi8(pr, zero);
            const __m256i pr_hi = _mm256_unpackhi_epi8(pr, zero);
            const __m256i r0 = _mm256_sub_epi16(in_lo, pr_lo);
            const __m256i r1 = _mm256_sub_epi16(in_hi, pr_hi);

            storeu_s16_8x2_avx2(r0, residual + 0 * residual_stride, 2 * residual_stride);
            storeu_s16_8x2_avx2(r1, residual + 1 * residual_stride, 2 * residual_stride);

            input += 4 * input_stride;
            pred += 4 * pred_stride;
            residual += 4 * residual_stride;
            y -= 4;
        } while (y);
    }

    SIMD_INLINE void residual_kernel16_avx2(
        const uint8_t *input, const uint32_t input_stride, const uint8_t *pred,
        const uint32_t pred_stride, int16_t *residual,
        const uint32_t residual_stride, const uint32_t area_height)
    {
        const __m256i zero = _mm256_setzero_si256();
        uint32_t y = area_height;

        do {
            const __m256i in0 = loadu_u8_16x2_avx2(input, input_stride);
            const __m256i pr0 = loadu_u8_16x2_avx2(pred, pred_stride);
            const __m256i in1 = _mm256_permute4x64_epi64(in0, 0xD8);
            const __m256i pr1 = _mm256_permute4x64_epi64(pr0, 0xD8);
            const __m256i in_lo = _mm256_unpacklo_epi8(in1, zero);
            const __m256i in_hi = _mm256_unpackhi_epi8(in1, zero);
            const __m256i pr_lo = _mm256_unpacklo_epi8(pr1, zero);
            const __m256i pr_hi = _mm256_unpackhi_epi8(pr1, zero);
            const __m256i re_lo = _mm256_sub_epi16(in_lo, pr_lo);
            const __m256i re_hi = _mm256_sub_epi16(in_hi, pr_hi);

            _mm256_storeu_si256((__m256i*)(residual + 0 * residual_stride), re_lo);
            _mm256_storeu_si256((__m256i*)(residual + 1 * residual_stride), re_hi);
            input += 2 * input_stride;
            pred += 2 * pred_stride;
            residual += 2 * residual_stride;
            y -= 2;
        } while (y);
    }

    static INLINE void Distortion_AVX2_INTRIN(const __m256i input,
        const __m256i recon, __m256i *const sum) {
        const __m256i in = _mm256_unpacklo_epi8(input, _mm256_setzero_si256());
        const __m256i re = _mm256_unpacklo_epi8(recon, _mm256_setzero_si256());
        const __m256i diff = _mm256_sub_epi16(in, re);
        const __m256i dist = _mm256_madd_epi16(diff, diff);
        *sum = _mm256_add_epi32(*sum, dist);
    }

    static INLINE void SpatialFullDistortionKernel16_AVX2_INTRIN(
        const uint8_t *const input, const uint8_t *const recon,
        __m256i *const sum)
    {
        const __m128i in8 = _mm_loadu_si128((__m128i *)input);
        const __m128i re8 = _mm_loadu_si128((__m128i *)recon);
        const __m256i in16 = _mm256_cvtepu8_epi16(in8);
        const __m256i re16 = _mm256_cvtepu8_epi16(re8);
        const __m256i diff = _mm256_sub_epi16(in16, re16);
        const __m256i dist = _mm256_madd_epi16(diff, diff);
        *sum = _mm256_add_epi32(*sum, dist);
    }

    static INLINE void FullDistortionKernel4_AVX2_INTRIN(
        const uint16_t* const input, const uint16_t* const recon,
        __m256i* const sum)
    {
        __m128i in = _mm_loadl_epi64((__m128i*)input);
        __m128i re = _mm_loadl_epi64((__m128i*)recon);
        __m128i max = _mm_max_epu16(in, re);
        __m128i min = _mm_min_epu16(in, re);
        __m128i diff = _mm_sub_epi16(max, min);
        diff = _mm_unpacklo_epi16(diff, _mm_setzero_si128());
        diff = _mm_mullo_epi32(diff, diff);

        *sum = _mm256_add_epi64(*sum, _mm256_cvtepu32_epi64(diff));
    }

    static INLINE void FullDistortionKernel16_AVX2_INTRIN(
        __m256i in, __m256i re, __m256i* const sum)
    {
        __m256i max = _mm256_max_epu16(in, re);
        __m256i min = _mm256_min_epu16(in, re);
        __m256i diff = _mm256_sub_epi16(max, min);
        __m256i diff_L = _mm256_unpacklo_epi16(diff, _mm256_setzero_si256());
        __m256i diff_H = _mm256_unpackhi_epi16(diff, _mm256_setzero_si256());
        __m256i mul_L = _mm256_mullo_epi32(diff_L, diff_L);
        __m256i mul_H = _mm256_mullo_epi32(diff_H, diff_H);

        __m256i sum_1 = _mm256_add_epi64(_mm256_unpackhi_epi32(mul_H, _mm256_setzero_si256()),
            _mm256_unpacklo_epi32(mul_H, _mm256_setzero_si256()));
        __m256i sum_2 = _mm256_add_epi64(_mm256_unpackhi_epi32(mul_L, _mm256_setzero_si256()),
            _mm256_unpacklo_epi32(mul_L, _mm256_setzero_si256()));
        *sum = _mm256_add_epi64(*sum, sum_1);
        *sum = _mm256_add_epi64(*sum, sum_2);

    }

    static INLINE void SpatialFullDistortionKernel32Leftover_AVX2_INTRIN(
        const uint8_t *const input, const uint8_t *const recon, __m256i *const sum0,
        __m256i *const sum1)
    {
        const __m256i in = _mm256_loadu_si256((__m256i *)input);
        const __m256i re = _mm256_loadu_si256((__m256i *)recon);
        const __m256i max = _mm256_max_epu8(in, re);
        const __m256i min = _mm256_min_epu8(in, re);
        const __m256i diff = _mm256_sub_epi8(max, min);
        const __m256i diff_L = _mm256_unpacklo_epi8(diff, _mm256_setzero_si256());
        const __m256i diff_H = _mm256_unpackhi_epi8(diff, _mm256_setzero_si256());
        const __m256i dist_L = _mm256_madd_epi16(diff_L, diff_L);
        const __m256i dist_H = _mm256_madd_epi16(diff_H, diff_H);
        *sum0 = _mm256_add_epi32(*sum0, dist_L);
        *sum1 = _mm256_add_epi32(*sum1, dist_H);
    }

    static INLINE void SpatialFullDistortionKernel32_AVX2_INTRIN(
        const uint8_t *const input, const uint8_t *const recon, __m256i *const sum)
    {
        const __m256i in = _mm256_loadu_si256((__m256i *)input);
        const __m256i re = _mm256_loadu_si256((__m256i *)recon);
        const __m256i max = _mm256_max_epu8(in, re);
        const __m256i min = _mm256_min_epu8(in, re);
        const __m256i diff = _mm256_sub_epi8(max, min);
        const __m256i diff_L = _mm256_unpacklo_epi8(diff, _mm256_setzero_si256());
        const __m256i diff_H = _mm256_unpackhi_epi8(diff, _mm256_setzero_si256());
        const __m256i dist_L = _mm256_madd_epi16(diff_L, diff_L);
        const __m256i dist_H = _mm256_madd_epi16(diff_H, diff_H);
        const __m256i dist = _mm256_add_epi32(dist_L, dist_H);
        *sum = _mm256_add_epi32(*sum, dist);
    }

    static INLINE int32_t Hadd32_AVX2_INTRIN(const __m256i src) {
        const __m128i src_L = _mm256_extracti128_si256(src, 0);
        const __m128i src_H = _mm256_extracti128_si256(src, 1);
        const __m128i sum = _mm_add_epi32(src_L, src_H);

        return Hadd32_SSE2_INTRIN(sum);
    }

#ifdef __cplusplus
}
#endif

#endif // EbPictureOperators_Inline_AVX2_h
