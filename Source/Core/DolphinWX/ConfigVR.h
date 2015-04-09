// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.
#define DETECT_WAIT_TIME     2500

#pragma once

#include "DolphinWX/InputConfigDiag.h"
#include "DolphinWX/VideoConfigDiag.h"

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

	void SaveXInputBinary(int Id, bool KBM, bool DInput, u32 Key, u32 DInputExtra);

	enum
	{
		ID_NOTEBOOK = 1000,
		ID_VRLOOKPAGE,
	};

protected:

	void Event_ClickSave(wxCommandEvent&);
	void Event_ClickReset(wxCommandEvent&);

	// Enables/disables UI elements depending on current config
	void OnUpdateUI(wxUpdateUIEvent& ev)
	{
		// Things which shouldn't be changed during emulation
		if (Core::IsRunning())
		{
#ifdef OCULUSSDK042
			async_timewarp_checkbox->Disable();
#endif
		}
		ev.Skip();
	}

	// Creates controls and connects their enter/leave window events to Evt_Enter/LeaveControl
	SettingCheckBox* CreateCheckBox(wxWindow* parent, const wxString& label, const wxString& description, bool &setting, bool reverse = false, long style = 0);
	SettingChoice* CreateChoice(wxWindow* parent, int& setting, const wxString& description, int num = 0, const wxString choices[] = nullptr, long style = 0);
	SettingRadioButton* CreateRadioButton(wxWindow* parent, const wxString& label, const wxString& description, bool &setting, bool reverse = false, long style = 0);
	SettingNumber* CreateNumber(wxWindow* parent, float &setting, const wxString& description, float min, float max, float inc, long style = 0);

	// Same as above but only connects enter/leave window events
	wxControl* RegisterControl(wxControl* const control, const wxString& description);

	void Evt_EnterControl(wxMouseEvent& ev);
	void Evt_LeaveControl(wxMouseEvent& ev);
	void CreateDescriptionArea(wxPanel* const page, wxBoxSizer* const sizer);

	SettingCheckBox* async_timewarp_checkbox;

private:

	bool button_already_clicked;
	wxButton* m_Ok;
	wxButton *ClickedButton;
	wxButton *m_Button_VRSettings[NUM_VR_HOTKEYS];
	wxComboBox* device_cbox;
	wxNotebook* Notebook;
	wxString OldLabel;
	VRDialog* m_vr_dialog;

	U32Setting* spin_extra_frames;
	U32Setting* spin_replay_buffer_divider;
	U32Setting* spin_replay_buffer;
	SettingNumber* spin_timewarp_tweak;
	SettingCheckBox* checkbox_pullup20_timewarp;
	SettingCheckBox* checkbox_pullup30_timewarp;
	SettingCheckBox* checkbox_pullup60_timewarp;
	SettingCheckBox* checkbox_pullup20;
	SettingCheckBox* checkbox_pullup30;
	SettingCheckBox* checkbox_pullup60;
	SettingCheckBox* checkbox_roll;
	SettingCheckBox* checkbox_pitch;
	SettingCheckBox* checkbox_yaw;
	SettingCheckBox* checkbox_x;
	SettingCheckBox* checkbox_y;
	SettingCheckBox* checkbox_z;
	SettingCheckBox* checkbox_keyhole;
	SettingNumber* keyhole_width;
	SettingCheckBox* checkbox_keyhole_snap;
	SettingNumber* keyhole_snap_size;

	void OnKeyholeCheckbox(wxCommandEvent& event);
	void OnKeyholeSnapCheckbox(wxCommandEvent& event);
	void OnYawCheckbox(wxCommandEvent& event);
	void OnPullupCheckbox(wxCommandEvent& event);

	void OnOk(wxCommandEvent& event);
	void OnClose(wxCloseEvent& event);
	void SetDevice(wxCommandEvent& event);
	void UpdateDeviceComboBox();
	void RefreshDevices(wxCommandEvent&);

	void DetectControl(wxCommandEvent& event);
	void ClearControl(wxEvent& event);
	void ConfigControl(wxEvent& event);
	void OnXInputPollCheckbox(wxCommandEvent& event);
	void OnFreeLookSensitivity(wxCommandEvent& event);
	void OnButtonClick(wxCommandEvent& event);
	void OnKeyDown(wxKeyEvent& event);
	int g_Pressed, g_Modkey;

	void SaveButtonMapping(int Id, bool KBM, int Key, int Modkey);
	void SetButtonText(int id, bool KBM, bool DInput, const wxString &keystr, const wxString &modkeystr = wxString(), const wxString &XInputMapping = wxString());
	void DoGetButtons(int id);
	void EndGetButtons();
	void EndGetButtonsXInput();

	void CreateGUIControls();
	void UpdateGUI();

	ciface::Core::Device::Control*   InputDetect(const unsigned int ms, ciface::Core::Device* const device);
	ciface::Core::DeviceQualifier    default_device;

	DECLARE_EVENT_TABLE();

	std::map<wxWindow*, wxString> ctrl_descs; // maps setting controls to their descriptions
	std::map<wxWindow*, wxStaticText*> desc_texts; // maps dialog tabs (which are the parents of the setting controls) to their description text objects

	VideoConfig &vconfig;
};

class VRDialog : public wxDialog
{
	int button_id;
public:
	VRDialog(CConfigVR* const parent, int from_button);

private:
	void OnCheckBoxXInput(wxCommandEvent& event);
	void OnCheckBoxDInputButtons(wxCommandEvent& event);
	void OnCheckBoxDInputOthers(wxCommandEvent& event);
};
