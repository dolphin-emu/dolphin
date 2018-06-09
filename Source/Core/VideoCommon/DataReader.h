// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstring>

#include "Common/CommonTypes.h"
#include "Common/Compiler.h"
#include "Common/Swap.h"

class DataReader
{
public:
  DataReader() = default;
  DataReader(u8* src, u8* end_) : buffer(src), end(end_) {}
  u8* GetPointer() { return buffer; }
  const u8* GetPointer() const { return buffer; }
  DataReader& operator=(u8* src)
  {
    buffer = src;
    return *this;
  }

  size_t size() const { return end - buffer; }
  template <typename T, bool swapped = true>
  DOLPHIN_FORCE_INLINE T Peek(int offset = 0) const
  {
    T data;
    std::memcpy(&data, &buffer[offset], sizeof(T));

    if (swapped)
      data = Common::FromBigEndian(data);

    return data;
  }

  template <typename T, bool swapped = true>
  DOLPHIN_FORCE_INLINE T Read()
  {
    const T result = Peek<T, swapped>();
    buffer += sizeof(T);
    return result;
  }

  template <typename T, bool swapped = false>
  DOLPHIN_FORCE_INLINE void Write(T data)
  {
    if (swapped)
      data = Common::FromBigEndian(data);

    std::memcpy(buffer, &data, sizeof(T));
    buffer += sizeof(T);
  }

  template <typename T = u8>
  void Skip(size_t data = 1)
  {
    buffer += sizeof(T) * data;
  }

private:
  u8* __restrict buffer = nullptr;
  u8* end = nullptr;
};
