// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wx/aui/framemanager.h>
#include <wx/panel.h>

class CBreakPointView;
class CCodeWindow;
class wxListEvent;

class CBreakPointWindow : public wxPanel
{
public:
  CBreakPointWindow(CCodeWindow* _pCodeWindow, wxWindow* parent, wxWindowID id = wxID_ANY,
                    const wxString& title = _("Breakpoints"),
                    const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
                    long style = wxTAB_TRAVERSAL | wxBORDER_NONE);
  ~CBreakPointWindow();

  void NotifyUpdate();
  void SaveAll();
  void LoadAll();

private:
  friend class CBreakPointBar;

  void OnDelete(wxCommandEvent& WXUNUSED(event));
  void OnClear(wxCommandEvent& WXUNUSED(event));
  void OnAddBreakPoint(wxCommandEvent& WXUNUSED(event));
  void OnAddMemoryCheck(wxCommandEvent& WXUNUSED(event));
  void Event_SaveAll(wxCommandEvent& WXUNUSED(event));
  void Event_LoadAll(wxCommandEvent& WXUNUSED(event));
  void OnSelectBP(wxListEvent& event);

  wxAuiManager m_mgr;
  CBreakPointView* m_BreakPointListView;
  CCodeWindow* m_pCodeWindow;
};
