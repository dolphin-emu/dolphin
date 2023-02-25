// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#if defined(USE_FMA)
#define VECTOR_NAMESPACE CPUCull_FMA
#elif defined(USE_AVX)
#define VECTOR_NAMESPACE CPUCull_AVX
#elif defined(USE_SSE41)
#define VECTOR_NAMESPACE CPUCull_SSE41
#elif defined(USE_SSE3)
#define VECTOR_NAMESPACE CPUCull_SSE3
#elif defined(USE_SSE)
#define VECTOR_NAMESPACE CPUCull_SSE
#elif defined(USE_NEON)
#define VECTOR_NAMESPACE CPUCull_NEON
#elif defined(NO_SIMD)
#define VECTOR_NAMESPACE CPUCull_Scalar
#else
#error This file is meant to be used by CPUCull.cpp only!
#endif

#if defined(__GNUC__) && defined(USE_FMA) && !(defined(__AVX__) && defined(__FMA__))
#define ATTR_TARGET __attribute__((target("avx,fma")))
#elif defined(__GNUC__) && defined(USE_AVX) && !defined(__AVX__)
#define ATTR_TARGET __attribute__((target("avx")))
#elif defined(__GNUC__) && defined(USE_SSE41) && !defined(__SSE4_1__)
#define ATTR_TARGET __attribute__((target("sse4.1")))
#elif defined(__GNUC__) && defined(USE_SSE3) && !defined(__SSE3__)
#define ATTR_TARGET __attribute__((target("sse3")))
#else
#define ATTR_TARGET
#endif

namespace VECTOR_NAMESPACE
{
#if defined(USE_SSE)
typedef __m128 Vector;
#elif defined(USE_NEON)
typedef float32x4_t Vector;
#else
struct alignas(16) Vector
{
  float x, y, z, w;
};
#endif
static_assert(sizeof(Vector) == 16);

#ifdef USE_NEON
ATTR_TARGET DOLPHIN_FORCE_INLINE static Vector vsetr_f32(float x, float y, float z, float w)
{
  float tmp[4] = {x, y, z, w};
  return vld1q_f32(tmp);
}
ATTR_TARGET DOLPHIN_FORCE_INLINE static void vuzp12q_f32(Vector& a, Vector& b)
{
  Vector tmp = vuzp2q_f32(a, b);
  a = vuzp1q_f32(a, b);
  b = tmp;
}
#endif
#ifdef USE_SSE
template <int i>
ATTR_TARGET DOLPHIN_FORCE_INLINE static Vector vector_broadcast(Vector v)
{
  return _mm_shuffle_ps(v, v, _MM_SHUFFLE(i, i, i, i));
}
#endif
#ifdef USE_AVX
template <int i>
ATTR_TARGET DOLPHIN_FORCE_INLINE static __m256 vector_broadcast(__m256 v)
{
  return _mm256_shuffle_ps(v, v, _MM_SHUFFLE(i, i, i, i));
}
#endif

#ifdef USE_AVX
ATTR_TARGET DOLPHIN_FORCE_INLINE static void TransposeYMM(__m256& o0, __m256& o1,  //
                                                          __m256& o2, __m256& o3)
{
  __m256d tmp0 = _mm256_castps_pd(_mm256_unpacklo_ps(o0, o1));
  __m256d tmp1 = _mm256_castps_pd(_mm256_unpacklo_ps(o2, o3));
  __m256d tmp2 = _mm256_castps_pd(_mm256_unpackhi_ps(o0, o1));
  __m256d tmp3 = _mm256_castps_pd(_mm256_unpackhi_ps(o2, o3));
  o0 = _mm256_castpd_ps(_mm256_unpacklo_pd(tmp0, tmp1));
  o1 = _mm256_castpd_ps(_mm256_unpackhi_pd(tmp0, tmp1));
  o2 = _mm256_castpd_ps(_mm256_unpacklo_pd(tmp2, tmp3));
  o3 = _mm256_castpd_ps(_mm256_unpackhi_pd(tmp2, tmp3));
}

ATTR_TARGET DOLPHIN_FORCE_INLINE static void LoadTransposedYMM(const void* source, __m256& o0,
                                                               __m256& o1, __m256& o2, __m256& o3)
{
  const Vector* vsource = static_cast<const Vector*>(source);
  o0 = _mm256_broadcast_ps(&vsource[0]);
  o1 = _mm256_broadcast_ps(&vsource[1]);
  o2 = _mm256_broadcast_ps(&vsource[2]);
  o3 = _mm256_broadcast_ps(&vsource[3]);
  TransposeYMM(o0, o1, o2, o3);
}

ATTR_TARGET DOLPHIN_FORCE_INLINE static void LoadPosYMM(const void* sourcel, const void* sourceh,
                                                        __m256& o0, __m256& o1, __m256& o2)
{
  const Vector* vsourcel = static_cast<const Vector*>(sourcel);
  const Vector* vsourceh = static_cast<const Vector*>(sourceh);
  o0 = _mm256_insertf128_ps(_mm256_castps128_ps256(vsourcel[0]), vsourceh[0], 1);
  o1 = _mm256_insertf128_ps(_mm256_castps128_ps256(vsourcel[1]), vsourceh[1], 1);
  o2 = _mm256_insertf128_ps(_mm256_castps128_ps256(vsourcel[2]), vsourceh[2], 1);
}

ATTR_TARGET DOLPHIN_FORCE_INLINE static void
LoadTransposedPosYMM(const void* source, __m256& o0, __m256& o1, __m256& o2, __m256& o3)
{
  const Vector* vsource = static_cast<const Vector*>(source);
  o0 = _mm256_broadcast_ps(&vsource[0]);
  o1 = _mm256_broadcast_ps(&vsource[1]);
  o2 = _mm256_broadcast_ps(&vsource[2]);
  o3 = _mm256_setr_ps(0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
  TransposeYMM(o0, o1, o2, o3);
}

ATTR_TARGET DOLPHIN_FORCE_INLINE static __m256 ApplyMatrixYMM(__m256 v, __m256 m0, __m256 m1,
                                                              __m256 m2, __m256 m3)
{
  __m256 output = _mm256_mul_ps(vector_broadcast<0>(v), m0);
#ifdef USE_FMA
  output = _mm256_fmadd_ps(vector_broadcast<1>(v), m1, output);
  output = _mm256_fmadd_ps(vector_broadcast<2>(v), m2, output);
  output = _mm256_fmadd_ps(vector_broadcast<3>(v), m3, output);
#else
  output = _mm256_add_ps(output, _mm256_mul_ps(vector_broadcast<1>(v), m1));
  output = _mm256_add_ps(output, _mm256_mul_ps(vector_broadcast<2>(v), m2));
  output = _mm256_add_ps(output, _mm256_mul_ps(vector_broadcast<3>(v), m3));
#endif
  return output;
}

ATTR_TARGET DOLPHIN_FORCE_INLINE static __m256
TransformVertexNoTransposeYMM(__m256 vertex, __m256 pos0, __m256 pos1, __m256 pos2,  //
                              __m256 proj0, __m256 proj1, __m256 proj2, __m256 proj3)
{
  __m256 mul0 = _mm256_mul_ps(vertex, pos0);
  __m256 mul1 = _mm256_mul_ps(vertex, pos1);
  __m256 mul2 = _mm256_mul_ps(vertex, pos2);
  __m256 mul3 = _mm256_setr_ps(0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
  __m256 output = _mm256_hadd_ps(_mm256_hadd_ps(mul0, mul1), _mm256_hadd_ps(mul2, mul3));
  return ApplyMatrixYMM(output, proj0, proj1, proj2, proj3);
}

template <bool PositionHas3Elems>
ATTR_TARGET DOLPHIN_FORCE_INLINE static __m256
TransformVertexYMM(__m256 vertex, __m256 pos0, __m256 pos1, __m256 pos2, __m256 pos3,  //
                   __m256 proj0, __m256 proj1, __m256 proj2, __m256 proj3)
{
  __m256 output = pos3;  // vertex.w is always 1.0
#ifdef USE_FMA
  output = _mm256_fmadd_ps(vector_broadcast<0>(vertex), pos0, output);
  output = _mm256_fmadd_ps(vector_broadcast<1>(vertex), pos1, output);
  if constexpr (PositionHas3Elems)
    output = _mm256_fmadd_ps(vector_broadcast<2>(vertex), pos2, output);
#else
  output = _mm256_add_ps(output, _mm256_mul_ps(vector_broadcast<0>(vertex), pos0));
  output = _mm256_add_ps(output, _mm256_mul_ps(vector_broadcast<1>(vertex), pos1));
  if constexpr (PositionHas3Elems)
    output = _mm256_add_ps(output, _mm256_mul_ps(vector_broadcast<2>(vertex), pos2));
#endif
  return ApplyMatrixYMM(output, proj0, proj1, proj2, proj3);
}

template <bool PositionHas3Elems, bool PerVertexPosMtx>
ATTR_TARGET DOLPHIN_FORCE_INLINE static __m256
LoadTransform2Vertices(const u8* v0data, const u8* v1data,                  //
                       __m256 pos0, __m256 pos1, __m256 pos2, __m256 pos3,  //
                       __m256 proj0, __m256 proj1, __m256 proj2, __m256 proj3)
{
  __m256 v01;
  if constexpr (PerVertexPosMtx)
  {
    // Vertex data layout always starts with posmtx data if available, then position data
    // Convenient for us, that means offsets are always fixed
    u32 v0idx = v0data[0] & 0x3f;
    u32 v1idx = v1data[0] & 0x3f;
    v0data += sizeof(u32);
    v1data += sizeof(u32);

    const float* v0fdata = reinterpret_cast<const float*>(v0data);
    const float* v1fdata = reinterpret_cast<const float*>(v1data);

    LoadPosYMM(&xfmem.posMatrices[v0idx * 4], &xfmem.posMatrices[v1idx * 4], pos0, pos1, pos2);

    if constexpr (PositionHas3Elems)
    {
      __m256 base = _mm256_set1_ps(1.0f);
      v01 = _mm256_blend_ps(_mm256_loadu2_m128(v1fdata, v0fdata), base, 0x88);
    }
    else
    {
      __m256 base = _mm256_unpacklo_ps(_mm256_setzero_ps(), _mm256_set1_ps(1.0f));
      __m256 v1 = _mm256_castpd_ps(_mm256_broadcast_sd(reinterpret_cast<const double*>(v1data)));
      __m128 v0 = _mm_loadl_pi(_mm_setzero_ps(), reinterpret_cast<const __m64*>(v0data));
      v01 = _mm256_blend_ps(_mm256_castps128_ps256(v0), v1, 0x30);
      v01 = _mm256_blend_ps(v01, base, 0xcc);
    }

    v01 = TransformVertexNoTransposeYMM(v01, pos0, pos1, pos2, proj0, proj1, proj2, proj3);
  }
  else
  {
    const float* v0fdata = reinterpret_cast<const float*>(v0data);
    const float* v1fdata = reinterpret_cast<const float*>(v1data);
    if constexpr (PositionHas3Elems)
    {
      v01 = _mm256_loadu2_m128(v1fdata, v0fdata);
    }
    else
    {
      __m256 v1 = _mm256_castpd_ps(_mm256_broadcast_sd(reinterpret_cast<const double*>(v1data)));
      __m128 v0 = _mm_loadl_pi(_mm_setzero_ps(), reinterpret_cast<const __m64*>(v0data));
      v01 = _mm256_blend_ps(_mm256_castps128_ps256(v0), v1, 0x30);
    }

#ifdef __clang__
    // Clang's optimizer is dumb, yay
    // It sees TransformVertexYMM doing broadcasts and is like
    // "let's broadcast *before* we combine v0 and v1!  Then we can use vbroadcastss!"
    // Prevent it from "optimizing" here
    asm("" : "+x"(v01)::);
#endif

    v01 = TransformVertexYMM<PositionHas3Elems>(v01, pos0, pos1, pos2, pos3,  //
                                                proj0, proj1, proj2, proj3);
  }

  return v01;
}

#endif

#ifndef USE_AVX
// Note: Assumes 16-byte aligned source
ATTR_TARGET DOLPHIN_FORCE_INLINE static void LoadTransposed(const void* source, Vector& o0,
                                                            Vector& o1, Vector& o2, Vector& o3)
{
#if defined(USE_SSE)
  const Vector* vsource = static_cast<const Vector*>(source);
  o0 = vsource[0];
  o1 = vsource[1];
  o2 = vsource[2];
  o3 = vsource[3];
  _MM_TRANSPOSE4_PS(o0, o1, o2, o3);
#elif defined(USE_NEON)
  float32x4x4_t ld = vld4q_f32(static_cast<const float*>(source));
  o0 = ld.val[0];
  o1 = ld.val[1];
  o2 = ld.val[2];
  o3 = ld.val[3];
#else
  const Vector* vsource = static_cast<const Vector*>(source);
  // clang-format off
  o0.x = vsource[0].x; o0.y = vsource[1].x; o0.z = vsource[2].x; o0.w = vsource[3].x;
  o1.x = vsource[0].y; o1.y = vsource[1].y; o1.z = vsource[2].y; o1.w = vsource[3].y;
  o2.x = vsource[0].z; o2.y = vsource[1].z; o2.z = vsource[2].z; o2.w = vsource[3].z;
  o3.x = vsource[0].w; o3.y = vsource[1].w; o3.z = vsource[2].w; o3.w = vsource[3].w;
  // clang-format on
#endif
}

ATTR_TARGET DOLPHIN_FORCE_INLINE static void LoadTransposedPos(const void* source, Vector& o0,
                                                               Vector& o1, Vector& o2, Vector& o3)
{
  const Vector* vsource = static_cast<const Vector*>(source);
#if defined(USE_SSE)
  o0 = vsource[0];
  o1 = vsource[1];
  o2 = vsource[2];
  o3 = _mm_setr_ps(0.0f, 0.0f, 0.0f, 1.0f);
  _MM_TRANSPOSE4_PS(o0, o1, o2, o3);
#elif defined(USE_NEON)
  float32x4x2_t ld01 = vld2q_f32(static_cast<const float*>(source));
  o0 = ld01.val[0];
  o1 = ld01.val[1];
  o2 = vsource[2];
  o3 = vsetr_f32(0.0f, 0.0f, 0.0f, 1.0f);
  vuzp12q_f32(o2, o3);
  vuzp12q_f32(o0, o2);
  vuzp12q_f32(o1, o3);
#else
  // clang-format off
  o0.x = vsource[0].x; o0.y = vsource[1].x; o0.z = vsource[2].x; o0.w = 0.0f;
  o1.x = vsource[0].y; o1.y = vsource[1].y; o1.z = vsource[2].y; o1.w = 0.0f;
  o2.x = vsource[0].z; o2.y = vsource[1].z; o2.z = vsource[2].z; o2.w = 0.0f;
  o3.x = vsource[0].w; o3.y = vsource[1].w; o3.z = vsource[2].w; o3.w = 1.0f;
  // clang-format on
#endif
}
#endif

#ifndef USE_NEON
ATTR_TARGET DOLPHIN_FORCE_INLINE static void LoadPos(const void* source,  //
                                                     Vector& o0, Vector& o1, Vector& o2)
{
  const Vector* vsource = static_cast<const Vector*>(source);
  o0 = vsource[0];
  o1 = vsource[1];
  o2 = vsource[2];
}
#endif

ATTR_TARGET DOLPHIN_FORCE_INLINE static Vector ApplyMatrix(Vector v, Vector m0, Vector m1,
                                                           Vector m2, Vector m3)
{
#if defined(USE_SSE)
  Vector output = _mm_mul_ps(vector_broadcast<0>(v), m0);
#ifdef USE_FMA
  output = _mm_fmadd_ps(vector_broadcast<1>(v), m1, output);
  output = _mm_fmadd_ps(vector_broadcast<2>(v), m2, output);
  output = _mm_fmadd_ps(vector_broadcast<3>(v), m3, output);
#else
  output = _mm_add_ps(output, _mm_mul_ps(vector_broadcast<1>(v), m1));
  output = _mm_add_ps(output, _mm_mul_ps(vector_broadcast<2>(v), m2));
  output = _mm_add_ps(output, _mm_mul_ps(vector_broadcast<3>(v), m3));
#endif
  return output;
#elif defined(USE_NEON)
  Vector output = vmulq_laneq_f32(m0, v, 0);
  output = vfmaq_laneq_f32(output, m1, v, 1);
  output = vfmaq_laneq_f32(output, m2, v, 2);
  output = vfmaq_laneq_f32(output, m3, v, 3);
  return output;
#else
  Vector output;
  output.x = v.x * m0.x + v.y * m1.x + v.z * m2.x + v.w * m3.x;
  output.y = v.x * m0.y + v.y * m1.y + v.z * m2.y + v.w * m3.y;
  output.z = v.x * m0.z + v.y * m1.z + v.z * m2.z + v.w * m3.z;
  output.w = v.x * m0.w + v.y * m1.w + v.z * m2.w + v.w * m3.w;
  return output;
#endif
}

#ifndef USE_NEON
ATTR_TARGET DOLPHIN_FORCE_INLINE static Vector
TransformVertexNoTranspose(Vector vertex, Vector pos0, Vector pos1, Vector pos2,  //
                           Vector proj0, Vector proj1, Vector proj2, Vector proj3)
{
#ifdef USE_SSE
  Vector mul0 = _mm_mul_ps(vertex, pos0);
  Vector mul1 = _mm_mul_ps(vertex, pos1);
  Vector mul2 = _mm_mul_ps(vertex, pos2);
  Vector mul3 = _mm_setr_ps(0.0f, 0.0f, 0.0f, 1.0f);
#ifdef USE_SSE3
  Vector output = _mm_hadd_ps(_mm_hadd_ps(mul0, mul1), _mm_hadd_ps(mul2, mul3));
#else
  Vector t0 = _mm_add_ps(_mm_unpacklo_ps(mul0, mul2), _mm_unpackhi_ps(mul0, mul2));
  Vector t1 = _mm_add_ps(_mm_unpacklo_ps(mul1, mul3), _mm_unpackhi_ps(mul1, mul3));
  Vector output = _mm_add_ps(_mm_unpacklo_ps(t0, t1), _mm_unpackhi_ps(t0, t1));
#endif
#else
  Vector output;
  output.x = vertex.x * pos0.x + vertex.y * pos0.y + vertex.z * pos0.z + vertex.w * pos0.w;
  output.y = vertex.x * pos1.x + vertex.y * pos1.y + vertex.z * pos1.z + vertex.w * pos1.w;
  output.z = vertex.x * pos2.x + vertex.y * pos2.y + vertex.z * pos2.z + vertex.w * pos2.w;
  output.w = 1.0f;
#endif
  output = ApplyMatrix(output, proj0, proj1, proj2, proj3);
  return output;
}
#endif

template <bool PositionHas3Elems>
ATTR_TARGET DOLPHIN_FORCE_INLINE static Vector
TransformVertex(Vector vertex, Vector pos0, Vector pos1, Vector pos2, Vector pos3,  //
                Vector proj0, Vector proj1, Vector proj2, Vector proj3)
{
  Vector output = pos3;  // vertex.w is always 1.0
#if defined(USE_FMA)
  output = _mm_fmadd_ps(vector_broadcast<0>(vertex), pos0, output);
  output = _mm_fmadd_ps(vector_broadcast<1>(vertex), pos1, output);
  if constexpr (PositionHas3Elems)
    output = _mm_fmadd_ps(vector_broadcast<2>(vertex), pos2, output);
#elif defined(USE_SSE)
  output = _mm_add_ps(output, _mm_mul_ps(vector_broadcast<0>(vertex), pos0));
  output = _mm_add_ps(output, _mm_mul_ps(vector_broadcast<1>(vertex), pos1));
  if constexpr (PositionHas3Elems)
    output = _mm_add_ps(output, _mm_mul_ps(vector_broadcast<2>(vertex), pos2));
#elif defined(USE_NEON)
  output = vfmaq_laneq_f32(output, pos0, vertex, 0);
  output = vfmaq_laneq_f32(output, pos1, vertex, 1);
  if constexpr (PositionHas3Elems)
    output = vfmaq_laneq_f32(output, pos2, vertex, 2);
#else
  output.x += vertex.x * pos0.x + vertex.y * pos1.x;
  output.y += vertex.x * pos0.y + vertex.y * pos1.y;
  output.z += vertex.x * pos0.z + vertex.y * pos1.z;
  if constexpr (PositionHas3Elems)
  {
    output.x += vertex.z * pos2.x;
    output.y += vertex.z * pos2.y;
    output.z += vertex.z * pos2.z;
  }
#endif
  output = ApplyMatrix(output, proj0, proj1, proj2, proj3);
  return output;
}

template <bool PositionHas3Elems, bool PerVertexPosMtx>
ATTR_TARGET DOLPHIN_FORCE_INLINE static Vector
LoadTransformVertex(const u8* data, Vector pos0, Vector pos1, Vector pos2, Vector pos3,
                    Vector proj0, Vector proj1, Vector proj2, Vector proj3)
{
  Vector vertex;
  if constexpr (PerVertexPosMtx)
  {
    // Vertex data layout always starts with posmtx data if available, then position data
    // Convenient for us, that means offsets are always fixed
    u32 idx = data[0] & 0x3f;
    data += sizeof(u32);

    const float* fdata = reinterpret_cast<const float*>(data);

#ifdef USE_NEON
    LoadTransposedPos(&xfmem.posMatrices[idx * 4], pos0, pos1, pos2, pos3);

    if constexpr (PositionHas3Elems)
    {
      vertex = vld1q_f32(fdata);
    }
    else
    {
      vertex = vcombine_f32(vld1_f32(fdata), vdup_n_f32(0.0f));
    }

    vertex = TransformVertex<PositionHas3Elems>(vertex, pos0, pos1, pos2, pos3,  //
                                                proj0, proj1, proj2, proj3);
#else
    LoadPos(&xfmem.posMatrices[idx * 4], pos0, pos1, pos2);

    if constexpr (PositionHas3Elems)
    {
#if defined(USE_SSE)
#ifdef USE_SSE41
      Vector base = _mm_set1_ps(1.0f);
      vertex = _mm_blend_ps(_mm_loadu_ps(fdata), base, 8);
#else
      Vector base = _mm_setr_ps(0.0f, 0.0f, 0.0f, 1.0f);
      Vector mask = _mm_castsi128_ps(_mm_setr_epi32(-1, -1, -1, 0));
      vertex = _mm_or_ps(_mm_and_ps(_mm_loadu_ps(fdata), mask), base);
#endif
#else
      vertex.x = fdata[0];
      vertex.y = fdata[1];
      vertex.z = fdata[2];
      vertex.w = 1.0f;
#endif
    }
    else
    {
#if defined(USE_SSE)
      Vector base = _mm_setr_ps(0.0f, 0.0f, 0.0f, 1.0f);
      vertex = _mm_loadl_pi(base, reinterpret_cast<const __m64*>(fdata));
#else
      vertex.x = fdata[0];
      vertex.y = fdata[1];
      vertex.z = 0.0f;
      vertex.w = 1.0f;
#endif
    }

    vertex = TransformVertexNoTranspose(vertex, pos0, pos1, pos2, proj0, proj1, proj2, proj3);
#endif
  }
  else
  {
    const float* fdata = reinterpret_cast<const float*>(data);
    if constexpr (PositionHas3Elems)
    {
#if defined(USE_SSE)
      vertex = _mm_loadu_ps(fdata);
#elif defined(USE_NEON)
      vertex = vld1q_f32(fdata);
#else
      vertex.x = fdata[0];
      vertex.y = fdata[1];
      vertex.z = fdata[2];
      vertex.w = 1.0f;
#endif
    }
    else
    {
#if defined(USE_SSE)
      vertex = _mm_loadl_pi(_mm_setzero_ps(), reinterpret_cast<const __m64*>(fdata));
#elif defined(USE_NEON)
      vertex = vcombine_f32(vld1_f32(fdata), vdup_n_f32(0.0f));
#else
      vertex.x = fdata[0];
      vertex.y = fdata[1];
      vertex.z = 0.0f;
      vertex.w = 1.0f;
#endif
    }

    vertex = TransformVertex<PositionHas3Elems>(vertex, pos0, pos1, pos2, pos3,  //
                                                proj0, proj1, proj2, proj3);
  }

  return vertex;
}

template <bool PositionHas3Elems, bool PerVertexPosMtx>
ATTR_TARGET static void TransformVertices(void* output, const void* vertices, u32 stride, int count)
{
  const VertexShaderManager& vsmanager = Core::System::GetInstance().GetVertexShaderManager();
  const u8* cvertices = static_cast<const u8*>(vertices);
  Vector* voutput = static_cast<Vector*>(output);
  u32 idx = g_main_cp_state.matrix_index_a.PosNormalMtxIdx & 0x3f;
#ifdef USE_AVX
  __m256 proj0, proj1, proj2, proj3;
  __m256 pos0, pos1, pos2, pos3;
  LoadTransposedYMM(vsmanager.constants.projection.data(), proj0, proj1, proj2, proj3);
  LoadTransposedPosYMM(&xfmem.posMatrices[idx * 4], pos0, pos1, pos2, pos3);
  for (int i = 1; i < count; i += 2)
  {
    const u8* v0data = cvertices;
    const u8* v1data = cvertices + stride;
    __m256 v01 = LoadTransform2Vertices<PositionHas3Elems, PerVertexPosMtx>(
        v0data, v1data, pos0, pos1, pos2, pos3, proj0, proj1, proj2, proj3);
    _mm256_store_ps(reinterpret_cast<float*>(voutput), v01);
    cvertices += stride * 2;
    voutput += 2;
  }
  if (count & 1)
  {
    *voutput = LoadTransformVertex<PositionHas3Elems, PerVertexPosMtx>(
        cvertices,                                                     //
        _mm256_castps256_ps128(pos0), _mm256_castps256_ps128(pos1),    //
        _mm256_castps256_ps128(pos2), _mm256_castps256_ps128(pos3),    //
        _mm256_castps256_ps128(proj0), _mm256_castps256_ps128(proj1),  //
        _mm256_castps256_ps128(proj2), _mm256_castps256_ps128(proj3));
  }
#else
  Vector proj0, proj1, proj2, proj3;
  Vector pos0, pos1, pos2, pos3;
  LoadTransposed(vsmanager.constants.projection.data(), proj0, proj1, proj2, proj3);
  LoadTransposedPos(&xfmem.posMatrices[idx * 4], pos0, pos1, pos2, pos3);
  for (int i = 0; i < count; i++)
  {
    *voutput = LoadTransformVertex<PositionHas3Elems, PerVertexPosMtx>(
        cvertices, pos0, pos1, pos2, pos3, proj0, proj1, proj2, proj3);
    cvertices += stride;
    voutput += 1;
  }
#endif
}

template <CullMode Mode>
ATTR_TARGET DOLPHIN_FORCE_INLINE static bool CullTriangle(const CPUCull::TransformedVertex& a,
                                                          const CPUCull::TransformedVertex& b,
                                                          const CPUCull::TransformedVertex& c)
{
  if (Mode == CullMode::All)
    return true;

  Vector va = reinterpret_cast<const Vector&>(a);
  Vector vb = reinterpret_cast<const Vector&>(b);
  Vector vc = reinterpret_cast<const Vector&>(c);

  // See videosoftware Clipper.cpp

#if defined(USE_SSE)
  Vector wxzya = _mm_shuffle_ps(va, va, _MM_SHUFFLE(1, 2, 0, 3));
  Vector wxzyc = _mm_shuffle_ps(vc, vc, _MM_SHUFFLE(1, 2, 0, 3));
  Vector ywzxb = _mm_shuffle_ps(vb, vb, _MM_SHUFFLE(0, 2, 3, 1));
  Vector part0 = _mm_mul_ps(va, wxzyc);
  Vector part1 = _mm_mul_ps(vc, wxzya);
  Vector part2 = _mm_mul_ps(_mm_sub_ps(part0, part1), ywzxb);
#ifdef USE_SSE3
  Vector part3 = _mm_movehdup_ps(part2);
#else
  Vector part3 = vector_broadcast<1>(part2);
#endif
  Vector part4 = vector_broadcast<3>(part2);
  Vector part5 = _mm_add_ss(_mm_add_ss(part2, part3), part4);
  float normal_z_dir;
  _mm_store_ss(&normal_z_dir, part5);
#elif defined(USE_NEON)
  Vector zero = vdupq_n_f32(0.0f);
  Vector wx0ya = vextq_f32(va, vzip1q_f32(va, zero), 3);
  Vector wx0yc = vextq_f32(vc, vzip1q_f32(vc, zero), 3);
  Vector ywxxb = vuzp2q_f32(vb, vdupq_laneq_f32(vb, 0));
  Vector part0 = vmulq_f32(va, wx0yc);
  Vector part1 = vmulq_f32(vc, wx0ya);
  Vector part2 = vmulq_f32(vsubq_f32(part0, part1), ywxxb);
  float normal_z_dir = vaddvq_f32(part2);
#else
  float normal_z_dir = (c.w * a.x - a.w * c.x) * b.y +  //
                       (c.x * a.y - a.x * c.y) * b.w +  //
                       (c.y * a.w - a.y * c.w) * b.x;
#endif
  bool cull = false;
  switch (Mode)
  {
  case CullMode::None:
    cull = normal_z_dir == 0;
    break;
  case CullMode::Front:
    cull = normal_z_dir <= 0;
    break;
  case CullMode::Back:
    cull = normal_z_dir >= 0;
    break;
  case CullMode::All:
    cull = true;
    break;
  }
  if (cull)
    return true;

#if defined(USE_SSE)
  Vector xyab = _mm_unpacklo_ps(va, vb);
  Vector zwab = _mm_unpackhi_ps(va, vb);
  Vector allx = _mm_shuffle_ps(xyab, vc, _MM_SHUFFLE(0, 0, 1, 0));
  Vector ally = _mm_shuffle_ps(xyab, vc, _MM_SHUFFLE(1, 1, 3, 2));
  Vector allpw = _mm_shuffle_ps(zwab, vc, _MM_SHUFFLE(3, 3, 3, 2));
  Vector allnw = _mm_xor_ps(allpw, _mm_set1_ps(-0.0f));
  __m128i x_gt_pw = _mm_castps_si128(_mm_cmple_ps(allpw, allx));
  __m128i y_gt_pw = _mm_castps_si128(_mm_cmple_ps(allpw, ally));
  __m128i x_lt_nw = _mm_castps_si128(_mm_cmplt_ps(allx, allnw));
  __m128i y_lt_nw = _mm_castps_si128(_mm_cmplt_ps(ally, allnw));
  __m128i any_out_of_bounds = _mm_packs_epi16(_mm_packs_epi32(x_lt_nw, y_lt_nw),  //
                                              _mm_packs_epi32(x_gt_pw, y_gt_pw));
  cull |= 0 != _mm_movemask_epi8(_mm_cmpeq_epi32(_mm_set1_epi32(~0), any_out_of_bounds));
#elif defined(USE_NEON)
  float64x2_t xyab = vreinterpretq_f64_f32(vzip1q_f32(va, vb));
  float64x2_t xycc = vreinterpretq_f64_f32(vzip1q_f32(vc, vc));
  float32x4_t allx = vreinterpretq_f32_f64(vzip1q_f64(xyab, xycc));
  float32x4_t ally = vreinterpretq_f32_f64(vzip2q_f64(xyab, xycc));
  float32x4_t allpw = vextq_f32(vzip2q_f32(va, vb), vdupq_laneq_f32(vc, 3), 2);
  float32x4_t allnw = vnegq_f32(allpw);
  uint16x8_t x_gt_pw = vreinterpretq_u16_u32(vcgtq_f32(allx, allpw));
  uint16x8_t y_gt_pw = vreinterpretq_u16_u32(vcgtq_f32(ally, allpw));
  uint16x8_t x_lt_nw = vreinterpretq_u16_u32(vcltq_f32(allx, allnw));
  uint16x8_t y_lt_nw = vreinterpretq_u16_u32(vcltq_f32(ally, allnw));
  uint8x16_t lt_nw = vreinterpretq_u8_u16(vuzp1q_u16(x_lt_nw, y_lt_nw));
  uint8x16_t gt_pw = vreinterpretq_u8_u16(vuzp1q_u16(x_gt_pw, y_gt_pw));
  uint32x4_t any_out_of_bounds = vreinterpretq_u32_u8(vuzp1q_u8(lt_nw, gt_pw));
  cull |= 0xFFFFFFFF == vmaxvq_u32(any_out_of_bounds);
#else
  cull |= a.x < -a.w && b.x < -b.w && c.x < -c.w;
  cull |= a.y < -a.w && b.y < -b.w && c.y < -c.w;
  cull |= a.x > a.w && b.x > b.w && c.x > c.w;
  cull |= a.y > a.w && b.y > b.w && c.y > c.w;
#endif

  return cull;
}

template <OpcodeDecoder::Primitive Primitive, CullMode Mode>
ATTR_TARGET static bool AreAllVerticesCulled(const CPUCull::TransformedVertex* transformed,
                                             int count)
{
  switch (Primitive)
  {
  case OpcodeDecoder::Primitive::GX_DRAW_QUADS:
  case OpcodeDecoder::Primitive::GX_DRAW_QUADS_2:
  {
    int i = 3;
    for (; i < count; i += 4)
    {
      if (!CullTriangle<Mode>(transformed[i - 3], transformed[i - 2], transformed[i - 1]))
        return false;
      if (!CullTriangle<Mode>(transformed[i - 3], transformed[i - 1], transformed[i - 0]))
        return false;
    }
    // three vertices remaining, so render a triangle
    if (i == count)
    {
      if (!CullTriangle<Mode>(transformed[i - 3], transformed[i - 2], transformed[i - 1]))
        return false;
    }
    break;
  }
  case OpcodeDecoder::Primitive::GX_DRAW_TRIANGLES:
    for (int i = 2; i < count; i += 3)
    {
      if (!CullTriangle<Mode>(transformed[i - 2], transformed[i - 1], transformed[i - 0]))
        return false;
    }
    break;
  case OpcodeDecoder::Primitive::GX_DRAW_TRIANGLE_STRIP:
  {
    bool wind = false;
    for (int i = 2; i < count; ++i)
    {
      if (!CullTriangle<Mode>(transformed[i - 2], transformed[i - !wind], transformed[i - wind]))
        return false;
      wind = !wind;
    }
    break;
  }
  case OpcodeDecoder::Primitive::GX_DRAW_TRIANGLE_FAN:
    for (int i = 2; i < count; ++i)
    {
      if (!CullTriangle<Mode>(transformed[0], transformed[i - 1], transformed[i]))
        return false;
    }
    break;
  }

  return true;
}

}  // namespace VECTOR_NAMESPACE

#undef ATTR_TARGET
#undef VECTOR_NAMESPACE
