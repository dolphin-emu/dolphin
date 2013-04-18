// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

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
