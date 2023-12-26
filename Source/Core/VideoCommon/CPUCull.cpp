// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/CPUCull.h"

#include "Common/Assert.h"
#include "Common/CPUDetect.h"
#include "Common/MathUtil.h"
#include "Common/MemoryUtil.h"
#include "Core/System.h"

#include "VideoCommon/CPMemory.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"
#include "VideoCommon/XFStateManager.h"

// We really want things like c.w * a.x - a.w * c.x to stay symmetric, so they cancel to zero on
// degenerate triangles.  Make sure the compiler doesn't optimize in fmas where not requested.
#ifdef _MSC_VER
#pragma fp_contract(off)
#else
// GCC doesn't support any in-file way to turn off fp contract yet
// Not ideal, but worst case scenario its cpu cull is worse at detecting degenerate triangles
// (Most likely to happen on arm, as we don't compile the cull code for x86 fma)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma STDC FP_CONTRACT OFF
#pragma GCC diagnostic pop
#endif

#if defined(_M_X86) || defined(_M_X86_64)
#define USE_SSE
#elif defined(_M_ARM_64)
#define USE_NEON
#else
#define NO_SIMD
#endif

#if defined(USE_SSE)
#include <immintrin.h>
#elif defined(USE_NEON)
#include <arm_neon.h>
#endif

#include "VideoCommon/CPUCullImpl.h"
#ifdef USE_SSE
#define USE_SSE3
#include "VideoCommon/CPUCullImpl.h"
#define USE_SSE41
#include "VideoCommon/CPUCullImpl.h"
#define USE_AVX
#include "VideoCommon/CPUCullImpl.h"
#define USE_FMA
#include "VideoCommon/CPUCullImpl.h"
#endif

#if defined(USE_SSE)
#if defined(__AVX__) && defined(__FMA__)
static constexpr int MIN_SSE = 51;
#elif defined(__AVX__)
static constexpr int MIN_SSE = 50;
#elif defined(__SSE4_1__)
static constexpr int MIN_SSE = 41;
#elif defined(__SSE3__)
static constexpr int MIN_SSE = 30;
#else
static constexpr int MIN_SSE = 0;
#endif
#endif

template <bool PositionHas3Elems, bool PerVertexPosMtx>
static CPUCull::TransformFunction GetTransformFunction()
{
#if defined(USE_SSE)
  if (MIN_SSE >= 51 || (cpu_info.bAVX && cpu_info.bFMA))
    return CPUCull_FMA::TransformVertices<PositionHas3Elems, PerVertexPosMtx>;
  else if (MIN_SSE >= 50 || cpu_info.bAVX)
    return CPUCull_AVX::TransformVertices<PositionHas3Elems, PerVertexPosMtx>;
  else if (PositionHas3Elems && PerVertexPosMtx && (MIN_SSE >= 41 || cpu_info.bSSE4_1))
    return CPUCull_SSE41::TransformVertices<PositionHas3Elems, PerVertexPosMtx>;
  else if (PositionHas3Elems && (MIN_SSE >= 30 || cpu_info.bSSE3))
    return CPUCull_SSE3::TransformVertices<PositionHas3Elems, PerVertexPosMtx>;
  else
    return CPUCull_SSE::TransformVertices<PositionHas3Elems, PerVertexPosMtx>;
#elif defined(USE_NEON)
  return CPUCull_NEON::TransformVertices<PositionHas3Elems, PerVertexPosMtx>;
#else
  return CPUCull_Scalar::TransformVertices<PositionHas3Elems, PerVertexPosMtx>;
#endif
}

template <OpcodeDecoder::Primitive Primitive, CullMode Mode>
static CPUCull::CullFunction GetCullFunction0()
{
#if defined(USE_SSE)
  // Note: AVX version only actually AVX on compilers that support __attribute__((target))
  // Sorry, MSVC + Sandy Bridge.  (Ivy+ and AMD see very little benefit thanks to mov elimination)
  if (MIN_SSE >= 50 || cpu_info.bAVX)
    return CPUCull_AVX::AreAllVerticesCulled<Primitive, Mode>;
  else if (MIN_SSE >= 30 || cpu_info.bSSE3)
    return CPUCull_SSE3::AreAllVerticesCulled<Primitive, Mode>;
  else
    return CPUCull_SSE::AreAllVerticesCulled<Primitive, Mode>;
#elif defined(USE_NEON)
  return CPUCull_NEON::AreAllVerticesCulled<Primitive, Mode>;
#else
  return CPUCull_Scalar::AreAllVerticesCulled<Primitive, Mode>;
#endif
}

template <OpcodeDecoder::Primitive Primitive>
static Common::EnumMap<CPUCull::CullFunction, CullMode::All> GetCullFunction1()
{
  return {
      GetCullFunction0<Primitive, CullMode::None>(),
      GetCullFunction0<Primitive, CullMode::Back>(),
      GetCullFunction0<Primitive, CullMode::Front>(),
      GetCullFunction0<Primitive, CullMode::All>(),
  };
}

CPUCull::~CPUCull() = default;

void CPUCull::Init()
{
  m_transform_table[false][false] = GetTransformFunction<false, false>();
  m_transform_table[false][true] = GetTransformFunction<false, true>();
  m_transform_table[true][false] = GetTransformFunction<true, false>();
  m_transform_table[true][true] = GetTransformFunction<true, true>();
  using Prim = OpcodeDecoder::Primitive;
  m_cull_table[Prim::GX_DRAW_QUADS] = GetCullFunction1<Prim::GX_DRAW_QUADS>();
  m_cull_table[Prim::GX_DRAW_QUADS_2] = GetCullFunction1<Prim::GX_DRAW_QUADS>();
  m_cull_table[Prim::GX_DRAW_TRIANGLES] = GetCullFunction1<Prim::GX_DRAW_TRIANGLES>();
  m_cull_table[Prim::GX_DRAW_TRIANGLE_STRIP] = GetCullFunction1<Prim::GX_DRAW_TRIANGLE_STRIP>();
  m_cull_table[Prim::GX_DRAW_TRIANGLE_FAN] = GetCullFunction1<Prim::GX_DRAW_TRIANGLE_FAN>();
}

bool CPUCull::AreAllVerticesCulled(VertexLoaderBase* loader, OpcodeDecoder::Primitive primitive,
                                   const u8* src, u32 count)
{
  ASSERT_MSG(VIDEO, primitive < OpcodeDecoder::Primitive::GX_DRAW_LINES,
             "CPUCull should not be called on lines or points");
  const u32 stride = loader->m_native_vtx_decl.stride;
  const bool posHas3Elems = loader->m_native_vtx_decl.position.components >= 3;
  const bool perVertexPosMtx = loader->m_native_vtx_decl.posmtx.enable;
  if (m_transform_buffer_size < count) [[unlikely]]
  {
    u32 new_size = MathUtil::NextPowerOf2(count);
    m_transform_buffer_size = new_size;
    m_transform_buffer.reset(static_cast<TransformedVertex*>(
        Common::AllocateAlignedMemory(new_size * sizeof(TransformedVertex), 32)));
  }

  // transform functions need the projection matrix to tranform to clip space
  auto& system = Core::System::GetInstance();
  system.GetVertexShaderManager().SetProjectionMatrix(system.GetXFStateManager());

  static constexpr Common::EnumMap<CullMode, CullMode::All> cullmode_invert = {
      CullMode::None, CullMode::Front, CullMode::Back, CullMode::All};

  CullMode cullmode = bpmem.genMode.cullmode;
  if (xfmem.viewport.ht > 0)  // See videosoftware Clipper.cpp:IsBackface
    cullmode = cullmode_invert[cullmode];
  const TransformFunction transform = m_transform_table[posHas3Elems][perVertexPosMtx];
  transform(m_transform_buffer.get(), src, stride, count);
  const CullFunction cull = m_cull_table[primitive][cullmode];
  return cull(m_transform_buffer.get(), count);
}

template <typename T>
void CPUCull::BufferDeleter<T>::operator()(T* ptr)
{
  Common::FreeAlignedMemory(ptr);
}
