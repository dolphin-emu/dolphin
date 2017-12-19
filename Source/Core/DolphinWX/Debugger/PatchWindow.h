// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wx/aui/framemanager.h>
#include <wx/panel.h>

class PatchView;

class PatchWindow : public wxPanel
{
public:
  PatchWindow(wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Patches"),
              const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
              long style = wxTAB_TRAVERSAL | wxBORDER_NONE);
  ~PatchWindow();

  void NotifyUpdate();

private:
  friend class PatchBar;

  void OnDelete(wxCommandEvent&);
  void OnClear(wxCommandEvent&);
  void OnToggleOnOff(wxCommandEvent&);

  wxAuiManager m_mgr;
  PatchView* m_patch_view;
};
