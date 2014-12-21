// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

// Top vertex loaders
// Metroid Prime: P I16-flt N I16-s16 T0 I16-u16 T1 i16-flt

#include <algorithm>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/x64Emitter.h"

#include "VideoCommon/CPMemory.h"
#include "VideoCommon/DataReader.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/VertexLoaderBase.h"
#include "VideoCommon/VertexLoaderUtils.h"

#if _M_SSE >= 0x401
#include <smmintrin.h>
#include <emmintrin.h>
#elif _M_SSE >= 0x301 && !(defined __GNUC__ && !defined __SSSE3__)
#include <tmmintrin.h>
#endif

#ifdef _M_X86
#define USE_VERTEX_LOADER_JIT
#endif

#ifdef WIN32
#define LOADERDECL __cdecl
#else
#define LOADERDECL
#endif

class VertexLoader;
typedef void (LOADERDECL *TPipelineFunction)(VertexLoader* loader);

// ARMTODO: This should be done in a better way
#ifndef _M_GENERIC
class VertexLoader : public Gen::X64CodeBlock, public VertexLoaderBase
#else
class VertexLoader : public VertexLoaderBase
#endif
{
public:
	// This class need a 16 byte alignment. As this is broken on
	// MSVC right now (Dec 2014), we use custom allocation.
	void* operator new (size_t size);
	void operator delete (void *p);

	VertexLoader(const TVtxDesc &vtx_desc, const VAT &vtx_attr);
	~VertexLoader();

	int RunVertices(int primitive, int count, DataReader src, DataReader dst) override;
	std::string GetName() const override { return "OldLoader"; }
	bool IsInitialized() override { return true; } // This vertex loader supports all formats

	// They are used for the communication with the loader functions
	// Duplicated (4x and 2x respectively) and used in SSE code in the vertex loader JIT
	GC_ALIGNED128(float m_posScale[4]);
	GC_ALIGNED64(float m_tcScale[8][2]);
	int m_tcIndex;
	int m_colIndex;
	int m_colElements[2];

	// Matrix components are first in GC format but later in PC format - we need to store it temporarily
	// when decoding each vertex.
	u8 m_curposmtx;
	u8 m_curtexmtx[8];
	int m_texmtxwrite;
	int m_texmtxread;
	bool m_vertexSkip;
	int m_skippedVertices;

private:
#ifndef USE_VERTEX_LOADER_JIT
	// Pipeline.
	TPipelineFunction m_PipelineStages[64];  // TODO - figure out real max. it's lower.
	int m_numPipelineStages;
#endif

	void CompileVertexTranslator();

	void WriteCall(TPipelineFunction);

#ifndef _M_GENERIC
	void WriteGetVariable(int bits, Gen::OpArg dest, void *address);
	void WriteSetVariable(int bits, void *address, Gen::OpArg dest);
#endif

	const u8 *m_compiledCode;
};

#if _M_SSE >= 0x301
static const __m128i kMaskSwap32_3 = _mm_set_epi32(0xFFFFFFFFL, 0x08090A0BL, 0x04050607L, 0x00010203L);
static const __m128i kMaskSwap32_2 = _mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0x04050607L, 0x00010203L);
static const __m128i kMaskSwap16to32l_3 = _mm_set_epi32(0xFFFFFFFFL, 0xFFFF0405L, 0xFFFF0203L, 0xFFFF0001L);
static const __m128i kMaskSwap16to32l_2 = _mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0xFFFF0203L, 0xFFFF0001L);
static const __m128i kMaskSwap16to32h_3 = _mm_set_epi32(0xFFFFFFFFL, 0x0405FFFFL, 0x0203FFFFL, 0x0001FFFFL);
static const __m128i kMaskSwap16to32h_2 = _mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0x0203FFFFL, 0x0001FFFFL);
static const __m128i kMask8to32l_3 = _mm_set_epi32(0xFFFFFFFFL, 0xFFFFFF02L, 0xFFFFFF01L, 0xFFFFFF00L);
static const __m128i kMask8to32l_2 = _mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0xFFFFFF01L, 0xFFFFFF00L);
static const __m128i kMask8to32h_3 = _mm_set_epi32(0xFFFFFFFFL, 0x02FFFFFFL, 0x01FFFFFFL, 0x00FFFFFFL);
static const __m128i kMask8to32h_2 = _mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0x01FFFFFFL, 0x00FFFFFFL);

template <typename T, bool threeIn, bool threeOut>
__forceinline void Vertex_Read_SSSE3(const T* pData, __m128 scale)
{
	__m128i coords, mask;

	int loadBytes = sizeof(T) * (2 + threeIn);
	if (loadBytes > 8)
		coords = _mm_loadu_si128((__m128i*)pData);
	else if (loadBytes > 4)
		coords = _mm_loadl_epi64((__m128i*)pData);
	else
		coords = _mm_cvtsi32_si128(*(u32*)pData);

	// Float case (no scaling)
	if (sizeof(T) == 4)
	{
		coords = _mm_shuffle_epi8(coords, threeIn ? kMaskSwap32_3 : kMaskSwap32_2);
		if (threeOut)
			_mm_storeu_si128((__m128i*)g_vertex_manager_write_ptr, coords);
		else
			_mm_storel_epi64((__m128i*)g_vertex_manager_write_ptr, coords);
	}
	else
	{
		// Byte swap, unpack, and move to high bytes for sign extend.
		if (std::is_unsigned<T>::value)
			mask = sizeof(T) == 2 ? (threeIn ? kMaskSwap16to32l_3 : kMaskSwap16to32l_2) : (threeIn ? kMask8to32l_3 : kMask8to32l_2);
		else
			mask = sizeof(T) == 2 ? (threeIn ? kMaskSwap16to32h_3 : kMaskSwap16to32h_2) : (threeIn ? kMask8to32h_3 : kMask8to32h_2);
		coords = _mm_shuffle_epi8(coords, mask);

		// Sign extend
		if (std::is_signed<T>::value)
			coords = _mm_srai_epi32(coords, 32 - sizeof(T) * 8);

		__m128 out = _mm_mul_ps(_mm_cvtepi32_ps(coords), scale);
		if (threeOut)
			_mm_storeu_ps((float*)g_vertex_manager_write_ptr, out);
		else
			_mm_storel_pi((__m64*)g_vertex_manager_write_ptr, out);
	}

	g_vertex_manager_write_ptr += sizeof(float) * (2 + threeOut);
}
#endif
