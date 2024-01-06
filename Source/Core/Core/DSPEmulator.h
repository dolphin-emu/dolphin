// Copyright 2011 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include "Common/CommonTypes.h"

namespace Core
{
class System;
}
class PointerWrap;

class DSPEmulator
{
public:
  virtual ~DSPEmulator();
  virtual bool IsLLE() const = 0;

  virtual bool Initialize(bool wii, bool dsp_thread) = 0;
  virtual void Shutdown() = 0;

  virtual void DoState(PointerWrap& p) = 0;
  virtual void PauseAndLock(bool do_lock) = 0;

  virtual void DSP_WriteMailBoxHigh(bool cpu_mailbox, u16 value) = 0;
  virtual void DSP_WriteMailBoxLow(bool cpu_mailbox, u16 value) = 0;
  virtual u16 DSP_ReadMailBoxHigh(bool cpu_mailbox) = 0;
  virtual u16 DSP_ReadMailBoxLow(bool cpu_mailbox) = 0;
  virtual u16 DSP_ReadControlRegister() = 0;
  virtual u16 DSP_WriteControlRegister(u16 value) = 0;
  virtual void DSP_Update(int cycles) = 0;
  virtual void DSP_StopSoundStream() = 0;
  virtual u32 DSP_UpdateRate() = 0;

protected:
  bool m_wii = false;
};

std::unique_ptr<DSPEmulator> CreateDSPEmulator(Core::System& system, bool hle);
