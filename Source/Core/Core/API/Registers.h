// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "Core/PowerPC/PowerPC.h"

namespace API::Registers
{

// Register reading

inline u32 Read_GPR(u32 index)
{
  return GPR(index);
}

inline double Read_FPR(u32 index)
{
  return rPS(index).PS0AsDouble();
}

// register writing

inline void Write_GPR(u32 index, u32 value)
{
  GPR(index) = value;
}

inline void Write_FPR(u32 index, double value)
{
  rPS(index).SetPS0(value);
}

}  // namespace API::Registers
#pragma once
