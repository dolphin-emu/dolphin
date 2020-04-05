/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2018 Intel Corporation.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QSIMD_P_H
#define QSIMD_P_H

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

#include <QtCore/private/qglobal_p.h>

/*
 * qt_module_config.prf defines the QT_COMPILER_SUPPORTS_XXX macros.
 * They mean the compiler supports the necessary flags and the headers
 * for the x86 and ARM intrinsics:
 *  - GCC: the -mXXX or march=YYY flag is necessary before #include
 *    up to 4.8; GCC >= 4.9 can include unconditionally
 *  - Intel CC: #include can happen unconditionally
 *  - MSVC: #include can happen unconditionally
 *  - RVCT: ???
 *
 * We will try to include all headers possible under this configuration.
 *
 * MSVC does not define __SSE2__ & family, so we will define them. MSVC 2013 &
 * up do define __AVX__ if the -arch:AVX option is passed on the command-line.
 *
 * Supported XXX are:
 *   Flag    | Arch |  GCC  | Intel CC |  MSVC  |
 *  ARM_NEON | ARM  | I & C | None     |   ?    |
 *  SSE2     | x86  | I & C | I & C    | I & C  |
 *  SSE3     | x86  | I & C | I & C    | I only |
 *  SSSE3    | x86  | I & C | I & C    | I only |
 *  SSE4_1   | x86  | I & C | I & C    | I only |
 *  SSE4_2   | x86  | I & C | I & C    | I only |
 *  AVX      | x86  | I & C | I & C    | I & C  |
 *  AVX2     | x86  | I & C | I & C    | I only |
 *  AVX512xx | x86  | I & C | I & C    | I only |
 * I = intrinsics; C = code generation
 *
 * Code can use the following constructs to determine compiler support & status:
 * - #ifdef __XXX__      (e.g: #ifdef __AVX__  or #ifdef __ARM_NEON__)
 *   If this test passes, then the compiler is already generating code for that
 *   given sub-architecture. The intrinsics for that sub-architecture are
 *   #included and can be used without restriction or runtime check.
 *
 * - #if QT_COMPILER_SUPPORTS(XXX)
 *   If this test passes, then the compiler is able to generate code for that
 *   given sub-architecture in another translation unit, given the right set of
 *   flags. Use of the intrinsics is not guaranteed. This is useful with
 *   runtime detection (see below).
 *
 * - #if QT_COMPILER_SUPPORTS_HERE(XXX)
 *   If this test passes, then the compiler is able to generate code for that
 *   given sub-architecture in this translation unit, even if it is not doing
 *   that now (it might be). Individual functions may be tagged with
 *   QT_FUNCTION_TARGET(XXX) to cause the compiler to generate code for that
 *   sub-arch. Only inside such functions is the use of the intrisics
 *   guaranteed to work. This is useful with runtime detection (see below).
 *
 * Runtime detection of a CPU sub-architecture can be done with the
 * qCpuHasFeature(XXX) function. There are two strategies for generating
 * optimized code like that:
 *
 * 1) place the optimized code in a different translation unit (C or assembly
 * sources) and pass the correct flags to the compiler to enable support. Those
 * sources must not include qglobal.h, which means they cannot include this
 * file either. The dispatcher function would look like this:
 *
 *      void foo()
 *      {
 *      #if QT_COMPILER_SUPPORTS(XXX)
 *          if (qCpuHasFeature(XXX)) {
 *              foo_optimized_xxx();
 *              return;
 *          }
 *      #endif
 *          foo_plain();
 *      }
 *
 * 2) place the optimized code in a function tagged with QT_FUNCTION_TARGET and
 * surrounded by #if QT_COMPILER_SUPPORTS_HERE(XXX). That code can freely use
 * other Qt code. The dispatcher function would look like this:
 *
 *      void foo()
 *      {
 *      #if QT_COMPILER_SUPPORTS_HERE(XXX)
 *          if (qCpuHasFeature(XXX)) {
 *              foo_optimized_xxx();
 *              return;
 *          }
 *      #endif
 *          foo_plain();
 *      }
 */

#if defined(__MINGW64_VERSION_MAJOR) || defined(Q_CC_MSVC)
#include <intrin.h>
#endif

#define QT_COMPILER_SUPPORTS(x)     (QT_COMPILER_SUPPORTS_ ## x - 0)

#if defined(Q_PROCESSOR_ARM)
#  define QT_COMPILER_SUPPORTS_HERE(x)    (__ARM_FEATURE_ ## x)
#  if defined(Q_CC_GNU) && !defined(Q_CC_INTEL) && Q_CC_GNU >= 600
     /* GCC requires attributes for a function */
#    define QT_FUNCTION_TARGET(x)  __attribute__((__target__(QT_FUNCTION_TARGET_STRING_ ## x)))
#  else
#    define QT_FUNCTION_TARGET(x)
#  endif
#  if !defined(__ARM_FEATURE_NEON) && defined(__ARM_NEON__)
#    define __ARM_FEATURE_NEON           // also support QT_COMPILER_SUPPORTS_HERE(NEON)
#  endif
#elif defined(Q_PROCESSOR_MIPS)
#  define QT_COMPILER_SUPPORTS_HERE(x)    (__ ## x ## __)
#  define QT_FUNCTION_TARGET(x)
#  if !defined(__MIPS_DSP__) && defined(__mips_dsp) && defined(Q_PROCESSOR_MIPS_32)
#    define __MIPS_DSP__
#  endif
#  if !defined(__MIPS_DSPR2__) && defined(__mips_dspr2) && defined(Q_PROCESSOR_MIPS_32)
#    define __MIPS_DSPR2__
#  endif
#elif defined(Q_PROCESSOR_X86) && defined(QT_COMPILER_SUPPORTS_SIMD_ALWAYS)
#  define QT_COMPILER_SUPPORTS_HERE(x)    ((__ ## x ## __) || QT_COMPILER_SUPPORTS(x))
#  if defined(Q_CC_GNU) && !defined(Q_CC_INTEL)
     /* GCC requires attributes for a function */
#    define QT_FUNCTION_TARGET(x)  __attribute__((__target__(QT_FUNCTION_TARGET_STRING_ ## x)))
#  else
#    define QT_FUNCTION_TARGET(x)
#  endif
#else
#  define QT_COMPILER_SUPPORTS_HERE(x)    (__ ## x ## __)
#  define QT_FUNCTION_TARGET(x)
#endif

#ifdef Q_PROCESSOR_X86
/* -- x86 intrinsic support -- */

#  if defined(Q_CC_MSVC) && (defined(_M_X64) || _M_IX86_FP >= 2)
// MSVC doesn't define __SSE2__, so do it ourselves
#    define __SSE__                         1
#    define __SSE2__                        1
#  endif

#  ifdef __SSE2__
// #include the intrinsics
#    include <immintrin.h>
#  endif

#  if defined(Q_CC_GNU) && !defined(Q_CC_INTEL)
// GCC 4.4 and Clang 2.8 added a few more intrinsics there
#    include <x86intrin.h>
#  endif

#  if defined(Q_CC_MSVC) && (defined(_M_AVX) || defined(__AVX__))
// Visual Studio defines __AVX__ when /arch:AVX is passed, but not the earlier macros
// See: https://msdn.microsoft.com/en-us/library/b0084kay.aspx
#    define __SSE3__                        1
#    define __SSSE3__                       1
// no Intel CPU supports SSE4a, so don't define it
#    define __SSE4_1__                      1
#    define __SSE4_2__                      1
#    ifndef __AVX__
#      define __AVX__                       1
#    endif
#  endif

#  if defined(__SSE4_2__) && defined(QT_COMPILER_SUPPORTS_SIMD_ALWAYS) && (defined(Q_CC_INTEL) || defined(Q_CC_MSVC))
// POPCNT instructions:
// All processors that support SSE4.2 support POPCNT
// (but neither MSVC nor the Intel compiler define this macro)
#    define __POPCNT__                      1
#  endif

// AVX intrinsics
#  if defined(__AVX__) && defined(QT_COMPILER_SUPPORTS_SIMD_ALWAYS) && (defined(Q_CC_INTEL) || defined(Q_CC_MSVC))
// AES, PCLMULQDQ instructions:
// All processors that support AVX support PCLMULQDQ
// (but neither MSVC nor the Intel compiler define this macro)
#    define __PCLMUL__                      1
#  endif

#  if defined(__AVX2__) && defined(QT_COMPILER_SUPPORTS_SIMD_ALWAYS) && (defined(Q_CC_INTEL) || defined(Q_CC_MSVC))
// F16C & RDRAND instructions:
// All processors that support AVX2 support F16C & RDRAND:
// (but neither MSVC nor the Intel compiler define these macros)
#    define __F16C__                        1
#    define __RDRND__                       1
#  endif

#  if defined(__BMI__) && !defined(__BMI2__) && defined(Q_CC_INTEL)
// BMI2 instructions:
// All processors that support BMI support BMI2 (and AVX2)
// (but neither MSVC nor the Intel compiler define this macro)
#    define __BMI2__                        1
#  endif

#  include "qsimd_x86_p.h"

// Haswell sub-architecture
//
// The Intel Core 4th generation was codenamed "Haswell" and introduced AVX2,
// BMI1, BMI2, FMA, LZCNT, MOVBE, which makes it a good divider for a
// sub-target for us. The first AMD processor with AVX2 support (Zen) has the
// same features.
//
// macOS's fat binaries support the "x86_64h" sub-architecture and the GNU libc
// ELF loader also supports a "haswell/" subdir (e.g., /usr/lib/haswell).
#  define QT_FUNCTION_TARGET_STRING_ARCH_HASWELL    "arch=haswell"
#  if defined(__AVX2__) && defined(__BMI__) && defined(__BMI2__) && defined(__F16C__) && \
    defined(__FMA__) && defined(__LZCNT__) && defined(__RDRND__)
#    define __haswell__       1
#  endif

// This constant does not include all CPU features found in a Haswell, only
// those that we'd have optimized code for.
// Note: must use Q_CONSTEXPR here, as this file may be compiled in C mode.
QT_BEGIN_NAMESPACE
static const quint64 CpuFeatureArchHaswell    = 0
        | CpuFeatureSSE2
        | CpuFeatureSSE3
        | CpuFeatureSSSE3
        | CpuFeatureSSE4_1
        | CpuFeatureSSE4_2
        | CpuFeatureFMA
        | CpuFeaturePOPCNT
        | CpuFeatureAVX
        | CpuFeatureF16C
        | CpuFeatureAVX2
        | CpuFeatureBMI
        | CpuFeatureBMI2;
QT_END_NAMESPACE

#endif  /* Q_PROCESSOR_X86 */

// Clang compiler fix, see http://lists.llvm.org/pipermail/cfe-commits/Week-of-Mon-20160222/151168.html
// This should be tweaked with an "upper version" of clang once we know which release fixes the
// issue. At that point we can rely on __ARM_FEATURE_CRC32 again.
#if defined(Q_CC_CLANG) && defined(Q_OS_DARWIN) && defined (__ARM_FEATURE_CRC32)
#  undef __ARM_FEATURE_CRC32
#endif

// NEON intrinsics
// note: as of GCC 4.9, does not support function targets for ARM
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#define QT_FUNCTION_TARGET_STRING_NEON      "+neon" // unused: gcc doesn't support function targets on non-aarch64, and on Aarch64 NEON is always available.
#ifndef __ARM_NEON__
// __ARM_NEON__ is not defined on AArch64, but we need it in our NEON detection.
#define __ARM_NEON__
#endif
#endif
// AArch64/ARM64
#if defined(Q_PROCESSOR_ARM_V8) && defined(__ARM_FEATURE_CRC32)
#if defined(Q_PROCESSOR_ARM_64)
// only available on aarch64
#define QT_FUNCTION_TARGET_STRING_CRC32      "+crc"
#endif
#  include <arm_acle.h>
#endif

#ifdef __cplusplus
#include <qatomic.h>

QT_BEGIN_NAMESPACE

#ifndef Q_PROCESSOR_X86
enum CPUFeatures {
#if defined(Q_PROCESSOR_ARM)
    CpuFeatureNEON          = 2,
    CpuFeatureARM_NEON      = CpuFeatureNEON,
    CpuFeatureCRC32         = 4,
#elif defined(Q_PROCESSOR_MIPS)
    CpuFeatureDSP           = 2,
    CpuFeatureDSPR2         = 4,
#endif

    // used only to indicate that the CPU detection was initialised
    QSimdInitialized        = 1
};

static const quint64 qCompilerCpuFeatures = 0
#if defined __ARM_NEON__
        | CpuFeatureNEON
#endif
#if defined __ARM_FEATURE_CRC32
        | CpuFeatureCRC32
#endif
#if defined __mips_dsp
        | CpuFeatureDSP
#endif
#if defined __mips_dspr2
        | CpuFeatureDSPR2
#endif
        ;
#endif

#ifdef Q_ATOMIC_INT64_IS_SUPPORTED
extern Q_CORE_EXPORT QBasicAtomicInteger<quint64> qt_cpu_features[1];
#else
extern Q_CORE_EXPORT QBasicAtomicInteger<unsigned> qt_cpu_features[2];
#endif
Q_CORE_EXPORT quint64 qDetectCpuFeatures();

#if defined(Q_PROCESSOR_X86) && QT_COMPILER_SUPPORTS_HERE(RDRND) && !defined(QT_BOOTSTRAPPED)
Q_CORE_EXPORT qsizetype qRandomCpu(void *, qsizetype) noexcept;
#else
static inline qsizetype qRandomCpu(void *, qsizetype) noexcept
{
    return 0;
}
#endif

static inline quint64 qCpuFeatures()
{
    quint64 features = qt_cpu_features[0].loadRelaxed();
#ifndef Q_ATOMIC_INT64_IS_SUPPORTED
    features |= quint64(qt_cpu_features[1].loadRelaxed()) << 32;
#endif
    if (Q_UNLIKELY(features == 0)) {
        features = qDetectCpuFeatures();
        Q_ASSUME(features != 0);
    }
    return features;
}

#define qCpuHasFeature(feature)     (((qCompilerCpuFeatures & CpuFeature ## feature) == CpuFeature ## feature) \
                                     || ((qCpuFeatures() & CpuFeature ## feature) == CpuFeature ## feature))

inline bool qHasHwrng()
{
#if defined(Q_PROCESSOR_X86) && QT_COMPILER_SUPPORTS_HERE(RDRND)
    return qCpuHasFeature(RDRND);
#else
    return false;
#endif
}

#define ALIGNMENT_PROLOGUE_16BYTES(ptr, i, length) \
    for (; i < static_cast<int>(qMin(static_cast<quintptr>(length), ((4 - ((reinterpret_cast<quintptr>(ptr) >> 2) & 0x3)) & 0x3))); ++i)

#define ALIGNMENT_PROLOGUE_32BYTES(ptr, i, length) \
    for (; i < static_cast<int>(qMin(static_cast<quintptr>(length), ((8 - ((reinterpret_cast<quintptr>(ptr) >> 2) & 0x7)) & 0x7))); ++i)

QT_END_NAMESPACE

#endif // __cplusplus

#define SIMD_EPILOGUE(i, length, max) \
    for (int _i = 0; _i < max && i < length; ++i, ++_i)

#endif // QSIMD_P_H
