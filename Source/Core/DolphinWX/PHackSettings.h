// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/string.h>
#include <wx/translation.h>
#include <wx/windowid.h>

#include "Common/IniFile.h"

class wxCheckBox;
class wxChoice;
class wxTextCtrl;
class wxWindow;

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
		wxTextCtrl *PHackZNear;
		wxTextCtrl *PHackZFar;

		enum {
			ID_PHACK_CHOICE = 1000,
			ID_PHACK_SZNEAR,
			ID_PHACK_SZFAR,
			ID_PHACK_ZNEAR,
			ID_PHACK_ZFAR,
		};

		IniFile PHPresetsIni;

		void SetRefresh(wxCommandEvent& event);
		void CreateGUIControls();

		void SavePHackData(wxCommandEvent& event);
		void LoadPHackData();
};
