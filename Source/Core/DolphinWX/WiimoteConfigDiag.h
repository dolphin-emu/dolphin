#pragma once

#include <map>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/windowid.h>

#include "Common/SysConf.h"
#include "Core/ConfigManager.h"
#include "Core/HW/Wiimote.h"

class InputPlugin;
class wxButton;
class wxWindow;

class WiimoteConfigDiag : public wxDialog
{
public:
	WiimoteConfigDiag(wxWindow* const parent, InputPlugin& plugin);

	void RefreshRealWiimotes(wxCommandEvent& event);

	void SelectSource(wxCommandEvent& event);
	void RevertSource();

	void ConfigEmulatedWiimote(wxCommandEvent& event);
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

private:
	void Cancel(wxCommandEvent& event);

	InputPlugin& m_plugin;

	std::map<wxWindowID, unsigned int> m_wiimote_index_from_ctrl_id;
	unsigned int m_orig_wiimote_sources[MAX_BBMOTES];

	wxButton* wiimote_configure_bt[MAX_WIIMOTES];
	std::map<wxWindowID, unsigned int> m_wiimote_index_from_conf_bt_id;
};
