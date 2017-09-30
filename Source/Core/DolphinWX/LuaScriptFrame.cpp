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
#include "DolphinWX/WxUtils.h"
#include "LuaScriptFrame.h"

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
  wxPanel* const panel = new wxPanel(this);
  panel->Bind(wxEVT_CLOSE_WINDOW, &LuaScriptFrame::OnClose, this);

  wxMenu* file = new wxMenu;

  wxMenuBar* menubar = new wxMenuBar;
  menubar->Append(file, "File");


  wxButton* const load_script = new wxButton(panel, wxID_ANY, _("Load Script"));
  load_script->Bind(wxEVT_BUTTON, &LuaScriptFrame::OnLoad, this);

  SetMenuBar(menubar);
}

void LuaScriptFrame::OnClose(wxCloseEvent& event)
{
  Destroy();
}

void LuaScriptFrame::OnLoad(wxEvent& event)
{

}
