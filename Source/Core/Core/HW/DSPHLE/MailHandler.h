// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <deque>
#include <utility>

#include "Common/CommonTypes.h"

namespace DSP
{
class DSPManager;
}
class PointerWrap;

namespace DSP::HLE
{
class CMailHandler
{
public:
  explicit CMailHandler(DSP::DSPManager& dsp);
  CMailHandler(const CMailHandler& other) = delete;
  CMailHandler(CMailHandler&& other) = delete;
  CMailHandler& operator=(const CMailHandler& other) = delete;
  CMailHandler& operator=(CMailHandler&& other) = delete;
  ~CMailHandler();

  // TODO: figure out correct timing for interrupts rather than defaulting to "immediately."
  void PushMail(u32 mail, bool interrupt = false, int cycles_into_future = 0);
  void SetHalted(bool halt);
  void DoState(PointerWrap& p);
  bool HasPending() const;

  // Clear any pending mail from the current uCode.  This is called by DSPHLE::SetUCode and
  // DSPHLE::SwapUCode. Since pending mail is an abstraction for DSPHLE and not something that
  // actually exists on real hardware, HLE implementations do not need to call this directly.
  // Note that this function does not reset m_last_mail, which will continue to read the same value
  // until the new uCode sends mail.
  void ClearPending();

  u16 ReadDSPMailboxHigh();
  u16 ReadDSPMailboxLow();

private:
  // The actual DSP only has a single pair of mail registers, and doesn't keep track of pending
  // mails. But for HLE, it's a lot easier to write all the mails that will be read ahead of time,
  // and then give them to the CPU in the requested order.
  std::deque<std::pair<u32, bool>> m_pending_mails;
  // If no pending mail exists, the last mail that was read is returned,
  // but with the top bit (0x80000000) cleared.
  u32 m_last_mail = 0;
  // When halted, the DSP itself is not running, but the last mail can be read.
  bool m_halted = false;

  DSP::DSPManager& m_dsp;
};
}  // namespace DSP::HLE
