// Copyright 2015 Dolphin Emulator Project
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
#include "LuaScriptFrame.h"
#include "Frame.h"

LuaScriptFrame::LuaScriptFrame(wxWindow* parent) : wxFrame(parent, wxID_ANY, _("Lua Console"), wxDefaultPosition, wxSize(431, 397), wxDEFAULT_FRAME_STYLE ^ wxRESIZE_BORDER)
{
  CreateGUI();
  SetIcons(WxUtils::GetDolphinIconBundle());

  Center();
  Show();
}

LuaScriptFrame::~LuaScriptFrame()
{
  // Disconnect Events
  this->Disconnect(wxID_ANY, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(LuaScriptFrame::OnExitClicked));
  Browse->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(LuaScriptFrame::BrowseOnButtonClick), NULL, this);
  run_button->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(LuaScriptFrame::RunOnButtonClick), NULL, this);
  stop_button->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(LuaScriptFrame::StopOnButtonClick), NULL, this);
}

void LuaScriptFrame::CreateGUI()
{
  //Get size of this frame
  //int frame_width, frame_height;
  //this->GetSize(&frame_width, &frame_height);

  this->SetSizeHints(wxDefaultSize, wxDefaultSize);

  m_menubar = new wxMenuBar(0);
  m_menu = new wxMenu();
  wxMenuItem* exit;
  exit = new wxMenuItem(m_menu, wxID_ANY, wxString(wxT("Exit")), wxEmptyString, wxITEM_NORMAL);
  m_menu->Append(exit);

  m_menubar->Append(m_menu, wxT("File"));

  this->SetMenuBar(m_menubar);

  wxBoxSizer* main_sizer;
  main_sizer = new wxBoxSizer(wxVERTICAL);

  script_file_label = new wxStaticText(this, wxID_ANY, wxT("Script File:"), wxDefaultPosition, wxDefaultSize, 0);
  script_file_label->Wrap(-1);
  main_sizer->Add(script_file_label, 0, wxALL, 5);

  m_textCtrl1 = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(300, -1), 0);
  m_textCtrl1->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));

  main_sizer->Add(m_textCtrl1, 0, wxALL, 5);

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
  this->Connect(exit->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(LuaScriptFrame::OnExitClicked));
  Browse->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(LuaScriptFrame::BrowseOnButtonClick), NULL, this);
  run_button->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(LuaScriptFrame::RunOnButtonClick), NULL, this);
  stop_button->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(LuaScriptFrame::StopOnButtonClick), NULL, this);
}

void LuaScriptFrame::Log(const char* message)
{
  output_console->AppendText(message);
}

int LuaScriptFrame::howdy(lua_State* state)
{
  // The number of function arguments will be on top of the stack.
  int args = lua_gettop(state);
  char buffer[512];
  itoa(args, buffer, 10);

  Log("howdy() was called with ");
  Log(buffer);
  Log(" arguments.\n");

  for (int n = 1; n <= args; ++n) {
    printf("  argument %d: '%s'\n", n, lua_tostring(state, n));
  }

  // Push the return value on top of the stack. NOTE: We haven't popped the
  // input arguments to our function. To be honest, I haven't checked if we
  // must, but at least in stack machines like the JVM, the stack will be
  // cleaned between each function call.

  lua_pushnumber(state, 123);

  // Let Lua know how many return values we've passed
  return 1;
}

void LuaScriptFrame::OnExitClicked(wxCommandEvent& event)
{

}

void LuaScriptFrame::BrowseOnButtonClick(wxCommandEvent &event)
{
  wxFileDialog* dialog = new wxFileDialog(this, _("Select Lua script."));

  if (dialog->ShowModal() == wxID_CANCEL)
    return;

  m_textCtrl1->SetValue(dialog->GetPath());
  dialog->Destroy();
}

void LuaScriptFrame::RunOnButtonClick(wxCommandEvent& event)
{
  lua_State* state = luaL_newstate();

  //Make standard libraries available to loaded script
  luaL_openlibs(state);

  //Register additinal functions with Lua
  lua_register(state, "howdy", howdy);

  if (luaL_loadfile(state, m_textCtrl1->GetValue()) != LUA_OK)
  {
    Log("Error opening file.\n");
    return;
  }

  if (lua_pcall(state, 0, LUA_MULTRET, 0) != LUA_OK)
  {
    Log("Error running file.\n");
    return;
  }
}

void LuaScriptFrame::StopOnButtonClick(wxCommandEvent& event)
{

}
