// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <string>
#include <vector>
#include <wx/arrstr.h>
#include <wx/dialog.h>
#include <wx/panel.h>

#include "Common/CommonTypes.h"
#include "Common/IniFile.h"

class wxButton;
class wxCheckBox;
class wxCheckListBox;
class wxCloseEvent;
class wxListBox;
class wxNotebook;
class wxStaticBox;
class wxStaticText;
class wxTextCtrl;

namespace Gecko
{
	class CodeConfigPanel;
}

wxDECLARE_EVENT(DOLPHIN_EVT_ADD_NEW_ACTION_REPLAY_CODE, wxCommandEvent);

class wxCheatsWindow final : public wxDialog
{
public:
	wxCheatsWindow(wxWindow* const parent);
	~wxCheatsWindow();
	void UpdateGUI();

private:
	struct CodeData;

	// --- GUI Controls ---
	wxButton* m_button_apply;
	wxNotebook* m_notebook_main;

	wxPanel* m_tab_cheats;
	wxPanel* m_tab_log;

	wxCheckBox* m_checkbox_log_ar;

	wxStaticText* m_label_code_name;
	wxStaticText* m_label_num_codes;

	wxCheckListBox* m_checklistbox_cheats_list;

	wxTextCtrl* m_textctrl_log;

	wxListBox* m_listbox_codes_list;

	wxStaticBox* m_groupbox_info;

	Gecko::CodeConfigPanel* m_geckocode_panel;
	IniFile m_gameini_default;
	IniFile m_gameini_local;
	std::string m_gameini_local_path;
	std::string m_game_id;
	u32 m_game_revision;

	bool m_ignore_ini_callback = false;

	void Init_ChildControls();

	void Load_ARCodes();
	void Load_GeckoCodes();

	// --- Wx Events Handlers ---
	// Cheat Search
	void OnNewARCodeCreated(wxCommandEvent& ev);

	// Close Button
	void OnEvent_ButtonClose_Press(wxCommandEvent& event);
	void OnEvent_Close(wxCloseEvent& ev);

	// Cheats List
	void OnEvent_CheatsList_ItemSelected(wxCommandEvent& event);
	void OnEvent_CheatsList_Update(wxCommandEvent& event);

	// Apply Changes Button
	void OnEvent_ApplyChanges_Press(wxCommandEvent& event);

	// Update Log Button
	void OnEvent_ButtonUpdateLog_Press(wxCommandEvent& event);
	void OnClearActionReplayLog(wxCommandEvent& event);

	// Enable Logging Checkbox
	void OnEvent_CheckBoxEnableLogging_StateChange(wxCommandEvent& event);
};
