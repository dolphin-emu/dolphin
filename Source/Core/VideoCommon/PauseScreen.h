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
  void UpdateControls(ImGuiIO& io);

  // Wiimote controls
  void UpdateWiimoteDpad(ImGuiIO& io);
  void UpdateWiimoteButtons(ImGuiIO& io, bool& back_pressed);

  // Wiimote accessory controls
  void UpdateWiimoteNunchukStick(ImGuiIO& io);
  void UpdateClassicControllerStick(ImGuiIO& io);
  void UpdateClassicControllerDPad(ImGuiIO& io);
  void UpdateClassicControllerButtons(ImGuiIO& io, bool& back_pressed);

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
