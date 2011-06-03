
#include "WiimoteConfigDiag.h"
#include "HW/Wiimote.h"
#include "HW/WiimoteReal/WiimoteReal.h"
#include "Frame.h"

#define _connect_macro_(b, f, c, s)	(b)->Connect(wxID_ANY, (c), wxCommandEventHandler(f), (wxObject*)0, (wxEvtHandler*)s)

const wxString& ConnectedWiimotesString()
{
	static wxString str;
	str.Printf(_("Connected to %i Wiimotes"), WiimoteReal::Initialize());
	return str;
}

WiimoteConfigPage::WiimoteConfigPage(wxWindow* const parent, const int index)
	: wxNotebookPage(parent, -1, wxDefaultPosition, wxDefaultSize)
	, m_index(index), orig_source(g_wiimote_sources[index])
{
	// input source
	const wxString src_choices[] = { _("None"),
		_("Emulated Wiimote"), _("Real Wiimote"), _("Hybrid Wiimote") };

	wxChoice* const input_src_choice = new wxChoice(this, -1, wxDefaultPosition, wxDefaultSize,
		sizeof(src_choices)/sizeof(*src_choices), src_choices);
	input_src_choice->Select(g_wiimote_sources[m_index]);
	_connect_macro_(input_src_choice, WiimoteConfigPage::SelectSource, wxEVT_COMMAND_CHOICE_SELECTED, this);

	wxStaticBoxSizer* const input_src_sizer = new wxStaticBoxSizer(wxHORIZONTAL, this, _("Input Source"));
	input_src_sizer->Add(input_src_choice, 1, wxEXPAND | wxALL, 5);

	// emulated wiimote
	wxButton* const configure_wiimote_emu_btn = new wxButton(this, -1, _("Configure"));
	wxStaticBoxSizer* const wiimote_emu_sizer = new wxStaticBoxSizer(wxHORIZONTAL, this, _("Emulated Wiimote"));
	wiimote_emu_sizer->Add(configure_wiimote_emu_btn, 1, wxEXPAND | wxALL, 5);
	_connect_macro_(configure_wiimote_emu_btn, WiimoteConfigDiag::ConfigEmulatedWiimote, wxEVT_COMMAND_BUTTON_CLICKED, parent->GetParent());

	// real wiimote
	connected_wiimotes_txt = new wxStaticText(this, -1, ConnectedWiimotesString());

	wxButton* const refresh_btn = new wxButton(this, -1, _("Refresh"), wxDefaultPosition);
	_connect_macro_(refresh_btn, WiimoteConfigDiag::RefreshRealWiimotes, wxEVT_COMMAND_BUTTON_CLICKED, parent->GetParent());

	wxStaticBoxSizer* const wiimote_real_sizer = new wxStaticBoxSizer(wxVERTICAL, this, _("Real Wiimote"));
	wiimote_real_sizer->AddStretchSpacer(1);
	wiimote_real_sizer->Add(connected_wiimotes_txt, 0, wxALIGN_CENTER | wxBOTTOM | wxLEFT | wxRIGHT, 5);
#ifdef _WIN32
	wxButton* const pairup_btn = new wxButton(this, -1, _("Pair Up"), wxDefaultPosition);
	_connect_macro_(pairup_btn, WiimoteConfigDiag::PairUpRealWiimotes, wxEVT_COMMAND_BUTTON_CLICKED, parent->GetParent());
	wiimote_real_sizer->Add(pairup_btn, 0, wxALIGN_CENTER | wxBOTTOM, 5);
#endif
	wiimote_real_sizer->Add(refresh_btn, 0, wxALIGN_CENTER, 5);
	wiimote_real_sizer->AddStretchSpacer(1);

	// sizers
	wxBoxSizer* const left_sizer = new wxBoxSizer(wxVERTICAL);
	left_sizer->Add(input_src_sizer, 0, wxEXPAND | wxBOTTOM, 5);
	left_sizer->Add(wiimote_emu_sizer, 0, wxEXPAND, 0);

	wxBoxSizer* const main_sizer = new wxBoxSizer(wxHORIZONTAL);
	main_sizer->Add(left_sizer, 1, wxLEFT | wxTOP | wxBOTTOM | wxEXPAND, 5);
	main_sizer->Add(wiimote_real_sizer, 1, wxALL | wxEXPAND, 5);

	SetSizerAndFit(main_sizer);
	Layout();
}

WiimoteGeneralConfigPage::WiimoteGeneralConfigPage(wxWindow* const parent)
	: wxPanel(parent, -1, wxDefaultPosition)

{
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

	WiiSensBarSens->SetMinSize(wxSize(100,-1));
	WiimoteSpkVolume->SetMinSize(wxSize(100,-1));

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

	WiiSensBarPos->SetSelection(SConfig::GetInstance().m_SYSCONF->GetData<u8>("BT.BAR"));
	WiiSensBarSens->SetValue(SConfig::GetInstance().m_SYSCONF->GetData<u32>("BT.SENS"));
	WiimoteSpkVolume->SetValue(SConfig::GetInstance().m_SYSCONF->GetData<u8>("BT.SPKV"));
	WiimoteMotor->SetValue(SConfig::GetInstance().m_SYSCONF->GetData<bool>("BT.MOT"));
	WiimoteReconnectOnLoad->SetValue(SConfig::GetInstance().m_WiimoteReconnectOnLoad);


	_connect_macro_(WiiSensBarPos, WiimoteGeneralConfigPage::OnSensorBarPos, wxEVT_COMMAND_CHOICE_SELECTED, this);
	_connect_macro_(WiiSensBarSens, WiimoteGeneralConfigPage::OnSensorBarSensitivity, wxEVT_COMMAND_SLIDER_UPDATED, this);
	_connect_macro_(WiimoteSpkVolume, WiimoteGeneralConfigPage::OnSpeakerVolume, wxEVT_COMMAND_SLIDER_UPDATED, this);
	_connect_macro_(WiimoteMotor, WiimoteGeneralConfigPage::OnMotor, wxEVT_COMMAND_CHECKBOX_CLICKED, this);
	_connect_macro_(WiimoteReconnectOnLoad, WiimoteGeneralConfigPage::OnReconnectOnLoad, wxEVT_COMMAND_CHECKBOX_CLICKED, this);


	wxBoxSizer* const main_sizer = new wxBoxSizer(wxVERTICAL);
	wxStaticBoxSizer* const general_sizer = new wxStaticBoxSizer(wxVERTICAL, this, _("General Settings"));
	wxFlexGridSizer* const choice_sizer = new wxFlexGridSizer(2, 5, 5);

	wxBoxSizer* const sensbarsens_sizer = new wxBoxSizer(wxHORIZONTAL);
	sensbarsens_sizer->Add(WiiSensBarSensMinText, 1, wxALIGN_CENTER_VERTICAL, 0);
	sensbarsens_sizer->Add(WiiSensBarSens);
	sensbarsens_sizer->Add(WiiSensBarSensMaxText, 1, wxALIGN_CENTER_VERTICAL, 0);

	wxBoxSizer* const spkvol_sizer = new wxBoxSizer(wxHORIZONTAL);
	spkvol_sizer->Add(WiimoteSpkVolumeMinText, 1, wxALIGN_CENTER_VERTICAL, 0);
	spkvol_sizer->Add(WiimoteSpkVolume);
	spkvol_sizer->Add(WiimoteSpkVolumeMaxText, 1, wxALIGN_CENTER_VERTICAL, 0);

	choice_sizer->Add(WiiSensBarPosText, 1, wxALIGN_CENTER_VERTICAL, 0);
	choice_sizer->Add(WiiSensBarPos);
	choice_sizer->Add(WiiSensBarSensText, 1, wxALIGN_CENTER_VERTICAL, 0);
	choice_sizer->Add(sensbarsens_sizer);
	choice_sizer->Add(WiimoteSpkVolumeText, 1, wxALIGN_CENTER_VERTICAL, 0);
	choice_sizer->Add(spkvol_sizer);


	wxGridSizer* const wiimote_sizer = new wxGridSizer(1, 5, 5);
	wiimote_sizer->Add(WiimoteMotor);
	wiimote_sizer->Add(WiimoteReconnectOnLoad);

	general_sizer->Add(choice_sizer);
	general_sizer->Add(wiimote_sizer);

	main_sizer->Add(general_sizer, 0, wxEXPAND | wxALL, 5);
	main_sizer->AddStretchSpacer();
	SetSizerAndFit(main_sizer);
	Layout();
}

WiimoteConfigDiag::WiimoteConfigDiag(wxWindow* const parent, InputPlugin& plugin)
	: wxDialog(parent, -1, _("Dolphin Wiimote Configuration"), wxDefaultPosition, wxDefaultSize)
	, m_plugin(plugin)
{
	m_pad_notebook = new wxNotebook(this, -1, wxDefaultPosition, wxDefaultSize, wxNB_DEFAULT);
	m_pad_notebook->AddPage(new WiimoteGeneralConfigPage(m_pad_notebook), wxString(_("General")));
	for (unsigned int i = 0; i < 4; ++i)
	{
		WiimoteConfigPage* const wpage = new WiimoteConfigPage(m_pad_notebook, i);
		m_pad_notebook->AddPage(wpage, wxString(_("Wiimote ")) + wxChar('1'+i));
		m_wiimote_config_pages.push_back(wpage);
	}
	m_pad_notebook->SetSelection(0);

	Connect(wxID_OK, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(WiimoteConfigDiag::Save));
	Connect(wxID_CANCEL, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(WiimoteConfigDiag::Cancel));

	wxBoxSizer* const main_sizer = new wxBoxSizer(wxVERTICAL);
	main_sizer->Add(m_pad_notebook, 1, wxEXPAND | wxALL, 5);
	main_sizer->Add(CreateButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	SetSizerAndFit(main_sizer);

	Center();
}

void WiimoteConfigDiag::ConfigEmulatedWiimote(wxCommandEvent&)
{
	InputConfigDialog* const m_emu_config_diag = new InputConfigDialog(this, m_plugin, _trans("Dolphin Emulated Wiimote Configuration"), m_pad_notebook->GetSelection()-1);
	m_emu_config_diag->ShowModal();
	m_emu_config_diag->Destroy();
}

void WiimoteConfigDiag::UpdateGUI()
{
	for (std::vector<WiimoteConfigPage*>::iterator it = m_wiimote_config_pages.begin(); it != m_wiimote_config_pages.end(); ++it)
		(*it)->connected_wiimotes_txt->SetLabel(ConnectedWiimotesString());
}

#ifdef _WIN32
void WiimoteConfigDiag::PairUpRealWiimotes(wxCommandEvent&)
{
	const int paired = WiimoteReal::PairUp();

	if (paired > 0)
	{
		// Will this message be anoying?
		//PanicAlertT("Paired %d wiimotes.", paired);
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

void WiimoteConfigPage::SelectSource(wxCommandEvent& event)
{
	// This needs to be changed now in order for refresh to work right.
	// Revert if the dialog is canceled.
	g_wiimote_sources[m_index] = event.GetInt();
}

void WiimoteConfigPage::UpdateWiimoteStatus()
{
	if (orig_source != g_wiimote_sources[m_index])
	{
		// Disconnect first, otherwise the new source doesn't seem to work
		CFrame::ConnectWiimote(m_index, false);
		// Connect wiimotes
		if (WIIMOTE_SRC_EMU & g_wiimote_sources[m_index])
			CFrame::ConnectWiimote(m_index, true);
		else if (WIIMOTE_SRC_REAL & g_wiimote_sources[m_index] && WiimoteReal::g_wiimotes[m_index])
			CFrame::ConnectWiimote(m_index, WiimoteReal::g_wiimotes[m_index]->IsConnected());
	}
}

void WiimoteConfigPage::RevertSource()
{
	g_wiimote_sources[m_index] = orig_source;
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
	for (std::vector<WiimoteConfigPage*>::iterator it = m_wiimote_config_pages.begin(); it != m_wiimote_config_pages.end(); ++it)
		(*it)->UpdateWiimoteStatus();

	inifile.Save(ini_filename);

	event.Skip();
}

void WiimoteConfigDiag::Cancel(wxCommandEvent& event)
{
	for (std::vector<WiimoteConfigPage*>::iterator it = m_wiimote_config_pages.begin(); it != m_wiimote_config_pages.end(); ++it)
		(*it)->RevertSource();

	event.Skip();
}
