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

#ifndef __HOTKEYDIALOG_h__
#define __HOTKEYDIALOG_h__

#include <wx/wx.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/combobox.h>
#include <wx/checkbox.h>
#include <wx/gbsizer.h>

#include "Common.h"
#include "CoreParameter.h"
#include "WXInputBase.h"

#if defined(HAVE_X11) && HAVE_X11
#include "X11InputBase.h"
#include <X11/Xlib.h>
#include <X11/keysym.h>
#endif

class HotkeyConfigDialog : public wxDialog
{
	public:
		HotkeyConfigDialog(wxWindow *parent,
				wxWindowID id = 1,
				const wxString &title = _("Hotkey Configuration"),
				const wxPoint& pos = wxDefaultPosition,
				const wxSize& size = wxDefaultSize,
				long style = wxDEFAULT_DIALOG_STYLE);
		virtual ~HotkeyConfigDialog();

	private:
		DECLARE_EVENT_TABLE();

		wxString OldLabel;

		wxButton *ClickedButton,
				 *m_Button_Hotkeys[NUM_HOTKEYS];

		wxTimer *m_ButtonMappingTimer;

		void OnButtonTimer(wxTimerEvent& WXUNUSED(event)) { DoGetButtons(GetButtonWaitingID); }
		void OnButtonClick(wxCommandEvent& event);
		void OnKeyDown(wxKeyEvent& event);
		void SaveButtonMapping(int Id, int Key, int Modkey);
		void CreateHotkeyGUIControls(void);

		void SetButtonText(int id, const wxString &keystr, const wxString &modkeystr = wxString());

		void DoGetButtons(int id);
		void EndGetButtons(void);

		int GetButtonWaitingID, GetButtonWaitingTimer, g_Pressed, g_Modkey;
};
#endif

