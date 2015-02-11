#pragma once

#include <array>
#include <map>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/windowid.h>

#include "Common/SysConf.h"
#include "Core/ConfigManager.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/SI_GCAdapter.h"

class InputConfig;
class wxButton;
class wxStaticBoxSizer;
class wxWindow;

class ControllerConfigDiag : public wxDialog
{
public:
	ControllerConfigDiag(wxWindow* const parent);

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
	void OnGameCubeAdapter(wxCommandEvent& event)
	{
		SConfig::GetInstance().m_GameCubeAdapter = event.IsChecked();
		event.Skip();
	}
	void OnGameCubeAdapterThread(wxCommandEvent& event)
	{
		SConfig::GetInstance().m_GameCubeAdapterThread = event.IsChecked();
		event.Skip();
	}
	void OnGameCubeAdapterScan(wxCommandEvent& event)
	{
		bool gcadapter_config = SConfig::GetInstance().m_GameCubeAdapter;
		// required for SI_GCAdapter::Shutdown() to proceed
		SConfig::GetInstance().m_GameCubeAdapter = true;
		SI_GCAdapter::Shutdown();
		SI_GCAdapter::Init();
		if (SI_GCAdapter::IsDetected())
		{
			m_gamecube_adapter->SetLabelText(_("Direct Connect"));
			m_gamecube_adapter->Enable();
			m_gamecube_adapter_thread->Enable();

			m_gamecube_adapter->SetValue(gcadapter_config);
			m_gamecube_adapter_thread->SetValue(SConfig::GetInstance().m_GameCubeAdapterThread);
		}
		else
		{
			m_gamecube_adapter->SetLabelText(_("Adapter Not Detected"));
			m_gamecube_adapter->Disable();
			m_gamecube_adapter->SetValue(false);
			m_gamecube_adapter_thread->Disable();
			m_gamecube_adapter_thread->SetValue(false);
		}
		SConfig::GetInstance().m_GameCubeAdapter = gcadapter_config;
		event.Skip();
	}

private:
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
	static const std::array<wxString, 8> m_gc_pad_type_strs;

	std::map<wxWindowID, unsigned int> m_wiimote_index_from_ctrl_id;
	unsigned int m_orig_wiimote_sources[MAX_BBMOTES];

	wxButton* wiimote_configure_bt[MAX_WIIMOTES];
	wxButton* gamecube_configure_bt[4];
	std::map<wxWindowID, unsigned int> m_wiimote_index_from_conf_bt_id;

	wxCheckBox* m_gamecube_adapter;
	wxCheckBox* m_gamecube_adapter_thread;
	wxButton* m_gamecube_adapter_scan;
};
