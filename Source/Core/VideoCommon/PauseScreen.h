#pragma once

#include "InputCommon/InputProfile.h"

#include <stack>

struct ImGuiIO;

void InitPauseScreenThread();
void ShutdownPauseScreenThread();

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
  void UpdateWiimoteButtons(ImGuiIO& io, bool& back_pressed, bool &accept_pressed);

  // Wiimote accessory controls
  void UpdateWiimoteNunchukStick(ImGuiIO& io);
  void UpdateClassicControllerStick(ImGuiIO& io);
  void UpdateClassicControllerDPad(ImGuiIO& io);
  void UpdateClassicControllerButtons(ImGuiIO& io, bool& back_pressed, bool& accept_pressed);

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
  bool m_back_held = false;
  bool m_accept_held = false;
};
