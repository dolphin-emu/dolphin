// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "Core/PowerPC/Jit64Common/EmuCodeBlock.h"
#include "Core/PowerPC/JitCommon/JitAsmCommon.h"

enum EQuantizeType : u32;

class Jit64;

class QuantizedMemoryRoutines : public EmuCodeBlock
{
public:
  explicit QuantizedMemoryRoutines(Jit64& jit) : EmuCodeBlock(jit) {}
  void GenQuantizedLoad(bool single, EQuantizeType type, int quantize);
  void GenQuantizedStore(bool single, EQuantizeType type, int quantize);

private:
  void GenQuantizedLoadFloat(bool single, bool isInline);
  void GenQuantizedStoreFloat(bool single, bool isInline);
};

class CommonAsmRoutines : public CommonAsmRoutinesBase, public QuantizedMemoryRoutines
{
public:
  explicit CommonAsmRoutines(Jit64& jit) : QuantizedMemoryRoutines(jit), m_jit(jit) {}
  void GenFrsqrte();
  void GenFres();
  void GenMfcr();

protected:
  void GenConvertDoubleToSingle();
  const u8* GenQuantizedLoadRuntime(bool single, EQuantizeType type);
  const u8* GenQuantizedStoreRuntime(bool single, EQuantizeType type);
  void GenQuantizedLoads();
  void GenQuantizedSingleLoads();
  void GenQuantizedStores();
  void GenQuantizedSingleStores();

  Jit64& m_jit;
};
