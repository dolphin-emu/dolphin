// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "Common/CommonTypes.h"
#include "Core/DSPEmulator.h"
#include "Core/HW/DSP.h"
#include "Core/HW/DSPHLE/MailHandler.h"

class PointerWrap;

namespace DSP::HLE
{
class UCodeInterface;

class DSPHLE : public DSPEmulator
{
public:
  DSPHLE();
  ~DSPHLE();

  bool Initialize(bool wii, bool dsp_thread) override;
  void Shutdown() override;
  bool IsLLE() const override { return false; }
  void DoState(PointerWrap& p) override;
  void PauseAndLock(bool do_lock, bool unpause_on_unlock = true) override;

  void DSP_WriteMailBoxHigh(bool cpu_mailbox, u16 value) override;
  void DSP_WriteMailBoxLow(bool cpu_mailbox, u16 value) override;
  u16 DSP_ReadMailBoxHigh(bool cpu_mailbox) override;
  u16 DSP_ReadMailBoxLow(bool cpu_mailbox) override;
  u16 DSP_ReadControlRegister() override;
  u16 DSP_WriteControlRegister(u16 value) override;
  void DSP_Update(int cycles) override;
  void DSP_StopSoundStream() override;
  u32 DSP_UpdateRate() override;

  CMailHandler& AccessMailHandler() { return m_mail_handler; }
  void SetUCode(u32 crc);
  void SwapUCode(u32 crc);

private:
  void SendMailToDSP(u32 mail);

  // Fake mailbox utility
  struct DSPState
  {
    u32 cpu_mailbox;
    u32 dsp_mailbox;

    void Reset()
    {
      cpu_mailbox = 0x00000000;
      dsp_mailbox = 0x00000000;
    }

    DSPState() { Reset(); }
  };
  DSPState m_dsp_state;

  std::unique_ptr<UCodeInterface> m_ucode;
  std::unique_ptr<UCodeInterface> m_last_ucode;

  DSP::UDSPControl m_dsp_control;
  CMailHandler m_mail_handler;

  bool m_halt;
  bool m_assert_interrupt;
};
}  // namespace DSP::HLE
