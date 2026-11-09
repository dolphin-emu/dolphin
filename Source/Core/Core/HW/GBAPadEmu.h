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
  static constexpr u16 GBA_BUTTON_A = 1u << 0;
  static constexpr u16 GBA_BUTTON_B = 1u << 1;
  static constexpr u16 GBA_BUTTON_SELECT = 1u << 2;
  static constexpr u16 GBA_BUTTON_START = 1u << 3;
  static constexpr u16 GBA_BUTTON_RIGHT = 1u << 4;
  static constexpr u16 GBA_BUTTON_LEFT = 1u << 5;
  static constexpr u16 GBA_BUTTON_UP = 1u << 6;
  static constexpr u16 GBA_BUTTON_DOWN = 1u << 7;
  static constexpr u16 GBA_BUTTON_R = 1u << 8;
  static constexpr u16 GBA_BUTTON_L = 1u << 9;

  static constexpr u16 GBA_ALL_BUTTONS = (1u << 10) - 1;

  static constexpr u16 PAD_STATUS_RESET_SIGNAL = 1u << 10;

  explicit GBAPad(unsigned int index);
  GCPadStatus GetInput();
  void SetReset(bool reset);

  std::string GetName() const override;

  InputConfig* GetConfig() const override;

  ControllerEmu::ControlGroup* GetGroup(GBAPadGroup group) const;

  void LoadDefaults(const ControllerInterface& ciface) override;

  static constexpr const char* BUTTONS_GROUP = _trans("Buttons");
  static constexpr const char* DPAD_GROUP = _trans("D-Pad");

  static constexpr const char* B_BUTTON = "B";
  static constexpr const char* A_BUTTON = "A";
  static constexpr const char* L_BUTTON = "L";
  static constexpr const char* R_BUTTON = "R";
  static constexpr const char* SELECT_BUTTON = _trans("SELECT");
  static constexpr const char* START_BUTTON = _trans("START");

private:
  ControllerEmu::Buttons* m_buttons;
  ControllerEmu::Buttons* m_dpad;
  bool m_reset_pending;

  const unsigned int m_index;
};
