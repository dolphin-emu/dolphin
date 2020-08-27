// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>

#include "Common/x64Reg.h"

// RSCRATCH and RSCRATCH2 are always scratch registers and can be used without
// limitation.
constexpr Gen::X64Reg RSCRATCH = Gen::RAX;
constexpr Gen::X64Reg RSCRATCH2 = Gen::RDX;
// RSCRATCH_EXTRA may be in the allocation order, so it has to be flushed
// before use.
constexpr Gen::X64Reg RSCRATCH_EXTRA = Gen::RCX;
// RMEM points to the start of emulated memory.
constexpr Gen::X64Reg RMEM = Gen::RBX;
// RPPCSTATE points to ppcState + 0x80.  It's offset because we want to be able
// to address as much as possible in a one-byte offset form.
constexpr Gen::X64Reg RPPCSTATE = Gen::RBP;

constexpr size_t CODE_SIZE = 1024 * 1024 * 32;
