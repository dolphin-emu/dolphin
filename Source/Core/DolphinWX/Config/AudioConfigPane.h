// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <wx/arrstr.h>
#include <wx/panel.h>

class DolphinSlider;
class wxCheckBox;
class wxChoice;
class wxRadioBox;
class wxSpinCtrl;
class wxStaticText;

class AudioConfigPane final : public wxPanel
{
public:
  AudioConfigPane(wxWindow* parent, wxWindowID id);

private:
  void InitializeGUI();
  void LoadGUIValues();
  void BindEvents();

  void OnDSPEngineRadioBoxChanged(wxCommandEvent&);
  void OnDPL2DecoderCheckBoxChanged(wxCommandEvent&);
  void OnVolumeSliderChanged(wxCommandEvent&);
  void OnStretchCheckBoxChanged(wxCommandEvent&);
  void OnStretchSliderChanged(wxCommandEvent&);

  wxArrayString m_dsp_engine_strings;

  wxRadioBox* m_dsp_engine_radiobox;
  wxCheckBox* m_dpl2_decoder_checkbox;
  DolphinSlider* m_volume_slider;
  wxStaticText* m_volume_text;
  wxCheckBox* m_stretch_checkbox;
  wxStaticText* m_stretch_label;
  DolphinSlider* m_stretch_slider;
  wxStaticText* m_stretch_text;
};
