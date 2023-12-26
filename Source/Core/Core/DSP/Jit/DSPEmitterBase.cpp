// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/DSP/Jit/DSPEmitterBase.h"

#if defined(_M_X86_64)
#include "Core/DSP/Jit/x64/DSPEmitter.h"
#endif

namespace DSP::JIT
{
DSPEmitter::~DSPEmitter() = default;

std::unique_ptr<DSPEmitter> CreateDSPEmitter([[maybe_unused]] DSPCore& dsp)
{
#if defined(_M_X86_64)
  return std::make_unique<x64::DSPEmitter>(dsp);
#else
  return std::make_unique<DSPEmitterNull>();
#endif
}
}  // namespace DSP::JIT
