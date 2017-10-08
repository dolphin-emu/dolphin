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

int printToTextCtrl(lua_State* L);
int frameAdvance(lua_State* L);
int getFrameCount(lua_State* L);
int getAnalog(lua_State* L);
int setAnalog(lua_State* L);
int getButtons(lua_State* L);
int setButtons(lua_State* L);

// GLOBAL IS NECESSARY FOR LOG TO WORK
LuaScriptFrame* currentWindow;

//External symbol
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
    registered_functions->insert(std::pair<const char*, LuaFunction>("getAnalog", getAnalog));
    registered_functions->insert(std::pair<const char*, LuaFunction>("setAnalog", setAnalog));
    registered_functions->insert(std::pair<const char*, LuaFunction>("getButtons", getButtons));
    registered_functions->insert(std::pair<const char*, LuaFunction>("setButtons", setButtons));
  }
}

LuaScriptFrame::~LuaScriptFrame()
{
  // Disconnect Events
  this->Disconnect(wxID_ANY, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(LuaScriptFrame::OnClearClicked));
  Browse->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(LuaScriptFrame::BrowseOnButtonClick), NULL, this);
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
  m_menu = new wxMenu();
  wxMenuItem* clear;
  clear = new wxMenuItem(m_menu, wxID_ANY, wxString(wxT("Clear")), wxEmptyString, wxITEM_NORMAL);
  m_menu->Append(clear);

  m_menubar->Append(m_menu, wxT("Console"));

  this->SetMenuBar(m_menubar);

  wxBoxSizer* main_sizer;
  main_sizer = new wxBoxSizer(wxVERTICAL);

  script_file_label = new wxStaticText(this, wxID_ANY, wxT("Script File:"), wxDefaultPosition, wxDefaultSize, 0);
  script_file_label->Wrap(-1);
  main_sizer->Add(script_file_label, 0, wxALL, 5);

  file_path = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(300, -1), 0);
  file_path->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));

  main_sizer->Add(file_path, 0, wxALL, 5);

  wxBoxSizer* buttons;
  buttons = new wxBoxSizer(wxHORIZONTAL);

  Browse = new wxButton(this, wxID_ANY, wxT("Browse..."), wxPoint(-1, -1), wxDefaultSize, 0);
  buttons->Add(Browse, 0, wxALL, 5);

  run_button = new wxButton(this, wxID_ANY, wxT("Run"), wxDefaultPosition, wxDefaultSize, 0);
  buttons->Add(run_button, 0, wxALL, 5);

  stop_button = new wxButton(this, wxID_ANY, wxT("Stop"), wxDefaultPosition, wxDefaultSize, 0);
  buttons->Add(stop_button, 0, wxALL, 5);


  main_sizer->Add(buttons, 1, wxEXPAND, 5);

  m_staticText2 = new wxStaticText(this, wxID_ANY, wxT("Output Console:"), wxDefaultPosition, wxDefaultSize, 0);
  m_staticText2->Wrap(-1);
  main_sizer->Add(m_staticText2, 0, wxALL, 5);

  output_console = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(415, 200), wxHSCROLL | wxTE_MULTILINE | wxTE_READONLY);
  main_sizer->Add(output_console, 0, wxALL, 6);


  this->SetSizer(main_sizer);
  this->Layout();

  this->Centre(wxBOTH);

  // Connect Events
  this->Connect(clear->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(LuaScriptFrame::OnClearClicked));
  Browse->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(LuaScriptFrame::BrowseOnButtonClick), NULL, this);
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

void LuaScriptFrame::SignalThreadFinished()
{
  lua_thread = nullptr;
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

  status->button |= pad_status->button;

  //TRYING THIS ONE THING
  //clearPad(pad_status);
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
    currentWindow->Log("Incorrect # of parameters passed to setAnalog.\n");
    return 0;
  }

  u8 xPos = lua_tointeger(L, 1);
  u8 yPos = lua_tointeger(L, 2);

  pad_status->stickX = xPos;
  pad_status->stickY = yPos;

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
    }
  }

  return 0;
}
#pragma endregion
