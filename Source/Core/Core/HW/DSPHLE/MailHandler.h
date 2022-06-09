// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <queue>
#include <utility>

#include "Common/CommonTypes.h"

class PointerWrap;

namespace DSP::HLE
{
class CMailHandler
{
public:
  CMailHandler();
  ~CMailHandler();

  // TODO: figure out correct timing for interrupts rather than defaulting to "immediately."
  void PushMail(u32 mail, bool interrupt = false, int cycles_into_future = 0);
  void Halt(bool _Halt);
  void DoState(PointerWrap& p);
  bool HasPending() const;

  // Clear any pending mail from the current uCode.  This is called by DSPHLE::SetUCode and
  // DSPHLE::SwapUCode. Since pending mail is an abstraction for DSPHLE and not something that
  // actually exists on real hardware, HLE implementations do not need to call this directly.
  void ClearPending();

  u16 ReadDSPMailboxHigh();
  u16 ReadDSPMailboxLow();

private:
  // mail handler
  std::queue<std::pair<u32, bool>> m_Mails;
};
}  // namespace DSP::HLE
