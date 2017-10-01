// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wx/artprov.h>
#include <wx/string.h>
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/icon.h>
#include <wx/menu.h>
#include <wx/gdicmn.h>
#include <wx/font.h>
#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/frame.h>
#include <wx/filedlg.h>

//Lua include stuff
#ifdef __cplusplus
#include <lua.hpp>
#else
#include <lua.h>
#include <lualib.h>
#include <laux.h>
#endif

class LuaScriptFrame final : public wxFrame
{
private:
  void CreateGUI();
  void Log(const char* message);
  int howdy(lua_State* state);
  void OnExitClicked(wxCommandEvent& event);
  void BrowseOnButtonClick(wxCommandEvent& event);
  void RunOnButtonClick(wxCommandEvent& event);
  void StopOnButtonClick(wxCommandEvent& event);

protected:
  wxMenuBar* m_menubar;
  wxMenu* m_menu;
  wxStaticText* script_file_label;
  wxTextCtrl* m_textCtrl1;
  wxButton* Browse;
  wxButton* run_button;
  wxButton* stop_button;
  wxStaticText* m_staticText2;
  wxTextCtrl* output_console;

public:

  LuaScriptFrame(wxWindow* parent);

  ~LuaScriptFrame();

  void LuaScriptFrameOnContextMenu(wxMouseEvent &event)
  {
    this->PopupMenu(m_menu, event.GetPosition());
  }
};
