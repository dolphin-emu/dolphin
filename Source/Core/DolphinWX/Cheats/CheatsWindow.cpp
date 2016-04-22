// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>
#include <wx/app.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/checklst.h>
#include <wx/dialog.h>
#include <wx/listbox.h>
#include <wx/msgdlg.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/StringUtil.h"
#include "Core/ActionReplay.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/GeckoCode.h"
#include "Core/GeckoCodeConfig.h"
#include "DolphinWX/Frame.h"
#include "DolphinWX/Globals.h"
#include "DolphinWX/Main.h"
#include "DolphinWX/WxUtils.h"
#include "DolphinWX/Cheats/CheatSearchTab.h"
#include "DolphinWX/Cheats/CheatsWindow.h"
#include "DolphinWX/Cheats/CreateCodeDialog.h"
#include "DolphinWX/Cheats/GeckoCodeDiag.h"

wxDEFINE_EVENT(DOLPHIN_EVT_ADD_NEW_ACTION_REPLAY_CODE, wxCommandEvent);

struct wxCheatsWindow::CodeData : public wxClientData
{
	ActionReplay::ARCode code;
};

wxCheatsWindow::wxCheatsWindow(wxWindow* const parent)
	: wxDialog(parent, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMAXIMIZE_BOX | wxMINIMIZE_BOX | wxDIALOG_NO_PARENT)
{
	// Create the GUI controls
	Init_ChildControls();

	// load codes
	UpdateGUI();
	wxTheApp->Bind(DOLPHIN_EVT_LOCAL_INI_CHANGED, &wxCheatsWindow::OnEvent_CheatsList_Update, this);

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

	m_checklistbox_cheats_list = new wxCheckListBox(m_tab_cheats, wxID_ANY, wxDefaultPosition, wxSize(300, 0), 0, nullptr, wxLB_HSCROLL);
	m_checklistbox_cheats_list->Bind(wxEVT_LISTBOX, &wxCheatsWindow::OnEvent_CheatsList_ItemSelected, this);

	m_label_code_name = new wxStaticText(m_tab_cheats, wxID_ANY, _("Name: "), wxDefaultPosition, wxDefaultSize, wxST_NO_AUTORESIZE);
	m_groupbox_info = new wxStaticBox(m_tab_cheats, wxID_ANY, _("Code Info"));

	m_label_num_codes = new wxStaticText(m_tab_cheats, wxID_ANY, _("Number Of Codes: "));
	m_listbox_codes_list = new wxListBox(m_tab_cheats, wxID_ANY, wxDefaultPosition, wxSize(120, 150), 0, nullptr, wxLB_HSCROLL);

	wxStaticBoxSizer* sGroupBoxInfo = new wxStaticBoxSizer(m_groupbox_info, wxVERTICAL);
	sGroupBoxInfo->Add(m_label_code_name, 0, wxEXPAND | wxALL, 5);
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
	wxButton* const button_clearlog = new wxButton(m_tab_log, wxID_ANY, _("Clear"));
	button_clearlog->Bind(wxEVT_BUTTON, &wxCheatsWindow::OnClearActionReplayLog, this);

	m_checkbox_log_ar = new wxCheckBox(m_tab_log, wxID_ANY, _("Enable AR Logging"));
	m_checkbox_log_ar->Bind(wxEVT_CHECKBOX, &wxCheatsWindow::OnEvent_CheckBoxEnableLogging_StateChange, this);

	m_checkbox_log_ar->SetValue(ActionReplay::IsSelfLogging());
	m_textctrl_log = new wxTextCtrl(m_tab_log, wxID_ANY, "", wxDefaultPosition, wxSize(100, -1), wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);

	wxBoxSizer *HStrip1 = new wxBoxSizer(wxHORIZONTAL);
	HStrip1->Add(m_checkbox_log_ar, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
	HStrip1->Add(button_updatelog, 0, wxALL, 5);
	HStrip1->Add(button_clearlog, 0, wxALL, 5);

	wxBoxSizer *sTabLog = new wxBoxSizer(wxVERTICAL);
	sTabLog->Add(HStrip1, 0, wxALL, 5);
	sTabLog->Add(m_textctrl_log, 1, wxALL | wxEXPAND, 5);

	m_tab_log->SetSizerAndFit(sTabLog);

	// Add Tabs to Notebook
	m_notebook_main->AddPage(m_tab_cheats, _("AR Codes"));
	m_geckocode_panel = new Gecko::CodeConfigPanel(m_notebook_main);
	m_notebook_main->AddPage(m_geckocode_panel, _("Gecko Codes"));
	m_notebook_main->AddPage(tab_cheat_search, _("Cheat Search"));
	m_notebook_main->AddPage(m_tab_log, _("Logging"));

	Bind(wxEVT_BUTTON, &wxCheatsWindow::OnEvent_ApplyChanges_Press, this, wxID_APPLY);
	Bind(wxEVT_BUTTON, &wxCheatsWindow::OnEvent_ButtonClose_Press, this, wxID_CANCEL);
	Bind(wxEVT_CLOSE_WINDOW, &wxCheatsWindow::OnEvent_Close, this);
	Bind(DOLPHIN_EVT_ADD_NEW_ACTION_REPLAY_CODE, &wxCheatsWindow::OnNewARCodeCreated, this);

	wxStdDialogButtonSizer* const sButtons = CreateStdDialogButtonSizer(wxAPPLY | wxCANCEL);
	m_button_apply = sButtons->GetApplyButton();
	SetEscapeId(wxID_CANCEL);
	SetAffirmativeId(wxID_CANCEL);

	wxBoxSizer* const sMain = new wxBoxSizer(wxVERTICAL);
	sMain->Add(m_notebook_main, 1, wxEXPAND | wxALL, 5);
	sMain->Add(sButtons, 0, wxRIGHT | wxBOTTOM | wxALIGN_RIGHT, 5);
	SetSizerAndFit(sMain);
}

void wxCheatsWindow::OnEvent_ButtonClose_Press(wxCommandEvent& WXUNUSED(event))
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
	const SConfig& parameters = SConfig::GetInstance();
	m_gameini_default = parameters.LoadDefaultGameIni();
	m_gameini_local = parameters.LoadLocalGameIni();
	m_game_id = parameters.GetUniqueID();
	m_game_revision = parameters.m_revision;
	m_gameini_local_path = File::GetUserPath(D_GAMESETTINGS_IDX) + m_game_id + ".ini";
	Load_ARCodes();
	Load_GeckoCodes();

	// enable controls
	m_button_apply->Enable(Core::IsRunning());

	wxString title = _("Cheat Manager");

	// write the ISO name in the title
	if (Core::IsRunning())
		SetTitle(title + StrToWxStr(": " + m_game_id + " - " + parameters.m_strName));
	else
		SetTitle(title);
}

void wxCheatsWindow::Load_ARCodes()
{
	m_checklistbox_cheats_list->Clear();

	if (!Core::IsRunning())
		return;

	m_checklistbox_cheats_list->Freeze();
	for (auto& code : ActionReplay::LoadCodes(m_gameini_default, m_gameini_local))
	{
		CodeData* cd = new CodeData();
		cd->code = std::move(code);
		int index = m_checklistbox_cheats_list->Append(wxCheckListBox::EscapeMnemonics(StrToWxStr(cd->code.name)),
		                                               cd);
		m_checklistbox_cheats_list->Check(index, cd->code.active);
	}
	m_checklistbox_cheats_list->Thaw();
}

void wxCheatsWindow::Load_GeckoCodes()
{
	m_geckocode_panel->LoadCodes(m_gameini_default, m_gameini_local, SConfig::GetInstance().GetUniqueID(), true);
}

void wxCheatsWindow::OnNewARCodeCreated(wxCommandEvent& ev)
{
	auto code = static_cast<ActionReplay::ARCode*>(ev.GetClientData());
	ActionReplay::AddCode(*code);

	CodeData* cd = new CodeData();
	cd->code = *code;
	int idx = m_checklistbox_cheats_list->Append(wxCheckListBox::EscapeMnemonics(StrToWxStr(code->name)),
	                                             cd);
	m_checklistbox_cheats_list->Check(idx, code->active);
}

void wxCheatsWindow::OnEvent_CheatsList_ItemSelected(wxCommandEvent& event)
{
	CodeData* cd = static_cast<CodeData*>(event.GetClientObject());

	m_label_code_name->SetLabelText(_("Name: ") + StrToWxStr(cd->code.name));
	m_label_code_name->Wrap(m_label_code_name->GetSize().GetWidth());
	m_label_code_name->InvalidateBestSize();
	m_label_num_codes->SetLabelText(wxString::Format("%s%zu", _("Number Of Codes: "), cd->code.ops.size()));

	m_listbox_codes_list->Freeze();
	m_listbox_codes_list->Clear();
	for (const ActionReplay::AREntry& entry : cd->code.ops)
	{
		m_listbox_codes_list->Append(wxString::Format("%08x %08x", entry.cmd_addr, entry.value));
	}
	m_listbox_codes_list->Thaw();

	m_tab_cheats->Layout();
}

void wxCheatsWindow::OnEvent_CheatsList_Update(wxCommandEvent& ev)
{
	ev.Skip();
	if (WxStrToStr(ev.GetString()) != m_game_id)
		return;
	if (m_ignore_ini_callback)
	{
		m_ignore_ini_callback = false;
		return;
	}
	UpdateGUI();
}

void wxCheatsWindow::OnEvent_ApplyChanges_Press(wxCommandEvent& ev)
{
	// Convert embedded metadata back into ARCode vector and update active states
	std::vector<ActionReplay::ARCode> code_vec;
	code_vec.reserve(m_checklistbox_cheats_list->GetCount());
	for (unsigned int i = 0; i < m_checklistbox_cheats_list->GetCount(); ++i)
	{
		CodeData* cd = static_cast<CodeData*>(m_checklistbox_cheats_list->GetClientObject(i));
		cd->code.active = m_checklistbox_cheats_list->IsChecked(i);
		code_vec.push_back(cd->code);
	}

	// Apply Action Replay code changes
	ActionReplay::ApplyCodes(code_vec);

	// Apply Gecko Code changes
	Gecko::SetActiveCodes(m_geckocode_panel->GetCodes());

	// Save gameini, with changed codes
	if (m_gameini_local_path.size())
	{
		ActionReplay::SaveCodes(&m_gameini_local, code_vec);
		Gecko::SaveCodes(m_gameini_local, m_geckocode_panel->GetCodes());
		m_gameini_local.Save(m_gameini_local_path);

		wxCommandEvent ini_changed(DOLPHIN_EVT_LOCAL_INI_CHANGED);
		ini_changed.SetString(StrToWxStr(m_game_id));
		ini_changed.SetInt(m_game_revision);
		m_ignore_ini_callback = true;
		wxTheApp->ProcessEvent(ini_changed);
	}

	ev.Skip();
}

void wxCheatsWindow::OnEvent_ButtonUpdateLog_Press(wxCommandEvent& WXUNUSED(event))
{
	wxBeginBusyCursor();
	m_textctrl_log->Freeze();
	m_textctrl_log->Clear();
	for (const std::string& text : ActionReplay::GetSelfLog())
	{
		m_textctrl_log->AppendText(StrToWxStr(text));
	}
	m_textctrl_log->Thaw();
	wxEndBusyCursor();
}

void wxCheatsWindow::OnClearActionReplayLog(wxCommandEvent& event)
{
	ActionReplay::ClearSelfLog();
	OnEvent_ButtonUpdateLog_Press(event);
}

void wxCheatsWindow::OnEvent_CheckBoxEnableLogging_StateChange(wxCommandEvent& WXUNUSED(event))
{
	ActionReplay::EnableSelfLogging(m_checkbox_log_ar->IsChecked());
}
