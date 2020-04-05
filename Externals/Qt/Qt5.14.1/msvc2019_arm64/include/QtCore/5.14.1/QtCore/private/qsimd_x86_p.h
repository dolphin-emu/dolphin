// This is a generated file. DO NOT EDIT.
// Please see util/x86simdgen/generate.pl
#ifndef QSIMD_P_H
#  error "Please include <private/qsimd_p.h> instead"
#endif
#ifndef QSIMD_X86_P_H
#define QSIMD_X86_P_H

#include "qsimd_p.h"

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

QT_BEGIN_NAMESPACE

// used only to indicate that the CPU detection was initialized
#define QSimdInitialized                            (Q_UINT64_C(1) << 0)

// in CPUID Leaf 1, EDX:
#define CpuFeatureSSE2                              (Q_UINT64_C(1) << 1)
#define QT_FUNCTION_TARGET_STRING_SSE2              "sse2"

// in CPUID Leaf 1, ECX:
#define CpuFeatureSSE3                              (Q_UINT64_C(1) << 2)
#define QT_FUNCTION_TARGET_STRING_SSE3              "sse3"
#define CpuFeatureSSSE3                             (Q_UINT64_C(1) << 3)
#define QT_FUNCTION_TARGET_STRING_SSSE3             "ssse3"
#define CpuFeatureFMA                               (Q_UINT64_C(1) << 4)
#define QT_FUNCTION_TARGET_STRING_FMA               "fma"
#define CpuFeatureSSE4_1                            (Q_UINT64_C(1) << 5)
#define QT_FUNCTION_TARGET_STRING_SSE4_1            "sse4.1"
#define CpuFeatureSSE4_2                            (Q_UINT64_C(1) << 6)
#define QT_FUNCTION_TARGET_STRING_SSE4_2            "sse4.2"
#define CpuFeatureMOVBE                             (Q_UINT64_C(1) << 7)
#define QT_FUNCTION_TARGET_STRING_MOVBE             "movbe"
#define CpuFeaturePOPCNT                            (Q_UINT64_C(1) << 8)
#define QT_FUNCTION_TARGET_STRING_POPCNT            "popcnt"
#define CpuFeatureAES                               (Q_UINT64_C(1) << 9)
#define QT_FUNCTION_TARGET_STRING_AES               "aes,sse4.2"
#define CpuFeatureAVX                               (Q_UINT64_C(1) << 10)
#define QT_FUNCTION_TARGET_STRING_AVX               "avx"
#define CpuFeatureF16C                              (Q_UINT64_C(1) << 11)
#define QT_FUNCTION_TARGET_STRING_F16C              "f16c"
#define CpuFeatureRDRND                             (Q_UINT64_C(1) << 12)
#define QT_FUNCTION_TARGET_STRING_RDRND             "rdrnd"

// in CPUID Leaf 7, Sub-leaf 0, EBX:
#define CpuFeatureBMI                               (Q_UINT64_C(1) << 13)
#define QT_FUNCTION_TARGET_STRING_BMI               "bmi"
#define CpuFeatureHLE                               (Q_UINT64_C(1) << 14)
#define QT_FUNCTION_TARGET_STRING_HLE               "hle"
#define CpuFeatureAVX2                              (Q_UINT64_C(1) << 15)
#define QT_FUNCTION_TARGET_STRING_AVX2              "avx2"
#define CpuFeatureBMI2                              (Q_UINT64_C(1) << 16)
#define QT_FUNCTION_TARGET_STRING_BMI2              "bmi2"
#define CpuFeatureRTM                               (Q_UINT64_C(1) << 17)
#define QT_FUNCTION_TARGET_STRING_RTM               "rtm"
#define CpuFeatureAVX512F                           (Q_UINT64_C(1) << 18)
#define QT_FUNCTION_TARGET_STRING_AVX512F           "avx512f"
#define CpuFeatureAVX512DQ                          (Q_UINT64_C(1) << 19)
#define QT_FUNCTION_TARGET_STRING_AVX512DQ          "avx512dq"
#define CpuFeatureRDSEED                            (Q_UINT64_C(1) << 20)
#define QT_FUNCTION_TARGET_STRING_RDSEED            "rdseed"
#define CpuFeatureAVX512IFMA                        (Q_UINT64_C(1) << 21)
#define QT_FUNCTION_TARGET_STRING_AVX512IFMA        "avx512ifma"
#define CpuFeatureAVX512PF                          (Q_UINT64_C(1) << 22)
#define QT_FUNCTION_TARGET_STRING_AVX512PF          "avx512pf"
#define CpuFeatureAVX512ER                          (Q_UINT64_C(1) << 23)
#define QT_FUNCTION_TARGET_STRING_AVX512ER          "avx512er"
#define CpuFeatureAVX512CD                          (Q_UINT64_C(1) << 24)
#define QT_FUNCTION_TARGET_STRING_AVX512CD          "avx512cd"
#define CpuFeatureSHA                               (Q_UINT64_C(1) << 25)
#define QT_FUNCTION_TARGET_STRING_SHA               "sha"
#define CpuFeatureAVX512BW                          (Q_UINT64_C(1) << 26)
#define QT_FUNCTION_TARGET_STRING_AVX512BW          "avx512bw"
#define CpuFeatureAVX512VL                          (Q_UINT64_C(1) << 27)
#define QT_FUNCTION_TARGET_STRING_AVX512VL          "avx512vl"

// in CPUID Leaf 7, Sub-leaf 0, ECX:
#define CpuFeatureAVX512VBMI                        (Q_UINT64_C(1) << 28)
#define QT_FUNCTION_TARGET_STRING_AVX512VBMI        "avx512vbmi"
#define CpuFeatureAVX512VBMI2                       (Q_UINT64_C(1) << 29)
#define QT_FUNCTION_TARGET_STRING_AVX512VBMI2       "avx512vbmi2"
#define CpuFeatureGFNI                              (Q_UINT64_C(1) << 30)
#define QT_FUNCTION_TARGET_STRING_GFNI              "gfni"
#define CpuFeatureVAES                              (Q_UINT64_C(1) << 31)
#define QT_FUNCTION_TARGET_STRING_VAES              "vaes"
#define CpuFeatureAVX512VNNI                        (Q_UINT64_C(1) << 32)
#define QT_FUNCTION_TARGET_STRING_AVX512VNNI        "avx512vnni"
#define CpuFeatureAVX512BITALG                      (Q_UINT64_C(1) << 33)
#define QT_FUNCTION_TARGET_STRING_AVX512BITALG      "avx512bitalg"
#define CpuFeatureAVX512VPOPCNTDQ                   (Q_UINT64_C(1) << 34)
#define QT_FUNCTION_TARGET_STRING_AVX512VPOPCNTDQ   "avx512vpopcntdq"

// in CPUID Leaf 7, Sub-leaf 0, EDX:
#define CpuFeatureAVX5124NNIW                       (Q_UINT64_C(1) << 35)
#define QT_FUNCTION_TARGET_STRING_AVX5124NNIW       "avx5124nniw"
#define CpuFeatureAVX5124FMAPS                      (Q_UINT64_C(1) << 36)
#define QT_FUNCTION_TARGET_STRING_AVX5124FMAPS      "avx5124fmaps"

static const quint64 qCompilerCpuFeatures = 0
#ifdef __SSE2__
         | CpuFeatureSSE2
#endif
#ifdef __SSE3__
         | CpuFeatureSSE3
#endif
#ifdef __SSSE3__
         | CpuFeatureSSSE3
#endif
#ifdef __FMA__
         | CpuFeatureFMA
#endif
#ifdef __SSE4_1__
         | CpuFeatureSSE4_1
#endif
#ifdef __SSE4_2__
         | CpuFeatureSSE4_2
#endif
#ifdef __MOVBE__
         | CpuFeatureMOVBE
#endif
#ifdef __POPCNT__
         | CpuFeaturePOPCNT
#endif
#ifdef __AES__
         | CpuFeatureAES
#endif
#ifdef __AVX__
         | CpuFeatureAVX
#endif
#ifdef __F16C__
         | CpuFeatureF16C
#endif
#ifdef __RDRND__
         | CpuFeatureRDRND
#endif
#ifdef __BMI__
         | CpuFeatureBMI
#endif
#ifdef __HLE__
         | CpuFeatureHLE
#endif
#ifdef __AVX2__
         | CpuFeatureAVX2
#endif
#ifdef __BMI2__
         | CpuFeatureBMI2
#endif
#ifdef __RTM__
         | CpuFeatureRTM
#endif
#ifdef __AVX512F__
         | CpuFeatureAVX512F
#endif
#ifdef __AVX512DQ__
         | CpuFeatureAVX512DQ
#endif
#ifdef __RDSEED__
         | CpuFeatureRDSEED
#endif
#ifdef __AVX512IFMA__
         | CpuFeatureAVX512IFMA
#endif
#ifdef __AVX512PF__
         | CpuFeatureAVX512PF
#endif
#ifdef __AVX512ER__
         | CpuFeatureAVX512ER
#endif
#ifdef __AVX512CD__
         | CpuFeatureAVX512CD
#endif
#ifdef __SHA__
         | CpuFeatureSHA
#endif
#ifdef __AVX512BW__
         | CpuFeatureAVX512BW
#endif
#ifdef __AVX512VL__
         | CpuFeatureAVX512VL
#endif
#ifdef __AVX512VBMI__
         | CpuFeatureAVX512VBMI
#endif
#ifdef __AVX512VBMI2__
         | CpuFeatureAVX512VBMI2
#endif
#ifdef __GFNI__
         | CpuFeatureGFNI
#endif
#ifdef __VAES__
         | CpuFeatureVAES
#endif
#ifdef __AVX512VNNI__
         | CpuFeatureAVX512VNNI
#endif
#ifdef __AVX512BITALG__
         | CpuFeatureAVX512BITALG
#endif
#ifdef __AVX512VPOPCNTDQ__
         | CpuFeatureAVX512VPOPCNTDQ
#endif
#ifdef __AVX5124NNIW__
         | CpuFeatureAVX5124NNIW
#endif
#ifdef __AVX5124FMAPS__
         | CpuFeatureAVX5124FMAPS
#endif
        ;

QT_END_NAMESPACE

#endif // QSIMD_X86_P_H
