// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/Config/Config.h"
#include "InputCommon/ControllerInterface/SDL/SDLGamepad.h"

namespace ciface::SDL
{

class DualSenseGamepad final : public Gamepad
{
public:
  DualSenseGamepad(SDL_Gamepad* gamepad, SDL_Joystick* joystick);
  ~DualSenseGamepad() override;

private:
  void HandleConfigChange();
  void SetTactileTriggers(bool enabled);

  Config::ConfigChangedCallbackID m_config_change_callback_id;

  std::mutex m_config_mutex;

  bool m_tactile_triggers_enabled = false;
};

}  // namespace ciface::SDL
