// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <string>

#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/gbsizer.h>
#include <wx/radiobox.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>

#include "AudioCommon/AudioCommon.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "DolphinWX/WxUtils.h"
#include "DolphinWX/Config/AudioConfigPane.h"

AudioConfigPane::AudioConfigPane(wxWindow* parent, wxWindowID id)
	: wxPanel(parent, id)
{
	InitializeGUI();
	LoadGUIValues();
	RefreshGUI();
}

void AudioConfigPane::InitializeGUI()
{
	m_dsp_engine_strings.Add(_("DSP HLE emulation (fast)"));
	m_dsp_engine_strings.Add(_("DSP LLE recompiler"));
	m_dsp_engine_strings.Add(_("DSP LLE interpreter (slow)"));

	m_dsp_engine_radiobox = new wxRadioBox(this, wxID_ANY, _("DSP Emulator Engine"), wxDefaultPosition, wxDefaultSize, m_dsp_engine_strings, 0, wxRA_SPECIFY_ROWS);
	m_dpl2_decoder_checkbox = new wxCheckBox(this, wxID_ANY, _("Dolby Pro Logic II decoder"));
	m_volume_slider = new wxSlider(this, wxID_ANY, 0, 0, 100, wxDefaultPosition, wxDefaultSize, wxSL_VERTICAL | wxSL_INVERSE);
	m_volume_text = new wxStaticText(this, wxID_ANY, "");
	m_audio_backend_choice = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_audio_backend_strings);
	m_audio_latency_spinctrl = new wxSpinCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 30);

	m_dsp_engine_radiobox->Bind(wxEVT_RADIOBOX, &AudioConfigPane::OnDSPEngineRadioBoxChanged, this);
	m_dpl2_decoder_checkbox->Bind(wxEVT_CHECKBOX, &AudioConfigPane::OnDPL2DecoderCheckBoxChanged, this);
	m_volume_slider->Bind(wxEVT_SLIDER, &AudioConfigPane::OnVolumeSliderChanged, this);
	m_audio_backend_choice->Bind(wxEVT_CHOICE, &AudioConfigPane::OnAudioBackendChanged, this);
	m_audio_latency_spinctrl->Bind(wxEVT_SPINCTRL, &AudioConfigPane::OnLatencySpinCtrlChanged, this);

	m_audio_backend_choice->SetToolTip(_("Changing this will have no effect while the emulator is running."));
	m_audio_latency_spinctrl->SetToolTip(_("Sets the latency (in ms). Higher values may reduce audio crackling. OpenAL backend only."));
#if defined(__APPLE__)
	m_dpl2_decoder_checkbox->SetToolTip(_("Enables Dolby Pro Logic II emulation using 5.1 surround. Not available on OS X."));
#else
	m_dpl2_decoder_checkbox->SetToolTip(_("Enables Dolby Pro Logic II emulation using 5.1 surround. OpenAL or Pulse backends only."));
#endif

	wxStaticBoxSizer* const dsp_engine_sizer = new wxStaticBoxSizer(wxVERTICAL, this, _("Sound Settings"));
	dsp_engine_sizer->Add(m_dsp_engine_radiobox, 0, wxALL | wxEXPAND, 5);
	dsp_engine_sizer->Add(m_dpl2_decoder_checkbox, 0, wxALL, 5);

	wxStaticBoxSizer* const volume_sizer = new wxStaticBoxSizer(wxVERTICAL, this, _("Volume"));
	volume_sizer->Add(m_volume_slider, 1, wxLEFT | wxRIGHT, 13);
	volume_sizer->Add(m_volume_text, 0, wxALIGN_CENTER | wxALL, 5);

	wxGridBagSizer* const backend_grid_sizer = new wxGridBagSizer();
	backend_grid_sizer->Add(new wxStaticText(this, wxID_ANY, _("Audio Backend:")), wxGBPosition(0, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
	backend_grid_sizer->Add(m_audio_backend_choice, wxGBPosition(0, 1), wxDefaultSpan, wxALL, 5);
	backend_grid_sizer->Add(new wxStaticText(this, wxID_ANY, _("Latency:")), wxGBPosition(1, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
	backend_grid_sizer->Add(m_audio_latency_spinctrl, wxGBPosition(1, 1), wxDefaultSpan, wxALL, 5);

	wxStaticBoxSizer* const backend_static_box_sizer = new wxStaticBoxSizer(wxHORIZONTAL, this, _("Backend Settings"));
	backend_static_box_sizer->Add(backend_grid_sizer, 0, wxEXPAND);

	wxBoxSizer* const dsp_audio_sizer = new wxBoxSizer(wxHORIZONTAL);
	dsp_audio_sizer->Add(dsp_engine_sizer, 1, wxEXPAND | wxALL, 5);
	dsp_audio_sizer->Add(volume_sizer, 0, wxEXPAND | wxALL, 5);

	wxBoxSizer* const main_sizer = new wxBoxSizer(wxVERTICAL);
	main_sizer->Add(dsp_audio_sizer, 0, wxALL | wxEXPAND);
	main_sizer->Add(backend_static_box_sizer, 0, wxALL | wxEXPAND, 5);

	SetSizerAndFit(main_sizer);
}

void AudioConfigPane::LoadGUIValues()
{
	PopulateBackendChoiceBox();

	const SCoreStartupParameter& startup_params = SConfig::GetInstance().m_LocalCoreStartupParameter;

	// Audio DSP Engine
	if (startup_params.bDSPHLE)
		m_dsp_engine_radiobox->SetSelection(0);
	else
		m_dsp_engine_radiobox->SetSelection(SConfig::GetInstance().m_DSPEnableJIT ? 1 : 2);

	m_volume_slider->Enable(SupportsVolumeChanges(SConfig::GetInstance().sBackend));
	m_volume_slider->SetValue(SConfig::GetInstance().m_Volume);

	m_volume_text->SetLabel(wxString::Format("%d %%", SConfig::GetInstance().m_Volume));

	m_dpl2_decoder_checkbox->Enable(std::string(SConfig::GetInstance().sBackend) == BACKEND_OPENAL
		|| std::string(SConfig::GetInstance().sBackend) == BACKEND_PULSEAUDIO);
	m_dpl2_decoder_checkbox->SetValue(startup_params.bDPL2Decoder);

	m_audio_latency_spinctrl->Enable(std::string(SConfig::GetInstance().sBackend) == BACKEND_OPENAL);
	m_audio_latency_spinctrl->SetValue(startup_params.iLatency);
}

void AudioConfigPane::RefreshGUI()
{
	if (Core::IsRunning())
	{
		m_audio_latency_spinctrl->Disable();
		m_audio_backend_choice->Disable();
		m_dpl2_decoder_checkbox->Disable();
		m_dsp_engine_radiobox->Disable();
	}
}

void AudioConfigPane::OnDSPEngineRadioBoxChanged(wxCommandEvent& event)
{
	SConfig::GetInstance().m_LocalCoreStartupParameter.bDSPHLE = m_dsp_engine_radiobox->GetSelection() == 0;
	SConfig::GetInstance().m_DSPEnableJIT = m_dsp_engine_radiobox->GetSelection() == 1;
	AudioCommon::UpdateSoundStream();
}

void AudioConfigPane::OnDPL2DecoderCheckBoxChanged(wxCommandEvent&)
{
	SConfig::GetInstance().m_LocalCoreStartupParameter.bDPL2Decoder = m_dpl2_decoder_checkbox->IsChecked();
}

void AudioConfigPane::OnVolumeSliderChanged(wxCommandEvent& event)
{
	SConfig::GetInstance().m_Volume = m_volume_slider->GetValue();
	AudioCommon::UpdateSoundStream();
	m_volume_text->SetLabel(wxString::Format("%d %%", m_volume_slider->GetValue()));
}

void AudioConfigPane::OnAudioBackendChanged(wxCommandEvent& event)
{
	m_volume_slider->Enable(SupportsVolumeChanges(WxStrToStr(m_audio_backend_choice->GetStringSelection())));
	m_audio_latency_spinctrl->Enable(WxStrToStr(m_audio_backend_choice->GetStringSelection()) == BACKEND_OPENAL);
	m_dpl2_decoder_checkbox->Enable(WxStrToStr(m_audio_backend_choice->GetStringSelection()) == BACKEND_OPENAL ||
	                                WxStrToStr(m_audio_backend_choice->GetStringSelection()) == BACKEND_PULSEAUDIO);

	// Don't save the translated BACKEND_NULLSOUND string
	SConfig::GetInstance().sBackend = m_audio_backend_choice->GetSelection() ?
		WxStrToStr(m_audio_backend_choice->GetStringSelection()) : BACKEND_NULLSOUND;

	AudioCommon::UpdateSoundStream();
}

void AudioConfigPane::OnLatencySpinCtrlChanged(wxCommandEvent& event)
{
	SConfig::GetInstance().m_LocalCoreStartupParameter.iLatency = m_audio_latency_spinctrl->GetValue();
}

void AudioConfigPane::PopulateBackendChoiceBox()
{
	for (const std::string& backend : AudioCommon::GetSoundBackends())
	{
		m_audio_backend_choice->Append(wxGetTranslation(StrToWxStr(backend)));

		int num = m_audio_backend_choice->FindString(StrToWxStr(SConfig::GetInstance().sBackend));
		m_audio_backend_choice->SetSelection(num);
	}
}

bool AudioConfigPane::SupportsVolumeChanges(const std::string& backend)
{
	//FIXME: this one should ask the backend whether it supports it.
	//       but getting the backend from string etc. is probably
	//       too much just to enable/disable a stupid slider...
	return (backend == BACKEND_COREAUDIO ||
	        backend == BACKEND_OPENAL ||
	        backend == BACKEND_XAUDIO2);
}
