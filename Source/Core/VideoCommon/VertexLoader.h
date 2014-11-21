// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

// Top vertex loaders
// Metroid Prime: P I16-flt N I16-s16 T0 I16-u16 T1 i16-flt

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>

#include "Common/CommonTypes.h"
#include "Common/x64Emitter.h"

#include "VideoCommon/CPMemory.h"
#include "VideoCommon/DataReader.h"
#include "VideoCommon/NativeVertexFormat.h"

#if _M_SSE >= 0x401
#include <smmintrin.h>
#include <emmintrin.h>
#elif _M_SSE >= 0x301 && !(defined __GNUC__ && !defined __SSSE3__)
#include <tmmintrin.h>
#endif

#ifdef _M_X86
#define USE_VERTEX_LOADER_JIT
#endif

// They are used for the communication with the loader functions
extern int tcIndex;
extern int colIndex;
extern int colElements[2];
GC_ALIGNED128(extern float posScale[4]);
GC_ALIGNED64(extern float tcScale[8][2]);

class VertexLoaderUID
{
	u32 vid[5];
	size_t hash;
public:
	VertexLoaderUID()
	{
	}

	VertexLoaderUID(const TVtxDesc& vtx_desc, const VAT& vat)
	{
		vid[0] = vtx_desc.Hex & 0xFFFFFFFF;
		vid[1] = vtx_desc.Hex >> 32;
		vid[2] = vat.g0.Hex & ~VAT_0_FRACBITS;
		vid[3] = vat.g1.Hex & ~VAT_1_FRACBITS;
		vid[4] = vat.g2.Hex & ~VAT_2_FRACBITS;
		hash = CalculateHash();
	}

	bool operator < (const VertexLoaderUID &other) const
	{
		// This is complex because of speed.
		if (vid[0] < other.vid[0])
			return true;
		else if (vid[0] > other.vid[0])
			return false;

		for (int i = 1; i < 5; ++i)
		{
			if (vid[i] < other.vid[i])
				return true;
			else if (vid[i] > other.vid[i])
				return false;
		}

		return false;
	}

	bool operator == (const VertexLoaderUID& rh) const
	{
		return hash == rh.hash && std::equal(vid, vid + sizeof(vid) / sizeof(vid[0]), rh.vid);
	}

	size_t GetHash() const
	{
		return hash;
	}

private:

	size_t CalculateHash()
	{
		size_t h = -1;

		for (auto word : vid)
		{
			h = h * 137 + word;
		}

		return h;
	}
};

// ARMTODO: This should be done in a better way
#ifndef _M_GENERIC
class VertexLoader : public Gen::X64CodeBlock
#else
class VertexLoader
#endif
{
public:
	VertexLoader(const TVtxDesc &vtx_desc, const VAT &vtx_attr);
	~VertexLoader();

	int GetVertexSize() const {return m_VertexSize;}
	u32 GetNativeComponents() const { return m_native_components; }
	const PortableVertexDeclaration& GetNativeVertexDeclaration() const
		{ return m_native_vtx_decl; }

	void SetupRunVertices(const VAT& vat, int primitive, int const count);
	void RunVertices(const VAT& vat, int primitive, int count);

	// For debugging / profiling
	void AppendToString(std::string *dest) const;
	int GetNumLoadedVerts() const { return m_numLoadedVertices; }

	NativeVertexFormat* GetNativeVertexFormat();
	static void ClearNativeVertexFormatCache() { s_native_vertex_map.clear(); }

private:
	int m_VertexSize;      // number of bytes of a raw GC vertex. Computed by CompileVertexTranslator.

	// GC vertex format
	TVtxAttr m_VtxAttr;  // VAT decoded into easy format
	TVtxDesc m_VtxDesc;  // Not really used currently - or well it is, but could be easily avoided.

	// PC vertex format
	u32 m_native_components;
	PortableVertexDeclaration m_native_vtx_decl;

#ifndef USE_VERTEX_LOADER_JIT
	// Pipeline.
	TPipelineFunction m_PipelineStages[64];  // TODO - figure out real max. it's lower.
	int m_numPipelineStages;
#endif

	const u8 *m_compiledCode;

	int m_numLoadedVertices;

	NativeVertexFormat* m_native_vertex_format;
	static std::unordered_map<PortableVertexDeclaration, std::unique_ptr<NativeVertexFormat>> s_native_vertex_map;

	void SetVAT(const VAT& vat);

	void CompileVertexTranslator();
	void ConvertVertices(int count);

	void WriteCall(TPipelineFunction);

#ifndef _M_GENERIC
	void WriteGetVariable(int bits, Gen::OpArg dest, void *address);
	void WriteSetVariable(int bits, void *address, Gen::OpArg dest);
#endif
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
			_mm_storeu_si128((__m128i*)VertexManager::s_pCurBufferPointer, coords);
		else
			_mm_storel_epi64((__m128i*)VertexManager::s_pCurBufferPointer, coords);
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
			_mm_storeu_ps((float*)VertexManager::s_pCurBufferPointer, out);
		else
			_mm_storel_pi((__m64*)VertexManager::s_pCurBufferPointer, out);
	}

	VertexManager::s_pCurBufferPointer += sizeof(float) * (2 + threeOut);
}
#endif