#include "PauseScreen.h"

#include "Core/Core.h"

#include "Common/MsgHandler.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "VideoCommon/RenderBase.h"

#include "imgui.h"

PauseScreen::~PauseScreen()
{
  Hide();
}

void PauseScreen::Hide()
{
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
  g_renderer->BeginUIFrame();
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
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
  g_controller_interface.UpdateInput();
  // io.NavInputs[ImGuiNavInput_DpadDown] = 1.0f;

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
    g_renderer->EndUIFrame();
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
