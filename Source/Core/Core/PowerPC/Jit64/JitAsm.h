// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <emmintrin.h>
#include "Common/CommonTypes.h"
#include "Core/PowerPC/Jit64Common/Jit64AsmCommon.h"

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
private:
  void Generate(const Gen::X64Reg* reserved_fprs, size_t reserved_fpr_count);
  void ResetStack();
  void GenerateCommon();
  u8* m_stack_top;
  __m128i m_reserved_fpr_values[16];

public:
  void Init(u8* stack_top, const Gen::X64Reg* reserved_fprs, const __m128i* reserved_fpr_values,
            size_t reserved_fpr_count);

  void Shutdown() { FreeCodeSpace(); }
};
