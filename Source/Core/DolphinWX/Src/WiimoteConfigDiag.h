
#ifndef WIIMOTE_CONFIG_DIAG_H
#define WIIMOTE_CONFIG_DIAG_H

#include <wx/wx.h>
#include <wx/listbox.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/combobox.h>
#include <wx/checkbox.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/spinctrl.h>

#include "InputConfigDiag.h"
#include "ConfigManager.h"
#include <HW/Wiimote.h>

#include <map>

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

	InputPlugin&	m_plugin;
	wxNotebook*		m_pad_notebook;

	std::map<wxWindowID, unsigned int> m_wiimote_index_from_ctrl_id;
	unsigned int m_orig_wiimote_sources[MAX_BBMOTES];

	wxButton* wiimote_configure_bt[MAX_WIIMOTES];
	std::map<wxWindowID, unsigned int> m_wiimote_index_from_conf_bt_id;
};


#endif
