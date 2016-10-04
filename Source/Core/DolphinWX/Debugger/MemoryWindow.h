// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wx/panel.h>
#include "Common/CommonTypes.h"

class CMemoryView;
class CCodeWindow;
class IniFile;
class wxButton;
class wxCheckBox;
class wxRadioBox;
class wxRadioButton;
class wxListBox;
class wxSearchCtrl;
class wxStaticText;
class wxTextCtrl;
class wxRadioButton;

class CMemoryWindow : public wxPanel
{
public:
  CMemoryWindow(wxWindow* parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize, long style = wxTAB_TRAVERSAL | wxBORDER_NONE,
                const wxString& name = _("Memory"));

  void Repopulate();

  void JumpToAddress(u32 _Address);

private:
  DECLARE_EVENT_TABLE()

  void OnDataTypeChanged(wxCommandEvent& event);
  void OnSearch(wxCommandEvent& event);
  void OnAddrBoxChange(wxCommandEvent& event);
  void OnValueChanged(wxCommandEvent&);
  void SetMemoryValueFromValBox(wxCommandEvent& event);
  void SetMemoryValue(wxCommandEvent& event);
  void OnDumpMemory(wxCommandEvent& event);
  void OnDumpMem2(wxCommandEvent& event);
  void OnDumpFakeVMEM(wxCommandEvent& event);
  void OnMemCheckOptionChange(wxCommandEvent& event);

  wxButton* btnSearch;
  wxRadioButton* m_rb_ascii;
  wxRadioButton* m_rb_hex;

  wxRadioBox* m_rbox_data_type;
  wxStaticText* m_search_result_msg;

  wxCheckBox* chkLog;
  wxRadioButton* rdbRead;
  wxRadioButton* rdbWrite;
  wxRadioButton* rdbReadWrite;

  CCodeWindow* m_code_window;

  CMemoryView* memview;

  wxSearchCtrl* addrbox;
  wxTextCtrl* valbox;

  u32 m_last_search_address = 0;
  bool m_continue_search = false;
};
