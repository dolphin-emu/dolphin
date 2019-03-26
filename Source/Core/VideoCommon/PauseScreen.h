#pragma once

#include "InputCommon/InputProfile.h"

#include <stack>

class PauseScreen
{
public:
  ~PauseScreen();
  void Display();
  void Hide();
  bool IsVisible() const { return m_visible; }

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

  bool m_visible = false;
};
