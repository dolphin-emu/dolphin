// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/Common.h"
#include "VideoCommon/VertexManagerBase.h"

extern u8* g_video_buffer_read_ptr;

class DataReader
{
public:
	__forceinline DataReader()
	: buffer(nullptr), end(nullptr) {}

	__forceinline DataReader(u8* src, u8* _end)
	: buffer(src), end(_end) {}

	__forceinline void WritePointer(u8** src)
	{
		*src = buffer;
	}

	__forceinline u8* operator=(u8* src)
	{
		buffer = src;
		return src;
	}

	__forceinline size_t size()
	{
		return end - buffer;
	}

	template <typename T, bool swapped = true> __forceinline T Peek(int offset = 0)
	{
		T data = *(T*)(buffer + offset);
		if (swapped)
			data = Common::FromBigEndian(data);
		return data;
	}

	template <typename T, bool swapped = true> __forceinline T Read()
	{
		const T result = Peek<T, swapped>();
		buffer += sizeof(T);
		return result;
	}

	template <typename T, bool swapped = false> __forceinline void Write(T data)
	{
		if (swapped)
			data = Common::FromBigEndian(data);
		*(T*)(buffer) = data;
		buffer += sizeof(T);
	}

	template <typename T = u8> __forceinline void Skip(size_t data = 1)
	{
		buffer += sizeof(T) * data;
	}

private:
	u8* __restrict buffer;
	u8* end;
};

__forceinline void DataSkip(u32 skip)
{
	g_video_buffer_read_ptr += skip;
}

// probably unnecessary
template <int count>
__forceinline void DataSkip()
{
	g_video_buffer_read_ptr += count;
}

template <typename T>
__forceinline T DataPeek(int _uOffset, u8** bufp = &g_video_buffer_read_ptr)
{
	auto const result = Common::FromBigEndian(*reinterpret_cast<T*>(*bufp + _uOffset));
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
__forceinline T DataRead(u8** bufp = &g_video_buffer_read_ptr)
{
	auto const result = DataPeek<T>(0, bufp);
	*bufp += sizeof(T);
	return result;
}

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

__forceinline u32 DataReadU32Unswapped()
{
	u32 tmp = *(u32*)g_video_buffer_read_ptr;
	g_video_buffer_read_ptr += 4;
	return tmp;
}

__forceinline u8* DataGetPosition()
{
	return g_video_buffer_read_ptr;
}

template <typename T>
__forceinline void DataWrite(T data)
{
	*(T*)VertexManager::s_pCurBufferPointer = data;
	VertexManager::s_pCurBufferPointer += sizeof(T);
}
