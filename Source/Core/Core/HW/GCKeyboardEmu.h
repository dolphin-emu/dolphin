// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "InputCommon/ControllerEmu/ControllerEmu.h"

struct KeyboardStatus;

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

class GCKeyboard : public ControllerEmu
{
public:
  explicit GCKeyboard(unsigned int index);
  KeyboardStatus GetInput() const;
  std::string GetName() const override;
  ControlGroup* GetGroup(KeyboardGroup group);
  void LoadDefaults(const ControllerInterface& ciface) override;

private:
  Buttons* m_keys0x;
  Buttons* m_keys1x;
  Buttons* m_keys2x;
  Buttons* m_keys3x;
  Buttons* m_keys4x;
  Buttons* m_keys5x;
  ControlGroup* m_options;

  const unsigned int m_index;
};
