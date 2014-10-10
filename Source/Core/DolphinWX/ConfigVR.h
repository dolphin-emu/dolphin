// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.
#define DETECT_WAIT_TIME     2500

#pragma once

#include <cstdio>
#include <string>

#include <wx/timer.h>

#include "Common/CommonTypes.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "DolphinWX/InputConfigDiag.h"

class InputConfig;

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

	enum
	{
		ID_NOTEBOOK = 1000,
		ID_VRLOOKPAGE,
	};

protected:

private:

	wxButton* m_Ok;
	wxButton *ClickedButton;
	wxButton *m_Button_VRSettings[NUM_VR_OPTIONS];
	wxComboBox* device_cbox;
	wxNotebook* Notebook;
	wxString OldLabel;
	//wxTimer m_ButtonMappingTimer;

	void OnOk(wxCommandEvent& event);
	void OnClose(wxCloseEvent& event);
	void SetDevice(wxCommandEvent& event);
	void UpdateDeviceComboBox();
	void RefreshDevices(wxCommandEvent&);

	void DetectControl(wxCommandEvent& event);
	bool DetectButton(wxButton* button, wxCommandEvent& event);
	//void OnButtonTimer(wxTimerEvent& WXUNUSED(event)) { DoGetButtons(GetButtonWaitingID); }
	void OnButtonClick(wxCommandEvent& event);
	void OnKeyDown(wxKeyEvent& event);
	//int GetButtonWaitingID, GetButtonWaitingTimer, g_Pressed, g_Modkey;
	int g_Pressed, g_Modkey;

	void SaveButtonMapping(int Id, int Key, int Modkey);
	void SetButtonText(int id, const wxString &keystr, const wxString &modkeystr = wxString());
	void DoGetButtons(int id);
	void EndGetButtons();
	
	void ConfigControl(wxEvent& event);

	void CreateGUIControls();
	void UpdateGUI();

	ciface::Core::Device::Control*   InputDetect(const unsigned int ms, ciface::Core::Device* const device);
	ciface::Core::DeviceQualifier    default_device;

	DECLARE_EVENT_TABLE();
};