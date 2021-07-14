// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/GBAPadEmu.h"

#include <fmt/format.h>

#include "InputCommon/ControllerEmu/Control/Input.h"
#include "InputCommon/ControllerEmu/ControlGroup/Buttons.h"
#include "InputCommon/GCPadStatus.h"

static const u16 dpad_bitmasks[] = {PAD_BUTTON_UP, PAD_BUTTON_DOWN, PAD_BUTTON_LEFT,
                                    PAD_BUTTON_RIGHT};

static const u16 button_bitmasks[] = {PAD_BUTTON_B,  PAD_BUTTON_A,  PAD_TRIGGER_L,
                                      PAD_TRIGGER_R, PAD_TRIGGER_Z, PAD_BUTTON_START};

static const char* const named_buttons[] = {"B", "A", "L", "R", _trans("SELECT"), _trans("START")};

GBAPad::GBAPad(const unsigned int index) : m_reset_pending(false), m_index(index)
{
  // Buttons
  groups.emplace_back(m_buttons = new ControllerEmu::Buttons(_trans("Buttons")));
  for (const char* named_button : named_buttons)
  {
    const ControllerEmu::Translatability translate =
        (named_button == std::string(_trans("SELECT")) ||
         named_button == std::string(_trans("START"))) ?
            ControllerEmu::Translate :
            ControllerEmu::DoNotTranslate;
    m_buttons->AddInput(translate, named_button);
  }

  // DPad
  groups.emplace_back(m_dpad = new ControllerEmu::Buttons(_trans("D-Pad")));
  for (const char* named_direction : named_directions)
  {
    m_dpad->AddInput(ControllerEmu::Translate, named_direction);
  }
}

std::string GBAPad::GetName() const
{
  return fmt::format("GBA{}", m_index + 1);
}

ControllerEmu::ControlGroup* GBAPad::GetGroup(GBAPadGroup group) const
{
  switch (group)
  {
  case GBAPadGroup::Buttons:
    return m_buttons;
  case GBAPadGroup::DPad:
    return m_dpad;
  default:
    return nullptr;
  }
}

GCPadStatus GBAPad::GetInput()
{
  const auto lock = GetStateLock();
  GCPadStatus pad = {};

  // Buttons
  m_buttons->GetState(&pad.button, button_bitmasks);

  // DPad
  m_dpad->GetState(&pad.button, dpad_bitmasks);

  // Use X button as a reset signal
  if (m_reset_pending)
    pad.button |= PAD_BUTTON_X;
  m_reset_pending = false;

  return pad;
}

void GBAPad::SetReset(bool reset)
{
  const auto lock = GetStateLock();
  m_reset_pending = reset;
}

void GBAPad::LoadDefaults(const ControllerInterface& ciface)
{
  EmulatedController::LoadDefaults(ciface);

  // Buttons
  m_buttons->SetControlExpression(0, "`Z`");  // B
  m_buttons->SetControlExpression(1, "`X`");  // A
  m_buttons->SetControlExpression(2, "`Q`");  // L
  m_buttons->SetControlExpression(3, "`W`");  // R
#ifdef _WIN32
  m_buttons->SetControlExpression(4, "`BACK`");    // Select
  m_buttons->SetControlExpression(5, "`RETURN`");  // Start
#else
  m_buttons->SetControlExpression(4, "`Backspace`");  // Select
  m_buttons->SetControlExpression(5, "`Return`");     // Start
#endif

  // D-Pad
  m_dpad->SetControlExpression(0, "`T`");  // Up
  m_dpad->SetControlExpression(1, "`G`");  // Down
  m_dpad->SetControlExpression(2, "`F`");  // Left
  m_dpad->SetControlExpression(3, "`H`");  // Right
}
