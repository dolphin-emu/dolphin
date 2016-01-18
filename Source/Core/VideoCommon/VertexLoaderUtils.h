// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstring>

#include "Common/Common.h"
#include "Common/CommonTypes.h"

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
__forceinline T DataPeek(int _uOffset, u8* bufp = g_video_buffer_read_ptr)
{
	T result;
	std::memcpy(&result, &bufp[_uOffset], sizeof(T));
	return Common::FromBigEndian(result);
}

template <typename T>
__forceinline T DataRead(u8** bufp = &g_video_buffer_read_ptr)
{
	auto const result = DataPeek<T>(0, *bufp);
	*bufp += sizeof(T);
	return result;
}

__forceinline u32 DataReadU32Unswapped()
{
	u32 result;
	std::memcpy(&result, g_video_buffer_read_ptr, sizeof(u32));
	g_video_buffer_read_ptr += sizeof(u32);
	return result;
}

__forceinline u8* DataGetPosition()
{
	return g_video_buffer_read_ptr;
}

template <typename T>
__forceinline void DataWrite(T data)
{
	std::memcpy(g_vertex_manager_write_ptr, &data, sizeof(T));
	g_vertex_manager_write_ptr += sizeof(T);
}
