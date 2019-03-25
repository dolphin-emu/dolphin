#pragma once

#include "InputCommon/InputProfile.h"

#include <stack>

class PauseScreen
{
public:
  PauseScreen();
  ~PauseScreen();
  void Display();

private:
  void DisplayMain();
  void DisplayOptions();
  void DisplayProfiles();

  enum class ScreenState
  {
    Main,
    Options,
    Profiles
  };

  std::stack<ScreenState> m_state_stack;

  // TODO: sync this with the cycler in HotkeyScheduler...
  InputProfile::ProfileCycler m_profile_cycler;
};
