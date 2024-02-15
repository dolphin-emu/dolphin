// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "Core/HW/DSPHLE/UCodes/UCodes.h"

namespace Memory
{
class MemoryManager;
}

namespace DSP::HLE
{
class DSPHLE;

// Computes two 32 bit integers to be returned to the game, based on the
// provided crypto parameters at the provided MRAM address. The integers are
// written back to RAM at the dest address provided in the crypto parameters.
void ProcessGBACrypto(Memory::MemoryManager& memory, u32 address);

class GBAUCode final : public UCodeInterface
{
public:
  GBAUCode(DSPHLE* dsphle, u32 crc);

  void Initialize() override;
  void HandleMail(u32 mail) override;
  void Update() override;
  void DoState(PointerWrap& p) override;

private:
  static constexpr u32 REQUEST_MAIL = 0xabba0000;

  enum class MailState
  {
    WaitingForRequest,
    WaitingForAddress,
    WaitingForNextTask,
  };

  MailState m_mail_state = MailState::WaitingForRequest;
};
}  // namespace DSP::HLE
