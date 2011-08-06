
#include "WiimoteConfigDiag.h"
#include "HW/Wiimote.h"
#include "HW/WiimoteReal/WiimoteReal.h"
#include "Frame.h"

#define _connect_macro_(b, f, c, s)	(b)->Connect(wxID_ANY, (c), wxCommandEventHandler(f), (wxObject*)0, (wxEvtHandler*)s)

const wxString& ConnectedWiimotesString()
{
	static wxString str;
	str.Printf(_("%i connected"), WiimoteReal::Initialize());
	return str;
}

WiimoteConfigDiag::WiimoteConfigDiag(wxWindow* const parent, InputPlugin& plugin)
	: wxDialog(parent, -1, _("Dolphin Wiimote Configuration"), wxDefaultPosition, wxDefaultSize)
	, m_plugin(plugin)
{
	wxBoxSizer* const main_sizer = new wxBoxSizer(wxVERTICAL);


	// "Wiimotes" controls
	wxStaticText* wiimote_label[4];
	wxChoice* wiimote_source_ch[4];

	for (unsigned int i = 0; i < 4; ++i)
	{
		wxString str;
		str.Printf(_("Wiimote %i"), i + 1);

		const wxString src_choices[] = { _("None"),
		_("Emulated Wiimote"), _("Real Wiimote"), _("Hybrid Wiimote") };

		// reserve four ids, so that we can calculate the index from the ids lateron
		// Stupid wx 2.8 doesn't support reserving sequential IDs, so we need to do that more complicated..
		int source_ctrl_id =  wxWindow::NewControlId();
		m_wiimote_index_from_ctrl_id.insert(std::pair<wxWindowID, unsigned int>(source_ctrl_id, i));

		int config_bt_id = wxWindow::NewControlId();
		m_wiimote_index_from_conf_bt_id.insert(std::pair<wxWindowID, unsigned int>(config_bt_id, i));

		wiimote_label[i] = new wxStaticText(this, wxID_ANY, str);
		wiimote_source_ch[i] = new wxChoice(this, source_ctrl_id, wxDefaultPosition, wxDefaultSize, sizeof(src_choices)/sizeof(*src_choices), src_choices);
		_connect_macro_(wiimote_source_ch[i], WiimoteConfigDiag::SelectSource, wxEVT_COMMAND_CHOICE_SELECTED, this);
		wiimote_configure_bt[i] = new wxButton(this, config_bt_id, _("Configure"));
		_connect_macro_(wiimote_configure_bt[i], WiimoteConfigDiag::ConfigEmulatedWiimote, wxEVT_COMMAND_BUTTON_CLICKED, this);

		m_orig_wiimote_sources[i] = g_wiimote_sources[i];
		wiimote_source_ch[i]->Select(m_orig_wiimote_sources[i]);
		if (m_orig_wiimote_sources[i] != WIIMOTE_SRC_EMU && m_orig_wiimote_sources[i] != WIIMOTE_SRC_HYBRID)
			wiimote_configure_bt[i]->Disable();
	}


	// "Wiimotes" layout
	wxStaticBoxSizer* const wiimote_group = new wxStaticBoxSizer(wxHORIZONTAL, this, _("Wiimotes"));
	wxFlexGridSizer* const wiimote_sizer = new wxFlexGridSizer(3, 5, 5);
	for (unsigned int i = 0; i < 4; ++i)
	{
		wiimote_sizer->Add(wiimote_label[i], 0, wxALIGN_CENTER_VERTICAL);
		wiimote_sizer->Add(wiimote_source_ch[i], 0, wxALIGN_CENTER_VERTICAL);
		wiimote_sizer->Add(wiimote_configure_bt[i]);
	}
	wiimote_group->Add(wiimote_sizer, 1, wxEXPAND, 5 );


	// "Real wiimotes" controls
	connected_wiimotes_txt = new wxStaticText(this, -1, ConnectedWiimotesString());

	wxButton* const refresh_btn = new wxButton(this, -1, _("Refresh"), wxDefaultPosition);
	_connect_macro_(refresh_btn, WiimoteConfigDiag::RefreshRealWiimotes, wxEVT_COMMAND_BUTTON_CLICKED, this);

#ifdef _WIN32
	wxButton* const pairup_btn = new wxButton(this, -1, _("Pair Up"), wxDefaultPosition);
	_connect_macro_(pairup_btn, WiimoteConfigDiag::PairUpRealWiimotes, wxEVT_COMMAND_BUTTON_CLICKED, this);
#endif


	// "Real wiimotes" layout
	wxStaticBoxSizer* const real_wiimotes_group = new wxStaticBoxSizer(wxHORIZONTAL, this, _("Real Wiimotes"));
	wxFlexGridSizer* const real_wiimotes_sizer = new wxFlexGridSizer(3, 5, 5);
	real_wiimotes_sizer->Add(connected_wiimotes_txt, 0, wxALIGN_CENTER_VERTICAL);
#ifdef _WIN32
	real_wiimotes_sizer->Add(pairup_btn);
#endif
	real_wiimotes_sizer->Add(refresh_btn);
	real_wiimotes_group->Add(real_wiimotes_sizer, 1, wxALL, 5);


	// "General Settings" controls
	const wxString str[] = { _("Bottom"), _("Top") };
	wxChoice* const WiiSensBarPos = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 2, str);
	wxSlider* const WiiSensBarSens = new wxSlider(this, wxID_ANY, 0, 0, 4, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL);
	wxSlider* const WiimoteSpkVolume = new wxSlider(this, wxID_ANY, 0, 0, 127);
	wxCheckBox* const WiimoteMotor = new wxCheckBox(this, wxID_ANY, _("Wiimote Motor"));
	wxCheckBox* const WiimoteReconnectOnLoad = new wxCheckBox(this, wxID_ANY, _("Reconnect Wiimote on State Loading"));

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
	WiimoteReconnectOnLoad->SetValue(SConfig::GetInstance().m_WiimoteReconnectOnLoad);

	_connect_macro_(WiiSensBarPos, WiimoteConfigDiag::OnSensorBarPos, wxEVT_COMMAND_CHOICE_SELECTED, this);
	_connect_macro_(WiiSensBarSens, WiimoteConfigDiag::OnSensorBarSensitivity, wxEVT_COMMAND_SLIDER_UPDATED, this);
	_connect_macro_(WiimoteSpkVolume, WiimoteConfigDiag::OnSpeakerVolume, wxEVT_COMMAND_SLIDER_UPDATED, this);
	_connect_macro_(WiimoteMotor, WiimoteConfigDiag::OnMotor, wxEVT_COMMAND_CHECKBOX_CLICKED, this);
	_connect_macro_(WiimoteReconnectOnLoad, WiimoteConfigDiag::OnReconnectOnLoad, wxEVT_COMMAND_CHECKBOX_CLICKED, this);


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
	general_wiimote_sizer->Add(WiimoteReconnectOnLoad);

	general_sizer->Add(choice_sizer);
	general_sizer->Add(general_wiimote_sizer);


	// Dialog layout
	main_sizer->Add(wiimote_group, 0, wxEXPAND | wxALL, 5);
	main_sizer->Add(real_wiimotes_group, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	main_sizer->Add(general_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	main_sizer->Add(CreateButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	Connect(wxID_OK, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(WiimoteConfigDiag::Save));
	Connect(wxID_CANCEL, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(WiimoteConfigDiag::Cancel));

	SetSizerAndFit(main_sizer);
	Center();
}


void WiimoteConfigDiag::ConfigEmulatedWiimote(wxCommandEvent& ev)
{
	InputConfigDialog* const m_emu_config_diag = new InputConfigDialog(this, m_plugin, _trans("Dolphin Emulated Wiimote Configuration"), m_wiimote_index_from_conf_bt_id[ev.GetId()]);
	m_emu_config_diag->ShowModal();
	m_emu_config_diag->Destroy();
}

void WiimoteConfigDiag::UpdateGUI()
{
	connected_wiimotes_txt->SetLabel(ConnectedWiimotesString());
}

#ifdef _WIN32
void WiimoteConfigDiag::PairUpRealWiimotes(wxCommandEvent&)
{
	const int paired = WiimoteReal::PairUp();

	if (paired > 0)
	{
		// TODO: Maybe add a label of newly paired up wiimotes?
		WiimoteReal::Refresh();
		UpdateGUI();
	}
	else if (paired < 0)
		PanicAlertT("A supported bluetooth device was not found!\n"
				"(Only the Microsoft bluetooth stack is supported.)");
}
#endif

void WiimoteConfigDiag::RefreshRealWiimotes(wxCommandEvent&)
{
	WiimoteReal::Refresh();
	UpdateGUI();
}

void WiimoteConfigDiag::SelectSource(wxCommandEvent& event)
{
	// This needs to be changed now in order for refresh to work right.
	// Revert if the dialog is canceled.
	int index = m_wiimote_index_from_ctrl_id[event.GetId()];
	g_wiimote_sources[index] = event.GetInt();
	if (g_wiimote_sources[index] != WIIMOTE_SRC_EMU && g_wiimote_sources[index] != WIIMOTE_SRC_HYBRID)
		wiimote_configure_bt[index]->Disable();
	else
		wiimote_configure_bt[index]->Enable();
}

void WiimoteConfigDiag::UpdateWiimoteStatus()
{
	for (int index = 0; index < 4; ++index)
	{
		if (m_orig_wiimote_sources[index] != g_wiimote_sources[index])
		{
			// Disconnect first, otherwise the new source doesn't seem to work
			CFrame::ConnectWiimote(index, false);
			// Connect wiimotes
			if (WIIMOTE_SRC_EMU & g_wiimote_sources[index])
				CFrame::ConnectWiimote(index, true);
			else if (WIIMOTE_SRC_REAL & g_wiimote_sources[index] && WiimoteReal::g_wiimotes[index])
				CFrame::ConnectWiimote(index, WiimoteReal::g_wiimotes[index]->IsConnected());
		}
	}
}

void WiimoteConfigDiag::RevertSource()
{
	for (int i = 0; i < 4; ++i)
		g_wiimote_sources[i] = m_orig_wiimote_sources[i];
}

void WiimoteConfigDiag::Save(wxCommandEvent& event)
{
	std::string ini_filename = File::GetUserPath(D_CONFIG_IDX) + WIIMOTE_INI_NAME ".ini";

	IniFile inifile;
	inifile.Load(ini_filename);

	for (unsigned int i=0; i<MAX_WIIMOTES; ++i)
	{
		std::string secname("Wiimote");
		secname += (char)('1' + i);
		IniFile::Section& sec = *inifile.GetOrCreateSection(secname.c_str());

		sec.Set("Source", (int)g_wiimote_sources[i]);
	}
	UpdateWiimoteStatus();

	inifile.Save(ini_filename);

	event.Skip();
}

void WiimoteConfigDiag::Cancel(wxCommandEvent& event)
{
	RevertSource();
	event.Skip();
}
