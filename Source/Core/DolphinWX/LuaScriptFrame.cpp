// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/LuaScripting.h"
#include <lua5.3/include/lua.hpp>
#include <lua5.3/include/lauxlib.h>

#include <wx/button.h>
#include <wx/filedlg.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/settings.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "Core/Core.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/GCPadEmu.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/Movie.h"
#include "Core/State.h"
#include "DolphinWX/Frame.h"
#include "DolphinWX/Main.h"
#include "DolphinWX/WxUtils.h"
#include "InputCommon/GCPadStatus.h"
#include "InputCommon/InputConfig.h"

namespace Lua
{
static int band(lua_State* L);

static int printToTextCtrl(lua_State* L);
static int frameAdvance(lua_State* L);
static int getFrameCount(lua_State* L);
static int getMovieLength(lua_State* L);
static int softReset(lua_State* L);

// Slots from 0-99.
static int saveState(lua_State* L);
static int loadState(lua_State* L);

// The argument should be a percentage of speed. I.E. setEmulatorSpeed(100) would set it to normal
// playback speed
static int setEmulatorSpeed(lua_State* L);

static int getAnalog(lua_State* L);
static int setAnalog(lua_State* L);
static int setAnalogPolar(lua_State* L);
static int getCStick(lua_State* L);
static int setCStick(lua_State* L);
static int setCStickPolar(lua_State* L);
static int getButtons(lua_State* L);
static int setButtons(lua_State* L);
static int setDPad(lua_State* L);
static int getTriggers(lua_State* L);
static int setTriggers(lua_State* L);

LuaScriptFrame* LuaScriptFrame::m_current_instance;

LuaScriptFrame::LuaScriptFrame(wxWindow* parent)
    : wxFrame(parent, wxID_ANY, _("Lua Console"), wxDefaultPosition, wxSize(431, 397),
              wxDEFAULT_FRAME_STYLE ^ wxRESIZE_BORDER)
{
  CreateGUI();
  SetIcons(WxUtils::GetDolphinIconBundle());

  Center();
  Show();

  m_current_instance = this;
  m_lua_thread = nullptr;
}

LuaScriptFrame::~LuaScriptFrame()
{
  // Stop currently executing Lua thread
  if (m_lua_thread)
  {
    // wxThread.Kill() crashes on non-windows platforms,
    // and right now I don't have a solution for Mac/Linux,
    // so I'm kind of cheating here.
#ifdef _WIN32
    m_lua_thread->Kill();
    m_lua_thread = nullptr;
#else
    m_lua_thread->m_destruction_flag = true;
    wxThread::This()->Sleep(1);
#endif
  }
  m_current_instance = nullptr;
  main_frame->m_lua_script_frame = nullptr;
}

void LuaScriptFrame::OnClose(wxCloseEvent& event)
{
	if (!m_lua_thread)
	{
		Destroy();
	}
	else
	{		
	    wxMessageBox(_("You must stop your script before closing this window!"));
	}
	return;
}

//
// CreateGUI
//
// Creates actual Lua console window.
//
void LuaScriptFrame::CreateGUI()
{
  SetSizeHints(wxDefaultSize, wxDefaultSize);

  CreateMenuBar();

  wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

  m_script_file_label =
      new wxStaticText(this, wxID_ANY, _("Script File:"), wxDefaultPosition, wxDefaultSize, 0);
  m_script_file_label->Wrap(-1);
  main_sizer->Add(m_script_file_label, 0, wxALL, 5);

  m_file_path =
      new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(300, -1), 0);
  m_file_path->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));

  main_sizer->Add(m_file_path, 0, wxALL, 5);

  wxBoxSizer* buttons = new wxBoxSizer(wxHORIZONTAL);

  m_browse_button = new wxButton(this, wxID_ANY, _("Browse..."), wxPoint(-1, -1), wxDefaultSize, 0);
  buttons->Add(m_browse_button, 0, wxALL, 5);

  m_run_button = new wxButton(this, wxID_ANY, _("Run"), wxDefaultPosition, wxDefaultSize, 0);
  buttons->Add(m_run_button, 0, wxALL, 5);

  m_stop_button = new wxButton(this, wxID_ANY, _("Stop"), wxDefaultPosition, wxDefaultSize, 0);
  buttons->Add(m_stop_button, 0, wxALL, 5);

  main_sizer->Add(buttons, 1, wxEXPAND, 5);

  m_output_console_literal =
      new wxStaticText(this, wxID_ANY, _("Output Console:"), wxDefaultPosition, wxDefaultSize, 0);
  m_output_console_literal->Wrap(-1);
  main_sizer->Add(m_output_console_literal, 0, wxALL, 5);

  m_output_console = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
                                    wxSize(415, 200), wxHSCROLL | wxTE_MULTILINE | wxTE_READONLY);
  main_sizer->Add(m_output_console, 0, wxALL, 6);

  SetSizer(main_sizer);
  Layout();

  Centre(wxBOTH);

  // Connect Events
  Bind(wxEVT_MENU, &LuaScriptFrame::OnClearClicked, this, m_clear->GetId());
  Bind(wxEVT_MENU, &LuaScriptFrame::OnDocumentationClicked, this, m_documentation->GetId());
  Bind(wxEVT_MENU, &LuaScriptFrame::OnAPIClicked, this, m_api->GetId());

#ifndef __WIN32
  Bind(wxEVT_CLOSE_WINDOW, &LuaScriptFrame::OnClose, this, wxID_ANY);
#endif

  m_browse_button->Bind(wxEVT_BUTTON, &LuaScriptFrame::BrowseOnButtonClick, this,
                        m_browse_button->GetId());

  m_run_button->Bind(wxEVT_BUTTON, &LuaScriptFrame::RunOnButtonClick, this, m_run_button->GetId());

  m_stop_button->Bind(wxEVT_BUTTON, &LuaScriptFrame::StopOnButtonClick, this,
                      m_stop_button->GetId());
}

void LuaScriptFrame::CreateMenuBar()
{
  m_menubar = new wxMenuBar(0);
  m_console_menu = new wxMenu();
  m_clear =
      new wxMenuItem(m_console_menu, wxID_ANY, wxString(_("Clear")), wxEmptyString, wxITEM_NORMAL);
  m_console_menu->Append(m_clear);
  m_help_menu = new wxMenu();
  m_documentation = new wxMenuItem(m_help_menu, wxID_ANY, wxString(_("Lua Documentation")),
                                   wxEmptyString, wxITEM_NORMAL);
  m_api = new wxMenuItem(m_help_menu, wxID_ANY, wxString(_("Dolphin Lua API")), wxEmptyString,
                         wxITEM_NORMAL);
  m_help_menu->Append(m_documentation);
  m_help_menu->Append(m_api);

  m_menubar->Append(m_console_menu, _("Console"));
  m_menubar->Append(m_help_menu, _("Help"));

  SetMenuBar(m_menubar);
}

void LuaScriptFrame::Log(const wxString& message)
{
  m_output_console->AppendText(message);
}

void LuaScriptFrame::OnClearClicked(wxCommandEvent& event)
{
  m_output_console->Clear();
}

void LuaScriptFrame::OnDocumentationClicked(wxCommandEvent& event)
{
  wxLaunchDefaultBrowser("https://www.lua.org/pil/contents.html");
}

void LuaScriptFrame::OnAPIClicked(wxCommandEvent& event)
{
  wxLaunchDefaultBrowser("https://github.com/NickDriscoll/dolphin/blob/Lua_scripting/Lua.md");
}

void LuaScriptFrame::BrowseOnButtonClick(wxCommandEvent& event)
{
  wxFileDialog dialog(this, _("Select Lua script."));

  if (dialog.ShowModal() == wxID_CANCEL)
    return;

  m_file_path->SetValue(dialog.GetPath());
  dialog.Destroy();
}

void LuaScriptFrame::RunOnButtonClick(wxCommandEvent& event)
{
  if (m_lua_thread == nullptr)
  {
    m_lua_thread = new LuaThread(this, m_file_path->GetValue());
    m_lua_thread->Run();
  }
}

void LuaScriptFrame::StopOnButtonClick(wxCommandEvent& event)
{
  if (m_lua_thread)
  {
    m_lua_thread->m_destruction_flag = true;
  }
}

void LuaScriptFrame::NullifyLuaThread()
{
  m_lua_thread = nullptr;
}

GCPadStatus& LuaScriptFrame::GetPadStatus()
{
  return m_lua_thread->m_pad_status;
}

GCPadStatus LuaScriptFrame::GetLastPadStatus()
{
  return m_lua_thread->m_last_pad_status;
}

LuaThread* LuaScriptFrame::GetLuaThread()
{
  return m_lua_thread;
}

LuaScriptFrame* LuaScriptFrame::GetCurrentInstance()
{
  if (m_current_instance == nullptr)
    m_current_instance = new LuaScriptFrame(main_frame);
  return m_current_instance;
}

// Functions to register with Lua
#pragma region Lua_Functs

static int band(lua_State* L)
{
  lua_pushinteger(L, lua_tointeger(L, 1) & lua_tointeger(L, 2));
  return 1;
}

// Prints a string to the text control of this frame
static int printToTextCtrl(lua_State* L)
{
  LuaScriptFrame::GetCurrentInstance()->GetEventHandler()->CallAfter(&LuaScriptFrame::Log, wxString(lua_tostring(L, 1)));
  LuaScriptFrame::GetCurrentInstance()->GetEventHandler()->CallAfter(&LuaScriptFrame::Log, wxString("\n"));

  return 0;
}

// Steps a frame if the emulator is paused, pauses it otherwise.
static int frameAdvance(lua_State* L)
{
  u64 current_frame = Movie::GetCurrentFrame();
  Core::DoFrameStep();

  // Block until a frame has actually processed
  // Prevents a script from executing it's main loop more than once per frame.
  while (current_frame == Movie::GetCurrentFrame());
  return 0;
}

static int getFrameCount(lua_State* L)
{
  lua_pushinteger(L, Movie::GetCurrentFrame());
  return 1;
}

static int getMovieLength(lua_State* L)
{
  if (Movie::IsMovieActive())
  {
    lua_pushinteger(L, Movie::GetTotalFrames());
    return 1;
  }
  else
  {
    return 0;
  }
}

static int softReset(lua_State* L)
{
  ProcessorInterface::ResetButton_Tap();
  return 0;
}

static int saveState(lua_State* L)
{
  int slot = lua_tointeger(L, 1);

  State::Save(slot);
  return 0;
}

static int loadState(lua_State* L)
{
  int slot = lua_tointeger(L, 1);

  State::Load(slot);
  return 0;
}

static int getAnalog(lua_State* L)
{
  lua_pushinteger(L, LuaScriptFrame::GetCurrentInstance()->GetLastPadStatus().stickX);
  lua_pushinteger(L, LuaScriptFrame::GetCurrentInstance()->GetLastPadStatus().stickY);

  return 2;
}

static int setAnalog(lua_State* L)
{
  if (lua_gettop(L) != 2)
  {
    return luaL_error(
        L, "Incorrect # of arguments passed to setAnalog. setAnalog expects two arguments/n");
  }

  u8 x_pos = lua_tointeger(L, 1);
  u8 y_pos = lua_tointeger(L, 2);

  LuaScriptFrame::GetCurrentInstance()->GetPadStatus().stickX = x_pos;
  LuaScriptFrame::GetCurrentInstance()->GetPadStatus().stickY = y_pos;

  return 0;
}

// Same thing as setAnalog except it takes polar coordinates
// Must use an m int the range [0, 128)
static int setAnalogPolar(lua_State* L)
{
  int m = lua_tointeger(L, 1);
  if (m < 0 || m >= 128)
  {
    return luaL_error(L, "m is outside of acceptable range [0, 128)");
  }

  // Gotta convert theta to radians
  double theta = lua_tonumber(L, 2) * M_PI / 180.0;

  // Round to the nearest whole number, then subtract 128 so that our
  //"origin" is the stick in neutral position.
  LuaScriptFrame::GetCurrentInstance()->GetPadStatus().stickX = static_cast<u8>(floor(m * cos(theta)) + 128);
  LuaScriptFrame::GetCurrentInstance()->GetPadStatus().stickY = static_cast<u8>(floor(m * sin(theta)) + 128);

  return 0;
}

static int getCStick(lua_State* L)
{
  lua_pushinteger(L, LuaScriptFrame::GetCurrentInstance()->GetLastPadStatus().substickX);
  lua_pushinteger(L, LuaScriptFrame::GetCurrentInstance()->GetLastPadStatus().substickY);

  return 2;
}

static int setCStick(lua_State* L)
{
  LuaScriptFrame::GetCurrentInstance()->GetPadStatus().substickX = lua_tointeger(L, 1);
  LuaScriptFrame::GetCurrentInstance()->GetPadStatus().substickY = lua_tointeger(L, 2);

  return 0;
}

static int setCStickPolar(lua_State* L)
{
  int m = lua_tointeger(L, 1);
  if (m < 0 || m >= 128)
  {
    return luaL_error(L, "m is outside of acceptable range [0, 128)");
  }

  int theta = lua_tointeger(L, 2);

  // Convert theta to radians
  theta = theta * M_PI / 180.0;

  LuaScriptFrame::GetCurrentInstance()->GetPadStatus().substickX = (u8)(floor(m * cos(theta)) + 128);
  LuaScriptFrame::GetCurrentInstance()->GetPadStatus().substickY = (u8)(floor(m * sin(theta)) + 128);

  return 0;
}

static int getButtons(lua_State* L)
{
  lua_pushinteger(L, LuaScriptFrame::GetCurrentInstance()->GetLastPadStatus().button);
  return 1;
}

// Each button passed in is set to pressed.
static int setButtons(lua_State* L)
{
  const char* s = lua_tostring(L, 1);

  for (size_t i = 0; i < strlen(s); i++)
  {
    if (s[i] == 'U')
    {
      LuaScriptFrame::GetCurrentInstance()->GetPadStatus().button = 0;
      break;
    }

    switch (s[i])
    {
    case 'A':
      LuaScriptFrame::GetCurrentInstance()->GetPadStatus().button |= PadButton::PAD_BUTTON_A;
      break;
    case 'B':
      LuaScriptFrame::GetCurrentInstance()->GetPadStatus().button |= PadButton::PAD_BUTTON_B;
      break;
    case 'X':
      LuaScriptFrame::GetCurrentInstance()->GetPadStatus().button |= PadButton::PAD_BUTTON_X;
      break;
    case 'Y':
      LuaScriptFrame::GetCurrentInstance()->GetPadStatus().button |= PadButton::PAD_BUTTON_Y;
      break;
    case 'S':
      LuaScriptFrame::GetCurrentInstance()->GetPadStatus().button |= PadButton::PAD_BUTTON_START;
      break;
    case 'Z':
      LuaScriptFrame::GetCurrentInstance()->GetPadStatus().button |= PadButton::PAD_TRIGGER_Z;
      break;
    }
  }

  return 0;
}

static int setDPad(lua_State* L)
{
  const char* str = lua_tostring(L, 1);

  for (size_t i = 0; i < strlen(str); i++)
  {
    switch (str[i])
    {
    case 'U':
      LuaScriptFrame::GetCurrentInstance()->GetPadStatus().button |= PadButton::PAD_BUTTON_UP;
      break;
    case 'D':
      LuaScriptFrame::GetCurrentInstance()->GetPadStatus().button |= PadButton::PAD_BUTTON_DOWN;
      break;
    case 'L':
      LuaScriptFrame::GetCurrentInstance()->GetPadStatus().button |= PadButton::PAD_BUTTON_LEFT;
      break;
    case 'R':
      LuaScriptFrame::GetCurrentInstance()->GetPadStatus().button |= PadButton::PAD_BUTTON_RIGHT;
      break;
    }
  }

  return 0;
}

static int setEmulatorSpeed(lua_State* L)
{
  SConfig::GetInstance().m_EmulationSpeed = lua_tonumber(L, 1) * 0.01f;

  return 0;
}

static int getTriggers(lua_State* L)
{
  lua_pushinteger(L, LuaScriptFrame::GetCurrentInstance()->GetLastPadStatus().triggerLeft);
  lua_pushinteger(L, LuaScriptFrame::GetCurrentInstance()->GetLastPadStatus().triggerRight);

  return 2;
}

static int setTriggers(lua_State* L)
{
  LuaScriptFrame::GetCurrentInstance()->GetPadStatus().triggerLeft = lua_tointeger(L, 1);
  LuaScriptFrame::GetCurrentInstance()->GetPadStatus().triggerRight = lua_tointeger(L, 2);

  return 0;
}

int luaopen_libs(lua_State* L)
{
  static const luaL_Reg bit[] =
  {
    {"band", band},
    {nullptr, nullptr}
  };

  static const luaL_Reg client[] =
  {
    {"print", printToTextCtrl },
    {nullptr, nullptr}
  };

  static const luaL_Reg emu[] =
  {
    {"frameAdvance", frameAdvance },
    {"getFrameCount", getFrameCount },
    {"getMovieLength", getMovieLength },
    {"softReset", softReset },
    {"saveState", saveState },
    {"loadState", loadState },
    {"setEmulatorSpeed", setEmulatorSpeed },
    {nullptr, nullptr}
  };

  static const luaL_Reg joypad[] =
  {
    {"getAnalog", getAnalog },
    {"setAnalog", setAnalog },
    {"setAnalogPolar", setAnalogPolar },
    {"getCStick", getCStick },
    {"setCStick", setCStick },
    {"setCStickPolar", setCStickPolar },
    {"getButtons", getButtons },
    {"setButtons", setButtons },
    {"setDPad", setDPad },
    {"getTriggers", getTriggers },
    {"setTriggers", setTriggers },
    {nullptr, nullptr}
  };

  luaL_newlib(L, bit);
  lua_setglobal(L, "bit");
  luaL_newlib(L, client);
  lua_setglobal(L, "client");
  luaL_newlib(L, emu);
  lua_setglobal(L, "emu");
  luaL_newlib(L, joypad);
  lua_setglobal(L, "joypad");
  return 1;
}
#pragma endregion
}  // namespace Lua
