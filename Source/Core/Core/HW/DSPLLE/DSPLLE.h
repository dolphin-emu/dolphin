// Copyright 2011 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <mutex>
#include <thread>

#include "Common/CommonTypes.h"
#include "Common/Flag.h"
#include "Core/DSP/DSPCore.h"
#include "Core/DSPEmulator.h"

class PointerWrap;

namespace DSP::LLE
{
class DSPLLE : public DSPEmulator
{
public:
  DSPLLE();
  ~DSPLLE();

  bool Initialize(bool wii, bool dsp_thread) override;
  void Shutdown() override;
  bool IsLLE() const override { return true; }
  void DoState(PointerWrap& p) override;
  void PauseAndLock(bool do_lock) override;

  void DSP_WriteMailBoxHigh(bool cpu_mailbox, u16 value) override;
  void DSP_WriteMailBoxLow(bool cpu_mailbox, u16 value) override;
  u16 DSP_ReadMailBoxHigh(bool cpu_mailbox) override;
  u16 DSP_ReadMailBoxLow(bool cpu_mailbox) override;
  u16 DSP_ReadControlRegister() override;
  u16 DSP_WriteControlRegister(u16 value) override;
  void DSP_Update(int cycles) override;
  void DSP_StopSoundStream() override;
  u32 DSP_UpdateRate() override;

private:
  static void DSPThread(DSPLLE* dsp_lle);

  DSPCore m_dsp_core;
  std::thread m_dsp_thread;
  std::mutex m_dsp_thread_mutex;
  bool m_is_dsp_on_thread = false;
  Common::Flag m_is_running;
  std::atomic<u32> m_cycle_count{};

  Common::Event m_dsp_event;
  Common::Event m_ppc_event;
  bool m_request_disable_thread = false;
};
}  // namespace DSP::LLE
