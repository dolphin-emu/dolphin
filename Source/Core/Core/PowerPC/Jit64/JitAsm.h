// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "Core/PowerPC/Jit64Common/Jit64AsmCommon.h"

namespace Gen
{
class X64CodeBlock;
}

// In Dolphin, we don't use inline assembly. Instead, we generate all machine-near
// code at runtime. In the case of fixed code like this, after writing it, we write
// protect the memory, essentially making it work just like precompiled code.

// There are some advantages to this approach:
//   1) No need to setup an external assembler in the build.
//   2) Cross platform, as long as it's x86/x64.
//   3) Can optimize code at runtime for the specific CPU model.
// There aren't really any disadvantages other than having to maintain a x86 emitter,
// which we have to do anyway :)
//
// To add a new asm routine, just add another const here, and add the code to Generate.
// Also, possibly increase the size of the code buffer.

class Jit64AsmRoutineManager : public CommonAsmRoutines
{
public:
  // NOTE: When making large additions to the AsmCommon code, you might
  // want to ensure this number is big enough.
  static constexpr size_t CODE_SIZE = 16384;

  explicit Jit64AsmRoutineManager(Jit64& jit);

  void Init();

  void ResetStack(Gen::X64CodeBlock& emitter);

private:
  void Generate();
  void GenerateCommon();
};
