// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include <wx/panel.h>

class wxButton;
class wxChoice;
class wxCommandEvent;
class wxFocusEvent;
class wxListBox;
class wxRadioBox;
class wxRadioButton;
class wxStaticText;
class wxTextCtrl;
class wxWindow;

class CheatSearchTab final : public wxPanel
{
public:
	CheatSearchTab(wxWindow* const parent);

private:
	class CheatSearchResult final
	{
	public:
		CheatSearchResult() : address(0), old_value(0) {}

		u32 address;
		u32 old_value;
	};

	std::vector<CheatSearchResult> m_search_results;
	unsigned int m_search_type_size;

	wxChoice* m_search_type;
	wxListBox* m_lbox_search_results;
	wxStaticText* m_label_results_count;
	wxTextCtrl* m_textctrl_value_x;

	wxButton* m_btn_init_scan;
	wxButton* m_btn_next_scan;

	wxRadioBox* m_data_sizes;

	struct
	{
		wxRadioButton* rad_oldvalue;
		wxRadioButton* rad_uservalue;

	} m_value_x_radiobtn;

	void UpdateCheatSearchResultsList();
	void StartNewSearch(wxCommandEvent& event);
	void FilterCheatSearchResults(wxCommandEvent& event);
	void CreateARCode(wxCommandEvent&);
	void ApplyFocus(wxFocusEvent&);
};
