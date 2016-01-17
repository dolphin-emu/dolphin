// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstring>
#include "Common/Common.h"
#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"

class DataReader
{
public:
	__forceinline DataReader()
	: buffer(nullptr), end(nullptr) {}

	__forceinline DataReader(u8* src, u8* _end)
	: buffer(src), end(_end) {}

	__forceinline u8* GetPointer()
	{
		return buffer;
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
		T data;
		std::memcpy(&data, &buffer[offset], sizeof(T));

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

		std::memcpy(buffer, &data, sizeof(T));
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
