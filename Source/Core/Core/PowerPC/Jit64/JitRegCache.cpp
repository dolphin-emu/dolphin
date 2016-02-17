// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <limits>

#include "Common/Assert.h"
#include "Common/BitSet.h"
#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"
#include "Common/x64Emitter.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64/JitRegCache.h"
#include "Core/PowerPC/JitCommon/Jit_Util.h"

using namespace Gen;
using namespace PowerPC;

OpArg GPRRegCache::GetDefaultLocation(size_t reg) const
{
	return PPCSTATE(gpr[reg]);
}

OpArg FPURegCache::GetDefaultLocation(size_t reg) const
{
	return PPCSTATE(ps[reg][0]);
}

template class RegCache<Jit64Reg::Type::GPR>;
template class RegCache<Jit64Reg::Type::FPU>;
