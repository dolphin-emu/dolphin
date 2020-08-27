// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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
  void Clear();
  void Halt(bool _Halt);
  void DoState(PointerWrap& p);
  bool IsEmpty() const;

  u16 ReadDSPMailboxHigh();
  u16 ReadDSPMailboxLow();

private:
  // mail handler
  std::queue<std::pair<u32, bool>> m_Mails;
};
}  // namespace DSP::HLE
