// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/DSP/Jit/DSPEmitterBase.h"

#if defined(_M_X86) || defined(_M_X86_64)
#include "Core/DSP/Jit/x64/DSPEmitter.h"
#endif

namespace DSP::JIT
{
DSPEmitter::~DSPEmitter() = default;

std::unique_ptr<DSPEmitter> CreateDSPEmitter()
{
#if defined(_M_X86) || defined(_M_X86_64)
  return std::make_unique<x64::DSPEmitter>();
#else
  return std::make_unique<DSPEmitterNull>();
#endif
}
}  // namespace DSP::JIT
