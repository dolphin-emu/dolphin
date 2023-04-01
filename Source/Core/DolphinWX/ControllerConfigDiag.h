// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <map>
#include <wx/dialog.h>

#include "Common/SysConf.h"
#include "Core/ConfigManager.h"
#include "Core/HW/Wiimote.h"
#include "InputCommon/GCAdapter.h"

class InputConfig;
class wxButton;
class wxStaticBoxSizer;

class ControllerConfigDiag : public wxDialog
{
public:
	ControllerConfigDiag(wxWindow* const parent);

private:
	void RefreshRealWiimotes(wxCommandEvent& event);

	void ConfigEmulatedWiimote(wxCommandEvent& event);

	void SelectSource(wxCommandEvent& event);
	void RevertSource();

	void Save(wxCommandEvent& event);

	void OnSensorBarPos(wxCommandEvent& event)
	{
		SConfig::GetInstance().m_SYSCONF->SetData("BT.BAR", event.GetInt());
		event.Skip();
	}

	void OnSensorBarSensitivity(wxCommandEvent& event)
	{
		SConfig::GetInstance().m_SYSCONF->SetData("BT.SENS", event.GetInt());
		event.Skip();
	}

	void OnSpeakerVolume(wxCommandEvent& event)
	{
		SConfig::GetInstance().m_SYSCONF->SetData("BT.SPKV", event.GetInt());
		event.Skip();
	}

	void OnMotor(wxCommandEvent& event)
	{
		SConfig::GetInstance().m_SYSCONF->SetData("BT.MOT", event.GetInt());
		event.Skip();
	}

	void OnContinuousScanning(wxCommandEvent& event)
	{
		SConfig::GetInstance().m_WiimoteContinuousScanning = event.IsChecked();
		WiimoteReal::Initialize();
		event.Skip();
	}

	void OnEnableSpeaker(wxCommandEvent& event)
	{
		SConfig::GetInstance().m_WiimoteEnableSpeaker = event.IsChecked();
		event.Skip();
	}

	wxStaticBoxSizer* CreateGamecubeSizer();
	wxStaticBoxSizer* CreateWiimoteConfigSizer();
	wxStaticBoxSizer* CreateBalanceBoardSizer();
	wxStaticBoxSizer* CreateRealWiimoteSizer();
	wxStaticBoxSizer* CreateGeneralWiimoteSettingsSizer();

	void Cancel(wxCommandEvent& event);
	void OnGameCubePortChanged(wxCommandEvent& event);
	void OnGameCubeConfigButton(wxCommandEvent& event);

	std::map<wxWindowID, unsigned int> m_gc_port_choice_ids;
	std::map<wxWindowID, unsigned int> m_gc_port_config_ids;
	std::array<wxString, 9> m_gc_pad_type_strs;

	std::map<wxWindowID, unsigned int> m_wiimote_index_from_ctrl_id;
	unsigned int m_orig_wiimote_sources[MAX_BBMOTES];

	wxButton* wiimote_configure_bt[MAX_WIIMOTES];
	wxButton* gamecube_configure_bt[4];
	std::map<wxWindowID, unsigned int> m_wiimote_index_from_conf_bt_id;
};
