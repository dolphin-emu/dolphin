// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <string>
#include <wx/frame.h>
#include <wx/panel.h>
#include "LuaScriptFrame.h"

LuaScriptFrame::LuaScriptFrame(wxWindow* parent) : wxFrame(parent, wxID_ANY, _("Lua Console"))
{
  CreateGUI();

}

LuaScriptFrame::~LuaScriptFrame()
{

}

void LuaScriptFrame::CreateGUI()
{
  wxPanel* const panel = new wxPanel(this);
  panel->Bind(wxEVT_CHAR_HOOK, &LuaScriptFrame::OnKeyDown, this);
}

void LuaScriptFrame::OnKeyDown(wxKeyEvent& event)
{

}
