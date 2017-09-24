// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <string>
#include <wx/frame.h>

class GameListCtrl;
class wxCheckBox;
class wxChoice;
class wxListBox;
class wxNotebook;
class wxSpinCtrl;
class wxStaticText;
class wxTextCtrl;

class LuaScriptFrame final : public wxFrame
{
public:
  LuaScriptFrame(wxWindow* parent);
  ~LuaScriptFrame();

private:
  void CreateGUI();
  void OnKeyDown(wxKeyEvent& event);
};
