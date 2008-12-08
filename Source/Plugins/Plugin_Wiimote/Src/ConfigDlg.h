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

#ifndef __CONFIGDIALOG_h__
#define __CONFIGDIALOG_h__

#include <wx/wx.h>
#include <wx/dialog.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/combobox.h>
#include <wx/checkbox.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/filepicker.h>
#include <wx/gbsizer.h>


#undef ConfigDialog_STYLE
#define ConfigDialog_STYLE wxCAPTION | wxSYSTEM_MENU | wxDIALOG_NO_PARENT | wxCLOSE_BOX


class ConfigDialog : public wxDialog
{
	public:
		ConfigDialog(wxWindow *parent, wxWindowID id = 1,
			const wxString &title = wxT("Wii Remote Plugin Configuration"),
			const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
			long style = ConfigDialog_STYLE);
		virtual ~ConfigDialog();
		void CloseClick(wxCommandEvent& event);

	private:
		DECLARE_EVENT_TABLE();

		wxBoxSizer* sGeneral;
		wxStaticBoxSizer* sbBasic;
		wxGridBagSizer* sBasic;
		
		wxButton *m_About;
		wxButton *m_Close;
		wxNotebook *m_Notebook;
		wxPanel *m_PageEmu, *m_PageReal;

		wxCheckBox *m_SidewaysDPad; // general settings
		wxCheckBox *m_WideScreen;
		wxCheckBox *m_NunchuckConnected, *m_ClassicControllerConnected;

		enum
		{
			ID_CLOSE = 1000,
			ID_ABOUTOGL,

			ID_NOTEBOOK,
			ID_PAGEEMU,
			ID_PAGEREAL,

			ID_SIDEWAYSDPAD,
			ID_WIDESCREEN,
			ID_NUNCHUCKCONNECTED, ID_CLASSICCONTROLLERCONNECTED
		};

		void OnClose(wxCloseEvent& event);
		void CreateGUIControls();

		void AboutClick(wxCommandEvent& event);
		void DoExtensionConnectedDisconnected();
		void GeneralSettingsChanged(wxCommandEvent& event);
};

#endif
