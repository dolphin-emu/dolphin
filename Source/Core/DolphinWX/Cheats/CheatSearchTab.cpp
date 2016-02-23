// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <cstring>
#include <wx/arrstr.h>
#include <wx/button.h>
#include <wx/choice.h>
#include <wx/listctrl.h>
#include <wx/panel.h>
#include <wx/radiobox.h>
#include <wx/radiobut.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/timer.h>

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
	m_update_timer.SetOwner(this);
	Bind(wxEVT_TIMER, &CheatSearchTab::OnTimerUpdate, this);

	// first scan button
	m_btn_init_scan = new wxButton(this, wxID_ANY, _("New Scan"));
	m_btn_init_scan->Bind(wxEVT_BUTTON, &CheatSearchTab::OnNewScanClicked, this);

	// next scan button
	m_btn_next_scan = new wxButton(this, wxID_ANY, _("Next Scan"));
	m_btn_next_scan->Bind(wxEVT_BUTTON, &CheatSearchTab::OnNextScanClicked, this);
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
	button_cheat_search_copy_address->Bind(wxEVT_BUTTON, &CheatSearchTab::OnCreateARCodeClicked, this);

	// results groupbox
	wxStaticBoxSizer* const sizer_cheat_search_results = new wxStaticBoxSizer(wxVERTICAL, this, _("Results"));
	sizer_cheat_search_results->Add(m_label_results_count, 0, wxALIGN_LEFT | wxALL, 5);
	sizer_cheat_search_results->Add(m_lview_search_results, 1, wxEXPAND | wxALL, 5);
	sizer_cheat_search_results->Add(button_cheat_search_copy_address, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 5);

	// search value textbox
	m_textctrl_value_x = new wxTextCtrl(this, wxID_ANY, "0x0", wxDefaultPosition, wxSize(96, -1));

	wxBoxSizer* const sizer_cheat_filter_text = new wxBoxSizer(wxHORIZONTAL);
	sizer_cheat_filter_text->Add(m_textctrl_value_x, 1, wxALIGN_CENTER_VERTICAL, 5);

	// Filter types in the compare dropdown
	// TODO: Implement between search
	wxArrayString filters;
	filters.Add(_("Unknown"));
	filters.Add(_("Not Equal"));
	filters.Add(_("Equal"));
	filters.Add(_("Greater Than"));
	filters.Add(_("Less Than"));

	m_search_type = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, filters);
	m_search_type->Select(0);

	wxStaticBoxSizer* const sizer_cheat_search_filter = new wxStaticBoxSizer(wxVERTICAL, this, _("Search (clear to use previous value)"));
	sizer_cheat_search_filter->Add(sizer_cheat_filter_text, 0, wxALL | wxEXPAND, 5);
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

void CheatSearchTab::OnNewScanClicked(wxCommandEvent& WXUNUSED(event))
{
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
		memcpy(&r.old_value, &Memory::m_pRAM[addr], m_search_type_size);
		m_search_results.push_back(r);
	}

	UpdateCheatSearchResultsList();
}

void CheatSearchTab::OnNextScanClicked(wxCommandEvent&)
{
	if (!Core::IsRunningAndStarted())
	{
		WxUtils::ShowErrorDialog(_("A game is not currently running."));
		return;
	}

	u32 user_x_val = 0;
	bool blank_user_value = m_textctrl_value_x->IsEmpty();
	if (!blank_user_value)
	{
		if (!ParseUserEnteredValue(&user_x_val))
			return;
	}

	FilterCheatSearchResults(user_x_val, blank_user_value);

	UpdateCheatSearchResultsList();
}

void CheatSearchTab::OnCreateARCodeClicked(wxCommandEvent&)
{
	long idx = m_lview_search_results->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (idx == wxNOT_FOUND)
		return;

	const u32 address = m_search_results[idx].address | ((m_search_type_size & ~1) << 24);

	CreateCodeDialog arcode_dlg(this, address);
	arcode_dlg.SetExtraStyle(arcode_dlg.GetExtraStyle() & ~wxWS_EX_BLOCK_EVENTS);
	arcode_dlg.ShowModal();
}

void CheatSearchTab::OnTimerUpdate(wxTimerEvent&)
{
	if (Core::GetState() != Core::CORE_RUN)
		return;

	// Only update the currently visible list rows.
	long first = m_lview_search_results->GetTopItem();
	long last = std::min(m_lview_search_results->GetItemCount(), m_lview_search_results->GetCountPerPage());

	m_lview_search_results->Freeze();

	while (first < last)
	{
		UpdateCheatSearchResultItem(first);
		first++;
	}

	m_lview_search_results->Thaw();
}

void CheatSearchTab::UpdateCheatSearchResultsList()
{
	m_update_timer.Stop();
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
			// Insert into the list control.
			wxString address_string = wxString::Format("0x%08X", m_search_results[i].address);
			long index = m_lview_search_results->InsertItem(static_cast<long>(i), address_string);

			UpdateCheatSearchResultItem(index);
		}

		m_lview_search_results->Thaw();

		// Half-second update interval
		m_update_timer.Start(500);
	}

	m_label_results_count->SetLabel(count_label);
}

void CheatSearchTab::UpdateCheatSearchResultItem(long index)
{
	u32 address_value = 0;
	std::memcpy(&address_value, &Memory::m_pRAM[m_search_results[index].address], m_search_type_size);

	u32 display_value = SwapValue(address_value);

	wxString buf;
	buf.Printf("0x%08X", display_value);
	m_lview_search_results->SetItem(index, 1, buf);

	float display_value_float = 0.0f;
	std::memcpy(&display_value_float, &display_value, sizeof(u32));
	buf.Printf("%e", display_value_float);
	m_lview_search_results->SetItem(index, 2, buf);

	double display_value_double = 0.0;
	std::memcpy(&display_value_double, &display_value, sizeof(u32));
	buf.Printf("%e", display_value_double);
	m_lview_search_results->SetItem(index, 3, buf);
}

enum class ComparisonMask
{
	EQUAL        = 0x1,
	GREATER_THAN = 0x2,
	LESS_THAN    = 0x4
};

static ComparisonMask operator | (ComparisonMask comp1, ComparisonMask comp2)
{
	return static_cast<ComparisonMask>(static_cast<int>(comp1) |
					   static_cast<int>(comp2));
}

static ComparisonMask operator & (ComparisonMask comp1, ComparisonMask comp2)
{
	return static_cast<ComparisonMask>(static_cast<int>(comp1) &
					   static_cast<int>(comp2));
}

void CheatSearchTab::FilterCheatSearchResults(u32 value, bool prev)
{
	static const std::array<ComparisonMask, 5> filters{{
	    ComparisonMask::EQUAL | ComparisonMask::GREATER_THAN | ComparisonMask::LESS_THAN, // Unknown
	    ComparisonMask::GREATER_THAN | ComparisonMask::LESS_THAN, // Not Equal
	    ComparisonMask::EQUAL,
	    ComparisonMask::GREATER_THAN,
	    ComparisonMask::LESS_THAN
	}};
	ComparisonMask filter_mask = filters[m_search_type->GetSelection()];

	std::vector<CheatSearchResult> filtered_results;
	filtered_results.reserve(m_search_results.size());

	for (CheatSearchResult& result : m_search_results)
	{
		if (prev)
			value = result.old_value;

		// with big endian, can just use memcmp for ><= comparison
		int cmp_result = std::memcmp(&Memory::m_pRAM[result.address], &value, m_search_type_size);
		ComparisonMask cmp_mask;
		if (cmp_result < 0)
			cmp_mask = ComparisonMask::LESS_THAN;
		else if (cmp_result)
			cmp_mask = ComparisonMask::GREATER_THAN;
		else
			cmp_mask = ComparisonMask::EQUAL;

		if (static_cast<int>(cmp_mask & filter_mask))
		{
			std::memcpy(&result.old_value, &Memory::m_pRAM[result.address], m_search_type_size);
			filtered_results.push_back(result);
		}
	}

	m_search_results.swap(filtered_results);
}

void CheatSearchTab::ResetListViewColumns()
{
	m_lview_search_results->AppendColumn(_("Address"));
	m_lview_search_results->AppendColumn(_("Value"));
	m_lview_search_results->AppendColumn(_("Value (float)"));
	m_lview_search_results->AppendColumn(_("Value (double)"));
}

bool CheatSearchTab::ParseUserEnteredValue(u32* out) const
{
	unsigned long parsed_x_val = 0;
	wxString x_val = m_textctrl_value_x->GetValue();

	if (!x_val.ToULong(&parsed_x_val, 0))
	{
		WxUtils::ShowErrorDialog(_("You must enter a valid decimal, hexadecimal or octal value."));
		return false;
	}

	*out = SwapValue(static_cast<u32>(parsed_x_val));
	return true;
}

u32 CheatSearchTab::SwapValue(u32 value) const
{
	switch (m_search_type_size)
	{
	case 2:
		*(u16*)&value = Common::swap16((u8*)&value);
		break;
	case 4:
		value = Common::swap32(value);
		break;
	}

	return value;
}
