// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef __CHEATSWINDOW_H__
#define __CHEATSWINDOW_H__

#include <wx/wx.h>
#include <wx/sizer.h>
#include <wx/filepicker.h>
#include <wx/statbmp.h>
#include <wx/imaglist.h>
#include <wx/treectrl.h>
#include <wx/gbsizer.h>
#include <wx/textctrl.h>
#include <wx/notebook.h>
#include <wx/mimetype.h>
#include <wx/colour.h>
#include <wx/listbox.h>

#include <string>
#include <vector>

#include "ActionReplay.h"

#include "GeckoCodeDiag.h"

#include "Filesystem.h"
#include "IniFile.h"

class CreateCodeDialog : public wxDialog
{
public:
	CreateCodeDialog(wxWindow* const parent, const u32 address);

protected:

	const u32 code_address;

	wxTextCtrl *textctrl_name, *textctrl_code, *textctrl_value;
	wxCheckBox *checkbox_use_hex;

	void PressOK(wxCommandEvent&);
	void PressCancel(wxCommandEvent&);
	void OnEvent_Close(wxCloseEvent& ev);
};

class CheatSearchTab : public wxPanel
{
public:
	CheatSearchTab(wxWindow* const parent);

protected:

	class CheatSearchResult
	{
	public:
		CheatSearchResult() : address(0), old_value(0) {}

		u32 address;
		u32 old_value;
	};

	std::vector<CheatSearchResult>	search_results;
	unsigned int search_type_size;

	wxChoice* search_type;
	wxListBox*	lbox_search_results;
	wxStaticText* label_results_count;
	wxTextCtrl*	textctrl_value_x;
	wxButton *btnInitScan, *btnNextScan;

	struct
	{
		wxRadioButton *rad_8, *rad_16, *rad_32;

	} size_radiobtn;

	struct
	{
		wxRadioButton *rad_oldvalue, *rad_uservalue;

	} value_x_radiobtn;

	void UpdateCheatSearchResultsList();
	void StartNewSearch(wxCommandEvent& event);
	void FilterCheatSearchResults(wxCommandEvent& event);
	void CreateARCode(wxCommandEvent&);
	void ApplyFocus(wxEvent&);
};

class wxCheatsWindow : public wxDialog
{
	friend class CreateCodeDialog;

	public:
		wxCheatsWindow(wxWindow* const parent);
		~wxCheatsWindow();
		void UpdateGUI();

	protected:

		struct ARCodeIndex {
			u32 uiIndex;
			size_t index;
		};

		// --- GUI Controls ---
		wxButton* button_apply;
		wxNotebook *m_Notebook_Main;

		wxPanel *m_Tab_Cheats;
		wxPanel *m_Tab_Log;

		wxCheckBox *m_CheckBox_LogAR;

		wxStaticText *m_Label_Codename;
		wxStaticText *m_Label_NumCodes;

		wxCheckListBox *m_CheckListBox_CheatsList;

		wxTextCtrl *m_TextCtrl_Log;

		wxListBox *m_ListBox_CodesList;

		wxStaticBox *m_GroupBox_Info;

		wxArrayString m_CheatStringList;

		std::vector<ARCodeIndex> indexList;

		Gecko::CodeConfigPanel *m_geckocode_panel;
		IniFile m_gameini;
		std::string m_gameini_path;

		void Init_ChildControls();

		void Load_ARCodes();
		void Load_GeckoCodes();

		// --- Wx Events Handlers ---

		// $ Close Button
		void OnEvent_ButtonClose_Press(wxCommandEvent& event);
		void OnEvent_Close(wxCloseEvent& ev);

		// $ Cheats List
		void OnEvent_CheatsList_ItemSelected(wxCommandEvent& event);
		void OnEvent_CheatsList_ItemToggled(wxCommandEvent& event);

		// $ Apply Changes Button
		void OnEvent_ApplyChanges_Press(wxCommandEvent& event);

		// $ Update Log Button
		void OnEvent_ButtonUpdateLog_Press(wxCommandEvent& event);

		// $ Enable Logging Checkbox
		void OnEvent_CheckBoxEnableLogging_StateChange(wxCommandEvent& event);
};

#endif

