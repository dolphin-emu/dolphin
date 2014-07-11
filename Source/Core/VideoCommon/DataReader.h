// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/VertexManagerBase.h"

extern u8* g_pVideoData;

#if _M_SSE >= 0x301 && !(defined __GNUC__ && !defined __SSSE3__)
#include <tmmintrin.h>
#endif

__forceinline void DataSkip(u32 skip)
{
	g_pVideoData += skip;
}

// probably unnecessary
template <int count>
__forceinline void DataSkip()
{
	g_pVideoData += count;
}

template <typename T>
__forceinline T DataPeek(int _uOffset)
{
	auto const result = Common::FromBigEndian(*reinterpret_cast<T*>(g_pVideoData + _uOffset));
	return result;
}

// TODO: kill these
__forceinline u8 DataPeek8(int _uOffset)
{
	return DataPeek<u8>(_uOffset);
}

__forceinline u16 DataPeek16(int _uOffset)
{
	return DataPeek<u16>(_uOffset);
}

__forceinline u32 DataPeek32(int _uOffset)
{
	return DataPeek<u32>(_uOffset);
}

template <typename T>
__forceinline T DataRead()
{
	auto const result = DataPeek<T>(0);
	DataSkip<sizeof(T)>();
	return result;
}

class DataReader
{
public:
	inline DataReader() : buffer(g_pVideoData), offset(0) {}
	inline ~DataReader() { g_pVideoData += offset; }
	template <typename T> inline T Read()
	{
		const T result = Common::FromBigEndian(*(T*)(buffer + offset));
		offset += sizeof(T);
		return result;
	}
private:
	u8 *buffer;
	int offset;
};

// TODO: kill these
__forceinline u8 DataReadU8()
{
	return DataRead<u8>();
}

__forceinline s8 DataReadS8()
{
	return DataRead<s8>();
}

__forceinline u16 DataReadU16()
{
	return DataRead<u16>();
}

__forceinline u32 DataReadU32()
{
	return DataRead<u32>();
}

#if _M_SSE >= 0x301
const __m128i bs_mask = _mm_set_epi32(0x0C0D0E0FL, 0x08090A0BL, 0x04050607L, 0x00010203L);

template<unsigned int N>
void DataReadU32xN_SSSE3(u32 *bufx16)
{
	memcpy(bufx16, g_pVideoData, sizeof(u32) * N);
	__m128i* buf = (__m128i *)bufx16;
	if (N>12) { _mm_store_si128(buf, _mm_shuffle_epi8(_mm_load_si128(buf), bs_mask)); buf++; }
	if (N>8)  { _mm_store_si128(buf, _mm_shuffle_epi8(_mm_load_si128(buf), bs_mask)); buf++; }
	if (N>4)  { _mm_store_si128(buf, _mm_shuffle_epi8(_mm_load_si128(buf), bs_mask)); buf++; }
	_mm_store_si128(buf, _mm_shuffle_epi8(_mm_load_si128(buf), bs_mask));
	g_pVideoData += (sizeof(u32) * N);
}

#endif

template<unsigned int N>
void DataReadU32xN(u32 *bufx16)
{
	memcpy(bufx16, g_pVideoData, sizeof(u32) * N);
	if (N >= 1) bufx16[0] = Common::swap32(bufx16[0]);
	if (N >= 2) bufx16[1] = Common::swap32(bufx16[1]);
	if (N >= 3) bufx16[2] = Common::swap32(bufx16[2]);
	if (N >= 4) bufx16[3] = Common::swap32(bufx16[3]);
	if (N >= 5) bufx16[4] = Common::swap32(bufx16[4]);
	if (N >= 6) bufx16[5] = Common::swap32(bufx16[5]);
	if (N >= 7) bufx16[6] = Common::swap32(bufx16[6]);
	if (N >= 8) bufx16[7] = Common::swap32(bufx16[7]);
	if (N >= 9) bufx16[8] = Common::swap32(bufx16[8]);
	if (N >= 10) bufx16[9] = Common::swap32(bufx16[9]);
	if (N >= 11) bufx16[10] = Common::swap32(bufx16[10]);
	if (N >= 12) bufx16[11] = Common::swap32(bufx16[11]);
	if (N >= 13) bufx16[12] = Common::swap32(bufx16[12]);
	if (N >= 14) bufx16[13] = Common::swap32(bufx16[13]);
	if (N >= 15) bufx16[14] = Common::swap32(bufx16[14]);
	if (N >= 16) bufx16[15] = Common::swap32(bufx16[15]);
	g_pVideoData += (sizeof(u32) * N);
}

__forceinline u32 DataReadU32Unswapped()
{
	u32 tmp = *(u32*)g_pVideoData;
	g_pVideoData += 4;
	return tmp;
}

__forceinline u8* DataGetPosition()
{
	return g_pVideoData;
}

template <typename T>
__forceinline void DataWrite(T data)
{
	*(T*)VertexManager::s_pCurBufferPointer = data;
	VertexManager::s_pCurBufferPointer += sizeof(T);
}

class DataWriter
{
public:
	inline DataWriter() : buffer(VertexManager::s_pCurBufferPointer), offset(0) {}
	inline ~DataWriter() { VertexManager::s_pCurBufferPointer += offset; }
	template <typename T> inline void Write(T data)
	{
		*(T*)(buffer+offset) = data;
		offset += sizeof(T);
	}
private:
	u8 *buffer;
	int offset;
};
