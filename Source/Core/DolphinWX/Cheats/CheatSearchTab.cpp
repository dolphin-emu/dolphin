// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <array>
#include <wx/button.h>
#include <wx/choice.h>
#include <wx/listctrl.h>
#include <wx/panel.h>
#include <wx/radiobox.h>
#include <wx/radiobut.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"
#include "Core/ActionReplay.h"
#include "Core/Core.h"
#include "Core/HW/Memmap.h"
#include "DolphinWX/WxUtils.h"
#include "DolphinWX/Cheats/CheatSearchTab.h"
#include "DolphinWX/Cheats/CreateCodeDialog.h"

namespace
{
	const unsigned int MAX_CHEAT_SEARCH_RESULTS_DISPLAY = 1024;
}

CheatSearchTab::CheatSearchTab(wxWindow* const parent)
	: wxPanel(parent)
{
	// first scan button
	m_btn_init_scan = new wxButton(this, wxID_ANY, _("New Scan"));
	m_btn_init_scan->Bind(wxEVT_BUTTON, &CheatSearchTab::StartNewSearch, this);

	// next scan button
	m_btn_next_scan = new wxButton(this, wxID_ANY, _("Next Scan"));
	m_btn_next_scan->Bind(wxEVT_BUTTON, &CheatSearchTab::FilterCheatSearchResults, this);
	m_btn_next_scan->Disable();

	// data sizes radiobox
	std::array<wxString, 3> data_size_names = { { _("8-bit"), _("16-bit"), _("32-bit") } };
	m_data_sizes = new wxRadioBox(this, wxID_ANY, _("Data Size"), wxDefaultPosition, wxDefaultSize, static_cast<int>(data_size_names.size()), data_size_names.data());

	// ListView for search results
	m_lview_search_results = new wxListView(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
	ResetListViewColumns();

	// Result count
	m_label_results_count = new wxStaticText(this, wxID_ANY, _("Count:"));

	// create AR code button
	wxButton* const button_cheat_search_copy_address = new wxButton(this, wxID_ANY, _("Create AR Code"));
	button_cheat_search_copy_address->Bind(wxEVT_BUTTON, &CheatSearchTab::CreateARCode, this);

	// results groupbox
	wxStaticBoxSizer* const sizer_cheat_search_results = new wxStaticBoxSizer(wxVERTICAL, this, _("Results"));
	sizer_cheat_search_results->Add(m_label_results_count, 0, wxALIGN_LEFT | wxALL, 5);
	sizer_cheat_search_results->Add(m_lview_search_results, 1, wxEXPAND | wxALL, 5);
	sizer_cheat_search_results->Add(button_cheat_search_copy_address, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 5);

	// Search value radio buttons
	m_value_x_radiobtn.rad_oldvalue = new wxRadioButton(this, wxID_ANY, _("Previous Value"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
	m_value_x_radiobtn.rad_uservalue = new wxRadioButton(this, wxID_ANY, "");
	m_value_x_radiobtn.rad_oldvalue->SetValue(true);

	// search value textbox
	m_textctrl_value_x = new wxTextCtrl(this, wxID_ANY, "0x0", wxDefaultPosition, wxSize(96, -1));
	m_textctrl_value_x->Bind(wxEVT_SET_FOCUS, &CheatSearchTab::ApplyFocus, this);

	wxBoxSizer* const sizer_cheat_filter_text = new wxBoxSizer(wxHORIZONTAL);
	sizer_cheat_filter_text->Add(m_value_x_radiobtn.rad_uservalue, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, 5);
	sizer_cheat_filter_text->Add(m_textctrl_value_x, 1, wxALIGN_CENTER_VERTICAL, 5);

	// value groupbox
	wxStaticBoxSizer* const sizer_cheat_search_filter_x = new wxStaticBoxSizer(wxVERTICAL, this, _("Value"));
	sizer_cheat_search_filter_x->Add(m_value_x_radiobtn.rad_oldvalue, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);
	sizer_cheat_search_filter_x->Add(sizer_cheat_filter_text, 0, wxALL | wxEXPAND, 5);

	// filter types in the compare dropdown
	static const wxString searches[] = {
		_("Unknown"),
		_("Not Equal"),
		_("Equal"),
		_("Greater Than"),
		_("Less Than"),
	// TODO: Implement between search.
		//_("Between"),
	};

	m_search_type = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, sizeof(searches) / sizeof(*searches), searches);
	m_search_type->Select(0);

	wxStaticBoxSizer* const sizer_cheat_search_filter = new wxStaticBoxSizer(wxVERTICAL, this, _("Search Filter"));
	sizer_cheat_search_filter->Add(sizer_cheat_search_filter_x, 0, wxALL | wxEXPAND, 5);
	sizer_cheat_search_filter->Add(m_search_type, 0, wxALL, 5);

	// left sizer
	wxBoxSizer* const sizer_left = new wxBoxSizer(wxVERTICAL);
	sizer_left->Add(sizer_cheat_search_results, 1, wxEXPAND, 5);

	// button sizer
	wxBoxSizer* boxButtons = new wxBoxSizer(wxHORIZONTAL);
	boxButtons->Add(m_btn_init_scan, 1, wxRIGHT, 5);
	boxButtons->Add(m_btn_next_scan, 1);

	// right sizer
	wxBoxSizer* const sizer_right = new wxBoxSizer(wxVERTICAL);
	sizer_right->Add(m_data_sizes, 0, wxEXPAND | wxBOTTOM, 5);
	sizer_right->Add(sizer_cheat_search_filter, 0, wxEXPAND | wxBOTTOM, 5);
	sizer_right->AddStretchSpacer(1);
	sizer_right->Add(boxButtons, 0, wxTOP | wxEXPAND, 5);

	// main sizer
	wxBoxSizer* const sizer_main = new wxBoxSizer(wxHORIZONTAL);
	sizer_main->Add(sizer_left, 1, wxEXPAND | wxALL, 5);
	sizer_main->Add(sizer_right, 0, wxEXPAND | wxALL, 5);

	SetSizerAndFit(sizer_main);
}

void CheatSearchTab::StartNewSearch(wxCommandEvent& WXUNUSED(event))
{
	const u8* const memptr = Memory::m_pRAM;
	if (!Core::IsRunningAndStarted())
	{
		WxUtils::ShowErrorDialog(_("A game is not currently running."));
		return;
	}

	// Determine the user-selected data size for this search.
	m_search_type_size = (1 << m_data_sizes->GetSelection());

	// Set up the search results efficiently to prevent automatic re-allocations.
	m_search_results.clear();
	m_search_results.reserve(Memory::RAM_SIZE / m_search_type_size);

	// Enable the "Next Scan" button.
	m_btn_next_scan->Enable();

	CheatSearchResult r;
	// can I assume cheatable values will be aligned like this?
	for (u32 addr = 0; addr != Memory::RAM_SIZE; addr += m_search_type_size)
	{
		r.address = addr;
		memcpy(&r.old_value, memptr + addr, m_search_type_size);
		m_search_results.push_back(r);
	}

	UpdateCheatSearchResultsList();
}

void CheatSearchTab::FilterCheatSearchResults(wxCommandEvent&)
{
	const u8* const memptr = Memory::m_pRAM;
	if (!Core::IsRunningAndStarted())
	{
		WxUtils::ShowErrorDialog(_("A game is not currently running."));
		return;
	}

	// Set up the sub-search results efficiently to prevent automatic re-allocations.
	std::vector<CheatSearchResult> filtered_results;
	filtered_results.reserve(m_search_results.size());

	// Determine the selected filter
	// 1 : equal
	// 2 : greater-than
	// 4 : less-than

	const int filters[] = { 7, 6, 1, 2, 4 };
	int filter_mask = filters[m_search_type->GetSelection()];

	if (m_value_x_radiobtn.rad_oldvalue->GetValue()) // using old value comparison
	{
		for (CheatSearchResult& result : m_search_results)
		{
			// with big endian, can just use memcmp for ><= comparison
			int cmp_result = memcmp(memptr + result.address, &result.old_value, m_search_type_size);
			if (cmp_result < 0)
				cmp_result = 4;
			else
				cmp_result = cmp_result ? 2 : 1;

			if (cmp_result & filter_mask)
			{
				memcpy(&result.old_value, memptr + result.address, m_search_type_size);
				filtered_results.push_back(result);
			}
		}
	}
	else // using user entered x value comparison
	{
		u32 user_x_val;

		// parse the user entered x value
		if (filter_mask != 7) // don't need the value for the "None" filter
		{
			unsigned long parsed_x_val = 0;
			wxString x_val = m_textctrl_value_x->GetValue();

			if (!x_val.ToULong(&parsed_x_val, 0))
			{
				WxUtils::ShowErrorDialog(_("You must enter a valid decimal, hexadecimal or octal value."));
				return;
			}

			user_x_val = (u32)parsed_x_val;

			// #ifdef LIL_ENDIAN :p
			switch (m_search_type_size)
			{
			case 1:
				break;
			case 2:
				*(u16*)&user_x_val = Common::swap16((u8*)&user_x_val);
				break;
			case 4:
				user_x_val = Common::swap32(user_x_val);
				break;
			}
			// #elseif BIG_ENDIAN
			// would have to move <u32 vals (8/16bit) to start of the user_x_val for the comparisons i use below
			// #endif
		}

		for (CheatSearchResult& result : m_search_results)
		{
			// with big endian, can just use memcmp for ><= comparison
			int cmp_result = memcmp(memptr + result.address, &user_x_val, m_search_type_size);
			if (cmp_result < 0)
				cmp_result = 4;
			else if (cmp_result)
				cmp_result = 2;
			else
				cmp_result = 1;

			if (cmp_result & filter_mask)
			{
				memcpy(&result.old_value, memptr + result.address, m_search_type_size);
				filtered_results.push_back(result);
			}
		}
	}

	m_search_results.swap(filtered_results);

	UpdateCheatSearchResultsList();
}

void CheatSearchTab::ApplyFocus(wxFocusEvent& ev)
{
	ev.Skip();
	m_value_x_radiobtn.rad_uservalue->SetValue(true);
}

void CheatSearchTab::UpdateCheatSearchResultsList()
{
	m_lview_search_results->ClearAll();
	ResetListViewColumns();

	wxString count_label = wxString::Format(_("Count: %lu"),
		(unsigned long)m_search_results.size());
	if (m_search_results.size() > MAX_CHEAT_SEARCH_RESULTS_DISPLAY)
	{
		count_label += _(" (too many to display)");
	}
	else
	{
		m_lview_search_results->Freeze();

		for (size_t i = 0; i < m_search_results.size(); i++)
		{
			u32 display_value = m_search_results[i].old_value;

			switch (m_search_type_size)
			{
			case 1:
				break;
			case 2:
				*(u16*)&display_value = Common::swap16((u8*)&display_value);
				break;
			case 4:
				display_value = Common::swap32(display_value);
				break;
			}

			// Insert into the list control.
			wxString buf;
			buf.Printf("0x%08X", m_search_results[i].address);
			long index = m_lview_search_results->InsertItem(static_cast<long>(i), buf);

			buf.Printf("0x%08X", display_value);
			m_lview_search_results->SetItem(index, 1, buf);
		}

		m_lview_search_results->Thaw();
	}

	m_label_results_count->SetLabel(count_label);
}

void CheatSearchTab::CreateARCode(wxCommandEvent&)
{
	long idx = m_lview_search_results->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);

	if (idx != wxNOT_FOUND)
	{
		const u32 address = m_search_results[idx].address | ((m_search_type_size & ~1) << 24);

		CreateCodeDialog arcode_dlg(this, address, ActionReplay::GetARCodes());
		arcode_dlg.SetExtraStyle(arcode_dlg.GetExtraStyle() & ~wxWS_EX_BLOCK_EVENTS);
		arcode_dlg.ShowModal();
	}
}

void CheatSearchTab::ResetListViewColumns()
{
	m_lview_search_results->AppendColumn(_("Address"));
	m_lview_search_results->AppendColumn(_("Value"));
}
