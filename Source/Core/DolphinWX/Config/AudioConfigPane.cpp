// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>

#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/gbsizer.h>
#include <wx/radiobox.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>

#include "AudioCommon/AudioCommon.h"
#include "Common/Common.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "DolphinWX/Config/AudioConfigPane.h"
#include "DolphinWX/DolphinSlider.h"
#include "DolphinWX/WxUtils.h"

AudioConfigPane::AudioConfigPane(wxWindow* parent, wxWindowID id) : wxPanel(parent, id)
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

  m_dsp_engine_radiobox =
      new wxRadioBox(this, wxID_ANY, _("DSP Emulator Engine"), wxDefaultPosition, wxDefaultSize,
                     m_dsp_engine_strings, 0, wxRA_SPECIFY_ROWS);
  m_dpl2_decoder_checkbox = new wxCheckBox(this, wxID_ANY, _("Dolby Pro Logic II decoder"));
  m_volume_slider = new DolphinSlider(this, wxID_ANY, 0, 0, 100, wxDefaultPosition, wxDefaultSize,
                                      wxSL_VERTICAL | wxSL_INVERSE);
  m_volume_text = new wxStaticText(this, wxID_ANY, "");
  m_audio_backend_choice =
      new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_audio_backend_strings);
  m_audio_latency_spinctrl =
      new wxSpinCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 30);
  m_audio_latency_label = new wxStaticText(this, wxID_ANY, _("Latency:"));

  m_dsp_engine_radiobox->Bind(wxEVT_RADIOBOX, &AudioConfigPane::OnDSPEngineRadioBoxChanged, this);
  m_dpl2_decoder_checkbox->Bind(wxEVT_CHECKBOX, &AudioConfigPane::OnDPL2DecoderCheckBoxChanged,
                                this);
  m_volume_slider->Bind(wxEVT_SLIDER, &AudioConfigPane::OnVolumeSliderChanged, this);
  m_audio_backend_choice->Bind(wxEVT_CHOICE, &AudioConfigPane::OnAudioBackendChanged, this);
  m_audio_latency_spinctrl->Bind(wxEVT_SPINCTRL, &AudioConfigPane::OnLatencySpinCtrlChanged, this);

  m_audio_backend_choice->SetToolTip(
      _("Changing this will have no effect while the emulator is running."));
  m_audio_latency_spinctrl->SetToolTip(_("Sets the latency (in ms). Higher values may reduce audio "
                                         "crackling. Certain backends only."));
  m_dpl2_decoder_checkbox->SetToolTip(
      _("Enables Dolby Pro Logic II emulation using 5.1 surround. Certain backends only."));

  const int space5 = FromDIP(5);

  wxStaticBoxSizer* const dsp_engine_sizer =
      new wxStaticBoxSizer(wxVERTICAL, this, _("Sound Settings"));
  dsp_engine_sizer->Add(m_dsp_engine_radiobox, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  dsp_engine_sizer->AddSpacer(space5);
  dsp_engine_sizer->AddStretchSpacer();

  wxStaticBoxSizer* const volume_sizer = new wxStaticBoxSizer(wxVERTICAL, this, _("Volume"));
  volume_sizer->Add(m_volume_slider, 1, wxALIGN_CENTER_HORIZONTAL);
  volume_sizer->Add(m_volume_text, 0, wxALIGN_CENTER_HORIZONTAL | wxLEFT | wxRIGHT, space5);
  volume_sizer->AddSpacer(space5);

  wxGridBagSizer* const backend_grid_sizer = new wxGridBagSizer(space5, space5);
  backend_grid_sizer->Add(new wxStaticText(this, wxID_ANY, _("Audio Backend:")), wxGBPosition(0, 0),
                          wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
  backend_grid_sizer->Add(m_audio_backend_choice, wxGBPosition(0, 1), wxDefaultSpan,
                          wxALIGN_CENTER_VERTICAL);
  backend_grid_sizer->Add(m_dpl2_decoder_checkbox, wxGBPosition(1, 0), wxGBSpan(1, 2),
                          wxALIGN_CENTER_VERTICAL);
  backend_grid_sizer->Add(m_audio_latency_label, wxGBPosition(2, 0), wxDefaultSpan,
                          wxALIGN_CENTER_VERTICAL);
  backend_grid_sizer->Add(m_audio_latency_spinctrl, wxGBPosition(2, 1), wxDefaultSpan,
                          wxALIGN_CENTER_VERTICAL);

  wxStaticBoxSizer* const backend_static_box_sizer =
      new wxStaticBoxSizer(wxVERTICAL, this, _("Backend Settings"));
  backend_static_box_sizer->AddSpacer(space5);
  backend_static_box_sizer->Add(backend_grid_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  backend_static_box_sizer->AddSpacer(space5);

  wxBoxSizer* const dsp_audio_sizer = new wxBoxSizer(wxHORIZONTAL);
  dsp_audio_sizer->AddSpacer(space5);
  dsp_audio_sizer->Add(dsp_engine_sizer, 1, wxEXPAND | wxTOP | wxBOTTOM, space5);
  dsp_audio_sizer->AddSpacer(space5);
  dsp_audio_sizer->Add(volume_sizer, 0, wxEXPAND | wxTOP | wxBOTTOM, space5);
  dsp_audio_sizer->AddSpacer(space5);

  wxBoxSizer* const main_sizer = new wxBoxSizer(wxVERTICAL);
  main_sizer->AddSpacer(space5);
  main_sizer->Add(dsp_audio_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  main_sizer->AddSpacer(space5);
  main_sizer->Add(backend_static_box_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  main_sizer->AddSpacer(space5);

  SetSizerAndFit(main_sizer);
}

void AudioConfigPane::LoadGUIValues()
{
  const SConfig& startup_params = SConfig::GetInstance();
  PopulateBackendChoiceBox();
  ToggleBackendSpecificControls(SConfig::GetInstance().sBackend);

  // Audio DSP Engine
  if (startup_params.bDSPHLE)
    m_dsp_engine_radiobox->SetSelection(0);
  else
    m_dsp_engine_radiobox->SetSelection(SConfig::GetInstance().m_DSPEnableJIT ? 1 : 2);

  m_volume_slider->SetValue(SConfig::GetInstance().m_Volume);
  m_volume_text->SetLabel(wxString::Format("%d %%", SConfig::GetInstance().m_Volume));
  m_dpl2_decoder_checkbox->SetValue(startup_params.bDPL2Decoder);
  m_audio_latency_spinctrl->SetValue(startup_params.iLatency);
}

void AudioConfigPane::ToggleBackendSpecificControls(const std::string& backend)
{
  m_dpl2_decoder_checkbox->Enable(AudioCommon::SupportsDPL2Decoder(backend));

  bool supports_latency_control = AudioCommon::SupportsLatencyControl(backend);
  m_audio_latency_spinctrl->Enable(supports_latency_control);
  m_audio_latency_label->Enable(supports_latency_control);

  bool supports_volume_changes = AudioCommon::SupportsVolumeChanges(backend);
  m_volume_slider->Enable(supports_volume_changes);
  m_volume_text->Enable(supports_volume_changes);
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
  SConfig::GetInstance().bDSPHLE = m_dsp_engine_radiobox->GetSelection() == 0;
  SConfig::GetInstance().m_DSPEnableJIT = m_dsp_engine_radiobox->GetSelection() == 1;
  AudioCommon::UpdateSoundStream();
}

void AudioConfigPane::OnDPL2DecoderCheckBoxChanged(wxCommandEvent&)
{
  SConfig::GetInstance().bDPL2Decoder = m_dpl2_decoder_checkbox->IsChecked();
}

void AudioConfigPane::OnVolumeSliderChanged(wxCommandEvent& event)
{
  SConfig::GetInstance().m_Volume = m_volume_slider->GetValue();
  AudioCommon::UpdateSoundStream();
  m_volume_text->SetLabel(wxString::Format("%d %%", m_volume_slider->GetValue()));
}

void AudioConfigPane::OnAudioBackendChanged(wxCommandEvent& event)
{
  // Don't save the translated BACKEND_NULLSOUND string
  SConfig::GetInstance().sBackend = m_audio_backend_choice->GetSelection() ?
                                        WxStrToStr(m_audio_backend_choice->GetStringSelection()) :
                                        BACKEND_NULLSOUND;
  ToggleBackendSpecificControls(WxStrToStr(m_audio_backend_choice->GetStringSelection()));
  AudioCommon::UpdateSoundStream();
}

void AudioConfigPane::OnLatencySpinCtrlChanged(wxCommandEvent& event)
{
  SConfig::GetInstance().iLatency = m_audio_latency_spinctrl->GetValue();
}

void AudioConfigPane::PopulateBackendChoiceBox()
{
  for (const std::string& backend : AudioCommon::GetSoundBackends())
  {
    m_audio_backend_choice->Append(wxGetTranslation(StrToWxStr(backend)));
  }

  int num = m_audio_backend_choice->FindString(StrToWxStr(SConfig::GetInstance().sBackend));
  m_audio_backend_choice->SetSelection(num);
}
