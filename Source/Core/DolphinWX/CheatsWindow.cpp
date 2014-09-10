// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <wx/button.h>
#include <wx/chartype.h>
#include <wx/checkbox.h>
#include <wx/checklst.h>
#include <wx/choice.h>
#include <wx/defs.h>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/listbox.h>
#include <wx/msgdlg.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/radiobut.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/string.h>
#include <wx/textctrl.h>
#include <wx/toplevel.h>
#include <wx/translation.h>
#include <wx/validate.h>

#include "Common/CommonTypes.h"
#include "Common/IniFile.h"
#include "Common/StringUtil.h"
#include "Core/ActionReplay.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreParameter.h"
#include "Core/GeckoCode.h"
#include "Core/GeckoCodeConfig.h"
#include "Core/HW/Memmap.h"
#include "DolphinWX/CheatsWindow.h"
#include "DolphinWX/Frame.h"
#include "DolphinWX/GeckoCodeDiag.h"
#include "DolphinWX/ISOProperties.h"
#include "DolphinWX/Main.h"
#include "DolphinWX/WxUtils.h"

class wxWindow;

#define MAX_CHEAT_SEARCH_RESULTS_DISPLAY  256

// meh
static wxCheatsWindow *g_cheat_window;

wxCheatsWindow::wxCheatsWindow(wxWindow* const parent)
	: wxDialog(parent, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER|wxMAXIMIZE_BOX|wxMINIMIZE_BOX|wxDIALOG_NO_PARENT)
{
	::g_cheat_window = this;

	// Create the GUI controls
	Init_ChildControls();

	// load codes
	UpdateGUI();

	SetSize(wxSize(-1, 600));
	Center();
	Show();
}

wxCheatsWindow::~wxCheatsWindow()
{
	main_frame->g_CheatsWindow = nullptr;
	::g_cheat_window = nullptr;
}

void wxCheatsWindow::Init_ChildControls()
{
	wxPanel* const panel = new wxPanel(this);

	// Main Notebook
	m_Notebook_Main = new wxNotebook(panel, wxID_ANY);

	// --- Tabs ---
	// $ Cheats List Tab
	m_Tab_Cheats = new wxPanel(m_Notebook_Main, wxID_ANY);

	m_CheckListBox_CheatsList = new wxCheckListBox(m_Tab_Cheats, wxID_ANY, wxDefaultPosition, wxSize(300, 0), m_CheatStringList, wxLB_HSCROLL, wxDefaultValidator);
	m_CheckListBox_CheatsList->Bind(wxEVT_LISTBOX, &wxCheatsWindow::OnEvent_CheatsList_ItemSelected, this);
	m_CheckListBox_CheatsList->Bind(wxEVT_CHECKLISTBOX, &wxCheatsWindow::OnEvent_CheatsList_ItemToggled, this);

	m_Label_Codename = new wxStaticText(m_Tab_Cheats, wxID_ANY, _("Name: "));
	m_GroupBox_Info = new wxStaticBox(m_Tab_Cheats, wxID_ANY, _("Code Info"));

	m_Label_NumCodes = new wxStaticText(m_Tab_Cheats, wxID_ANY, _("Number Of Codes: "));
	m_ListBox_CodesList = new wxListBox(m_Tab_Cheats, wxID_ANY, wxDefaultPosition, wxSize(120, 150), 0, nullptr, wxLB_HSCROLL);

	wxStaticBoxSizer* sGroupBoxInfo = new wxStaticBoxSizer(m_GroupBox_Info, wxVERTICAL);
	sGroupBoxInfo->Add(m_Label_Codename, 0, wxALL, 5);
	sGroupBoxInfo->Add(m_Label_NumCodes, 0, wxALL, 5);
	sGroupBoxInfo->Add(m_ListBox_CodesList, 1, wxALL, 5);

	wxBoxSizer* sizer_tab_cheats = new wxBoxSizer(wxHORIZONTAL);
	sizer_tab_cheats->Add(m_CheckListBox_CheatsList, 1, wxEXPAND | wxTOP | wxBOTTOM | wxLEFT, 10);
	sizer_tab_cheats->Add(sGroupBoxInfo, 0, wxALIGN_LEFT | wxEXPAND | wxALL, 5);

	m_Tab_Cheats->SetSizerAndFit(sizer_tab_cheats);

	// $ Cheat Search Tab
	wxPanel* const tab_cheat_search = new CheatSearchTab(m_Notebook_Main);

	// $ Log Tab
	m_Tab_Log = new wxPanel(m_Notebook_Main, wxID_ANY);

	wxButton* const button_updatelog = new wxButton(m_Tab_Log, wxID_ANY, _("Update"));
	button_updatelog->Bind(wxEVT_BUTTON, &wxCheatsWindow::OnEvent_ButtonUpdateLog_Press, this);

	m_CheckBox_LogAR = new wxCheckBox(m_Tab_Log, wxID_ANY, _("Enable AR Logging"));
	m_CheckBox_LogAR->Bind(wxEVT_CHECKBOX, &wxCheatsWindow::OnEvent_CheckBoxEnableLogging_StateChange, this);

	m_CheckBox_LogAR->SetValue(ActionReplay::IsSelfLogging());
	m_TextCtrl_Log = new wxTextCtrl(m_Tab_Log, wxID_ANY, "", wxDefaultPosition, wxSize(100, -1), wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);

	wxBoxSizer *HStrip1 = new wxBoxSizer(wxHORIZONTAL);
	HStrip1->Add(m_CheckBox_LogAR, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5);
	HStrip1->Add(button_updatelog, 0, wxALL, 5);

	wxBoxSizer *sTabLog = new wxBoxSizer(wxVERTICAL);
	sTabLog->Add(HStrip1, 0, wxALL, 5);
	sTabLog->Add(m_TextCtrl_Log, 1, wxALL|wxEXPAND, 5);

	m_Tab_Log->SetSizerAndFit(sTabLog);

	// Add Tabs to Notebook
	m_Notebook_Main->AddPage(m_Tab_Cheats, _("AR Codes"));
	m_geckocode_panel = new Gecko::CodeConfigPanel(m_Notebook_Main);
	m_Notebook_Main->AddPage(m_geckocode_panel, _("Gecko Codes"));
	m_Notebook_Main->AddPage(tab_cheat_search, _("Cheat Search"));
	m_Notebook_Main->AddPage(m_Tab_Log, _("Logging"));

	// Button Strip
	button_apply = new wxButton(panel, wxID_APPLY, _("Apply"));
	button_apply->Bind(wxEVT_BUTTON, &wxCheatsWindow::OnEvent_ApplyChanges_Press, this);
	wxButton* const button_cancel = new wxButton(panel, wxID_CANCEL, _("Cancel"));
	button_cancel->Bind(wxEVT_BUTTON, &wxCheatsWindow::OnEvent_ButtonClose_Press, this);

	Bind(wxEVT_CLOSE_WINDOW, &wxCheatsWindow::OnEvent_Close, this);

	wxStdDialogButtonSizer* const sButtons = new wxStdDialogButtonSizer();
	sButtons->AddButton(button_apply);
	sButtons->AddButton(button_cancel);
	sButtons->Realize();

	wxBoxSizer* const sMain = new wxBoxSizer(wxVERTICAL);
	sMain->Add(m_Notebook_Main, 1, wxEXPAND|wxALL, 5);
	sMain->Add(sButtons, 0, wxRIGHT | wxBOTTOM | wxALIGN_RIGHT, 5);
	panel->SetSizerAndFit(sMain);

	wxBoxSizer* const frame_szr = new wxBoxSizer(wxVERTICAL);
	frame_szr->Add(panel, 1, wxEXPAND);
	SetSizerAndFit(frame_szr);
}

CheatSearchTab::CheatSearchTab(wxWindow* const parent)
	: wxPanel(parent, -1)
{
	// first scan button
	btnInitScan = new wxButton(this, -1, _("New Scan"));
	btnInitScan->Bind(wxEVT_BUTTON, &CheatSearchTab::StartNewSearch, this);

	// next scan button
	btnNextScan = new wxButton(this, -1, _("Next Scan"));
	btnNextScan->Bind(wxEVT_BUTTON, &CheatSearchTab::FilterCheatSearchResults, this);
	btnNextScan->Disable();

	// data size radio buttons
	size_radiobtn.rad_8 = new wxRadioButton(this, -1, _("8 bit"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
	size_radiobtn.rad_16 = new wxRadioButton(this, -1, _("16 bit"));
	size_radiobtn.rad_32 = new wxRadioButton(this, -1, _("32 bit"));
	size_radiobtn.rad_8->SetValue(true);

	// data sizes groupbox
	wxStaticBoxSizer* const sizer_cheat_new_search = new wxStaticBoxSizer(wxHORIZONTAL, this, _("Data Size"));
	sizer_cheat_new_search->Add(size_radiobtn.rad_8, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxALIGN_CENTER_VERTICAL, 5);
	sizer_cheat_new_search->Add(size_radiobtn.rad_16, 0, wxRIGHT | wxBOTTOM | wxALIGN_CENTER_VERTICAL, 5);
	sizer_cheat_new_search->Add(size_radiobtn.rad_32, 0, wxRIGHT | wxBOTTOM | wxALIGN_CENTER_VERTICAL, 5);

	// result controls
	lbox_search_results = new wxListBox(this, -1);
	label_results_count = new wxStaticText(this, -1, _("Count:"));

	// create AR code button
	wxButton* const button_cheat_search_copy_address = new wxButton(this, -1, _("Create AR Code"));
	button_cheat_search_copy_address->Bind(wxEVT_BUTTON, &CheatSearchTab::CreateARCode, this);

	// results groupbox
	wxStaticBoxSizer* const sizer_cheat_search_results = new wxStaticBoxSizer(wxVERTICAL, this, _("Results"));
	sizer_cheat_search_results->Add(label_results_count, 0, wxALIGN_LEFT | wxALL, 5);
	sizer_cheat_search_results->Add(lbox_search_results, 1, wxEXPAND | wxALL, 5);
	sizer_cheat_search_results->Add(button_cheat_search_copy_address, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 5);

	// Search value radio buttons
	value_x_radiobtn.rad_oldvalue = new wxRadioButton(this, -1, _("Previous Value"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
	value_x_radiobtn.rad_uservalue = new wxRadioButton(this, -1, "");
	value_x_radiobtn.rad_oldvalue->SetValue(true);

	// search value textbox
	textctrl_value_x = new wxTextCtrl(this, -1, "0x0", wxDefaultPosition, wxSize(96,-1));
	textctrl_value_x->Bind(wxEVT_SET_FOCUS, &CheatSearchTab::ApplyFocus, this);

	wxBoxSizer* const sizer_cheat_filter_text = new wxBoxSizer(wxHORIZONTAL);
	sizer_cheat_filter_text->Add(value_x_radiobtn.rad_uservalue, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, 5);
	sizer_cheat_filter_text->Add(textctrl_value_x, 1, wxALIGN_CENTER_VERTICAL, 5);

	// value groupbox
	wxStaticBoxSizer* const sizer_cheat_search_filter_x = new wxStaticBoxSizer(wxVERTICAL, this, _("Value"));
	sizer_cheat_search_filter_x->Add(value_x_radiobtn.rad_oldvalue, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);
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

	search_type = new wxChoice(this, -1, wxDefaultPosition, wxDefaultSize, sizeof(searches)/sizeof(*searches), searches);
	search_type->Select(0);

	wxStaticBoxSizer* const sizer_cheat_search_filter = new wxStaticBoxSizer(wxVERTICAL, this, _("Search Filter"));
	sizer_cheat_search_filter->Add(sizer_cheat_search_filter_x, 0, wxALL | wxEXPAND, 5);
	sizer_cheat_search_filter->Add(search_type, 0, wxALL, 5);

	// left sizer
	wxBoxSizer* const sizer_left = new wxBoxSizer(wxVERTICAL);
	sizer_left->Add(sizer_cheat_search_results, 1, wxEXPAND, 5);

	// button sizer
	wxBoxSizer* boxButtons = new wxBoxSizer(wxHORIZONTAL);
	boxButtons->Add(btnInitScan, 1, wxRIGHT, 5);
	boxButtons->Add(btnNextScan, 1);

	// right sizer
	wxBoxSizer* const sizer_right = new wxBoxSizer(wxVERTICAL);
	sizer_right->Add(sizer_cheat_new_search, 0, wxBOTTOM, 5);
	sizer_right->Add(sizer_cheat_search_filter, 0, wxEXPAND | wxBOTTOM, 5);
	sizer_right->AddStretchSpacer(1);
	sizer_right->Add(boxButtons, 0, wxTOP | wxEXPAND, 5);

	// main sizer
	wxBoxSizer* const sizer_main = new wxBoxSizer(wxHORIZONTAL);
	sizer_main->Add(sizer_left, 1, wxEXPAND | wxALL, 5);
	sizer_main->Add(sizer_right, 0, wxEXPAND | wxALL, 5);

	SetSizerAndFit(sizer_main);
}

void wxCheatsWindow::OnEvent_ButtonClose_Press(wxCommandEvent& WXUNUSED (event))
{
	Close();
}

void wxCheatsWindow::OnEvent_Close(wxCloseEvent& ev)
{
	Destroy();
}

// load codes for a new ISO ID
void wxCheatsWindow::UpdateGUI()
{
	// load code
	m_gameini_default = SConfig::GetInstance().m_LocalCoreStartupParameter.LoadDefaultGameIni();
	m_gameini_local = SConfig::GetInstance().m_LocalCoreStartupParameter.LoadLocalGameIni();
	m_gameini_local_path = SConfig::GetInstance().m_LocalCoreStartupParameter.m_strGameIniLocal;
	Load_ARCodes();
	Load_GeckoCodes();

	// enable controls
	button_apply->Enable(Core::IsRunning());

	wxString title = _("Cheats Manager");

	// write the ISO name in the title
	if (Core::IsRunning())
		SetTitle(title + ": " + SConfig::GetInstance().m_LocalCoreStartupParameter.GetUniqueID() + " - " + SConfig::GetInstance().m_LocalCoreStartupParameter.m_strName);
	else
		SetTitle(title);
}

void wxCheatsWindow::Load_ARCodes()
{
	using namespace ActionReplay;

	m_CheckListBox_CheatsList->Clear();

	if (!Core::IsRunning())
		return;

	indexList.clear();
	size_t size = GetCodeListSize();
	for (size_t i = 0; i < size; i++)
	{
		ARCode code = GetARCode(i);
		ARCodeIndex ind;
		u32 index = m_CheckListBox_CheatsList->Append(StrToWxStr(code.name));
		m_CheckListBox_CheatsList->Check(index, code.active);
		ind.index = i;
		ind.uiIndex = index;
		indexList.push_back(ind);
	}
}

void wxCheatsWindow::Load_GeckoCodes()
{
	m_geckocode_panel->LoadCodes(m_gameini_default, m_gameini_local, SConfig::GetInstance().m_LocalCoreStartupParameter.GetUniqueID(), true);
}

void wxCheatsWindow::OnEvent_CheatsList_ItemSelected(wxCommandEvent& WXUNUSED (event))
{
	using namespace ActionReplay;

	int index = m_CheckListBox_CheatsList->GetSelection();
	for (size_t i = 0; i < indexList.size(); i++)
	{
		if ((int)indexList[i].uiIndex == index)
		{
			ARCode code = GetARCode(i);
			m_Label_Codename->SetLabel(_("Name: ") + StrToWxStr(code.name));

			std::string numcodes = StringFromFormat("Number of Codes: %lu", (unsigned long)code.ops.size());
			m_Label_NumCodes->SetLabel(StrToWxStr(numcodes));
			m_ListBox_CodesList->Clear();

			for (const AREntry& entry : code.ops)
			{
				std::string ops = StringFromFormat("%08x %08x", entry.cmd_addr, entry.value);
				m_ListBox_CodesList->Append(StrToWxStr(ops));
			}
		}
	}
}

void wxCheatsWindow::OnEvent_CheatsList_ItemToggled(wxCommandEvent& WXUNUSED (event))
{
	int index = m_CheckListBox_CheatsList->GetSelection();
	for (const ARCodeIndex& code_index : indexList)
	{
		if ((int)code_index.uiIndex == index)
		{
			ActionReplay::SetARCode_IsActive(m_CheckListBox_CheatsList->IsChecked(index), code_index.index);
		}
	}
}

void wxCheatsWindow::OnEvent_ApplyChanges_Press(wxCommandEvent& ev)
{
	// Apply AR Code changes
	for (const ARCodeIndex& code_index : indexList)
	{
		ActionReplay::SetARCode_IsActive(m_CheckListBox_CheatsList->IsChecked(code_index.uiIndex), code_index.index);
	}

	// Apply Gecko Code changes
	Gecko::SetActiveCodes(m_geckocode_panel->GetCodes());

	// Save gameini, with changed gecko codes
	if (m_gameini_local_path.size())
	{
		Gecko::SaveCodes(m_gameini_local, m_geckocode_panel->GetCodes());
		m_gameini_local.Save(m_gameini_local_path);
	}

	ev.Skip();
}

void wxCheatsWindow::OnEvent_ButtonUpdateLog_Press(wxCommandEvent& WXUNUSED (event))
{
	m_TextCtrl_Log->Clear();
	for (const std::string& text : ActionReplay::GetSelfLog())
	{
		m_TextCtrl_Log->AppendText(StrToWxStr(text));
	}
}

void wxCheatsWindow::OnEvent_CheckBoxEnableLogging_StateChange(wxCommandEvent& WXUNUSED (event))
{
	ActionReplay::EnableSelfLogging(m_CheckBox_LogAR->IsChecked());
}

void CheatSearchTab::StartNewSearch(wxCommandEvent& WXUNUSED (event))
{
	const u8* const memptr = Memory::GetPointer(0);
	if (nullptr == memptr)
	{
		WxUtils::ShowErrorDialog(_("A game is not currently running."));
		return;
	}

	// Determine the user-selected data size for this search.
	search_type_size =
		size_radiobtn.rad_8->GetValue() +
		(size_radiobtn.rad_16->GetValue() << 1) +
		(size_radiobtn.rad_32->GetValue() << 2);

	// Set up the search results efficiently to prevent automatic re-allocations.
	search_results.clear();
	search_results.reserve(Memory::RAM_SIZE / search_type_size);

	// Enable the "Next Scan" button.
	btnNextScan->Enable();

	CheatSearchResult r;
	// can I assume cheatable values will be aligned like this?
	for (u32 addr = 0; addr != Memory::RAM_SIZE; addr += search_type_size)
	{
		r.address = addr;
		memcpy(&r.old_value, memptr + addr, search_type_size);
		search_results.push_back(r);
	}

	UpdateCheatSearchResultsList();
}

void CheatSearchTab::FilterCheatSearchResults(wxCommandEvent&)
{
	const u8* const memptr = Memory::GetPointer(0);
	if (nullptr == memptr)
	{
		WxUtils::ShowErrorDialog(_("A game is not currently running."));
		return;
	}

	// Set up the sub-search results efficiently to prevent automatic re-allocations.
	std::vector<CheatSearchResult> filtered_results;
	filtered_results.reserve(search_results.size());


	// Determine the selected filter
	// 1 : equal
	// 2 : greater-than
	// 4 : less-than

	const int filters[] = {7, 6, 1, 2, 4};
	int filter_mask = filters[search_type->GetSelection()];

	if (value_x_radiobtn.rad_oldvalue->GetValue()) // using old value comparison
	{
		for (CheatSearchResult& result : search_results)
		{
			// with big endian, can just use memcmp for ><= comparison
			int cmp_result = memcmp(memptr + result.address, &result.old_value, search_type_size);
			if (cmp_result < 0)
				cmp_result = 4;
			else
				cmp_result = cmp_result ? 2 : 1;

			if (cmp_result & filter_mask)
			{
				memcpy(&result.old_value, memptr + result.address, search_type_size);
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
			wxString x_val = textctrl_value_x->GetValue();

			if (!x_val.ToULong(&parsed_x_val, 0))
			{
				WxUtils::ShowErrorDialog(_("You must enter a valid decimal, hexadecimal or octal value."));
				return;
			}

			user_x_val = (u32)parsed_x_val;

			// #ifdef LIL_ENDIAN :p
			switch (search_type_size)
			{
			case 1 :
				break;
			case 2 :
				*(u16*)&user_x_val = Common::swap16((u8*)&user_x_val);
				break;
			case 4 :
				user_x_val = Common::swap32(user_x_val);
				break;
			}
			// #elseif BIG_ENDIAN
			// would have to move <u32 vals (8/16bit) to start of the user_x_val for the comparisons i use below
			// #endif
		}

		for (CheatSearchResult& result : search_results)
		{
			// with big endian, can just use memcmp for ><= comparison
			int cmp_result = memcmp(memptr + result.address, &user_x_val, search_type_size);
			if (cmp_result < 0)
				cmp_result = 4;
			else if (cmp_result)
				cmp_result = 2;
			else
				cmp_result = 1;

			if (cmp_result & filter_mask)
			{
				memcpy(&result.old_value, memptr + result.address, search_type_size);
				filtered_results.push_back(result);
			}
		}
	}

	search_results.swap(filtered_results);

	UpdateCheatSearchResultsList();
}

void CheatSearchTab::ApplyFocus(wxEvent& ev)
{
	ev.Skip();
	value_x_radiobtn.rad_uservalue->SetValue(true);
}

void CheatSearchTab::UpdateCheatSearchResultsList()
{
	lbox_search_results->Clear();

	wxString count_label = _("Count:") + wxString::Format(" %lu",
		(unsigned long)search_results.size());
	if (search_results.size() > MAX_CHEAT_SEARCH_RESULTS_DISPLAY)
	{
		count_label += _(" (too many to display)");
	}
	else
	{
		for (const CheatSearchResult& result : search_results)
		{
			u32 display_value = result.old_value;

			// #ifdef LIL_ENDIAN :p
			switch (search_type_size)
			{
			case 1 :
				break;
			case 2 :
				*(u16*)&display_value = Common::swap16((u8*)&display_value);
				break;
			case 4 :
				display_value = Common::swap32(display_value);
				break;
			}
			// #elseif BIG_ENDIAN
			// need to do some stuff in here (for 8 and 16bit) for bigendian
			// #endif
			std::string rowfmt = StringFromFormat("0x%%08x    0x%%0%ux    %%u/%%i", search_type_size*2);

			lbox_search_results->Append(
				wxString::Format(rowfmt.c_str(), result.address, display_value, display_value, display_value));
		}
	}

	label_results_count->SetLabel(count_label);
}

void CheatSearchTab::CreateARCode(wxCommandEvent&)
{
	const int sel = lbox_search_results->GetSelection();
	if (sel >= 0)
	{
		const u32 address = search_results[sel].address | ((search_type_size & ~1) << 24);

		CreateCodeDialog arcode_dlg(this, address);
		arcode_dlg.ShowModal();
	}
}

CreateCodeDialog::CreateCodeDialog(wxWindow* const parent, const u32 address)
	: wxDialog(parent, -1, _("Create AR Code"))
	, code_address(address)
{
	wxStaticText* const label_name = new wxStaticText(this, -1, _("Name: "));
	textctrl_name = new wxTextCtrl(this, -1, wxEmptyString, wxDefaultPosition, wxSize(256,-1));

	wxStaticText* const label_code = new wxStaticText(this, -1, _("Code: "));
	textctrl_code = new wxTextCtrl(this, -1, wxString::Format("0x%08x", address));
	textctrl_code->Disable();

	wxStaticText* const label_value = new wxStaticText(this, -1, _("Value: "));
	textctrl_value = new wxTextCtrl(this, -1, "0");

	checkbox_use_hex = new wxCheckBox(this, -1, _("Use Hex"));
	checkbox_use_hex->SetValue(true);

	wxBoxSizer* const sizer_value_label = new wxBoxSizer(wxHORIZONTAL);
	sizer_value_label->Add(label_value, 0, wxRIGHT, 5);
	sizer_value_label->Add(checkbox_use_hex);

	// main sizer
	wxBoxSizer* const sizer_main = new wxBoxSizer(wxVERTICAL);
	sizer_main->Add(label_name, 0, wxALL, 5);
	sizer_main->Add(textctrl_name, 0, wxALL, 5);
	sizer_main->Add(label_code, 0, wxALL, 5);
	sizer_main->Add(textctrl_code, 0, wxALL, 5);
	sizer_main->Add(sizer_value_label, 0, wxALL, 5);
	sizer_main->Add(textctrl_value, 0, wxALL, 5);
	sizer_main->Add(CreateButtonSizer(wxOK | wxCANCEL | wxNO_DEFAULT), 0, wxALL, 5);

	Bind(wxEVT_BUTTON, &CreateCodeDialog::PressOK, this, wxID_OK);
	Bind(wxEVT_BUTTON, &CreateCodeDialog::PressCancel, this, wxID_CANCEL);
	Bind(wxEVT_CLOSE_WINDOW, &CreateCodeDialog::OnEvent_Close, this);

	SetSizerAndFit(sizer_main);
	SetFocus();
}

void CreateCodeDialog::PressOK(wxCommandEvent& ev)
{
	const wxString code_name = textctrl_name->GetValue();
	if (code_name.empty())
	{
		WxUtils::ShowErrorDialog(_("You must enter a name."));
		return;
	}

	long code_value;
	int base = checkbox_use_hex->IsChecked() ? 16 : 10;
	if (!textctrl_value->GetValue().ToLong(&code_value, base))
	{
		WxUtils::ShowErrorDialog(_("Invalid value."));
		return;
	}

	//wxString full_code = textctrl_code->GetValue();
	//full_code += ' ';
	//full_code += wxString::Format("0x%08x", code_value);

	// create the new code
	ActionReplay::ARCode new_cheat;
	new_cheat.active = false;
	new_cheat.name = WxStrToStr(code_name);
	const ActionReplay::AREntry new_entry(code_address, code_value);
	new_cheat.ops.push_back(new_entry);

	// pretty hacky - add the code to the gameini
	{
	CISOProperties isoprops(SConfig::GetInstance().m_LastFilename, this);
	// add the code to the isoproperties arcode list
	arCodes.push_back(new_cheat);
	// save the gameini
	isoprops.SaveGameConfig();
	isoprops.ActionReplayList_Load(); // loads the new arcodes
	//ActionReplay::UpdateActiveList();
	}

	// refresh arcode list in other tab
	::g_cheat_window->Load_ARCodes();

	Close();
}

void CreateCodeDialog::PressCancel(wxCommandEvent& ev)
{
	Close();
}

void CreateCodeDialog::OnEvent_Close(wxCloseEvent& ev)
{
	Destroy();
}
