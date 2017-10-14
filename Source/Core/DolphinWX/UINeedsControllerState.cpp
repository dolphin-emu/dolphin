// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <atomic>

static std::atomic<bool> s_needs_controller_state{false};

void SetUINeedsControllerState(bool needs_controller_state)
{
  s_needs_controller_state = needs_controller_state;
}

bool GetUINeedsControllerState()
{
  return s_needs_controller_state;
}
