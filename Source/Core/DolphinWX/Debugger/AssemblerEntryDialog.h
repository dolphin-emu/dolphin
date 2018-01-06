// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wx/textdlg.h>

#include "Common/CommonTypes.h"

class wxStaticText;

class AssemblerEntryDialog : public wxTextEntryDialog
{
public:
  AssemblerEntryDialog(u32 address, wxWindow* parent, const wxString& message,
                       const wxString& caption = wxGetTextFromUserPromptStr,
                       const wxString& value = wxEmptyString, long style = wxTextEntryDialogStyle,
                       const wxPoint& pos = wxDefaultPosition);

  bool Create(wxWindow* parent, const wxString& message,
              const wxString& caption = wxGetTextFromUserPromptStr,
              const wxString& value = wxEmptyString, long style = wxTextEntryDialogStyle,
              const wxPoint& pos = wxDefaultPosition);

protected:
  u32 m_address;
  wxStaticText* m_preview;

private:
  void OnTextChanged(wxCommandEvent& evt);
};
