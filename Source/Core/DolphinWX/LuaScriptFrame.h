// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <string>
#include <wx/artprov.h>
#include <wx/string.h>
#include <wx/menu.h>
#include <wx/gdicmn.h>
#include <wx/font.h>
#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/icon.h>
#include <wx/panel.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/frame.h>

class LuaScriptFrame final : public wxFrame
{
  /*
public:
  LuaScriptFrame(wxWindow* parent);
  ~LuaScriptFrame();

private:
  void CreateGUI();
  void OnClose(wxCloseEvent& event);
  void OnLoad(wxEvent& event);
  */

private:
  void CreateGUI();
  void OnClose(wxCloseEvent& event);
  void OnLoad(wxEvent& event);

protected:
  wxMenuBar* m_menubar1;
  wxMenu* m_menu1;
  wxPanel* m_panel1;
  wxStaticText* script_file_label;
  wxTextCtrl* m_textCtrl1;
  wxButton* Browse;

  // Virtual event handlers, overide them in your derived class
  virtual void BrowseOnButtonClick(wxCommandEvent& event) { event.Skip(); }


public:

  LuaScriptFrame(wxWindow* parent);

  ~LuaScriptFrame();

  void MyFrame1OnContextMenu(wxMouseEvent &event)
  {
    this->PopupMenu(m_menu1, event.GetPosition());
  }
};
