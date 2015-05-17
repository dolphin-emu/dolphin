// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/Common.h"
#include "VideoCommon/VertexManagerBase.h"

extern u8* g_video_buffer_read_ptr;
extern u8* g_vertex_manager_write_ptr;


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
	*(T*)g_vertex_manager_write_ptr = data;
	g_vertex_manager_write_ptr += sizeof(T);
}
