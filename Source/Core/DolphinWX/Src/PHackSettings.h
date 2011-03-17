// Copyright (C) 2003 Dolphin Project.

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

#ifndef __PHACK_SETTINGS_h__
#define __PHACK_SETTINGS_h__

#include <wx/wx.h>
#include <wx/spinctrl.h>
#include "ISOProperties.h"

class CPHackSettings : public wxDialog
{
	public:
		CPHackSettings(wxWindow* parent,
			wxWindowID id = 1,
			const wxString& title = _("Custom Projection Hack Settings"),
			const wxPoint& pos = wxDefaultPosition,
			const wxSize& size = wxDefaultSize,
			long style = wxDEFAULT_DIALOG_STYLE);
		virtual ~CPHackSettings();

	private:
		DECLARE_EVENT_TABLE();

		wxChoice *PHackChoice;
		wxCheckBox *PHackSZNear;
		wxCheckBox *PHackSZFar;
		wxCheckBox *PHackExP;
		wxTextCtrl *PHackZNear;
		wxTextCtrl *PHackZFar;

		enum {
			ID_PHACK_CHOICE = 1000,
			ID_PHACK_SZNEAR,
			ID_PHACK_SZFAR,
			ID_PHACK_ZNEAR,
			ID_PHACK_ZFAR,
			ID_PHACK_EXP
		};

		IniFile PHPresetsIni;

		void SetRefresh(wxCommandEvent& event);
		void CreateGUIControls();

		void SavePHackData(wxCommandEvent& event);
		void LoadPHackData();
};

#endif // __PHACK_SETTINGS_h__
