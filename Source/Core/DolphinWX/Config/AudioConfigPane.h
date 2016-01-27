// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <wx/arrstr.h>
#include <wx/panel.h>

class wxCheckBox;
class wxChoice;
class wxRadioBox;
class wxSlider;
class wxSpinCtrl;
class wxStaticText;

class AudioConfigPane final : public wxPanel
{
public:
	AudioConfigPane(wxWindow* parent, wxWindowID id);

private:
	void InitializeGUI();
	void LoadGUIValues();
	void RefreshGUI();

	void PopulateBackendChoiceBox();
	void PopulateFilterBoxes();
	static bool SupportsVolumeChanges(const std::string&);

	void OnDSPEngineRadioBoxChanged(wxCommandEvent&);
	void OnDPL2DecoderCheckBoxChanged(wxCommandEvent&);
	void OnVolumeSliderChanged(wxCommandEvent&);
	void OnAudioBackendChanged(wxCommandEvent&);
	void OnLatencySpinCtrlChanged(wxCommandEvent&);
	void OnFilterPresetChange(wxCommandEvent&);
	void OnFilterTapsChange(wxCommandEvent&);
	void OnFilterResolutionChange(wxCommandEvent&);
	void OnClearCachePress(wxCommandEvent&);

	wxArrayString m_dsp_engine_strings;
	wxArrayString m_audio_backend_strings;
	wxArrayString m_filter_preset_strings;

	wxRadioBox* m_dsp_engine_radiobox;
	wxCheckBox* m_dpl2_decoder_checkbox;
	wxSlider* m_volume_slider;
	wxStaticText* m_volume_text;
	wxChoice* m_audio_backend_choice;
	wxSpinCtrl* m_audio_latency_spinctrl;
	wxChoice* m_filter_preset_choice;
	wxSpinCtrl* m_filter_taps_spinctrl;
	wxSpinCtrl* m_filter_resolution_spinctrl;
	wxButton* m_clear_cache_button;
};
