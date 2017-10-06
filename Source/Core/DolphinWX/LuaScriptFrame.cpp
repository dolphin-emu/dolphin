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
#include "LuaScripting.h"

// GLOBAL IS NECESSARY FOR LOG TO WORK
LuaScriptFrame* currentWindow;

LuaScriptFrame::LuaScriptFrame(wxWindow* parent) : wxFrame(parent, wxID_ANY, _("Lua Console"), wxDefaultPosition, wxSize(431, 397), wxDEFAULT_FRAME_STYLE ^ wxRESIZE_BORDER)
{
  CreateGUI();
  SetIcons(WxUtils::GetDolphinIconBundle());

  Center();
  Show();

  currentWindow = this;
  lua_thread = nullptr;

  //Initialize virtual controller
  pad_status = (GCPadStatus*)malloc(sizeof(GCPadStatus));
  pad_status->button = 0;
  pad_status->stickX = GCPadStatus::MAIN_STICK_CENTER_X;
  pad_status->stickY = GCPadStatus::MAIN_STICK_CENTER_Y;
  pad_status->triggerLeft = 0;
  pad_status->triggerRight = 0;
  pad_status->analogA = GCPadStatus::C_STICK_CENTER_X;
  pad_status->analogB = GCPadStatus::C_STICK_CENTER_X;
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

  //Free pad
  free(pad_status);
  pad_status = nullptr;
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

void LuaScriptFrame::GetValues(GCPadStatus* status)
{
  if (lua_thread == nullptr)
    return;

  status->stickX = pad_status->stickX;
  status->stickY = pad_status->stickY;
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
  Core::DoFrameStep();
  return 0;
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

  currentWindow->pad_status->stickX = xPos;
  currentWindow->pad_status->stickY = yPos;

  return 0;
}
#pragma endregion
