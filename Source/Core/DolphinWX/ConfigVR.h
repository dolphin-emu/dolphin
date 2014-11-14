// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.
#define DETECT_WAIT_TIME     2500

#pragma once

#include "DolphinWX/InputConfigDiag.h"

class InputConfig;
class VRDialog;

class CConfigVR : public wxDialog
{
	friend class VRDialog;
public:

	CConfigVR(wxWindow* parent,
		wxWindowID id = 1,
		const wxString& title = _("VR Configuration"),
		const wxPoint& pos = wxDefaultPosition,
		const wxSize& size = wxDefaultSize,
		long style = wxDEFAULT_DIALOG_STYLE);
	virtual ~CConfigVR();

	void SaveXInputBinary(int Id, bool KBM, u32 Key);

	enum
	{
		ID_NOTEBOOK = 1000,
		ID_VRLOOKPAGE,
	};

protected:

private:

	bool button_already_clicked;
	wxButton* m_Ok;
	wxButton *ClickedButton;
	wxButton *m_Button_VRSettings[NUM_VR_HOTKEYS];
	wxComboBox* device_cbox;
	wxNotebook* Notebook;
	wxString OldLabel;
	VRDialog* m_vr_dialog;

	void OnOk(wxCommandEvent& event);
	void OnClose(wxCloseEvent& event);
	void SetDevice(wxCommandEvent& event);
	void UpdateDeviceComboBox();
	void RefreshDevices(wxCommandEvent&);

	void DetectControl(wxCommandEvent& event);
	void ClearControl(wxEvent& event);
	void ConfigControl(wxEvent& event);
	void OnXInputPollCheckbox(wxCommandEvent& event);
	void OnFreeLookScale(wxCommandEvent& event);
	void OnButtonClick(wxCommandEvent& event);
	void OnKeyDown(wxKeyEvent& event);
	int g_Pressed, g_Modkey;

	void SaveButtonMapping(int Id, bool KBM, int Key, int Modkey);
	void SetButtonText(int id, bool KBM, const wxString &keystr, const wxString &modkeystr = wxString(), const wxString &XInputMapping = wxString());
	void DoGetButtons(int id);
	void EndGetButtons();
	void EndGetButtonsXInput();

	void CreateGUIControls();
	void UpdateGUI();

	ciface::Core::Device::Control*   InputDetect(const unsigned int ms, ciface::Core::Device* const device);
	ciface::Core::DeviceQualifier    default_device;

	DECLARE_EVENT_TABLE();
};

class VRDialog : public wxDialog
{
	int button_id;
public:
	VRDialog(CConfigVR* const parent, int from_button);

private:
	void OnCheckBox(wxCommandEvent& event);
};
