// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#define SLIDER_TICK_COUNT    100
#define DETECT_WAIT_TIME     2500
#define PREVIEW_UPDATE_TIME  25
#define DEFAULT_HIGH_VALUE   100

// might have to change this setup for Wiimote
#define PROFILES_PATH       "Profiles/"

#include <cstddef>
#include <string>
#include <vector>
#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/eventfilter.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/timer.h>

#include "InputCommon/ControllerEmu.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/Device.h"

class InputConfig;
class wxComboBox;
class wxListBox;
class wxNotebook;
class wxSlider;
class wxStaticBitmap;
class wxStaticText;
class wxTextCtrl;

class PadSetting
{
protected:
	PadSetting(wxControl* const _control) : wxcontrol(_control) { wxcontrol->SetClientData(this); }

public:
	virtual void UpdateGUI() = 0;
	virtual void UpdateValue() = 0;

	virtual ~PadSetting() {}

	wxControl* const wxcontrol;
};

class PadSettingExtension : public PadSetting
{
public:
	PadSettingExtension(wxWindow* const parent, ControllerEmu::Extension* const ext);
	void UpdateGUI() override;
	void UpdateValue() override;

	ControllerEmu::Extension* const extension;
};

class PadSettingSpin : public PadSetting
{
public:
	PadSettingSpin(wxWindow* const parent, ControllerEmu::ControlGroup::Setting* const _setting)
		: PadSetting(new wxSpinCtrl(parent, wxID_ANY, wxEmptyString, wxDefaultPosition,
					    wxSize(54, -1), 0, _setting->low, _setting->high, (int)(_setting->value * 100)))
		, setting(_setting) {}

	void UpdateGUI() override;
	void UpdateValue() override;

	ControllerEmu::ControlGroup::Setting* const setting;
};

class PadSettingCheckBox : public PadSetting
{
public:
	PadSettingCheckBox(wxWindow* const parent, ControllerEmu::ControlGroup::Setting* const setting);
	void UpdateGUI() override;
	void UpdateValue() override;

	ControllerEmu::ControlGroup::Setting* const setting;
};

class InputEventFilter : public wxEventFilter
{
public:
	InputEventFilter()
	{
		wxEvtHandler::AddFilter(this);
	}

	~InputEventFilter()
	{
		wxEvtHandler::RemoveFilter(this);
	}

	int FilterEvent(wxEvent& event) override;

	void BlockEvents(bool block) { m_block = block; }

private:
	static bool ShouldCatchEventType(wxEventType type)
	{
		return type == wxEVT_KEY_DOWN || type == wxEVT_KEY_UP ||
			type == wxEVT_CHAR || type == wxEVT_CHAR_HOOK ||
			type == wxEVT_LEFT_DOWN || type == wxEVT_LEFT_UP ||
			type == wxEVT_MIDDLE_DOWN || type == wxEVT_MIDDLE_UP ||
			type == wxEVT_RIGHT_DOWN || type == wxEVT_RIGHT_UP;
	}

	bool m_block = false;
};

class GamepadPage;

class ControlDialog : public wxDialog
{
public:
	ControlDialog(GamepadPage* const parent, InputConfig& config, ControllerInterface::ControlReference* const ref);

	bool Validate() override;

	int GetRangeSliderValue() const;

	ControllerInterface::ControlReference* const control_reference;
	InputConfig& m_config;

private:
	wxStaticBoxSizer* CreateControlChooser(GamepadPage* const parent);

	void UpdateGUI();
	void UpdateListContents();
	void SelectControl(const std::string& name);

	void DetectControl(wxCommandEvent& event);
	void ClearControl(wxCommandEvent& event);
	void SetDevice(wxCommandEvent& event);

	void SetSelectedControl(wxCommandEvent& event);
	void AppendControl(wxCommandEvent& event);

	bool GetExpressionForSelectedControl(wxString &expr);

	GamepadPage* const m_parent;
	wxComboBox*        device_cbox;
	wxTextCtrl*        textctrl;
	wxListBox*         control_lbox;
	wxSlider*          range_slider;
	wxStaticText*      m_bound_label;
	wxStaticText*      m_error_label;
	InputEventFilter   m_event_filter;
	ciface::Core::DeviceQualifier m_devq;
};

class ExtensionButton : public wxButton
{
public:
	ExtensionButton(wxWindow* const parent, ControllerEmu::Extension* const ext)
		: wxButton(parent, wxID_ANY, _("Configure"), wxDefaultPosition)
		, extension(ext) {}

	ControllerEmu::Extension* const extension;
};

class ControlButton : public wxButton
{
public:
	ControlButton(wxWindow* const parent, ControllerInterface::ControlReference* const _ref, const unsigned int width, const std::string& label = "");

	ControllerInterface::ControlReference* const control_reference;
};

class ControlGroupBox : public wxBoxSizer
{
public:
	ControlGroupBox(ControllerEmu::ControlGroup* const group, wxWindow* const parent, GamepadPage* const eventsink);
	~ControlGroupBox();

	std::vector<PadSetting*> options;

	ControllerEmu::ControlGroup* const control_group;
	wxStaticBitmap*                    static_bitmap;
	std::vector<ControlButton*>        control_buttons;
};

class ControlGroupsSizer : public wxBoxSizer
{
public:
	ControlGroupsSizer(ControllerEmu* const controller, wxWindow* const parent, GamepadPage* const eventsink, std::vector<ControlGroupBox*>* const groups = nullptr);
};

class InputConfigDialog;

class GamepadPage : public wxPanel
{
	friend class InputConfigDialog;
	friend class ControlDialog;

public:
	GamepadPage(wxWindow* parent, InputConfig& config, const int pad_num, InputConfigDialog* const config_dialog);

	void UpdateGUI();

	void RefreshDevices(wxCommandEvent& event);

	void LoadProfile(wxCommandEvent& event);
	void SaveProfile(wxCommandEvent& event);
	void DeleteProfile(wxCommandEvent& event);

	void ConfigControl(wxEvent& event);
	void ClearControl(wxEvent& event);
	void DetectControl(wxCommandEvent& event);

	void ConfigExtension(wxCommandEvent& event);

	void SetDevice(wxCommandEvent& event);

	void ClearAll(wxCommandEvent& event);
	void LoadDefaults(wxCommandEvent& event);

	void AdjustControlOption(wxCommandEvent& event);
	void AdjustSetting(wxCommandEvent& event);
	void AdjustSettingUI(wxCommandEvent& event);

	void GetProfilePath(std::string& path);

	wxComboBox* profile_cbox;
	wxComboBox* device_cbox;

	std::vector<ControlGroupBox*> control_groups;
	std::vector<ControlButton*>   control_buttons;

protected:

	ControllerEmu* const controller;

private:

	ControlDialog*           m_control_dialog;
	InputConfigDialog* const m_config_dialog;
	InputConfig&             m_config;
	InputEventFilter         m_event_filter;

	bool DetectButton(ControlButton* button);
	bool m_iterate = false;
};

class InputConfigDialog : public wxDialog
{
public:
	InputConfigDialog(wxWindow* const parent, InputConfig& config, const wxString& name, const int tab_num = 0);

	void ClickSave(wxCommandEvent& event);

	void UpdateDeviceComboBox();
	void UpdateProfileComboBox();

	void UpdateControlReferences();
	void UpdateBitmaps(wxTimerEvent&);

private:

	wxNotebook*               m_pad_notebook;
	std::vector<GamepadPage*> m_padpages;
	InputConfig&              m_config;
	wxTimer                   m_update_timer;
};
