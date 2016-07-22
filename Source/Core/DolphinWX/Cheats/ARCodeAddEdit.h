// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wx/dialog.h>

#include "Core/ActionReplay.h"

class wxTextCtrl;

class ARCodeAddEdit : public wxDialog
{
public:
  ARCodeAddEdit(ActionReplay::ARCode code, wxWindow* parent, wxWindowID id = wxID_ANY,
                const wxString& title = _("Edit ActionReplay Code"),
                const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
                long style = wxDEFAULT_DIALOG_STYLE);

  const ActionReplay::ARCode GetCode() const { return m_code; }
private:
  ActionReplay::ARCode m_code;
  wxTextCtrl* EditCheatName;
  wxTextCtrl* EditCheatCode;

  void SaveCheatData(wxCommandEvent& event);
};
