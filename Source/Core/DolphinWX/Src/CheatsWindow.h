// Copyright (C) 2003-2008 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#ifndef __CHEATSWINDOW_H__
#define __CHEATSWINDOW_H__

#include <wx/wx.h>
#include <wx/sizer.h>
#include <wx/filepicker.h>
#include <wx/statbmp.h>
#include <wx/imaglist.h>
#include <wx/treectrl.h>
#include <wx/gbsizer.h>
#include <wx/notebook.h>
#include <wx/mimetype.h>
#include <wx/colour.h>
#include <wx/listbox.h>
#include <string>

#include "ActionReplay.h"

#include "Filesystem.h"
#include "IniFile.h"

class wxCheatsWindow : public wxFrame
{
	public:

		wxCheatsWindow(wxFrame* parent, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize);

		virtual ~wxCheatsWindow();

	protected:

		struct ARCodeIndex {
			u32 uiIndex;
			size_t index;
		};

		// Event Table
		DECLARE_EVENT_TABLE();

		// --- GUI Controls ---
		wxGridBagSizer* m_Sizer_TabCheats;

		wxNotebook *m_Notebook_Main;

		wxPanel *m_Tab_Cheats;
		wxPanel *m_Tab_Log;

		wxButton *m_Button_Close;
		wxButton *m_Button_ApplyCodes;
		wxButton *m_Button_UpdateLog;

		wxCheckBox *m_CheckBox_LogAR;

		wxStaticText *m_Label_Codename;
		wxStaticText *m_Label_NumCodes;

		wxCheckListBox *m_CheckListBox_CheatsList;

		wxTextCtrl *m_TextCtrl_Log;

		wxListBox *m_ListBox_CodesList;

		wxStaticBox *m_GroupBox_Info;

		wxArrayString m_CheatStringList;

		std::vector<ARCodeIndex> indexList;

		// GUI IDs
		enum
		{
			ID_NOTEBOOK_MAIN,
			ID_TAB_CHEATS,
			ID_TAB_LOG,
			ID_BUTTON_CLOSE,
			ID_CHECKLISTBOX_CHEATSLIST,
			ID_LABEL_CODENAME,
			ID_GROUPBOX_INFO,
			ID_BUTTON_APPLYCODES,
			ID_LABEL_NUMCODES,
			ID_LISTBOX_CODESLIST,
			ID_BUTTON_UPDATELOG,
			ID_CHECKBOX_LOGAR,
			ID_TEXTCTRL_LOG
		};

		void Init_ChildControls();

		void Load_ARCodes();

		// --- Wx Events Handlers ---
		// $ Window
		void OnEvent_Window_Resize(wxSizeEvent& event);
		void OnEvent_Window_Close(wxCloseEvent& event);

		// $ Close Button
		void OnEvent_ButtonClose_Press(wxCommandEvent& event);

		// $ Cheats List
		void OnEvent_CheatsList_ItemSelected(wxCommandEvent& event);
		void OnEvent_CheatsList_ItemToggled(wxCommandEvent& event);

		// $ Update Active Codes Button
		void OnEvent_ButtonUpdateCodes_Press(wxCommandEvent& event);

		// $ Update Log Button
		void OnEvent_ButtonUpdateLog_Press(wxCommandEvent& event);

		// $ Enable Logging Checkbox
		void OnEvent_CheckBoxEnableLogging_StateChange(wxCommandEvent& event);
};

#endif