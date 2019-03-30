#pragma once

#include "InputCommon/InputProfile.h"

#include <stack>

struct ImGuiIO;

class PauseScreen
{
public:
  ~PauseScreen();
  void Display();
  void Hide();
  bool IsVisible() const { return m_visible; }

private:
  void UpdateControls(ImGuiIO&);
  void DisplayMain();
  void DisplayOptions();
  void DisplayGraphics();
  void DisplayControls();
  void DisplayWiiControls();

  enum class ScreenState
  {
    Main,
    Options,
    Graphics,
    Controls
  };

  std::stack<ScreenState> m_state_stack;

  bool m_visible = false;
};
