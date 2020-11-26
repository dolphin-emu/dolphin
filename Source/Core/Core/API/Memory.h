// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "Core/HW/Memmap.h"

namespace API::Memory
{

// memchecks

void AddMemcheck(u32 addr);
void RemoveMemcheck(u32 addr);

// memory reading: just directly forward to the MMU

inline u8 Read_U8(u32 addr)
{
  return PowerPC::HostRead_U8(addr);
}

inline u16 Read_U16(u32 addr)
{
  return PowerPC::HostRead_U16(addr);
}

inline u32 Read_U32(u32 addr)
{
  return PowerPC::HostRead_U32(addr);
}

inline u64 Read_U64(u32 addr)
{
  return PowerPC::HostRead_U64(addr);
}

inline s8 Read_S8(u32 addr)
{
  return PowerPC::HostRead_S8(addr);
}

inline s16 Read_S16(u32 addr)
{
  return PowerPC::HostRead_S16(addr);
}

inline s32 Read_S32(u32 addr)
{
  return PowerPC::HostRead_S32(addr);
}

inline s64 Read_S64(u32 addr)
{
  return PowerPC::HostRead_S64(addr);
}

inline float Read_F32(u32 addr)
{
  return PowerPC::HostRead_F32(addr);
}

inline double Read_F64(u32 addr)
{
  return PowerPC::HostRead_F64(addr);
}

// memory writing: arguments of write functions are swapped (address first) to be consistent with other scripting APIs

inline void Write_U8(u32 addr, u8 val)
{
  PowerPC::HostWrite_U8(val, addr);
}

inline void Write_U16(u32 addr, u16 val)
{
  PowerPC::HostWrite_U16(val, addr);
}

inline void Write_U32(u32 addr, u32 val)
{
  PowerPC::HostWrite_U32(val, addr);
}

inline void Write_U64(u32 addr, u64 val)
{
  PowerPC::HostWrite_U64(val, addr);
}

inline void Write_S8(u32 addr, s8 val)
{
  PowerPC::HostWrite_S8(val, addr);
}

inline void Write_S16(u32 addr, s16 val)
{
  PowerPC::HostWrite_S16(val, addr);
}

inline void Write_S32(u32 addr, s32 val)
{
  PowerPC::HostWrite_S32(val, addr);
}

inline void Write_S64(u32 addr, s64 val)
{
  PowerPC::HostWrite_S64(val, addr);
}

inline void Write_F32(u32 addr, float val)
{
  PowerPC::HostWrite_F32(val, addr);
}

inline void Write_F64(u32 addr, double val)
{
  PowerPC::HostWrite_F64(val, addr);
}

}  // namespace API::Memory
