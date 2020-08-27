// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "InputCommon/ControllerEmu/ControllerEmu.h"

struct KeyboardStatus;

namespace ControllerEmu
{
class ControlGroup;
class Buttons;
}  // namespace ControllerEmu

enum class KeyboardGroup
{
  Kb0x,
  Kb1x,
  Kb2x,
  Kb3x,
  Kb4x,
  Kb5x,

  Options
};

class GCKeyboard : public ControllerEmu::EmulatedController
{
public:
  explicit GCKeyboard(unsigned int index);
  KeyboardStatus GetInput() const;
  std::string GetName() const override;
  ControllerEmu::ControlGroup* GetGroup(KeyboardGroup group);
  void LoadDefaults(const ControllerInterface& ciface) override;

private:
  ControllerEmu::Buttons* m_keys0x;
  ControllerEmu::Buttons* m_keys1x;
  ControllerEmu::Buttons* m_keys2x;
  ControllerEmu::Buttons* m_keys3x;
  ControllerEmu::Buttons* m_keys4x;
  ControllerEmu::Buttons* m_keys5x;
  ControllerEmu::ControlGroup* m_options;

  const unsigned int m_index;
};
