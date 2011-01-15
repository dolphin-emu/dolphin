// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "Config.h"
#include "ConfigDlg.h"

BEGIN_EVENT_TABLE(DSPConfigDialogHLE, wxDialog)
	EVT_BUTTON(wxID_OK, DSPConfigDialogHLE::SettingsChanged)
	EVT_CHECKBOX(ID_ENABLE_HLE_AUDIO, DSPConfigDialogHLE::SettingsChanged)
	EVT_CHECKBOX(ID_ENABLE_DTK_MUSIC, DSPConfigDialogHLE::SettingsChanged)
	EVT_CHECKBOX(ID_ENABLE_THROTTLE, DSPConfigDialogHLE::SettingsChanged)
	EVT_CHOICE(ID_FREQUENCY, DSPConfigDialogHLE::SettingsChanged)
	EVT_CHOICE(ID_BACKEND, DSPConfigDialogHLE::BackendChanged)
	EVT_COMMAND_SCROLL(ID_VOLUME, DSPConfigDialogHLE::VolumeChanged)
END_EVENT_TABLE()

DSPConfigDialogHLE::DSPConfigDialogHLE(wxWindow *parent, wxWindowID id,
		const wxString &title, const wxPoint &position, const wxSize& size, long style)
: wxDialog(parent, id, title, position, size, style)
{
	wxButton *m_OK = new wxButton(this, wxID_OK, _("OK"),
			wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

	wxStaticBoxSizer *sbSettings = new wxStaticBoxSizer(wxVERTICAL, this, _("Sound Settings"));
	wxStaticBoxSizer *sbSettingsV = new wxStaticBoxSizer(wxVERTICAL, this, _("Volume"));

	// Create items
	m_buttonEnableHLEAudio = new wxCheckBox(this, ID_ENABLE_HLE_AUDIO, _("Enable HLE Audio"),
			wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_buttonEnableDTKMusic = new wxCheckBox(this, ID_ENABLE_DTK_MUSIC, _("Enable DTK Music"),
			wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_buttonEnableThrottle = new wxCheckBox(this, ID_ENABLE_THROTTLE, _("Enable Audio Throttle"),
			wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

	wxStaticText *FrequencyText = new wxStaticText(this, wxID_ANY, _("Sample Rate"),
		   	wxDefaultPosition, wxDefaultSize, 0);
	m_FrequencySelection = new wxChoice(this, ID_FREQUENCY, wxDefaultPosition, wxDefaultSize,
			wxArrayRates, 0, wxDefaultValidator, wxEmptyString);

	wxStaticText *BackendText = new wxStaticText(this, wxID_ANY, _("Audio Backend"),
		   	wxDefaultPosition, wxDefaultSize, 0);
	m_BackendSelection = new wxChoice(this, ID_BACKEND, wxDefaultPosition, wxDefaultSize,
			wxArrayBackends, 0, wxDefaultValidator, wxEmptyString);

	m_volumeSlider = new wxSlider(this, ID_VOLUME, ac_Config.m_Volume, 1, 100,
			wxDefaultPosition, wxDefaultSize, wxSL_VERTICAL | wxSL_INVERSE);
	m_volumeSlider->Enable(SupportsVolumeChanges(ac_Config.sBackend));
	m_volumeText = new wxStaticText(this, wxID_ANY, wxString::Format(wxT("%d %%"),
				ac_Config.m_Volume), wxDefaultPosition, wxDefaultSize, 0);

	// Update values
	m_buttonEnableHLEAudio->SetValue(g_Config.m_EnableHLEAudio ? true : false);
	m_buttonEnableDTKMusic->SetValue(ac_Config.m_EnableDTKMusic ? true : false);
	m_buttonEnableThrottle->SetValue(ac_Config.m_EnableThrottle ? true : false);

	// Add tooltips
	m_buttonEnableHLEAudio->SetToolTip(_("This is usually used to play voice and sound effects."));
	m_buttonEnableDTKMusic->SetToolTip(_("This is used to play music tracks, like BGM."));
	m_buttonEnableThrottle->SetToolTip(_("This is used to control game speed by sound throttle.\nDisabling this could cause abnormal game speed, such as too fast.\nBut sometimes enabling this could cause constant noise.\n\nKeyboard Shortcut <TAB>:  Hold down to instantly disable Throttle."));
	m_FrequencySelection->
		SetToolTip(_("Changing this will have no effect while the emulator is running!"));
	m_BackendSelection->
		SetToolTip(_("Changing this will have no effect while the emulator is running!"));

	// Create sizer and add items to dialog
	wxBoxSizer *sMain = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer *sSettings = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer *sBackend = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer *sFrequency = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer *sButtons = new wxBoxSizer(wxHORIZONTAL);

	sbSettings->Add(m_buttonEnableHLEAudio, 0, wxALL, 5);
	sbSettings->Add(m_buttonEnableDTKMusic, 0, wxALL, 5);
	sbSettings->Add(m_buttonEnableThrottle, 0, wxALL, 5);

	sFrequency->Add(FrequencyText, 0, wxALIGN_LEFT|wxALL, 1);
	sFrequency->Add(m_FrequencySelection, 0, wxALL, 1);

	m_FrequencySelection->Append(_("48,000 Hz"));
	m_FrequencySelection->Append(_("32,000 Hz"));
	#ifdef __APPLE__
	int num = m_FrequencySelection->FindString(wxString::FromAscii(ac_Config.sFrequency));
	#else
	int num = m_FrequencySelection->FindString(wxString::FromAscii(ac_Config.sFrequency.c_str()));
	#endif
	m_FrequencySelection->SetSelection(num);

	sbSettings->Add(sFrequency, 0, wxALL, 7);


	sBackend->Add(BackendText, 0, wxALIGN_LEFT|wxALL, 1);
	sBackend->Add(m_BackendSelection, 0, wxALL, 1);
	sbSettings->Add(sBackend, 0, wxALL, 7);

	sbSettingsV->Add(m_volumeSlider, 1, wxLEFT|wxRIGHT|wxALIGN_CENTER, 6);
	sbSettingsV->Add(m_volumeText, 0, wxALL|wxALIGN_LEFT, 4);

	sSettings->Add(sbSettings, 0, wxALL|wxEXPAND, 4);
	sSettings->Add(sbSettingsV, 0, wxALL|wxEXPAND, 4);
	sMain->Add(sSettings, 0, wxALL|wxEXPAND, 4);

	sButtons->AddStretchSpacer();
	sButtons->Add(m_OK, 0, wxALL, 1);
	sMain->Add(sButtons, 0, wxALL|wxEXPAND, 4);
	SetSizerAndFit(sMain);

	// Center window
	CenterOnParent();
}

// Add audio output options
void DSPConfigDialogHLE::AddBackend(const char* backend)
{
	// Update values
	m_BackendSelection->Append(wxString::FromAscii(backend));
#ifdef __APPLE__
	int num = m_BackendSelection->FindString(wxString::FromAscii(ac_Config.sBackend));
#else
	int num = m_BackendSelection->FindString(wxString::FromAscii(ac_Config.sBackend.c_str()));
#endif
	m_BackendSelection->SetSelection(num);
}

void DSPConfigDialogHLE::ClearBackends()
{
	m_BackendSelection->Clear();
}

DSPConfigDialogHLE::~DSPConfigDialogHLE()
{
}

void DSPConfigDialogHLE::VolumeChanged(wxScrollEvent& WXUNUSED(event))
{
	ac_Config.m_Volume = m_volumeSlider->GetValue();
	ac_Config.Update();

	m_volumeText->SetLabel(wxString::Format(wxT("%d %%"), m_volumeSlider->GetValue()));
}

void DSPConfigDialogHLE::SettingsChanged(wxCommandEvent& event)
{
	g_Config.m_EnableHLEAudio = m_buttonEnableHLEAudio->GetValue();
	ac_Config.m_EnableDTKMusic = m_buttonEnableDTKMusic->GetValue();
	ac_Config.m_EnableThrottle = m_buttonEnableThrottle->GetValue();

#ifdef __APPLE__
	strncpy(ac_Config.sBackend, m_BackendSelection->GetStringSelection().mb_str(), 128);
	strncpy(ac_Config.sFrequency, m_FrequencySelection->GetStringSelection().mb_str(), 128);
#else
	ac_Config.sBackend = m_BackendSelection->GetStringSelection().mb_str();
	ac_Config.sFrequency = m_FrequencySelection->GetStringSelection().mb_str();
#endif
	ac_Config.Update();
	g_Config.Save();

	if (event.GetId() == wxID_OK)
		EndModal(wxID_OK);
}

bool DSPConfigDialogHLE::SupportsVolumeChanges(std::string backend)
{
	//FIXME: this one should ask the backend whether it supports it.
	//       but getting the backend from string etc. is probably
	//       too much just to enable/disable a stupid slider...
	return (backend == BACKEND_DIRECTSOUND ||
			backend == BACKEND_COREAUDIO ||
			backend == BACKEND_OPENAL ||
			backend == BACKEND_XAUDIO2 ||
			backend == BACKEND_PULSEAUDIO);
}

void DSPConfigDialogHLE::BackendChanged(wxCommandEvent& event)
{
	m_volumeSlider->Enable(SupportsVolumeChanges(
				std::string(m_BackendSelection->GetStringSelection().mb_str())));
}
