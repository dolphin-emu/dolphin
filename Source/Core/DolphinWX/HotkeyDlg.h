// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <wx/defs.h>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/string.h>
#include <wx/timer.h>
#include <wx/translation.h>
#include <wx/windowid.h>

#include "Core/CoreParameter.h"

#if defined(HAVE_X11) && HAVE_X11
#include <X11/Xlib.h>
#include <X11/keysym.h>
#endif

class wxButton;
class wxTimerEvent;
class wxWindow;

class HotkeyConfigDialog : public wxDialog
{
public:
	HotkeyConfigDialog(wxWindow* parent,
			wxWindowID id = 1,
			const wxString &title = _("Hotkey Configuration"),
			const wxPoint& pos = wxDefaultPosition,
			const wxSize& size = wxDefaultSize,
			long style = wxDEFAULT_DIALOG_STYLE);
	virtual ~HotkeyConfigDialog();

private:
	wxString OldLabel;

	wxButton* ClickedButton;
	wxButton* m_Button_Hotkeys[NUM_HOTKEYS];

	wxTimer m_ButtonMappingTimer;

	void OnButtonTimer(wxTimerEvent& WXUNUSED(event)) { DoGetButtons(GetButtonWaitingID); }
	void OnButtonClick(wxCommandEvent& event);
	void OnKeyDown(wxKeyEvent& event);
	void SaveButtonMapping(int Id, int Key, int Modkey);
	void CreateHotkeyGUIControls();

	void SetButtonText(int id, const wxString &keystr, const wxString &modkeystr = wxString());

	void DoGetButtons(int id);
	void EndGetButtons();

	int GetButtonWaitingID, GetButtonWaitingTimer, g_Pressed, g_Modkey;
};
