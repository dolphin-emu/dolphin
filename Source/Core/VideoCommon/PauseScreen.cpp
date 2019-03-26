#include "PauseScreen.h"

#include "Core/Core.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/GCPadEmu.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

#include "Common/MsgHandler.h"
#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/Setting/BooleanSetting.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"
#include "InputCommon/ControllerEmu/StickGate.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "VideoCommon/RenderBase.h"

#include "imgui.h"

namespace
{
static const u16 wiimote_dpad_bitmasks[] = {WiimoteEmu::Wiimote::PAD_UP, WiimoteEmu::Wiimote::PAD_DOWN,
                                    WiimoteEmu::Wiimote::PAD_LEFT, WiimoteEmu::Wiimote::PAD_RIGHT};

static const u16 wiimote_button_bitmasks[] = {
    WiimoteEmu::Wiimote::BUTTON_A,     WiimoteEmu::Wiimote::BUTTON_B,
    WiimoteEmu::Wiimote::BUTTON_ONE,   WiimoteEmu::Wiimote::BUTTON_TWO,
    WiimoteEmu::Wiimote::BUTTON_MINUS, WiimoteEmu::Wiimote::BUTTON_PLUS,
    WiimoteEmu::Wiimote::BUTTON_HOME};
}  // namespace

PauseScreen::~PauseScreen()
{
  Hide();
}

void PauseScreen::Hide()
{
  ImGuiIO& io = ImGui::GetIO();
  // io.WantCaptureKeyboard = false;
  io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;
  io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
  g_renderer->BeginUIFrame();
  g_renderer->RenderUIFrame();
  g_renderer->EndUIFrame();
  m_visible = false;
}

void PauseScreen::Display()
{
  if (!m_visible)
  {
    m_state_stack.push(ScreenState::Main);
    m_visible = true;
  }
  ImGuiIO& io = ImGui::GetIO();

  g_renderer->BeginUIFrame();

  const float scale = io.DisplayFramebufferScale.x;
  ImGui::SetNextWindowSize(ImVec2(400.0f * scale, 100.0f * scale), ImGuiCond_Always);
  ImGui::SetNextWindowPosCenter(ImGuiCond_Always);
  if (ImGui::Begin(GetStringT("Pause Screen").c_str(), nullptr,
                   ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
                       ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                       ImGuiWindowFlags_AlwaysAutoResize))
  {
    switch (m_state_stack.top())
    {
    case ScreenState::Main:
      DisplayMain();
      break;
    case ScreenState::Options:
      DisplayOptions();
      break;
    case ScreenState::Profiles:
      DisplayProfiles();
      break;
    }

    ImGui::End();
    g_renderer->DrawLastXFBFrame();
    g_renderer->RenderUIFrame();

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
    // io.WantCaptureKeyboard = true;
    g_controller_interface.UpdateInput();
    UpdateControls(io);
    g_renderer->EndUIFrame();
  }
}

void PauseScreen::UpdateControls(ImGuiIO& io)
{
  memset(io.NavInputs, 0, sizeof(io.NavInputs));

  auto* wiimote_dpad_group = static_cast<ControllerEmu::Buttons*>(
      Wiimote::GetWiimoteGroup(0, WiimoteEmu::WiimoteGroup::DPad));

  u16 wiimote_dpad_buttons = 0;
  wiimote_dpad_group->GetState(&wiimote_dpad_buttons, wiimote_dpad_bitmasks);

  if ((wiimote_dpad_buttons & WiimoteEmu::Wiimote::PAD_DOWN) != 0)
  {
    io.NavInputs[ImGuiNavInput_DpadDown] = 1.0f;
  }

  if ((wiimote_dpad_buttons & WiimoteEmu::Wiimote::PAD_UP) != 0)
  {
    io.NavInputs[ImGuiNavInput_DpadUp] = 1.0f;
  }

  auto* wiimote_buttons_group = static_cast<ControllerEmu::Buttons*>(
      Wiimote::GetWiimoteGroup(0, WiimoteEmu::WiimoteGroup::Buttons));

  u16 wiimote_buttons = 0;
  wiimote_buttons_group->GetState(&wiimote_buttons, wiimote_button_bitmasks);

  if ((wiimote_buttons & WiimoteEmu::Wiimote::BUTTON_A) != 0)
  {
    io.NavInputs[ImGuiNavInput_Activate] = 1.0f;
  }
}

void PauseScreen::DisplayMain()
{
  if (ImGui::Button("Resume Emulation"))
  {
    Core::SetState(Core::State::Running);
  }
  else if (ImGui::Button("Options"))
  {
    m_state_stack.push(ScreenState::Options);
  }
  else if (ImGui::Button("Quit Dolphin"))
  {
    PanicAlert("Exit called...");
  }
}

void PauseScreen::DisplayOptions()
{
  if (ImGui::Button("Wii Controller Profiles"))
  {
    m_state_stack.push(ScreenState::Profiles);
  }
  else if (ImGui::Button("Back..."))
  {
    m_state_stack.pop();
  }
}

void PauseScreen::DisplayProfiles()
{
  // TODO: Get current profile name...
  // TODO: Handle multiple controllers...

  if (ImGui::Button("Previous Wiimote (1) Profile"))
  {
    m_profile_cycler.PreviousWiimoteProfile(0);
  }
  else if (ImGui::Button("Next Wiimote (1) Profile"))
  {
    m_profile_cycler.NextWiimoteProfile(0);
  }
  else if (ImGui::Button("Back..."))
  {
    m_state_stack.pop();
  }
}
