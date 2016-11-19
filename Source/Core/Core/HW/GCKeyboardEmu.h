// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "InputCommon/ControllerEmu.h"

struct KeyboardStatus;

enum KeyboardGoup
{
  KBGP_0X,
  KBGP_1X,
  KBGP_2X,
  KBGP_3X,
  KBGP_4X,
  KBGP_5X,

  KBGP_OPTIONS
};

class GCKeyboard : public ControllerEmu
{
public:
  GCKeyboard(const unsigned int index);
  KeyboardStatus GetInput() const;
  std::string GetName() const override;
  ControlGroup* GetGroup(KeyboardGoup group);
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
