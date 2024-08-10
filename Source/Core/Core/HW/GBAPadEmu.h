// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/Common.h"

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

  InputConfig* GetConfig() const override;

  ControllerEmu::ControlGroup* GetGroup(GBAPadGroup group) const;

  void LoadDefaults(const ControllerInterface& ciface) override;

  static constexpr auto BUTTONS_GROUP = _trans("Buttons");
  static constexpr auto DPAD_GROUP = _trans("D-Pad");

  static constexpr auto B_BUTTON = "B";
  static constexpr auto A_BUTTON = "A";
  static constexpr auto L_BUTTON = "L";
  static constexpr auto R_BUTTON = "R";
  static constexpr auto SELECT_BUTTON = _trans("SELECT");
  static constexpr auto START_BUTTON = _trans("START");

private:
  ControllerEmu::Buttons* m_buttons;
  ControllerEmu::Buttons* m_dpad;
  bool m_reset_pending;

  const unsigned int m_index;
};
