// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include <wx/arrstr.h>
#include <wx/panel.h>

class wxButton;
class wxCheckBox;
class wxChoice;
class wxRadioBox;

class GeneralConfigPane final : public wxPanel
{
public:
  GeneralConfigPane(wxWindow* parent, wxWindowID id);

private:
  void InitializeGUI();
  void LoadGUIValues();
  void BindEvents();

  void OnDualCoreCheckBoxChanged(wxCommandEvent&);
  void OnGPUDeterminsmChanged(wxCommandEvent&);
  void OnIdleSkipCheckBoxChanged(wxCommandEvent&);
  void OnCheatCheckBoxChanged(wxCommandEvent&);
  void OnThrottlerChoiceChanged(wxCommandEvent&);
  void OnCPUEngineRadioBoxChanged(wxCommandEvent&);
  void OnAnalyticsCheckBoxChanged(wxCommandEvent&);
  void OnAnalyticsNewIdButtonClick(wxCommandEvent&);

  wxArrayString m_throttler_array_string;
  wxArrayString m_gpu_determinism_string;
  wxArrayString m_cpu_engine_array_string;

  wxCheckBox* m_dual_core_checkbox;
  wxCheckBox* m_idle_skip_checkbox;
  wxCheckBox* m_cheats_checkbox;

  wxCheckBox* m_analytics_checkbox;
  wxButton* m_analytics_new_id;

  wxChoice* m_throttler_choice;
  wxChoice* m_gpu_determinism;

  wxRadioBox* m_cpu_engine_radiobox;
};
