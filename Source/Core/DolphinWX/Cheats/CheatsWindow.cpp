// Copyright 2014 Dolphin Emulator Project
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
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/listbox.h>
#include <wx/msgdlg.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/string.h>
#include <wx/textctrl.h>
#include <wx/translation.h>
#include <wx/validate.h>
#include <wx/window.h>

#include "Common/CommonTypes.h"
#include "Common/IniFile.h"
#include "Common/StringUtil.h"
#include "Core/ActionReplay.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreParameter.h"
#include "Core/GeckoCode.h"
#include "Core/GeckoCodeConfig.h"
#include "DolphinWX/Frame.h"
#include "DolphinWX/Main.h"
#include "DolphinWX/WxUtils.h"
#include "DolphinWX/Cheats/CheatSearchTab.h"
#include "DolphinWX/Cheats/CheatsWindow.h"
#include "DolphinWX/Cheats/CreateCodeDialog.h"
#include "DolphinWX/Cheats/GeckoCodeDiag.h"

wxCheatsWindow::wxCheatsWindow(wxWindow* const parent)
	: wxDialog(parent, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER|wxMAXIMIZE_BOX|wxMINIMIZE_BOX|wxDIALOG_NO_PARENT)
{
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
}

void wxCheatsWindow::Init_ChildControls()
{
	// Main Notebook
	m_notebook_main = new wxNotebook(this, wxID_ANY);

	// --- Tabs ---
	// Cheats List Tab
	m_tab_cheats = new wxPanel(m_notebook_main, wxID_ANY);

	m_checklistbox_cheats_list = new wxCheckListBox(m_tab_cheats, wxID_ANY, wxDefaultPosition, wxSize(300, 0), m_cheat_string_list, wxLB_HSCROLL, wxDefaultValidator);
	m_checklistbox_cheats_list->Bind(wxEVT_LISTBOX, &wxCheatsWindow::OnEvent_CheatsList_ItemSelected, this);
	m_checklistbox_cheats_list->Bind(wxEVT_CHECKLISTBOX, &wxCheatsWindow::OnEvent_CheatsList_ItemToggled, this);

	m_label_code_name = new wxStaticText(m_tab_cheats, wxID_ANY, _("Name: "));
	m_groupbox_info = new wxStaticBox(m_tab_cheats, wxID_ANY, _("Code Info"));

	m_label_num_codes = new wxStaticText(m_tab_cheats, wxID_ANY, _("Number Of Codes: "));
	m_listbox_codes_list = new wxListBox(m_tab_cheats, wxID_ANY, wxDefaultPosition, wxSize(120, 150), 0, nullptr, wxLB_HSCROLL);

	wxStaticBoxSizer* sGroupBoxInfo = new wxStaticBoxSizer(m_groupbox_info, wxVERTICAL);
	sGroupBoxInfo->Add(m_label_code_name, 0, wxALL, 5);
	sGroupBoxInfo->Add(m_label_num_codes, 0, wxALL, 5);
	sGroupBoxInfo->Add(m_listbox_codes_list, 1, wxALL, 5);

	wxBoxSizer* sizer_tab_cheats = new wxBoxSizer(wxHORIZONTAL);
	sizer_tab_cheats->Add(m_checklistbox_cheats_list, 1, wxEXPAND | wxTOP | wxBOTTOM | wxLEFT, 10);
	sizer_tab_cheats->Add(sGroupBoxInfo, 0, wxALIGN_LEFT | wxEXPAND | wxALL, 5);

	m_tab_cheats->SetSizerAndFit(sizer_tab_cheats);

	// Cheat Search Tab
	wxPanel* const tab_cheat_search = new CheatSearchTab(m_notebook_main);

	// Log Tab
	m_tab_log = new wxPanel(m_notebook_main, wxID_ANY);

	wxButton* const button_updatelog = new wxButton(m_tab_log, wxID_ANY, _("Update"));
	button_updatelog->Bind(wxEVT_BUTTON, &wxCheatsWindow::OnEvent_ButtonUpdateLog_Press, this);

	m_checkbox_log_ar = new wxCheckBox(m_tab_log, wxID_ANY, _("Enable AR Logging"));
	m_checkbox_log_ar->Bind(wxEVT_CHECKBOX, &wxCheatsWindow::OnEvent_CheckBoxEnableLogging_StateChange, this);

	m_checkbox_log_ar->SetValue(ActionReplay::IsSelfLogging());
	m_textctrl_log = new wxTextCtrl(m_tab_log, wxID_ANY, "", wxDefaultPosition, wxSize(100, -1), wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);

	wxBoxSizer *HStrip1 = new wxBoxSizer(wxHORIZONTAL);
	HStrip1->Add(m_checkbox_log_ar, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5);
	HStrip1->Add(button_updatelog, 0, wxALL, 5);

	wxBoxSizer *sTabLog = new wxBoxSizer(wxVERTICAL);
	sTabLog->Add(HStrip1, 0, wxALL, 5);
	sTabLog->Add(m_textctrl_log, 1, wxALL|wxEXPAND, 5);

	m_tab_log->SetSizerAndFit(sTabLog);

	// Add Tabs to Notebook
	m_notebook_main->AddPage(m_tab_cheats, _("AR Codes"));
	m_geckocode_panel = new Gecko::CodeConfigPanel(m_notebook_main);
	m_notebook_main->AddPage(m_geckocode_panel, _("Gecko Codes"));
	m_notebook_main->AddPage(tab_cheat_search, _("Cheat Search"));
	m_notebook_main->AddPage(m_tab_log, _("Logging"));

	// Button Strip
	m_button_apply = new wxButton(this, wxID_APPLY, _("Apply"));
	m_button_apply->Bind(wxEVT_BUTTON, &wxCheatsWindow::OnEvent_ApplyChanges_Press, this);
	wxButton* const button_cancel = new wxButton(this, wxID_CANCEL, _("Cancel"));
	button_cancel->Bind(wxEVT_BUTTON, &wxCheatsWindow::OnEvent_ButtonClose_Press, this);

	Bind(wxEVT_CLOSE_WINDOW, &wxCheatsWindow::OnEvent_Close, this);
	Bind(UPDATE_CHEAT_LIST_EVENT, &wxCheatsWindow::OnEvent_CheatsList_Update, this);

	wxStdDialogButtonSizer* const sButtons = new wxStdDialogButtonSizer();
	sButtons->AddButton(m_button_apply);
	sButtons->AddButton(button_cancel);
	sButtons->Realize();

	wxBoxSizer* const sMain = new wxBoxSizer(wxVERTICAL);
	sMain->Add(m_notebook_main, 1, wxEXPAND|wxALL, 5);
	sMain->Add(sButtons, 0, wxRIGHT | wxBOTTOM | wxALIGN_RIGHT, 5);
	SetSizerAndFit(sMain);
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
	m_button_apply->Enable(Core::IsRunning());

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

	m_checklistbox_cheats_list->Clear();

	if (!Core::IsRunning())
		return;

	m_index_list.clear();
	size_t size = GetCodeListSize();
	for (size_t i = 0; i < size; i++)
	{
		ARCode code = GetARCode(i);
		ARCodeIndex ind;
		u32 index = m_checklistbox_cheats_list->Append(StrToWxStr(code.name));
		m_checklistbox_cheats_list->Check(index, code.active);
		ind.index = i;
		ind.uiIndex = index;
		m_index_list.push_back(ind);
	}
}

void wxCheatsWindow::Load_GeckoCodes()
{
	m_geckocode_panel->LoadCodes(m_gameini_default, m_gameini_local, SConfig::GetInstance().m_LocalCoreStartupParameter.GetUniqueID(), true);
}

void wxCheatsWindow::OnEvent_CheatsList_ItemSelected(wxCommandEvent& WXUNUSED (event))
{
	using namespace ActionReplay;

	int index = m_checklistbox_cheats_list->GetSelection();
	for (size_t i = 0; i < m_index_list.size(); i++)
	{
		if ((int)m_index_list[i].uiIndex == index)
		{
			ARCode code = GetARCode(i);
			m_label_code_name->SetLabel(_("Name: ") + StrToWxStr(code.name));

			std::string numcodes = StringFromFormat("Number of Codes: %lu", (unsigned long)code.ops.size());
			m_label_num_codes->SetLabel(StrToWxStr(numcodes));
			m_listbox_codes_list->Clear();

			for (const AREntry& entry : code.ops)
			{
				std::string ops = StringFromFormat("%08x %08x", entry.cmd_addr, entry.value);
				m_listbox_codes_list->Append(StrToWxStr(ops));
			}
		}
	}
}

void wxCheatsWindow::OnEvent_CheatsList_ItemToggled(wxCommandEvent& WXUNUSED (event))
{
	int index = m_checklistbox_cheats_list->GetSelection();
	for (const ARCodeIndex& code_index : m_index_list)
	{
		if ((int)code_index.uiIndex == index)
		{
			ActionReplay::SetARCode_IsActive(m_checklistbox_cheats_list->IsChecked(index), code_index.index);
		}
	}
}

void wxCheatsWindow::OnEvent_CheatsList_Update(wxCommandEvent& event)
{
	Load_ARCodes();
}

void wxCheatsWindow::OnEvent_ApplyChanges_Press(wxCommandEvent& ev)
{
	// Apply AR Code changes
	for (const ARCodeIndex& code_index : m_index_list)
	{
		ActionReplay::SetARCode_IsActive(m_checklistbox_cheats_list->IsChecked(code_index.uiIndex), code_index.index);
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
	m_textctrl_log->Clear();
	for (const std::string& text : ActionReplay::GetSelfLog())
	{
		m_textctrl_log->AppendText(StrToWxStr(text));
	}
}

void wxCheatsWindow::OnEvent_CheckBoxEnableLogging_StateChange(wxCommandEvent& WXUNUSED (event))
{
	ActionReplay::EnableSelfLogging(m_checkbox_log_ar->IsChecked());
}
