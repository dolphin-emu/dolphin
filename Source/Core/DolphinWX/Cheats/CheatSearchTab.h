// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include <wx/panel.h>
#include <wx/timer.h>

#include "Common/CommonTypes.h"

class wxButton;
class wxChoice;
class wxFocusEvent;
class wxListEvent;
class wxListView;
class wxRadioBox;
class wxRadioButton;
class wxStaticText;
class wxTextCtrl;

class CheatSearchTab final : public wxPanel
{
public:
  CheatSearchTab(wxWindow* const parent);

  void UpdateGUI();

private:
  class CheatSearchResult final
  {
  public:
    CheatSearchResult() : address(0), old_value(0) {}
    u32 address;
    u32 old_value;
  };

  void UpdateCheatSearchResultsList();
  void UpdateCheatSearchResultItem(long index);
  void FilterCheatSearchResults(u32 value, bool prev);
  bool ParseUserEnteredValue(u32* out) const;
  u32 SwapValue(u32 value) const;

  void OnNewScanClicked(wxCommandEvent&);
  void OnNextScanClicked(wxCommandEvent&);
  void OnCreateARCodeClicked(wxCommandEvent&);
  void OnListViewItemActivated(wxListEvent&);
  void OnListViewItemSelected(wxListEvent&);
  void OnTimerUpdate(wxTimerEvent&);

  std::vector<CheatSearchResult> m_search_results;
  unsigned int m_search_type_size;
  bool m_scan_is_initialized = false;

  wxChoice* m_search_type;
  wxListView* m_lview_search_results;
  wxStaticText* m_label_results_count;
  wxTextCtrl* m_textctrl_value_x;

  wxButton* m_btn_create_code;
  wxButton* m_btn_init_scan;
  wxButton* m_btn_next_scan;
  wxStaticText* m_label_scan_disabled;

  wxRadioBox* m_data_sizes;

  wxTimer m_update_timer;
};
