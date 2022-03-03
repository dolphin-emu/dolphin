// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "InputCommon/ControllerEmu/ControllerEmu.h"

struct GCPadStatus;

namespace ControllerEmu
{
class Buttons;
}  // namespace ControllerEmu

enum class GBAPadGroup
{
  DPad,
  Buttons
};

class GBAPad : public ControllerEmu::EmulatedController
{
public:
  explicit GBAPad(unsigned int index);
  GCPadStatus GetInput();
  void SetReset(bool reset);

  std::string GetName() const override;

  ControllerEmu::ControlGroup* GetGroup(GBAPadGroup group) const;

  void LoadDefaults(const ControllerInterface& ciface) override;

private:
  ControllerEmu::Buttons* m_buttons;
  ControllerEmu::Buttons* m_dpad;
  bool m_reset_pending;

  const unsigned int m_index;
};
