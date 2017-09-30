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

LuaScriptFrame::LuaScriptFrame(wxWindow* parent) : wxFrame(parent, wxID_ANY, _("Lua Console"))
{
  CreateGUI();
  SetIcons(WxUtils::GetDolphinIconBundle());

  Center();
  Show();
}

LuaScriptFrame::~LuaScriptFrame()
{

}

void LuaScriptFrame::CreateGUI()
{
  /*
  wxPanel* const panel = new wxPanel(this);
  panel->Bind(wxEVT_CLOSE_WINDOW, &LuaScriptFrame::OnClose, this);

  wxMenu* file = new wxMenu;
  wxMenuItem* load = new wxMenuItem(file, wxID_ANY, _("Load Script"));
  file->Append(load);

  wxMenuBar* menubar = new wxMenuBar;
  menubar->Append(file, "File");

  SetMenuBar(menubar);
  */

  this->SetSizeHints(wxDefaultSize, wxDefaultSize);

  m_menubar1 = new wxMenuBar(0);
  this->SetMenuBar(m_menubar1);

  m_menu1 = new wxMenu();
  wxMenuItem* file;
  file = new wxMenuItem(m_menu1, wxID_ANY, wxString(wxT("File")), wxEmptyString, wxITEM_NORMAL);
  m_menu1->Append(file);

  this->Connect(wxEVT_RIGHT_DOWN, wxMouseEventHandler(LuaScriptFrame::MyFrame1OnContextMenu), NULL, this);

  wxBoxSizer* bSizer1;
  bSizer1 = new wxBoxSizer(wxVERTICAL);

  m_panel1 = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
  bSizer1->Add(m_panel1, 1, wxEXPAND | wxALL, 5);

  script_file_label = new wxStaticText(this, wxID_ANY, wxT("Script File:"), wxDefaultPosition, wxDefaultSize, 0);
  script_file_label->Wrap(-1);
  bSizer1->Add(script_file_label, 0, wxALL, 5);

  m_textCtrl1 = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(300, -1), 0);
  bSizer1->Add(m_textCtrl1, 0, wxALL, 5);

  Browse = new wxButton(this, wxID_ANY, wxT("Browse..."), wxPoint(-1, -1), wxDefaultSize, 0);
  bSizer1->Add(Browse, 0, wxALL, 5);


  this->SetSizer(bSizer1);
  this->Layout();

  this->Centre(wxBOTH);

  // Connect Events
  Browse->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(LuaScriptFrame::BrowseOnButtonClick), NULL, this);
}

void LuaScriptFrame::OnClose(wxCloseEvent& event)
{

}

void LuaScriptFrame::OnLoad(wxEvent& event)
{

}
