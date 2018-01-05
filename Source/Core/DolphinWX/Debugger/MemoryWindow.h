// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wx/panel.h>
#include "Common/CommonTypes.h"

class CMemoryView;
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
  explicit CMemoryWindow(wxWindow* parent, wxWindowID id = wxID_ANY,
                         const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
                         long style = wxTAB_TRAVERSAL | wxBORDER_NONE,
                         const wxString& name = _("Memory"));

  void Repopulate();

  void JumpToAddress(u32 address);

private:
  enum class SearchType
  {
    FindNext,
    FindPrevious
  };

  void CreateGUI();
  wxSizer* CreateRightHandSideSizer();
  wxSizer* CreateSearchSizer();
  wxSizer* CreateDumpSizer();
  wxSizer* CreateSearchTypeSizer();
  wxSizer* CreateMemcheckOptionSizer();

  void OnDataTypeChanged(wxCommandEvent& event);
  void OnFindNext(wxCommandEvent& event);
  void OnFindPrevious(wxCommandEvent& event);
  void OnSearchAddressChanged(wxCommandEvent& event);
  void OnValueChanged(wxCommandEvent&);
  void OnSetMemoryValueFromValBox(wxCommandEvent& event);
  void OnSetMemoryValue(wxCommandEvent& event);
  void OnDumpMemory(wxCommandEvent& event);
  void OnDumpMem2(wxCommandEvent& event);
  void OnDumpFakeVMEM(wxCommandEvent& event);
  void OnMemCheckOptionChange(wxCommandEvent& event);

  void Search(SearchType search_type);

  wxButton* m_btn_find_next;
  wxButton* m_btn_find_previous;
  wxRadioButton* m_rb_ascii;
  wxRadioButton* m_rb_hex;

  wxRadioBox* m_rbox_data_type;
  wxStaticText* m_search_result_msg;

  wxCheckBox* m_log_checkbox;
  wxRadioButton* m_read_radio_btn;
  wxRadioButton* m_write_radio_btn;
  wxRadioButton* m_read_write_radio_btn;

  CMemoryView* m_memory_view;

  wxSearchCtrl* m_address_search_ctrl;
  wxTextCtrl* m_value_text_ctrl;

  u32 m_last_search_address = 0;
  bool m_continue_search = false;
};
