// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "Core/HW/DSPHLE/UCodes/UCodes.h"

namespace DSP::HLE
{
class DSPHLE;

class CARDUCode final : public UCodeInterface
{
public:
  CARDUCode(DSPHLE* dsphle, u32 crc);

  void Initialize() override;
  void HandleMail(u32 mail) override;
  void Update() override;
  void DoState(PointerWrap& p) override;

private:
  enum class State
  {
    WaitingForRequest,
    WaitingForAddress,
    WaitingForNextTask,
  };

  // Currently unused, will be used in a later version
  State m_state = State::WaitingForRequest;
};
}  // namespace DSP::HLE
