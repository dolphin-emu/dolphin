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

#ifndef __CONFIGDLG_H__
#define __CONFIGDLG_H__

#include <wx/wx.h>
#include <wx/dialog.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/combobox.h>
#include <wx/checkbox.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/gbsizer.h>


#undef CONFIGDIALOG_STYLE
#define CONFIGDIALOG_STYLE wxCAPTION | wxSYSTEM_MENU | wxDIALOG_NO_PARENT | wxCLOSE_BOX


class ConfigDialog : public wxDialog
{

		
	public:
		ConfigDialog(wxWindow *parent, wxWindowID id = 1, const wxString &title = wxT("Pad Configuration"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = CONFIGDIALOG_STYLE);
		virtual ~ConfigDialog();

        private:
		DECLARE_EVENT_TABLE();
		wxNotebook *m_Notebook;
		wxPanel *m_Controller[4];
		wxButton *m_Close;

		wxStaticBoxSizer *sDevice[4];
		wxBoxSizer *sDeviceTop[4];
		wxBoxSizer *sDeviceBottom[4];
		wxGridBagSizer* sPage[4];
		wxStaticBoxSizer *sButtons[4];
                wxBoxSizer *hButtons[4][2];
		wxStaticBoxSizer *sTriggerL[4];
		wxStaticBoxSizer *sTriggerR[4];
		wxStaticBoxSizer *sStick[4];
		wxStaticBoxSizer *sCStick[4];
		wxStaticBoxSizer *sDPad[4];

		wxChoice *m_DeviceName[4];
		wxCheckBox *m_Attached[4];
		wxCheckBox *m_Disable[4];
		wxCheckBox *m_Rumble[4];

		wxButton *m_ButtonA[4];
		wxButton *m_ButtonB[4];
		wxButton *m_ButtonX[4];
		wxButton *m_ButtonY[4];
		wxButton *m_ButtonZ[4];
		wxButton *m_ButtonStart[4];
		wxButton *m_TriggerL[4];
		wxButton *m_ButtonL[4];
		wxButton *m_TriggerR[4];
		wxButton *m_ButtonR[4];
		wxButton *m_StickUp[4];
		wxButton *m_StickDown[4];
		wxButton *m_StickLeft[4];
		wxButton *m_StickRight[4];
		wxButton *m_CStickUp[4];
		wxButton *m_CStickDown[4];
		wxButton *m_CStickLeft[4];
		wxButton *m_CStickRight[4];
		wxButton *m_DPadUp[4];
		wxButton *m_DPadDown[4];
		wxButton *m_DPadLeft[4];
		wxButton *m_DPadRight[4];
		
		enum
		{
			////GUI Enum Control ID Start
			ID_CLOSE = 1000,
			ID_NOTEBOOK,
			ID_CONTROLLERPAGE1,
			ID_CONTROLLERPAGE2,
			ID_CONTROLLERPAGE3,
			ID_CONTROLLERPAGE4,

			ID_DEVICENAME,
			ID_ATTACHED,
			ID_DISABLE,
			ID_RUMBLE,

			ID_DUMMY_VALUE_ //don't remove this value unless you have other enum values
		};
	
		void OnClose(wxCloseEvent& event);
		void CreateGUIControls();
		void OnCloseClick(wxCommandEvent& event);
		void OnKeyDown(wxKeyEvent& event);
		void AttachedCheck(wxCommandEvent& event);
		void DisableCheck(wxCommandEvent& event);
		void RumbleCheck(wxCommandEvent& event);
		void OnButtonClick(wxCommandEvent& event);
		
		int keyPress;
		wxButton *clickedButton;
                wxString oldLabel;
		/*DInput m_dinput;*/
};

#endif
