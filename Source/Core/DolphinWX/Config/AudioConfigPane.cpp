// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/Config/AudioConfigPane.h"

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
#include "DolphinWX/DolphinSlider.h"
#include "DolphinWX/WxEventUtils.h"
#include "DolphinWX/WxUtils.h"

AudioConfigPane::AudioConfigPane(wxWindow* parent, wxWindowID id) : wxPanel(parent, id)
{
  CheckNeedForLatencyControl();
  InitializeGUI();
  LoadGUIValues();
  BindEvents();
}

void AudioConfigPane::InitializeGUI()
{
  m_dsp_engine_strings.Add(_("DSP HLE Emulation (fast)"));
  m_dsp_engine_strings.Add(_("DSP LLE Recompiler"));
  m_dsp_engine_strings.Add(_("DSP LLE Interpreter (slow)"));

  m_dsp_engine_radiobox =
      new wxRadioBox(this, wxID_ANY, _("DSP Emulation Engine"), wxDefaultPosition, wxDefaultSize,
                     m_dsp_engine_strings, 0, wxRA_SPECIFY_ROWS);
  m_dpl2_decoder_checkbox = new wxCheckBox(this, wxID_ANY, _("Dolby Pro Logic II Decoder"));
  m_volume_slider = new DolphinSlider(this, wxID_ANY, 0, 0, 100, wxDefaultPosition, wxDefaultSize,
                                      wxSL_VERTICAL | wxSL_INVERSE);
  m_volume_text = new wxStaticText(this, wxID_ANY, "");
  m_audio_backend_choice =
      new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_audio_backend_strings);
  if (m_latency_control_supported)
  {
    m_audio_latency_spinctrl = new wxSpinCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                              wxSP_ARROW_KEYS, 0, 200);
    m_audio_latency_label = new wxStaticText(this, wxID_ANY, _("Latency (ms):"));
  }

  m_stretch_checkbox = new wxCheckBox(this, wxID_ANY, _("Enable Audio Stretching"));
  m_stretch_label = new wxStaticText(this, wxID_ANY, _("Buffer Size:"));
  m_stretch_slider =
      new DolphinSlider(this, wxID_ANY, 80, 5, 300, wxDefaultPosition, wxDefaultSize);
  m_stretch_text = new wxStaticText(this, wxID_ANY, "");

  if (m_latency_control_supported)
  {
    m_audio_latency_spinctrl->SetToolTip(
        _("Sets the latency (in ms). Higher values may reduce audio "
          "crackling. Certain backends only."));
  }
  m_dpl2_decoder_checkbox->SetToolTip(
      _("Enables Dolby Pro Logic II emulation using 5.1 surround. Certain backends only."));
  m_stretch_checkbox->SetToolTip(_("Enables stretching of the audio to match emulation speed."));
  m_stretch_slider->SetToolTip(_("Size of stretch buffer in milliseconds. "
                                 "Values too low may cause audio crackling."));

  const int space5 = FromDIP(5);

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
  if (m_latency_control_supported)
  {
    backend_grid_sizer->Add(m_audio_latency_label, wxGBPosition(2, 0), wxDefaultSpan,
                            wxALIGN_CENTER_VERTICAL);
    backend_grid_sizer->Add(m_audio_latency_spinctrl, wxGBPosition(2, 1), wxDefaultSpan,
                            wxALIGN_CENTER_VERTICAL);
  }

  wxStaticBoxSizer* const backend_static_box_sizer =
      new wxStaticBoxSizer(wxVERTICAL, this, _("Backend Settings"));
  backend_static_box_sizer->AddSpacer(space5);
  backend_static_box_sizer->Add(backend_grid_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  backend_static_box_sizer->AddSpacer(space5);

  wxBoxSizer* const dsp_audio_sizer = new wxBoxSizer(wxHORIZONTAL);
  dsp_audio_sizer->AddSpacer(space5);
  dsp_audio_sizer->Add(m_dsp_engine_radiobox, 1, wxEXPAND | wxTOP | wxBOTTOM, space5);
  dsp_audio_sizer->AddSpacer(space5);
  dsp_audio_sizer->Add(volume_sizer, 0, wxEXPAND | wxTOP | wxBOTTOM, space5);
  dsp_audio_sizer->AddSpacer(space5);

  wxGridBagSizer* const latency_sizer = new wxGridBagSizer();
  latency_sizer->Add(m_stretch_slider, wxGBPosition(0, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
  latency_sizer->Add(m_stretch_text, wxGBPosition(0, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);

  wxGridBagSizer* const stretching_grid_sizer = new wxGridBagSizer(space5, space5);
  stretching_grid_sizer->Add(m_stretch_checkbox, wxGBPosition(0, 0), wxGBSpan(1, 2),
                             wxALIGN_CENTER_VERTICAL);
  stretching_grid_sizer->Add(m_stretch_label, wxGBPosition(1, 0), wxDefaultSpan,
                             wxALIGN_CENTER_VERTICAL);
  stretching_grid_sizer->Add(latency_sizer, wxGBPosition(1, 1), wxDefaultSpan,
                             wxALIGN_CENTER_VERTICAL);

  wxStaticBoxSizer* const stretching_box_sizer =
      new wxStaticBoxSizer(wxVERTICAL, this, _("Audio Stretching Settings"));
  stretching_box_sizer->AddSpacer(space5);
  stretching_box_sizer->Add(stretching_grid_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  stretching_box_sizer->AddSpacer(space5);

  wxBoxSizer* const main_sizer = new wxBoxSizer(wxVERTICAL);
  main_sizer->AddSpacer(space5);
  main_sizer->Add(dsp_audio_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  main_sizer->AddSpacer(space5);
  main_sizer->Add(backend_static_box_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  main_sizer->AddSpacer(space5);
  main_sizer->Add(stretching_box_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
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
  if (m_latency_control_supported)
  {
    m_audio_latency_spinctrl->SetValue(startup_params.iLatency);
  }
  m_stretch_checkbox->SetValue(startup_params.m_audio_stretch);
  m_stretch_slider->Enable(startup_params.m_audio_stretch);
  m_stretch_slider->SetValue(startup_params.m_audio_stretch_max_latency);
  m_stretch_text->Enable(startup_params.m_audio_stretch);
  m_stretch_text->SetLabel(wxString::Format("%d ms", startup_params.m_audio_stretch_max_latency));
  m_stretch_label->Enable(startup_params.m_audio_stretch);
}

void AudioConfigPane::ToggleBackendSpecificControls(const std::string& backend)
{
  m_dpl2_decoder_checkbox->Enable(AudioCommon::SupportsDPL2Decoder(backend));

  if (m_latency_control_supported)
  {
    bool supports_latency_control = AudioCommon::SupportsLatencyControl(backend);
    m_audio_latency_spinctrl->Enable(supports_latency_control);
    m_audio_latency_label->Enable(supports_latency_control);
  }

  bool supports_volume_changes = AudioCommon::SupportsVolumeChanges(backend);
  m_volume_slider->Enable(supports_volume_changes);
  m_volume_text->Enable(supports_volume_changes);
}

void AudioConfigPane::BindEvents()
{
  m_dsp_engine_radiobox->Bind(wxEVT_RADIOBOX, &AudioConfigPane::OnDSPEngineRadioBoxChanged, this);
  m_dsp_engine_radiobox->Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreNotRunning);

  m_dpl2_decoder_checkbox->Bind(wxEVT_CHECKBOX, &AudioConfigPane::OnDPL2DecoderCheckBoxChanged,
                                this);
  m_dpl2_decoder_checkbox->Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreNotRunning);

  m_volume_slider->Bind(wxEVT_SLIDER, &AudioConfigPane::OnVolumeSliderChanged, this);

  m_audio_backend_choice->Bind(wxEVT_CHOICE, &AudioConfigPane::OnAudioBackendChanged, this);
  m_audio_backend_choice->Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreNotRunning);

  if (m_latency_control_supported)
  {
    m_audio_latency_spinctrl->Bind(wxEVT_SPINCTRL, &AudioConfigPane::OnLatencySpinCtrlChanged,
                                   this);
    m_audio_latency_spinctrl->Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreNotRunning);
  }

  m_stretch_checkbox->Bind(wxEVT_CHECKBOX, &AudioConfigPane::OnStretchCheckBoxChanged, this);
  m_stretch_slider->Bind(wxEVT_SLIDER, &AudioConfigPane::OnStretchSliderChanged, this);
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

void AudioConfigPane::OnStretchCheckBoxChanged(wxCommandEvent& event)
{
  const bool stretch_enabled = m_stretch_checkbox->GetValue();
  SConfig::GetInstance().m_audio_stretch = stretch_enabled;
  m_stretch_slider->Enable(stretch_enabled);
  m_stretch_text->Enable(stretch_enabled);
  m_stretch_label->Enable(stretch_enabled);
}

void AudioConfigPane::OnStretchSliderChanged(wxCommandEvent& event)
{
  SConfig::GetInstance().m_audio_stretch_max_latency = m_stretch_slider->GetValue();
  m_stretch_text->SetLabel(wxString::Format("%d ms", m_stretch_slider->GetValue()));
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

void AudioConfigPane::CheckNeedForLatencyControl()
{
  std::vector<std::string> backends = AudioCommon::GetSoundBackends();
  m_latency_control_supported =
      std::any_of(backends.cbegin(), backends.cend(), AudioCommon::SupportsLatencyControl);
}
