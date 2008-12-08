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

#ifndef __CDebugger_h__
#define __CDebugger_h__


// wx stuff, I'm not sure if we use all these
#ifndef WX_PRECOMP
	#include <wx/wx.h>
	#include <wx/dialog.h>
#else
	#include <wx/wxprec.h>
#endif

#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/statbox.h>
#include <wx/statbmp.h>

#include <wx/sizer.h>
#include <wx/filepicker.h>
#include <wx/listctrl.h>
#include <wx/imaglist.h>
#include <wx/notebook.h> // notebook

#include "../Globals.h"

class CPBView;
class IniFile;

// Window settings - I'm not sure what these do. I just copied them gtom elsewhere basically.
#undef CDebugger_STYLE

#define CDebugger_STYLE wxDEFAULT_FRAME_STYLE | wxCLIP_CHILDREN | wxNO_FULL_REPAINT_ON_RESIZE

class CDebugger : public wxDialog
{
	private:
		DECLARE_EVENT_TABLE();
		
	public:
		CDebugger(wxWindow *parent, wxWindowID id = 1, const wxString &title = _("Sound Debugger"),
			const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
			long style = CDebugger_STYLE);

		virtual ~CDebugger();

		void Save(IniFile& _IniFile) const;
		void Load(IniFile& _IniFile);
		void DoHide(); void DoShow();

		void NotifyUpdate();
		void OnUpdate(wxCommandEvent& event);

		void GeneralSettings(wxCommandEvent& event);
		void LogSettings(wxCommandEvent& event);
		void DoShowHideConsole();
		void ChangeFrequency(wxCommandEvent& event);
		void DoChangeFrequency();
		void ChangePreset(wxCommandEvent& event);
		void DoChangePreset();	

		void Ap(wxCommandEvent& event);
		void Am(wxCommandEvent& event);
		void Bp(wxCommandEvent& event);
		void Bm(wxCommandEvent& event);

		CPBView* m_GPRListView;

		int gUpdFreq;
		bool bInfoLog;
		bool bPrimLog;
		

	private:

		// declarations
		wxNotebook *m_Notebook; // notebook
		wxPanel *m_PageMain;

		wxCheckBox *m_Check[7];
		wxCheckListBox * m_options, * m_settings;
		wxRadioButton *m_Radio[5];
		wxRadioBox *m_RadioBox[3];
		wxStaticBox *m_Label[2];
		wxPanel *m_Controller;

		// WARNING: Make sure these are not also elsewhere, for example in resource.h.
		enum
		{
			ID_NOTEBOOK  = 2000, ID_PAGEMAIN, // notebook

			ID_SAVETOFILE, ID_SHOWCONSOLE, // options
			IDC_CHECK2,
			IDC_CHECK3,
			IDC_CHECK4,
			IDC_CHECK5,
			IDC_CHECK6,
			IDC_CHECK7,
			IDC_CHECK8,
			IDC_CHECK9,

			ID_CHECKLIST1,

			IDC_RADIO0,
			IDC_RADIO1,
			IDC_RADIO2,
			IDC_RADIO3,

			IDG_LABEL1,
			IDG_LABEL2,

			ID_UPD,
			ID_AP,
			ID_AM,
			ID_BP,
			ID_BM,
			ID_GPR,
			ID_DUMMY_VALUE_ //don't remove this value unless you have other enum values

		};
		
		void OnShow(wxShowEvent& event);
		void OnClose(wxCloseEvent& event);		
		void CreateGUIControls();		
};

#endif
