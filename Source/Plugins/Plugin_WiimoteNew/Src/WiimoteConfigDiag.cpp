
#include "WiimoteConfigDiag.h"
#include "WiimoteReal/WiimoteReal.h"

#define _connect_macro_(b, f, c, s)	(b)->Connect(wxID_ANY, (c), wxCommandEventHandler( f ), (wxObject*)0, (wxEvtHandler*)s)

WiimoteConfigPage::WiimoteConfigPage(wxWindow* const parent, const int index)
	: wxNotebookPage(parent, -1, wxDefaultPosition, wxDefaultSize)
	, m_index(index)
{
	// input source
	m_input_src_choice = new wxChoice(this, -1, wxDefaultPosition);
	m_input_src_choice->Append(wxT("None"));
	m_input_src_choice->Append(wxT("Emulated Wiimote"));
	m_input_src_choice->Append(wxT("Real Wiimote"));
	m_input_src_choice->Select(g_wiimote_sources[m_index]);
	_connect_macro_(m_input_src_choice, WiimoteConfigPage::SelectSource, wxEVT_COMMAND_CHOICE_SELECTED, this);

	wxStaticBoxSizer* const input_src_sizer = new wxStaticBoxSizer(wxHORIZONTAL, this, wxT("Input Source"));
	input_src_sizer->Add(m_input_src_choice, 1, wxEXPAND | wxALL, 5);

	// emulated wiimote
	wxButton* const configure_wiimote_emu_btn = new wxButton(this, -1, wxT("Configure"));
	wxStaticBoxSizer* const wiimote_emu_sizer = new wxStaticBoxSizer(wxHORIZONTAL, this, wxT("Emulated Wiimote"));
	wiimote_emu_sizer->Add(configure_wiimote_emu_btn, 1, wxEXPAND | wxALL, 5);
	_connect_macro_(configure_wiimote_emu_btn, WiimoteConfigDiag::ConfigEmulatedWiimote, wxEVT_COMMAND_BUTTON_CLICKED, parent->GetParent());

	// real wiimote
	m_connected_wiimotes_txt = new wxStaticText(this, -1, wxEmptyString);
	wxStaticBoxSizer* const wiimote_real_sizer = new wxStaticBoxSizer(wxVERTICAL, this, wxT("Real Wiimote"));
	wxButton* const refresh_btn = new wxButton(this, -1, wxT("Refresh"), wxDefaultPosition);
	_connect_macro_(refresh_btn, WiimoteConfigPage::RefreshRealWiimotes, wxEVT_COMMAND_BUTTON_CLICKED, this);

	wiimote_real_sizer->Add(m_connected_wiimotes_txt, 1, wxALIGN_CENTER | wxALL, 5);
	wiimote_real_sizer->Add(refresh_btn, 1, wxALIGN_CENTER | wxALL, 5);

	m_connected_wiimotes_txt->SetLabel(wxString(wxT("Connected to ")) + wxChar(wxT('0') + WiimoteReal::Initialize()) + wxT(" Real Wiimotes"));

	// sizers
	wxBoxSizer* const left_sizer = new wxBoxSizer(wxVERTICAL);
	left_sizer->Add(input_src_sizer, 1, wxEXPAND | wxBOTTOM, 5);
	left_sizer->Add(wiimote_emu_sizer, 1, wxEXPAND, 0);

	wxBoxSizer* const main_sizer = new wxBoxSizer(wxHORIZONTAL);
	main_sizer->Add(left_sizer, 1, wxLEFT | wxTOP | wxBOTTOM | wxEXPAND, 5);
	main_sizer->Add(wiimote_real_sizer, 1, wxALL | wxEXPAND, 5);

	SetSizerAndFit(main_sizer);
	Layout();
}

WiimoteConfigDiag::WiimoteConfigDiag(wxWindow* const parent, InputPlugin& plugin)
	: wxDialog(parent, -1, wxT("Dolphin Wiimote Configuration"), wxDefaultPosition, wxDefaultSize)
	, m_plugin(plugin)
{
	m_pad_notebook = new wxNotebook(this, -1, wxDefaultPosition, wxDefaultSize, wxNB_DEFAULT);
	for (unsigned int i = 0; i < 4; ++i)
	{
		WiimoteConfigPage* const wpage = new WiimoteConfigPage(m_pad_notebook, i);
		//m_padpages.push_back(wpage);
		m_pad_notebook->AddPage(wpage, wxString(wxT("Wiimote ")) + wxChar('1'+i));
	}

	wxButton* const ok_button = new wxButton(this, -1, wxT("OK"), wxDefaultPosition);
	_connect_macro_(ok_button, WiimoteConfigDiag::Save, wxEVT_COMMAND_BUTTON_CLICKED, this);

	wxBoxSizer* const main_sizer = new wxBoxSizer(wxVERTICAL);
	main_sizer->Add(m_pad_notebook, 1, wxEXPAND | wxALL, 5);
	main_sizer->Add(ok_button, 0, wxALIGN_RIGHT | wxRIGHT | wxBOTTOM, 5);

	SetSizerAndFit(main_sizer);

	Center();
}

void WiimoteConfigDiag::ConfigEmulatedWiimote(wxCommandEvent& event)
{
	InputConfigDialog* const m_emu_config_diag = new InputConfigDialog(this, m_plugin, "Dolphin Emulated Wiimote Configuration", m_pad_notebook->GetSelection());
	m_emu_config_diag->ShowModal();
	m_emu_config_diag->Destroy();
}

void WiimoteConfigPage::RefreshRealWiimotes(wxCommandEvent& event)
{
	WiimoteReal::Refresh();
	m_connected_wiimotes_txt->SetLabel(wxString(wxT("Connected to ")) + wxChar(wxT('0') + WiimoteReal::Initialize()) + wxT(" Real Wiimotes"));
}

void WiimoteConfigPage::SelectSource(wxCommandEvent& event)
{
	// should be kinda fine, maybe should just set when user clicks OK, w/e change it later
	g_wiimote_sources[m_index] = m_input_src_choice->GetSelection();
}

void WiimoteConfigDiag::Save(wxCommandEvent& event)
{
	std::string ini_filename = (std::string(File::GetUserPath(D_CONFIG_IDX)) + g_plugin.ini_name + ".ini" );

	IniFile inifile;
	inifile.Load(ini_filename);

	for (unsigned int i=0; i<MAX_WIIMOTES; ++i)
	{
		std::string secname("Wiimote");
		secname += (char)('1' + i);
		IniFile::Section& sec = *inifile.GetOrCreateSection(secname.c_str());

		sec.Set("Source", (int)g_wiimote_sources[i]);
	}

	inifile.Save(ini_filename);

	Close();
}