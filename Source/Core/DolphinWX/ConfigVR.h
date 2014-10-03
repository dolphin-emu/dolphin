// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <cstdio>
#include <string>

#include <wx/bitmap.h>
#include <wx/dcmemory.h>
#include <wx/defs.h>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/string.h>
#include <wx/timer.h>
#include <wx/toplevel.h>
#include <wx/translation.h>
#include <wx/windowid.h>

#include "Common/CommonTypes.h"
//#include "InputCommon/GCPadStatus.h"

class wxBoxSizer;
class wxButton;
class wxCheckBox;
class wxChoice;
class wxDirPickerCtrl;
class wxFileDirPickerEvent;
class wxFilePickerCtrl;
class wxGridBagSizer;
class wxListBox;
class wxNotebook;
class wxPanel;
class wxRadioBox;
class wxSlider;
class wxSpinCtrl;
class wxSpinEvent;
class wxStaticBitmap;
class wxStaticBoxSizer;
class wxStaticText;
class wxTextCtrl;
class wxTimerEvent;
class wxWindow;

class CConfigVR : public wxDialog
{
public:

	CConfigVR(wxWindow* parent,
		wxWindowID id = 1,
		const wxString& title = _("VR Configuration"),
		const wxPoint& pos = wxDefaultPosition,
		const wxSize& size = wxDefaultSize,
		long style = wxDEFAULT_DIALOG_STYLE);
	virtual ~CConfigVR();

	//void SetTurbo(wxMouseEvent& event);
	//void SetTurboFalse(wxMouseEvent& event);
	//void ButtonTurbo();
	void OnOk(wxCommandEvent& event);
	void CloseClick(wxCommandEvent& event);
	void OnSelectionChanged(wxCommandEvent& event);
	void OnConfig(wxCommandEvent& event);
	//void OnMouseDownL(wxMouseEvent& event);
	//void OnMouseUpR(wxMouseEvent& event);
	//void UpdateFromSliders(wxCommandEvent& event);
	//void UpdateFromText(wxCommandEvent& event);

	wxBitmap CreateStickBitmap(int x, int y);

	enum
	{
		ID_NOTEBOOK = 1000,
		ID_VRLOOKPAGE,
	};

private:
	u8 mainX, mainY, cX, cY, lTrig, rTrig;
	enum
	{
		ID_MAIN_X_SLIDER = 1000,
	};

	wxNotebook* Notebook;

	// Graphics
	wxChoice* GraphicSelection;
	wxButton* GraphicConfig;

	wxButton* m_Ok;

	FILE* pStream;

	wxString OldLabel;

	wxButton *ClickedButton;
	wxButton *m_Button_VRSettings[NUM_VR_OPTIONS];

	wxTimer m_ButtonMappingTimer;

	//void OnButtonTimer(wxTimerEvent& WXUNUSED(event)) { DoGetButtons(GetButtonWaitingID); }
	void OnButtonClick(wxCommandEvent& event);
	void OnKeyDown(wxKeyEvent& event);
	void SaveButtonMapping(int Id, int Key, int Modkey);
	void CreateHotkeyGUIControls();

	void SetButtonText(int id, const wxString &keystr, const wxString &modkeystr = wxString());

	void DoGetButtons(int id);
	void EndGetButtons();

	//int GetButtonWaitingID, GetButtonWaitingTimer, g_Pressed, g_Modkey;
	int g_Pressed, g_Modkey;

	void InitializeGUILists();
	void InitializeGUIValues();
	void InitializeGUITooltips();

	void CreateGUIControls();
	void UpdateGUI();
	void OnClose(wxCloseEvent& event);

	void DisplaySettingsChanged(wxCommandEvent& event);
	//void OnSpin(wxSpinEvent& event);

	DECLARE_EVENT_TABLE();
};
