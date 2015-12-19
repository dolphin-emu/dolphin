// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <array>
#include <map>
#include <string>
#include <utility>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/stattext.h>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/SysConf.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HotkeyManager.h"
#include "Core/Movie.h"
#include "Core/NetPlayProto.h"
#include "Core/HW/GCKeyboard.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/SI.h"
#if defined(__LIBUSB__) || defined (_WIN32)
#include "Core/HW/SI_GCAdapter.h"
#endif
#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"
#include "DolphinWX/ControllerConfigDiag.h"
#include "DolphinWX/InputConfigDiag.h"

#if defined(HAVE_XRANDR) && HAVE_XRANDR
#include "DolphinWX/X11Utils.h"
#endif

wxDEFINE_EVENT(wxEVT_ADAPTER_UPDATE, wxCommandEvent);

ControllerConfigDiag::ControllerConfigDiag(wxWindow* const parent)
	: wxDialog(parent, wxID_ANY, _("Dolphin Controller Configuration"))
{
	m_gc_pad_type_strs = {{
		_("None"),
		_("Standard Controller"),
		_("Steering Wheel"),
		_("Dance Mat"),
		_("TaruKonga (Bongos)"),
		_("GBA"),
		_("Keyboard"),
		_("AM Baseboard")
	}};

	wxBoxSizer* const main_sizer = new wxBoxSizer(wxVERTICAL);

	// Combine all UI controls into their own encompassing sizer.
	wxBoxSizer* control_sizer = new wxBoxSizer(wxVERTICAL);
	control_sizer->Add(CreateGamecubeSizer(), 0, wxEXPAND | wxALL, 5);
	control_sizer->Add(CreateWiimoteConfigSizer(), 0, wxEXPAND | wxALL, 5);

	main_sizer->Add(control_sizer, 0, wxEXPAND);
	main_sizer->Add(CreateButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	Bind(wxEVT_BUTTON, &ControllerConfigDiag::Save, this, wxID_OK);
	Bind(wxEVT_BUTTON, &ControllerConfigDiag::Cancel, this, wxID_CANCEL);

	SetLayoutAdaptationMode(wxDIALOG_ADAPTATION_MODE_ENABLED);
	SetSizerAndFit(main_sizer);
	Center();
	Bind(wxEVT_ADAPTER_UPDATE, &ControllerConfigDiag::UpdateAdapter, this);
}

wxStaticBoxSizer* ControllerConfigDiag::CreateGamecubeSizer()
{
	wxStaticBoxSizer* const gamecube_static_sizer = new wxStaticBoxSizer(wxVERTICAL, this, _("GameCube Controllers"));
	wxFlexGridSizer* const gamecube_flex_sizer = new wxFlexGridSizer(3, 5, 5);

	wxStaticText* pad_labels[4];
	wxChoice* pad_type_choices[4];

	for (int i = 0; i < 4; i++)
	{
		pad_labels[i] = new wxStaticText(this, wxID_ANY, wxString::Format(_("Port %i"), i + 1));

		// Create an ID for the config button.
		const wxWindowID button_id = wxWindow::NewControlId();
		m_gc_port_config_ids.emplace(button_id, i);
		gamecube_configure_bt[i] = new wxButton(this, button_id, _("Configure"), wxDefaultPosition, wxSize(100, 25));
		gamecube_configure_bt[i]->Bind(wxEVT_BUTTON, &ControllerConfigDiag::OnGameCubeConfigButton, this);

		// Create a control ID for the choice boxes on the fly.
		const wxWindowID choice_id = wxWindow::NewControlId();
		m_gc_port_choice_ids.emplace(choice_id, i);

		// Only add AM-Baseboard to the first pad.
		if (i == 0)
			pad_type_choices[i] = new wxChoice(this, choice_id, wxDefaultPosition, wxDefaultSize, m_gc_pad_type_strs.size(), m_gc_pad_type_strs.data());
		else
			pad_type_choices[i] = new wxChoice(this, choice_id, wxDefaultPosition, wxDefaultSize, m_gc_pad_type_strs.size() - 1, m_gc_pad_type_strs.data());

		pad_type_choices[i]->Bind(wxEVT_CHOICE, &ControllerConfigDiag::OnGameCubePortChanged, this);

		// Disable controller type selection for certain circumstances.
		if (NetPlay::IsNetPlayRunning() || Movie::IsMovieActive())
			pad_type_choices[i]->Disable();

		// Set the saved pad type as the default choice.
		switch (SConfig::GetInstance().m_SIDevice[i])
		{
		case SIDEVICE_GC_CONTROLLER:
			pad_type_choices[i]->SetStringSelection(m_gc_pad_type_strs[1]);
			break;
		case SIDEVICE_GC_STEERING:
			pad_type_choices[i]->SetStringSelection(m_gc_pad_type_strs[2]);
			break;
		case SIDEVICE_DANCEMAT:
			pad_type_choices[i]->SetStringSelection(m_gc_pad_type_strs[3]);
			break;
		case SIDEVICE_GC_TARUKONGA:
			pad_type_choices[i]->SetStringSelection(m_gc_pad_type_strs[4]);
			break;
		case SIDEVICE_GC_GBA:
			pad_type_choices[i]->SetStringSelection(m_gc_pad_type_strs[5]);
			gamecube_configure_bt[i]->Disable();
			break;
		case SIDEVICE_GC_KEYBOARD:
			pad_type_choices[i]->SetStringSelection(m_gc_pad_type_strs[6]);
			break;
		case SIDEVICE_AM_BASEBOARD:
			pad_type_choices[i]->SetStringSelection(m_gc_pad_type_strs[7]);
			break;
		default:
			pad_type_choices[i]->SetStringSelection(m_gc_pad_type_strs[0]);
			gamecube_configure_bt[i]->Disable();
			break;
		}

		// Add to the sizer
		gamecube_flex_sizer->Add(pad_labels[i], 0, wxALIGN_CENTER_VERTICAL);
		gamecube_flex_sizer->Add(pad_type_choices[i], 0, wxALIGN_CENTER_VERTICAL);
		gamecube_flex_sizer->Add(gamecube_configure_bt[i], 1, wxEXPAND);
	}

	gamecube_static_sizer->Add(gamecube_flex_sizer, 1, wxEXPAND, 5);
	gamecube_static_sizer->AddSpacer(5);

	wxStaticBoxSizer* const gamecube_adapter_group = new wxStaticBoxSizer(wxVERTICAL, this, _("GameCube Adapter"));
	wxBoxSizer* const gamecube_adapter_sizer = new wxBoxSizer(wxHORIZONTAL);

	wxCheckBox* const gamecube_adapter = new wxCheckBox(this, wxID_ANY, _("Direct Connect"));
	gamecube_adapter->Bind(wxEVT_CHECKBOX, &ControllerConfigDiag::OnGameCubeAdapter, this);

	wxCheckBox* const gamecube_rumble = new wxCheckBox(this, wxID_ANY, _("Rumble"));
	gamecube_rumble->SetValue(SConfig::GetInstance().m_AdapterRumble);
	gamecube_rumble->Bind(wxEVT_CHECKBOX, &ControllerConfigDiag::OnAdapterRumble, this);

	m_adapter_status = new wxStaticText(this, wxID_ANY, _("Adapter Not Detected"));

	gamecube_adapter_group->Add(m_adapter_status, 0, wxEXPAND);
	gamecube_adapter_sizer->Add(gamecube_adapter, 0, wxEXPAND);
	gamecube_adapter_sizer->Add(gamecube_rumble, 0, wxEXPAND);
	gamecube_adapter_group->Add(gamecube_adapter_sizer, 0, wxEXPAND);
	gamecube_static_sizer->Add(gamecube_adapter_group, 0, wxEXPAND);

#if defined(__LIBUSB__) || defined (_WIN32)
	gamecube_adapter->SetValue(SConfig::GetInstance().m_GameCubeAdapter);
	if (!SI_GCAdapter::IsDetected())
	{
		if (!SI_GCAdapter::IsDriverDetected())
		{
			m_adapter_status->SetLabelText(_("Driver Not Detected"));
			gamecube_adapter->Disable();
			gamecube_adapter->SetValue(false);
			gamecube_rumble->Disable();
		}
	}
	else
	{
		m_adapter_status->SetLabelText(_("Adapter Detected"));
	}
	if (Core::GetState() != Core::CORE_UNINITIALIZED)
	{
		gamecube_adapter->Disable();
	}
	SI_GCAdapter::SetAdapterCallback(std::bind(&ControllerConfigDiag::ScheduleAdapterUpdate, this));
#endif

	return gamecube_static_sizer;
}

void ControllerConfigDiag::ScheduleAdapterUpdate()
{
	wxQueueEvent(this, new wxCommandEvent(wxEVT_ADAPTER_UPDATE));
}

void ControllerConfigDiag::UpdateAdapter(wxCommandEvent& ev)
{
#if defined(__LIBUSB__) || defined (_WIN32)
	bool unpause = Core::PauseAndLock(true);
	if (SI_GCAdapter::IsDetected())
		m_adapter_status->SetLabelText(_("Adapter Detected"));
	else
		m_adapter_status->SetLabelText(_("Adapter Not Detected"));
	Core::PauseAndLock(false, unpause);
#endif
}

wxStaticBoxSizer* ControllerConfigDiag::CreateWiimoteConfigSizer()
{
	wxStaticText* wiimote_label[4];
	wxChoice* wiimote_source_ch[4];

	for (unsigned int i = 0; i < MAX_WIIMOTES; ++i)
	{
		wxString wiimote_str = wxString::Format(_("Wiimote %i"), i + 1);

		static const std::array<wxString, 4> src_choices = {{
			_("None"), _("Emulated Wiimote"), _("Real Wiimote"), _("Hybrid Wiimote")
		}};

		// reserve four ids, so that we can calculate the index from the ids later on
		// Stupid wx 2.8 doesn't support reserving sequential IDs, so we need to do that more complicated..
		int source_ctrl_id =  wxWindow::NewControlId();
		m_wiimote_index_from_ctrl_id.emplace(source_ctrl_id, i);

		int config_bt_id = wxWindow::NewControlId();
		m_wiimote_index_from_conf_bt_id.emplace(config_bt_id, i);

		wiimote_label[i] = new wxStaticText(this, wxID_ANY, wiimote_str);
		wiimote_source_ch[i] = new wxChoice(this, source_ctrl_id, wxDefaultPosition, wxDefaultSize, src_choices.size(), src_choices.data());
		wiimote_source_ch[i]->Bind(wxEVT_CHOICE, &ControllerConfigDiag::SelectSource, this);
		wiimote_configure_bt[i] = new wxButton(this, config_bt_id, _("Configure"), wxDefaultPosition, wxSize(80, 25));
		wiimote_configure_bt[i]->Bind(wxEVT_BUTTON, &ControllerConfigDiag::ConfigEmulatedWiimote, this);

		// Disable controller type selection for certain circumstances.
		bool wii_game_started = SConfig::GetInstance().bWii || Core::GetState() == Core::CORE_UNINITIALIZED;
		if (NetPlay::IsNetPlayRunning() || Movie::IsMovieActive() || !wii_game_started)
			wiimote_source_ch[i]->Disable();

		m_orig_wiimote_sources[i] = g_wiimote_sources[i];
		wiimote_source_ch[i]->Select(m_orig_wiimote_sources[i]);
		if (!wii_game_started || (m_orig_wiimote_sources[i] != WIIMOTE_SRC_EMU && m_orig_wiimote_sources[i] != WIIMOTE_SRC_HYBRID))
			wiimote_configure_bt[i]->Disable();
	}

	// "Wiimotes" layout
	wxStaticBoxSizer* const wiimote_group = new wxStaticBoxSizer(wxVERTICAL,this, _("Wiimotes"));
	wxBoxSizer* const wiimote_control_section = new wxBoxSizer(wxHORIZONTAL);
	wxFlexGridSizer* const wiimote_sizer = new wxFlexGridSizer(3, 5, 5);
	for (unsigned int i = 0; i < 4; ++i)
	{
		wiimote_sizer->Add(wiimote_label[i], 0, wxALIGN_CENTER_VERTICAL);
		wiimote_sizer->Add(wiimote_source_ch[i], 0, wxALIGN_CENTER_VERTICAL);
		wiimote_sizer->Add(wiimote_configure_bt[i]);
	}
	wiimote_control_section->Add(wiimote_sizer, 1, wxEXPAND, 5 );

	// Disable some controls when emulation is running
	if (Core::GetState() != Core::CORE_UNINITIALIZED && NetPlay::IsNetPlayRunning())
	{
		for (int i = 0; i < 4; ++i)
		{
			wiimote_label[i]->Disable();
			wiimote_source_ch[i]->Disable();
		}
	}

	wiimote_group->Add(wiimote_control_section, 0, wxEXPAND);
	wiimote_group->AddSpacer(5);
	wiimote_group->Add(CreateBalanceBoardSizer(), 0, wxEXPAND);
	wiimote_group->AddSpacer(5);
	wiimote_group->Add(CreateRealWiimoteSizer(), 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM);
	wiimote_group->AddSpacer(5);
	wiimote_group->Add(CreateGeneralWiimoteSettingsSizer(), 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM);

	return wiimote_group;
}

wxStaticBoxSizer* ControllerConfigDiag::CreateBalanceBoardSizer()
{
	wxStaticBoxSizer* const bb_group = new wxStaticBoxSizer(wxHORIZONTAL, this, _("Balance Board"));
	wxFlexGridSizer* const bb_sizer = new wxFlexGridSizer(1, 5, 5);
	int source_ctrl_id =  wxWindow::NewControlId();

	m_wiimote_index_from_ctrl_id.emplace(source_ctrl_id, WIIMOTE_BALANCE_BOARD);

	static const std::array<wxString, 2> src_choices = {{
		_("None"), _("Real Balance Board")
	}};

	wxChoice* const bb_source = new wxChoice(this, source_ctrl_id, wxDefaultPosition, wxDefaultSize, src_choices.size(), src_choices.data());
	bb_source->Bind(wxEVT_CHOICE, &ControllerConfigDiag::SelectSource, this);

	m_orig_wiimote_sources[WIIMOTE_BALANCE_BOARD] = g_wiimote_sources[WIIMOTE_BALANCE_BOARD];
	bb_source->Select(m_orig_wiimote_sources[WIIMOTE_BALANCE_BOARD] ? 1 : 0);

	bb_sizer->Add(bb_source, 0, wxALIGN_CENTER_VERTICAL);

	bb_group->Add(bb_sizer, 1, wxEXPAND, 5);

	// Disable when emulation is running.
	if (Core::GetState() != Core::CORE_UNINITIALIZED)
		bb_source->Disable();

	return bb_group;
}

wxStaticBoxSizer* ControllerConfigDiag::CreateRealWiimoteSizer()
{
	// "Real wiimotes" controls
	wxButton* const refresh_btn = new wxButton(this, wxID_ANY, _("Refresh"));
	refresh_btn->Bind(wxEVT_BUTTON, &ControllerConfigDiag::RefreshRealWiimotes, this);

	wxStaticBoxSizer* const real_wiimotes_group = new wxStaticBoxSizer(wxVERTICAL, this, _("Real Wiimotes"));
	wxBoxSizer* const real_wiimotes_sizer = new wxBoxSizer(wxHORIZONTAL);

	if (!WiimoteReal::g_wiimote_scanner.IsReady())
		real_wiimotes_group->Add(new wxStaticText(this, wxID_ANY, _("A supported Bluetooth device could not be found.\n"
		                                                            "You must manually connect your Wiimotes.")), 0, wxALIGN_CENTER | wxALL, 5);

	wxCheckBox* const continuous_scanning = new wxCheckBox(this, wxID_ANY, _("Continuous Scanning"));
	continuous_scanning->Bind(wxEVT_CHECKBOX, &ControllerConfigDiag::OnContinuousScanning, this);
	continuous_scanning->SetValue(SConfig::GetInstance().m_WiimoteContinuousScanning);

	real_wiimotes_sizer->Add(continuous_scanning, 0, wxALIGN_CENTER_VERTICAL);
	real_wiimotes_sizer->AddStretchSpacer();
	real_wiimotes_sizer->Add(refresh_btn, 0, wxALL | wxALIGN_CENTER, 5);

	real_wiimotes_group->Add(real_wiimotes_sizer, 0, wxEXPAND);

	return real_wiimotes_group;
}

wxStaticBoxSizer* ControllerConfigDiag::CreateGeneralWiimoteSettingsSizer()
{
	const wxString str[] = { _("Bottom"), _("Top") };
	wxChoice* const WiiSensBarPos = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 2, str);
	wxSlider* const WiiSensBarSens = new wxSlider(this, wxID_ANY, 0, 0, 4);
	wxSlider* const WiimoteSpkVolume = new wxSlider(this, wxID_ANY, 0, 0, 127);
	wxCheckBox* const WiimoteMotor = new wxCheckBox(this, wxID_ANY, _("Wiimote Motor"));

	auto wiimote_speaker = new wxCheckBox(this, wxID_ANY, _("Enable Speaker Data"));
	wiimote_speaker->Bind(wxEVT_CHECKBOX, &ControllerConfigDiag::OnEnableSpeaker, this);
	wiimote_speaker->SetValue(SConfig::GetInstance().m_WiimoteEnableSpeaker);

	wxStaticText* const WiiSensBarPosText = new wxStaticText(this, wxID_ANY, _("Sensor Bar Position:"));
	wxStaticText* const WiiSensBarSensText = new wxStaticText(this, wxID_ANY, _("IR Sensitivity:"));
	wxStaticText* const WiiSensBarSensMinText = new wxStaticText(this, wxID_ANY, _("Min"));
	wxStaticText* const WiiSensBarSensMaxText = new wxStaticText(this, wxID_ANY, _("Max"));
	wxStaticText* const WiimoteSpkVolumeText = new wxStaticText(this, wxID_ANY, _("Speaker Volume:"));
	wxStaticText* const WiimoteSpkVolumeMinText = new wxStaticText(this, wxID_ANY, _("Min"));
	wxStaticText* const WiimoteSpkVolumeMaxText = new wxStaticText(this, wxID_ANY, _("Max"));

	// With some GTK themes, no minimum size will be applied - so do this manually here
	WiiSensBarSens->SetMinSize(wxSize(100,-1));
	WiimoteSpkVolume->SetMinSize(wxSize(100,-1));

	// Disable some controls when emulation is running
	if (Core::GetState() != Core::CORE_UNINITIALIZED)
	{
		WiiSensBarPos->Disable();
		WiiSensBarSens->Disable();
		WiimoteSpkVolume->Disable();
		WiimoteMotor->Disable();
		WiiSensBarPosText->Disable();
		WiiSensBarSensText->Disable();
		WiiSensBarSensMinText->Disable();
		WiiSensBarSensMaxText->Disable();
		WiimoteSpkVolumeText->Disable();
		WiimoteSpkVolumeMinText->Disable();
		WiimoteSpkVolumeMaxText->Disable();
	}

	// "General Settings" initialization
	WiiSensBarPos->SetSelection(SConfig::GetInstance().m_SYSCONF->GetData<u8>("BT.BAR"));
	WiiSensBarSens->SetValue(SConfig::GetInstance().m_SYSCONF->GetData<u32>("BT.SENS"));
	WiimoteSpkVolume->SetValue(SConfig::GetInstance().m_SYSCONF->GetData<u8>("BT.SPKV"));
	WiimoteMotor->SetValue(SConfig::GetInstance().m_SYSCONF->GetData<bool>("BT.MOT"));

	WiiSensBarPos->Bind(wxEVT_CHOICE, &ControllerConfigDiag::OnSensorBarPos, this);
	WiiSensBarSens->Bind(wxEVT_SLIDER, &ControllerConfigDiag::OnSensorBarSensitivity, this);
	WiimoteSpkVolume->Bind(wxEVT_SLIDER, &ControllerConfigDiag::OnSpeakerVolume, this);
	WiimoteMotor->Bind(wxEVT_CHECKBOX, &ControllerConfigDiag::OnMotor, this);

	// "General Settings" layout
	wxStaticBoxSizer* const general_sizer = new wxStaticBoxSizer(wxVERTICAL, this, _("General Settings"));
	wxFlexGridSizer* const choice_sizer = new wxFlexGridSizer(2, 5, 5);

	wxBoxSizer* const sensbarsens_sizer = new wxBoxSizer(wxHORIZONTAL);
	sensbarsens_sizer->Add(WiiSensBarSensMinText, 0, wxALIGN_CENTER_VERTICAL);
	sensbarsens_sizer->Add(WiiSensBarSens);
	sensbarsens_sizer->Add(WiiSensBarSensMaxText, 0, wxALIGN_CENTER_VERTICAL);

	wxBoxSizer* const spkvol_sizer = new wxBoxSizer(wxHORIZONTAL);
	spkvol_sizer->Add(WiimoteSpkVolumeMinText, 0, wxALIGN_CENTER_VERTICAL);
	spkvol_sizer->Add(WiimoteSpkVolume);
	spkvol_sizer->Add(WiimoteSpkVolumeMaxText, 0, wxALIGN_CENTER_VERTICAL);

	choice_sizer->Add(WiiSensBarPosText, 0, wxALIGN_CENTER_VERTICAL);
	choice_sizer->Add(WiiSensBarPos);
	choice_sizer->Add(WiiSensBarSensText, 0, wxALIGN_CENTER_VERTICAL);
	choice_sizer->Add(sensbarsens_sizer);
	choice_sizer->Add(WiimoteSpkVolumeText, 0, wxALIGN_CENTER_VERTICAL);
	choice_sizer->Add(spkvol_sizer);

	wxGridSizer* const general_wiimote_sizer = new wxGridSizer(1, 5, 5);
	general_wiimote_sizer->Add(WiimoteMotor);
	general_wiimote_sizer->Add(wiimote_speaker);

	general_sizer->Add(choice_sizer);
	general_sizer->Add(general_wiimote_sizer);

	return general_sizer;
}


void ControllerConfigDiag::ConfigEmulatedWiimote(wxCommandEvent& ev)
{
	InputConfig* const wiimote_plugin = Wiimote::GetConfig();

	HotkeyManagerEmu::Enable(false);

	InputConfigDialog m_ConfigFrame(this, *wiimote_plugin, _("Dolphin Emulated Wiimote Configuration"), m_wiimote_index_from_conf_bt_id[ev.GetId()]);
	m_ConfigFrame.ShowModal();

	HotkeyManagerEmu::Enable(true);
}

void ControllerConfigDiag::RefreshRealWiimotes(wxCommandEvent&)
{
	WiimoteReal::Refresh();
}

void ControllerConfigDiag::SelectSource(wxCommandEvent& event)
{
	// This needs to be changed now in order for refresh to work right.
	// Revert if the dialog is canceled.
	int index = m_wiimote_index_from_ctrl_id[event.GetId()];

	if (index != WIIMOTE_BALANCE_BOARD)
	{
		WiimoteReal::ChangeWiimoteSource(index, event.GetInt());
		if (g_wiimote_sources[index] != WIIMOTE_SRC_EMU && g_wiimote_sources[index] != WIIMOTE_SRC_HYBRID)
			wiimote_configure_bt[index]->Disable();
		else
			wiimote_configure_bt[index]->Enable();
	}
	else
	{
		WiimoteReal::ChangeWiimoteSource(index, event.GetInt() ? WIIMOTE_SRC_REAL : WIIMOTE_SRC_NONE);
	}
}

void ControllerConfigDiag::RevertSource()
{
	for (int i = 0; i < MAX_BBMOTES; ++i)
		g_wiimote_sources[i] = m_orig_wiimote_sources[i];
}

void ControllerConfigDiag::Save(wxCommandEvent& event)
{
	std::string ini_filename = File::GetUserPath(D_CONFIG_IDX) + WIIMOTE_INI_NAME ".ini";

	IniFile inifile;
	inifile.Load(ini_filename);

	for (unsigned int i=0; i<MAX_WIIMOTES; ++i)
	{
		std::string secname("Wiimote");
		secname += (char)('1' + i);
		IniFile::Section& sec = *inifile.GetOrCreateSection(secname);

		sec.Set("Source", (int)g_wiimote_sources[i]);
	}

	std::string secname("BalanceBoard");
	IniFile::Section& sec = *inifile.GetOrCreateSection(secname);
	sec.Set("Source", (int)g_wiimote_sources[WIIMOTE_BALANCE_BOARD]);

	inifile.Save(ini_filename);

	event.Skip();
}

void ControllerConfigDiag::Cancel(wxCommandEvent& event)
{
	RevertSource();
	event.Skip();
}

void ControllerConfigDiag::OnGameCubePortChanged(wxCommandEvent& event)
{
	const unsigned int device_num = m_gc_port_choice_ids[event.GetId()];
	const wxString device_name = event.GetString();

	SIDevices tempType;
	if (device_name == m_gc_pad_type_strs[1])
	{
		tempType = SIDEVICE_GC_CONTROLLER;
		gamecube_configure_bt[device_num]->Enable();
	}
	else if (device_name == m_gc_pad_type_strs[2])
	{
		tempType = SIDEVICE_GC_STEERING;
		gamecube_configure_bt[device_num]->Enable();
	}
	else if (device_name == m_gc_pad_type_strs[3])
	{
		tempType = SIDEVICE_DANCEMAT;
		gamecube_configure_bt[device_num]->Enable();
	}
	else if (device_name == m_gc_pad_type_strs[4])
	{
		tempType = SIDEVICE_GC_TARUKONGA;
		gamecube_configure_bt[device_num]->Enable();
	}
	else if (device_name == m_gc_pad_type_strs[5])
	{
		tempType = SIDEVICE_GC_GBA;
		gamecube_configure_bt[device_num]->Disable();
	}
	else if (device_name == m_gc_pad_type_strs[6])
	{
		tempType = SIDEVICE_GC_KEYBOARD;
		gamecube_configure_bt[device_num]->Enable();
	}
	else if (device_name == m_gc_pad_type_strs[7])
	{
		tempType = SIDEVICE_AM_BASEBOARD;
		gamecube_configure_bt[device_num]->Enable();
	}
	else
	{
		tempType = SIDEVICE_NONE;
		gamecube_configure_bt[device_num]->Disable();
	}

	SConfig::GetInstance().m_SIDevice[device_num] = tempType;

	if (Core::IsRunning())
		SerialInterface::ChangeDevice(tempType, device_num);
}

void ControllerConfigDiag::OnGameCubeConfigButton(wxCommandEvent& event)
{
	InputConfig* const pad_plugin = Pad::GetConfig();
	InputConfig* const key_plugin = Keyboard::GetConfig();
	const int port_num = m_gc_port_config_ids[event.GetId()];

	HotkeyManagerEmu::Enable(false);

	if (SConfig::GetInstance().m_SIDevice[port_num] == SIDEVICE_GC_KEYBOARD)
	{
		InputConfigDialog m_ConfigFrame(this, *key_plugin, _("GameCube Controller Configuration"), port_num);
		m_ConfigFrame.ShowModal();
	}
	else
	{
		InputConfigDialog m_ConfigFrame(this, *pad_plugin, _("GameCube Controller Configuration"), port_num);
		m_ConfigFrame.ShowModal();
	}

	HotkeyManagerEmu::Enable(true);
}

ControllerConfigDiag::~ControllerConfigDiag()
{
#if defined(__LIBUSB__) || defined (_WIN32)
	SI_GCAdapter::SetAdapterCallback(nullptr);
#endif
}
