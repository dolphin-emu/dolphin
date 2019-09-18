// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

namespace IForm
{
enum
{
  UNKNOWN,
#define ILLEGAL(name)
#define INST(name, type, flags, cycles) name,
#define TABLE(name, bits, shift, start, end)
#include "Core/PowerPC/Instructions.in.cpp"
#undef ILLEGAL
#undef INST
#undef TABLE
  NUM_IFORMS
};
using IForm = u8;
}  // namespace IForm
