// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

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

