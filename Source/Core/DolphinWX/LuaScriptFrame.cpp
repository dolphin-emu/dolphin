// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <string>
#include <wx/frame.h>
#include <wx/panel.h>
#include <wx/menu.h>
#include <wx/button.h>
#include <wx/textctrl.h>
#include <wx/sizer.h>
#include "DolphinWX/WxUtils.h"
#include "DolphinWX/Main.h"
#include "Frame.h"
#include "Core\Core.h"
#include "Core\HW\GCPad.h"
#include "Core\HW\GCPadEmu.h"
#include "InputCommon\InputConfig.h"
#include "Core\Movie.h"
#include "LuaScripting.h"

namespace Lua
{
  int printToTextCtrl(lua_State* L);
  int frameAdvance(lua_State* L);
  int getFrameCount(lua_State* L);
  int getMovieLength(lua_State* L);
  int softReset(lua_State* L);

  // Slots from 0-99.
  int saveState(lua_State* L);
  int loadState(lua_State* L);

  //The argument should be a percentage of speed. I.E. setEmulatorSpeed(100) would set it to normal playback speed
  int setEmulatorSpeed(lua_State* L);

  int getAnalog(lua_State* L);
  int setAnalog(lua_State* L);
  int setAnalogPolar(lua_State* L);
  int getCStick(lua_State* L);
  int setCStick(lua_State* L);
  int setCStickPolar(lua_State* L);
  int getButtons(lua_State* L);
  int setButtons(lua_State* L);
  int setDPad(lua_State* L);
  int getTriggers(lua_State* L);
  int setTriggers(lua_State* L);

  // GLOBAL IS NECESSARY FOR LOG TO WORK
  LuaScriptFrame* currentWindow;

  //External symbols
  std::map<const char*, LuaFunction>* registered_functions;
  std::mutex lua_mutex;

  LuaScriptFrame::LuaScriptFrame(wxWindow* parent) : wxFrame(parent, wxID_ANY, _("Lua Console"), wxDefaultPosition, wxSize(431, 397), wxDEFAULT_FRAME_STYLE ^ wxRESIZE_BORDER)
  {
    CreateGUI();
    SetIcons(WxUtils::GetDolphinIconBundle());

    Center();
    Show();

    currentWindow = this;
    lua_thread = nullptr;

    //Create function map if it doesn't already exist
    if (!registered_functions)
    {
      registered_functions = new std::map<const char*, LuaFunction>;

      registered_functions->insert(std::pair<const char*, LuaFunction>("print", printToTextCtrl));
      registered_functions->insert(std::pair<const char*, LuaFunction>("frameAdvance", frameAdvance));
      registered_functions->insert(std::pair<const char*, LuaFunction>("getFrameCount", getFrameCount));
      registered_functions->insert(std::pair<const char*, LuaFunction>("getMovieLength", getMovieLength));
      registered_functions->insert(std::pair<const char*, LuaFunction>("softReset", softReset));
      registered_functions->insert(std::pair<const char*, LuaFunction>("saveState", saveState));
      registered_functions->insert(std::pair<const char*, LuaFunction>("loadState", loadState));
      registered_functions->insert(std::pair<const char*, LuaFunction>("setEmulatorSpeed", setEmulatorSpeed));
      registered_functions->insert(std::pair<const char*, LuaFunction>("getAnalog", getAnalog));
      registered_functions->insert(std::pair<const char*, LuaFunction>("setAnalog", setAnalog));
      registered_functions->insert(std::pair<const char*, LuaFunction>("getCStick", getCStick));
      registered_functions->insert(std::pair<const char*, LuaFunction>("setCStick", setCStick));
      registered_functions->insert(std::pair<const char*, LuaFunction>("setCStickPolar", setCStickPolar));
      registered_functions->insert(std::pair<const char*, LuaFunction>("setAnalogPolar", setAnalogPolar));
      registered_functions->insert(std::pair<const char*, LuaFunction>("getButtons", getButtons));
      registered_functions->insert(std::pair<const char*, LuaFunction>("setButtons", setButtons));
      registered_functions->insert(std::pair<const char*, LuaFunction>("setDPad", setDPad));
      registered_functions->insert(std::pair<const char*, LuaFunction>("getTriggers", getTriggers));
      registered_functions->insert(std::pair<const char*, LuaFunction>("setTriggers", setTriggers));
    }
  }

  LuaScriptFrame::~LuaScriptFrame()
  {
    // Disconnect Events
    this->Disconnect(wxID_ANY, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(LuaScriptFrame::OnClearClicked));
    browse_button->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(LuaScriptFrame::BrowseOnButtonClick), NULL, this);
    run_button->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(LuaScriptFrame::RunOnButtonClick), NULL, this);
    stop_button->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(LuaScriptFrame::StopOnButtonClick), NULL, this);

    //Stop currently executing Lua thread
    if (lua_thread != nullptr && lua_thread->IsRunning())
    {
      lua_thread->Kill();
      lua_thread = nullptr;
    }

    main_frame->m_lua_script_frame = nullptr;

    //Nullify GC manipulator function to prevent crash when lua console is closed
    Movie::s_gc_manip_funcs[Movie::GCManipIndex::LuaGCManip] = nullptr;
  }


  //
  //CreateGUI
  //
  //Creates actual Lua console window.
  //
  void LuaScriptFrame::CreateGUI()
  {
    this->SetSizeHints(wxDefaultSize, wxDefaultSize);

    m_menubar = new wxMenuBar(0);
    console_menu = new wxMenu();
    clear = new wxMenuItem(console_menu, wxID_ANY, wxString(_("Clear")), wxEmptyString, wxITEM_NORMAL);
    console_menu->Append(clear);
    help_menu = new wxMenu();
    documentation = new wxMenuItem(help_menu, wxID_ANY, wxString(_("Lua Documentation")), wxEmptyString, wxITEM_NORMAL);
    api = new wxMenuItem(help_menu, wxID_ANY, wxString(_("Dolphin Lua API")), wxEmptyString, wxITEM_NORMAL);
    help_menu->Append(documentation);
    help_menu->Append(api);

    m_menubar->Append(console_menu, _("Console"));
    m_menubar->Append(help_menu, _("Help"));

    this->SetMenuBar(m_menubar);

    wxBoxSizer* main_sizer;
    main_sizer = new wxBoxSizer(wxVERTICAL);

    script_file_label = new wxStaticText(this, wxID_ANY, _("Script File:"), wxDefaultPosition, wxDefaultSize, 0);
    script_file_label->Wrap(-1);
    main_sizer->Add(script_file_label, 0, wxALL, 5);

    file_path = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(300, -1), 0);
    file_path->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));

    main_sizer->Add(file_path, 0, wxALL, 5);

    wxBoxSizer* buttons;
    buttons = new wxBoxSizer(wxHORIZONTAL);

    browse_button = new wxButton(this, wxID_ANY, _("Browse..."), wxPoint(-1, -1), wxDefaultSize, 0);
    buttons->Add(browse_button, 0, wxALL, 5);

    run_button = new wxButton(this, wxID_ANY, _("Run"), wxDefaultPosition, wxDefaultSize, 0);
    buttons->Add(run_button, 0, wxALL, 5);

    stop_button = new wxButton(this, wxID_ANY, _("Stop"), wxDefaultPosition, wxDefaultSize, 0);
    buttons->Add(stop_button, 0, wxALL, 5);


    main_sizer->Add(buttons, 1, wxEXPAND, 5);

    output_console_literal = new wxStaticText(this, wxID_ANY, _("Output Console:"), wxDefaultPosition, wxDefaultSize, 0);
    output_console_literal->Wrap(-1);
    main_sizer->Add(output_console_literal, 0, wxALL, 5);

    output_console = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(415, 200), wxHSCROLL | wxTE_MULTILINE | wxTE_READONLY);
    main_sizer->Add(output_console, 0, wxALL, 6);


    this->SetSizer(main_sizer);
    this->Layout();

    this->Centre(wxBOTH);

    // Connect Events
    this->Connect(clear->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(LuaScriptFrame::OnClearClicked));
    this->Connect(documentation->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(LuaScriptFrame::OnDocumentationClicked));
    this->Connect(api->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(LuaScriptFrame::OnAPIClicked));
    browse_button->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(LuaScriptFrame::BrowseOnButtonClick), NULL, this);
    run_button->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(LuaScriptFrame::RunOnButtonClick), NULL, this);
    stop_button->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(LuaScriptFrame::StopOnButtonClick), NULL, this);
  }

  void LuaScriptFrame::Log(const char* message)
  {
    output_console->AppendText(message);
  }

  void LuaScriptFrame::OnClearClicked(wxCommandEvent& event)
  {
    output_console->Clear();
  }

  void LuaScriptFrame::OnDocumentationClicked(wxCommandEvent& event)
  {
    wxLaunchDefaultBrowser("https://www.lua.org/pil/contents.html");
  }

  void LuaScriptFrame::OnAPIClicked(wxCommandEvent& event)
  {
    wxLaunchDefaultBrowser("https://github.com/NickDriscoll/dolphin/blob/Lua_scripting/Lua.md");
  }

  void LuaScriptFrame::BrowseOnButtonClick(wxCommandEvent &event)
  {
    wxFileDialog* dialog = new wxFileDialog(this, _("Select Lua script."));

    if (dialog->ShowModal() == wxID_CANCEL)
      return;

    file_path->SetValue(dialog->GetPath());
    dialog->Destroy();
  }

  void LuaScriptFrame::RunOnButtonClick(wxCommandEvent& event)
  {
    if (lua_thread == nullptr)
    {
      lua_thread = new LuaThread(this, file_path->GetValue());
      lua_thread->Run();
    }
  }

  void LuaScriptFrame::StopOnButtonClick(wxCommandEvent& event)
  {
    //Kill current Lua thread
    if (lua_thread != nullptr && lua_thread->IsRunning())
    {
      lua_thread->Kill();
      lua_thread = nullptr;
    }
  }

  // The callback function that tells the emulator what to actually press
  void LuaScriptFrame::GetValues(GCPadStatus* status)
  {
    // We lock here to prevent an issue where pad_status could be deleted during the
    // middle of this function's execution
    std::unique_lock<std::mutex> lock(lua_mutex);

    if (lua_thread == nullptr)
      return;

    if (pad_status->stickX != GCPadStatus::MAIN_STICK_CENTER_X)
      status->stickX = pad_status->stickX;

    if (pad_status->stickY != GCPadStatus::MAIN_STICK_CENTER_Y)
      status->stickY = pad_status->stickY;

    if (pad_status->triggerLeft != 0)
      status->triggerLeft = pad_status->triggerLeft;

    if (pad_status->triggerRight != 0)
      status->triggerRight = pad_status->triggerRight;

    if (pad_status->substickX != GCPadStatus::C_STICK_CENTER_X)
      status->substickX = pad_status->substickX;

    if (pad_status->substickY != GCPadStatus::C_STICK_CENTER_Y)
      status->substickY = pad_status->substickY;

    status->button |= pad_status->button;
  }

  void LuaScriptFrame::NullifyLuaThread()
  {
    lua_thread = nullptr;
  }

  // Sets status to the default values
  void clearPad(GCPadStatus* status)
  {
    status->button = 0;
    status->stickX = GCPadStatus::MAIN_STICK_CENTER_X;
    status->stickY = GCPadStatus::MAIN_STICK_CENTER_Y;
    status->triggerLeft = 0;
    status->triggerRight = 0;
    status->analogA = GCPadStatus::C_STICK_CENTER_X;
    status->analogB = GCPadStatus::C_STICK_CENTER_X;
  }

  //Functions to register with Lua
#pragma region Lua_Functs

// Prints a string to the text control of this frame
  int printToTextCtrl(lua_State* L)
  {
    currentWindow->Log(lua_tostring(L, 1));
    currentWindow->Log("\n");

    return 0;
  }

  // Steps a frame if the emulator is paused, pauses it otherwise.
  int frameAdvance(lua_State* L)
  {
    u64 currentFrame = Movie::GetCurrentFrame();
    Core::DoFrameStep();

    //Block until a frame has actually processed
    //Prevents a script from executing it's main loop more than once per frame.
    while (currentFrame == Movie::GetCurrentFrame());
    return 0;
  }

  int getFrameCount(lua_State* L)
  {
    lua_pushinteger(L, Movie::GetCurrentFrame());
    return 1;
  }

  int getMovieLength(lua_State *L)
  {
    if (Movie::IsMovieActive())
    {
      lua_pushinteger(L, Movie::GetTotalFrames());
    }
    else
    {
      luaL_error(L, "No active movie.");
    }
    return 1;
  }

  int softReset(lua_State* L)
  {
    ProcessorInterface::ResetButton_Tap();
    return 0;
  }

  int saveState(lua_State* L)
  {
    int slot = lua_tointeger(L, 1);

    State::Save(slot);
    return 0;
  }

  int loadState(lua_State* L)
  {
    int slot = lua_tointeger(L, 1);

    State::Load(slot);
    return 0;
  }

  int getAnalog(lua_State* L)
  {
    lua_pushinteger(L, pad_status->stickX);
    lua_pushinteger(L, pad_status->stickY);

    return 2;
  }

  int setAnalog(lua_State* L)
  {
    if (lua_gettop(L) != 2)
    {
      luaL_error(L, "Incorrect # of parameters passed to setAnalog.\n");
    }

    u8 xPos = lua_tointeger(L, 1);
    u8 yPos = lua_tointeger(L, 2);

    pad_status->stickX = xPos;
    pad_status->stickY = yPos;

    return 0;
  }

  //Same thing as setAnalog except it takes polar coordinates
  //Must use an m int the range [0, 128)
  int setAnalogPolar(lua_State* L)
  {
    int m = lua_tointeger(L, 1);
    if (m < 0 || m >= 128)
    {
      luaL_error(L, "m is outside of acceptable range [0, 128)");
    }

    //Gotta convert theta to radians
    double theta = lua_tonumber(L, 2) * M_PI / 180.0;

    //Round to the nearest whole number, then subtract 128 so that our
    //"origin" is the stick in neutral position.
    pad_status->stickX = (u8)(floor(m * cos(theta)) + 128);
    pad_status->stickY = (u8)(floor(m * sin(theta)) + 128);

    return 0;
  }

  int getCStick(lua_State* L)
  {
    lua_pushinteger(L, pad_status->substickX);
    lua_pushinteger(L, pad_status->substickY);

    return 2;
  }

  int setCStick(lua_State* L)
  {
    pad_status->substickX = lua_tointeger(L, 1);
    pad_status->substickY = lua_tointeger(L, 2);

    return 0;
  }

  int setCStickPolar(lua_State* L)
  {
    int m = lua_tointeger(L, 1);
    if (m < 0 || m >= 128)
    {
      luaL_error(L, "m is outside of acceptable range [0, 128)");
    }

    int theta = lua_tointeger(L, 2);

    //Convert theta to radians
    theta = theta * M_PI / 180.0;

    pad_status->substickX = (u8)(floor(m * cos(theta)) + 128);
    pad_status->substickY = (u8)(floor(m * sin(theta)) + 128);

    return 0;
  }

  int getButtons(lua_State* L)
  {
    lua_pushinteger(L, pad_status->button);
    return 1;
  }

  //Each button passed in is set to pressed.
  int setButtons(lua_State* L)
  {
    const char* s = lua_tostring(L, 1);

    for (int i = 0; i < strlen(s); i++)
    {
      if (s[i] == 'U')
      {
        pad_status->button = 0;
        break;
      }

      switch (s[i])
      {
      case 'A':
        pad_status->button |= PadButton::PAD_BUTTON_A;
        break;
      case 'B':
        pad_status->button |= PadButton::PAD_BUTTON_B;
        break;
      case 'X':
        pad_status->button |= PadButton::PAD_BUTTON_X;
        break;
      case 'Y':
        pad_status->button |= PadButton::PAD_BUTTON_Y;
        break;
      case 'S':
        pad_status->button |= PadButton::PAD_BUTTON_START;
        break;
      case 'Z':
        pad_status->button |= PadButton::PAD_TRIGGER_Z;
        break;
      }
    }

    return 0;
  }

  int setDPad(lua_State* L)
  {
    const char* str = lua_tostring(L, 1);

    for (int i = 0; i < strlen(str); i++)
    {
      switch (str[i])
      {
      case 'U':
        pad_status->button |= PadButton::PAD_BUTTON_UP;
        break;
      case 'D':
        pad_status->button |= PadButton::PAD_BUTTON_DOWN;
        break;
      case 'L':
        pad_status->button |= PadButton::PAD_BUTTON_LEFT;
        break;
      case 'R':
        pad_status->button |= PadButton::PAD_BUTTON_RIGHT;
        break;
      }
    }

    return 0;
  }

  int setEmulatorSpeed(lua_State* L)
  {
    SConfig::GetInstance().m_EmulationSpeed = lua_tonumber(L, 1) * 0.01f;

    return 0;
  }

  int getTriggers(lua_State* L)
  {
    lua_pushinteger(L, pad_status->triggerLeft);
    lua_pushinteger(L, pad_status->triggerRight);

    return 2;
  }

  int setTriggers(lua_State* L)
  {
    pad_status->triggerLeft = lua_tointeger(L, 1);
    pad_status->triggerRight = lua_tointeger(L, 2);

    return 0;
  }
#pragma endregion
}
