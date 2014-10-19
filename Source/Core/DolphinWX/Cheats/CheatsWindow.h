// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
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
class wxChoice;
class wxCloseEvent;
class wxCommandEvent;
class wxEvent;
class wxListBox;
class wxNotebook;
class wxRadioButton;
class wxStaticBox;
class wxStaticText;
class wxTextCtrl;
class wxWindow;

namespace Gecko
{
	class CodeConfigPanel;
}

class wxCheatsWindow final : public wxDialog
{
public:
	wxCheatsWindow(wxWindow* const parent);
	~wxCheatsWindow();
	void UpdateGUI();

private:
	struct ARCodeIndex
	{
		u32 uiIndex;
		size_t index;
	};

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

	wxArrayString m_cheat_string_list;

	std::vector<ARCodeIndex> m_index_list;

	Gecko::CodeConfigPanel* m_geckocode_panel;
	IniFile m_gameini_default;
	IniFile m_gameini_local;
	std::string m_gameini_local_path;

	void Init_ChildControls();

	void Load_ARCodes();
	void Load_GeckoCodes();

	// --- Wx Events Handlers ---

	// Close Button
	void OnEvent_ButtonClose_Press(wxCommandEvent& event);
	void OnEvent_Close(wxCloseEvent& ev);

	// Cheats List
	void OnEvent_CheatsList_ItemSelected(wxCommandEvent& event);
	void OnEvent_CheatsList_ItemToggled(wxCommandEvent& event);
	void OnEvent_CheatsList_Update(wxCommandEvent& event);

	// Apply Changes Button
	void OnEvent_ApplyChanges_Press(wxCommandEvent& event);

	// Update Log Button
	void OnEvent_ButtonUpdateLog_Press(wxCommandEvent& event);

	// Enable Logging Checkbox
	void OnEvent_CheckBoxEnableLogging_StateChange(wxCommandEvent& event);
};
