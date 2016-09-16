// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wx/dialog.h>

class CBreakPointWindow;
class wxRadioButton;
class wxStaticText;
class wxTextCtrl;

class MemoryCheckDlg : public wxDialog
{
public:
  MemoryCheckDlg(CBreakPointWindow* parent);

private:
  CBreakPointWindow* m_parent;
  wxStaticText* m_textAddress;
  wxStaticText* m_textStartAddress;
  wxStaticText* m_textEndAddress;
  wxRadioButton* m_radioLog;
  wxRadioButton* m_radioBreak;
  wxRadioButton* m_radioBreakLog;
  wxRadioButton* m_radioRead;
  wxRadioButton* m_radioWrite;
  wxRadioButton* m_radioReadWrite;
  wxRadioButton* m_radioAddress;
  wxRadioButton* m_radioRange;
  wxTextCtrl* m_pEditAddress;
  wxTextCtrl* m_pEditEndAddress;
  wxTextCtrl* m_pEditStartAddress;

  void OnRadioButtonClick(wxCommandEvent& event);
  void OnOK(wxCommandEvent& event);
};
