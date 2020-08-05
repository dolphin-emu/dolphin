// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Memory.h"

#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/BreakPoints.h"

namespace API::Memory
{

void AddMemcheck(u32 addr)
{
  TMemCheck memcheck;
  memcheck.start_address = addr;
  memcheck.end_address = addr;
  PowerPC::memchecks.Add(memcheck);
}

void RemoveMemcheck(u32 addr)
{
  PowerPC::memchecks.Remove(addr);
}

}  // namespace API::Memory
