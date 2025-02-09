﻿/*
 * Copyright(c) 2019 Intel Corporation
 * SPDX - License - Identifier: BSD - 2 - Clause - Patent
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// Workaround to eliminate the compiling warning on linux
// The macro will conflict with definition in gtest.h
#ifdef __USE_GNU
#undef __USE_GNU  // defined in EbThreads.h
#endif
#ifdef _GNU_SOURCE
#undef _GNU_SOURCE  // defined in EbThreads.h
#endif

#include "random.h"
#include "aom_dsp_rtcd.h"
#include "EbDefinitions.h"
#include "EbPictureOperators_AVX2.h"
#include "EbPictureOperators_AVX512.h"
#include "EbPictureOperators_C.h"
#include "EbPictureOperators.h"
#include "EbUnitTestUtility.h"
#include "util.h"

namespace {
    using svt_av1_test_tool::SVTRandom;

typedef uint64_t (*SpatialFullDistortionKernelFunc)(
    uint8_t *input, uint32_t input_offset, uint32_t input_stride,
    uint8_t *recon, uint32_t recon_offset, uint32_t recon_stride,
    uint32_t area_width, uint32_t area_height);

class SpatialFullDistortionTest
    : public ::testing::TestWithParam<SpatialFullDistortionKernelFunc> {
  public:
    SpatialFullDistortionTest() : func_(GetParam()) {
    }

    ~SpatialFullDistortionTest();

    void SetUp() {
        input_stride_ = eb_create_random_aligned_stride(MAX_SB_SIZE, 64);
        recon_stride_ = eb_create_random_aligned_stride(MAX_SB_SIZE, 64);
        input_ = reinterpret_cast<uint8_t *>(
            malloc(sizeof(*input_) * MAX_SB_SIZE * input_stride_));
        recon_ = reinterpret_cast<uint8_t *>(
            malloc(sizeof(*recon_) * MAX_SB_SIZE * recon_stride_));
    }
    void TearDown() {
        free(recon_);
        free(input_);
        aom_clear_system_state();
    }

  protected:
    void RunCheckOutput();
    void RunSpeedTest();

    void init_data() {
        eb_buf_random_u8(input_, MAX_SB_SIZE * input_stride_);
        eb_buf_random_u8(recon_, MAX_SB_SIZE * recon_stride_);
    }

    SpatialFullDistortionKernelFunc func_;
    uint8_t *input_;
    uint8_t *recon_;
    uint32_t input_stride_;
    uint32_t recon_stride_;
};

SpatialFullDistortionTest::~SpatialFullDistortionTest() {
}

void SpatialFullDistortionTest::RunCheckOutput() {
    for (int i = 0; i < 10; i++) {
        init_data();
        for (uint32_t area_width = 4; area_width <= 128; area_width += 4) {
            for (uint32_t area_height = 4; area_height <= 32;
                 area_height += 4) {
                const uint64_t dist_org =
                    spatial_full_distortion_kernel_c(input_,
                                                     0,
                                                     input_stride_,
                                                     recon_,
                                                     0,
                                                     recon_stride_,
                                                     area_width,
                                                     area_height);
                const uint64_t dist_opt = func_(input_,
                                                0,
                                                input_stride_,
                                                recon_,
                                                0,
                                                recon_stride_,
                                                area_width,
                                                area_height);

                EXPECT_EQ(dist_org, dist_opt)
                    << area_width << "x" << area_height;
            }
        }
    }
}

void SpatialFullDistortionTest::RunSpeedTest() {
    uint64_t dist_org, dist_opt;
    double time_c, time_o;
    uint64_t start_time_seconds, start_time_useconds;
    uint64_t middle_time_seconds, middle_time_useconds;
    uint64_t finish_time_seconds, finish_time_useconds;

    init_data();

    for (uint32_t area_width = 4; area_width <= 128; area_width += 4) {
        const uint32_t area_height = area_width;
        const int num_loops = 1000000000 / (area_width * area_height);
        EbStartTime(&start_time_seconds, &start_time_useconds);

        for (int i = 0; i < num_loops; ++i) {
            dist_org = spatial_full_distortion_kernel_c(input_,
                                                        0,
                                                        input_stride_,
                                                        recon_,
                                                        0,
                                                        recon_stride_,
                                                        area_width,
                                                        area_height);
        }

        EbStartTime(&middle_time_seconds, &middle_time_useconds);

        for (int i = 0; i < num_loops; ++i) {
            dist_opt = func_(input_,
                             0,
                             input_stride_,
                             recon_,
                             0,
                             recon_stride_,
                             area_width,
                             area_height);
        }
        EbStartTime(&finish_time_seconds, &finish_time_useconds);

        EXPECT_EQ(dist_org, dist_opt) << area_width << "x" << area_height;

        EbComputeOverallElapsedTimeMs(start_time_seconds,
                                      start_time_useconds,
                                      middle_time_seconds,
                                      middle_time_useconds,
                                      &time_c);
        EbComputeOverallElapsedTimeMs(middle_time_seconds,
                                      middle_time_useconds,
                                      finish_time_seconds,
                                      finish_time_useconds,
                                      &time_o);
        printf("Average Nanoseconds per Function Call\n");
        printf("    spatial_full_distortion_kernel_c  (%dx%d) : %6.2f\n",
               area_width,
               area_height,
               1000000 * time_c / num_loops);
        printf(
            "    spatial_full_distortion_kernel_opt(%dx%d) : %6.2f   "
            "(Comparison: %5.2fx)\n",
            area_width,
            area_height,
            1000000 * time_o / num_loops,
            time_c / time_o);
    }
}

TEST_P(SpatialFullDistortionTest, CheckOutput) {
    RunCheckOutput();
}

TEST_P(SpatialFullDistortionTest, DISABLED_Speed) {
    RunSpeedTest();
}

INSTANTIATE_TEST_CASE_P(AVX2, SpatialFullDistortionTest,
                        ::testing::Values(spatial_full_distortion_kernel_avx2));

#ifndef NON_AVX512_SUPPORT
INSTANTIATE_TEST_CASE_P(
    AVX512, SpatialFullDistortionTest,
    ::testing::Values(spatial_full_distortion_kernel_avx512));
#endif

typedef enum { VAL_MIN, VAL_MAX, VAL_RANDOM } TestPattern;
TestPattern TEST_PATTERNS[] = {VAL_MIN, VAL_MAX, VAL_RANDOM};
typedef std::tuple<uint32_t, uint32_t> AreaSize;

/**
 * @Brief Base class for SpatialFullDistortionFunc test.
 */
class SpatialFullDistortionFuncTestBase : public ::testing::Test {
    void SetUp() override {
        input_stride_ = eb_create_random_aligned_stride(MAX_SB_SIZE, 64);
        recon_stride_ = eb_create_random_aligned_stride(MAX_SB_SIZE, 64);
        input_test_size_ = MAX_SB_SIZE * input_stride_;
        recon_test_size_ = MAX_SB_SIZE * recon_stride_;
        input_ = reinterpret_cast<uint8_t *>(
            malloc(sizeof(*input_) * input_test_size_));
        recon_ = reinterpret_cast<uint8_t *>(
            malloc(sizeof(*recon_) * recon_test_size_));
    }
    void TearDown() override {
        free(recon_);
        free(input_);
        aom_clear_system_state();
    }

  protected:
    virtual void RunCheckOutput() {
    }

    void init_data() {
        const uint8_t mask = (1 << 8) - 1;
        switch (test_pattern_) {
        case VAL_MIN: {
            memset(input_, 0, input_test_size_ * sizeof(input_[0]));
            memset(recon_, 0, recon_test_size_ * sizeof(recon_[0]));
            break;
        }
        case VAL_MAX: {
            memset(input_, mask, input_test_size_ * sizeof(input_[0]));
            memset(recon_, mask, recon_test_size_ * sizeof(recon_[0]));
            break;
        }
        case VAL_RANDOM: {
            eb_buf_random_u8(input_, input_test_size_);
            eb_buf_random_u8(recon_, recon_test_size_);
            break;
        }
        default: break;
        }
    }

    int input_test_size_, recon_test_size_;
    uint8_t *input_;
    uint8_t *recon_;
    uint32_t input_stride_;
    uint32_t recon_stride_;
    uint32_t area_width_, area_height_;
    TestPattern test_pattern_;
};

typedef struct TestParam {
    AreaSize area_size;
    SpatialFullDistortionKernelFunc sse2_test_func;
    SpatialFullDistortionKernelFunc avx2_test_func;
    SpatialFullDistortionKernelFunc avx512_test_func;
} TestParam;
#ifndef NON_AVX512_SUPPORT
TestParam SPATIAL_TEST_PARAM[] = {
    {AreaSize(128, 128),
     spatial_full_distortion_kernel128x_n_sse2_intrin,
     spatial_full_distortion_kernel128x_n_avx2_intrin,
     spatial_full_distortion_kernel128x_n_avx512_intrin},
    {AreaSize(64, 64),
     spatial_full_distortion_kernel64x_n_sse2_intrin,
     spatial_full_distortion_kernel64x_n_avx2_intrin,
     spatial_full_distortion_kernel64x_n_avx512_intrin},
    {AreaSize(32, 32),
     spatial_full_distortion_kernel32x_n_sse2_intrin,
     spatial_full_distortion_kernel32x_n_avx2_intrin,
     spatial_full_distortion_kernel32x_n_avx512_intrin},
    {AreaSize(16, 16),
     spatial_full_distortion_kernel16x_n_sse2_intrin,
     spatial_full_distortion_kernel16x_n_avx2_intrin,
     nullptr},
    {AreaSize(8, 8),
     spatial_full_distortion_kernel8x_n_sse2_intrin,
     spatial_full_distortion_kernel8x_n_avx2_intrin,
     nullptr},
    {AreaSize(4, 4),
     spatial_full_distortion_kernel4x_n_sse2_intrin,
     spatial_full_distortion_kernel4x_n_avx2_intrin,
     nullptr}};
#else
TestParam SPATIAL_TEST_PARAM[] = {
    {AreaSize(128, 128),
     spatial_full_distortion_kernel128x_n_sse2_intrin,
     spatial_full_distortion_kernel128x_n_avx2_intrin,
     nullptr},
    {AreaSize(64, 64),
     spatial_full_distortion_kernel64x_n_sse2_intrin,
     spatial_full_distortion_kernel64x_n_avx2_intrin,
     nullptr},
    {AreaSize(32, 32),
     spatial_full_distortion_kernel32x_n_sse2_intrin,
     spatial_full_distortion_kernel32x_n_avx2_intrin,
     nullptr},
    {AreaSize(16, 16),
     spatial_full_distortion_kernel16x_n_sse2_intrin,
     spatial_full_distortion_kernel16x_n_avx2_intrin,
     nullptr},
    {AreaSize(8, 8),
     spatial_full_distortion_kernel8x_n_sse2_intrin,
     spatial_full_distortion_kernel8x_n_avx2_intrin,
     nullptr},
    {AreaSize(4, 4),
     spatial_full_distortion_kernel4x_n_sse2_intrin,
     spatial_full_distortion_kernel4x_n_avx2_intrin,
     nullptr}};
#endif
typedef std::tuple<TestParam, TestPattern> SpatialTestParam;

/**
 * @brief Unit test for spatial distortion calculation functions include:
 *  - spatial_full_distortion_kernel{4, 8, 16, 32, 64,
      128}x_n_{sse2,avx2,avx512}_intrin
 *
 *
 * Test strategy:
 *  This test case combine different area width{4-128} x area
 * height{4-128} and different test pattern(VAL_MIN, VAL_MAX, VAL_RANDOM). Check
 * the result by compare result from reference function and avx2/sse2 function.
 *
 *
 * Expect result:
 *  Results from reference function and sse2/avx2/avx512 function are
 * equal.
 *
 *
 * Test cases:
 *  Width {4, 8, 16, 32, 64, 128} x height{ 4, 8, 16, 32, 64, 128}
 *  Test vector pattern {VAL_MIN, VAL_MIN, VAL_RANDOM}
 *
 */
class SpatialFullDistortionFuncTest
    : public SpatialFullDistortionFuncTestBase,
      public ::testing::WithParamInterface<SpatialTestParam> {
  public:
    SpatialFullDistortionFuncTest() {
        area_width_ = std::get<0>(TEST_GET_PARAM(0).area_size);
        area_height_ = std::get<1>(TEST_GET_PARAM(0).area_size);
        sse2_func_ = TEST_GET_PARAM(0).sse2_test_func;
        avx2_func_ = TEST_GET_PARAM(0).avx2_test_func;
        avx512_func_ = TEST_GET_PARAM(0).avx512_test_func;
        test_pattern_ = TEST_GET_PARAM(1);
    }

    ~SpatialFullDistortionFuncTest() {
    }

  protected:
    void RunCheckOutput();
    SpatialFullDistortionKernelFunc sse2_func_, avx2_func_, avx512_func_;
};

void SpatialFullDistortionFuncTest::RunCheckOutput() {
    for (int i = 0; i < 10; i++) {
        init_data();
        const uint64_t dist_sse2 = sse2_func_(input_,
                                              0,
                                              input_stride_,
                                              recon_,
                                              0,
                                              recon_stride_,
                                              area_width_,
                                              area_height_);
        const uint64_t dist_avx2 = avx2_func_(input_,
                                              0,
                                              input_stride_,
                                              recon_,
                                              0,
                                              recon_stride_,
                                              area_width_,
                                              area_height_);
        uint64_t dist_avx512 = 0;
        if (avx512_func_)
            dist_avx512 = avx512_func_(input_,
                                       0,
                                       input_stride_,
                                       recon_,
                                       0,
                                       recon_stride_,
                                       area_width_,
                                       area_height_);
        const uint64_t dist_c = spatial_full_distortion_kernel_c(input_,
                                                                 0,
                                                                 input_stride_,
                                                                 recon_,
                                                                 0,
                                                                 recon_stride_,
                                                                 area_width_,
                                                                 area_height_);
        EXPECT_EQ(dist_sse2, dist_c)
            << "Compare sse2 vs c Spatial distortion result error";
        EXPECT_EQ(dist_avx2, dist_c)
            << "Compare avx2 vs c Spatial distortion result error";
        if (avx512_func_)
            EXPECT_EQ(dist_avx512, dist_c)
                << "Compare avx512 vs c Spatial distortion result error";
    }
}

TEST_P(SpatialFullDistortionFuncTest, SpatialFuncTest) {
    RunCheckOutput();
}

INSTANTIATE_TEST_CASE_P(
    SpatialFunc, SpatialFullDistortionFuncTest,
    ::testing::Combine(::testing::ValuesIn(SPATIAL_TEST_PARAM),
                       ::testing::ValuesIn(TEST_PATTERNS)));

AreaSize TEST_AREA_SIZES[] = {
    AreaSize(4, 4),    AreaSize(4, 8),    AreaSize(8, 4),   AreaSize(8, 8),
    AreaSize(16, 16),  AreaSize(12, 16),  AreaSize(4, 16),  AreaSize(16, 4),
    AreaSize(16, 8),   AreaSize(20, 16),  AreaSize(24, 16), AreaSize(28, 16),
    AreaSize(8, 16),   AreaSize(32, 32),  AreaSize(32, 8),  AreaSize(16, 32),
    AreaSize(8, 32),   AreaSize(32, 16),  AreaSize(16, 64), AreaSize(64, 16),
    AreaSize(64, 64),  AreaSize(64, 32),  AreaSize(32, 64), AreaSize(128, 128),
    AreaSize(96, 128), AreaSize(64, 128), AreaSize(128, 64)};
typedef std::tuple<AreaSize, TestPattern, SpatialFullDistortionKernelFunc>
    SpatialKernelTestParam;

/**
 * @brief Unit test for spatial distortion calculation functions include:
 *  - spatial_full_distortion_kernel_{avx2,avx512}
 *
 *
 * Test strategy:
 *  This test case combine different area width{4-128} x area
 * height{4-128} and different test pattern(VAL_MIN, VAL_MAX, VAL_RANDOM). Check
 * the result by compare result from reference function and avx2/sse2 function.
 *
 *
 * Expect result:
 *  Results from reference function and avx2/avx512 function are
 * equal.
 *
 *
 * Test cases:
 *  Width {4, 8, 12, 16, 20, 24, 28, 32, 64, 96, 128} x height{ 4, 8, 16, 32,
 * 64, 128} Test vector pattern {VAL_MIN, VAL_MIN, VAL_RANDOM}
 *
 */
class SpatialFullDistortionKernelFuncTest
    : public SpatialFullDistortionFuncTestBase,
      public ::testing::WithParamInterface<SpatialKernelTestParam> {
  public:
    SpatialFullDistortionKernelFuncTest() {
        area_width_ = std::get<0>(TEST_GET_PARAM(0));
        area_height_ = std::get<1>(TEST_GET_PARAM(0));
        test_func_ = TEST_GET_PARAM(2);
        test_pattern_ = TEST_GET_PARAM(1);
    }

    ~SpatialFullDistortionKernelFuncTest() {
    }

  protected:
    void RunCheckOutput();
    SpatialFullDistortionKernelFunc test_func_;
};

void SpatialFullDistortionKernelFuncTest::RunCheckOutput() {
    for (int i = 0; i < 10; i++) {
        init_data();
        const uint64_t dist_test = test_func_(input_,
                                              0,
                                              input_stride_,
                                              recon_,
                                              0,
                                              recon_stride_,
                                              area_width_,
                                              area_height_);
        const uint64_t dist_c = spatial_full_distortion_kernel_c(input_,
                                                                 0,
                                                                 input_stride_,
                                                                 recon_,
                                                                 0,
                                                                 recon_stride_,
                                                                 area_width_,
                                                                 area_height_);

        EXPECT_EQ(dist_test, dist_c)
            << "Compare Spatial distortion result error";
    }
}

TEST_P(SpatialFullDistortionKernelFuncTest, SpatialKernelFuncTest) {
    RunCheckOutput();
}
#ifndef NON_AVX512_SUPPORT
INSTANTIATE_TEST_CASE_P(
    SpatialKernelFunc, SpatialFullDistortionKernelFuncTest,
    ::testing::Combine(
        ::testing::ValuesIn(TEST_AREA_SIZES),
        ::testing::ValuesIn(TEST_PATTERNS),
        ::testing::Values(spatial_full_distortion_kernel_avx2,
                          spatial_full_distortion_kernel_avx512)));
#else
INSTANTIATE_TEST_CASE_P(
    SpatialKernelFunc, SpatialFullDistortionKernelFuncTest,
    ::testing::Combine(::testing::ValuesIn(TEST_AREA_SIZES),
                       ::testing::ValuesIn(TEST_PATTERNS),
                       ::testing::Values(spatial_full_distortion_kernel_avx2)));
#endif

class FullDistortionKernel16BitsFuncTest
    : public SpatialFullDistortionFuncTestBase,
      public ::testing::WithParamInterface<SpatialKernelTestParam> {
    void SetUp() override {
        input_stride_ = eb_create_random_aligned_stride(MAX_SB_SIZE, 64);
        recon_stride_ = eb_create_random_aligned_stride(MAX_SB_SIZE, 64);
        input_test_size_ = MAX_SB_SIZE * input_stride_ * 2;
        recon_test_size_ = MAX_SB_SIZE * recon_stride_ * 2;
        input_ = reinterpret_cast<uint8_t *>(
            malloc(sizeof(*input_) * input_test_size_));
        recon_ = reinterpret_cast<uint8_t *>(
            malloc(sizeof(*recon_) * recon_test_size_));
    }

    void init_data() {
        const uint16_t mask = (1 << 16) - 1;
        uint16_t *input_16bit = (uint16_t *)input_;
        uint16_t *recon_16bit = (uint16_t *)recon_;
        SVTRandom rnd = SVTRandom(0, mask);

        switch (test_pattern_) {
        case VAL_MIN: {
            for (int i = 0; i < (input_test_size_ / 2); i++)
                input_16bit[i] = 0;
            for (int i = 0; i < (recon_test_size_ / 2); i++)
                recon_16bit[i] = mask;
            break;
        }
        case VAL_MAX: {
            for (int i = 0; i < (input_test_size_ / 2); i++)
                input_16bit[i] = mask;
            for (int i = 0; i < (recon_test_size_ / 2); i++)
                recon_16bit[i] = 0;
            break;
        }
        case VAL_RANDOM: {
            for (int i = 0; i < (input_test_size_ / 2); i++)
                input_16bit[i] = rnd.random();
            for (int i = 0; i < (recon_test_size_ / 2); i++)
                recon_16bit[i] = rnd.random();
            break;
        }
        default: break;
        }
    }

  public:
    FullDistortionKernel16BitsFuncTest() {
        area_width_ = std::get<0>(TEST_GET_PARAM(0));
        area_height_ = std::get<1>(TEST_GET_PARAM(0));
        test_func_ = TEST_GET_PARAM(2);
        test_pattern_ = TEST_GET_PARAM(1);
    }

    ~FullDistortionKernel16BitsFuncTest() {
    }

  protected:
    void RunCheckOutput();
    void RunSpeedTest();
    SpatialFullDistortionKernelFunc test_func_;
};

void FullDistortionKernel16BitsFuncTest::RunCheckOutput() {
    for (int i = 0; i < 10; i++) {
        init_data();
        const uint64_t dist_test = test_func_(input_,
                                              0,
                                              input_stride_,
                                              recon_,
                                              0,
                                              recon_stride_,
                                              area_width_,
                                              area_height_);
        const uint64_t dist_c = full_distortion_kernel16_bits_c(input_,
                                                                0,
                                                                input_stride_,
                                                                recon_,
                                                                0,
                                                                recon_stride_,
                                                                area_width_,
                                                                area_height_);

        EXPECT_EQ(dist_test, dist_c)
            << "Compare Full distortion kernel 16 bits result error";
    }
}

void FullDistortionKernel16BitsFuncTest::RunSpeedTest() {
    uint64_t dist_org, dist_opt;
    double time_c, time_o;
    uint64_t start_time_seconds, start_time_useconds;
    uint64_t middle_time_seconds, middle_time_useconds;
    uint64_t finish_time_seconds, finish_time_useconds;
    init_data();

    for (uint32_t area_width = 4; area_width <= 128; area_width += 4) {
        const uint32_t area_height = area_width;
        const int num_loops = 1000000000 / (area_width * area_height);
        EbStartTime(&start_time_seconds, &start_time_useconds);

        for (int i = 0; i < num_loops; ++i) {
            dist_org = full_distortion_kernel16_bits_c(input_,
                                                       0,
                                                       input_stride_,
                                                       recon_,
                                                       0,
                                                       recon_stride_,
                                                       area_width,
                                                       area_height);
        }

        EbStartTime(&middle_time_seconds, &middle_time_useconds);

        for (int i = 0; i < num_loops; ++i) {
            dist_opt = test_func_(input_,
                                  0,
                                  input_stride_,
                                  recon_,
                                  0,
                                  recon_stride_,
                                  area_width,
                                  area_height);
        }
        EbStartTime(&finish_time_seconds, &finish_time_useconds);

        EXPECT_EQ(dist_org, dist_opt) << area_width << "x" << area_height;

        EbComputeOverallElapsedTimeMs(start_time_seconds,
                                      start_time_useconds,
                                      middle_time_seconds,
                                      middle_time_useconds,
                                      &time_c);
        EbComputeOverallElapsedTimeMs(middle_time_seconds,
                                      middle_time_useconds,
                                      finish_time_seconds,
                                      finish_time_useconds,
                                      &time_o);
        printf("Average Nanoseconds per Function Call\n");
        printf("    full_distortion_kernel16_bits_c  (%dx%d) : %6.2f\n",
               area_width,
               area_height,
               1000000 * time_c / num_loops);
        printf(
               "    full_distortion_kernel16_bits_opt(%dx%d) : %6.2f   "
               "(Comparison: %5.2fx)\n",
               area_width,
               area_height,
               1000000 * time_o / num_loops,
               time_c / time_o);
    }
}

TEST_P(FullDistortionKernel16BitsFuncTest, FullDistortionKernel16FuncTest) {
    RunCheckOutput();
}

TEST_P(FullDistortionKernel16BitsFuncTest, DISABLED_Speed) {
    RunSpeedTest();
}

INSTANTIATE_TEST_CASE_P(
    FullDistortionKernel16FuncTest, FullDistortionKernel16BitsFuncTest,
    ::testing::Combine(::testing::ValuesIn(TEST_AREA_SIZES),
                       ::testing::ValuesIn(TEST_PATTERNS),
                       ::testing::Values(full_distortion_kernel16_bits_avx2)));

}  // namespace
