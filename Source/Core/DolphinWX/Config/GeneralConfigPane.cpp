// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/Config/GeneralConfigPane.h"

#include <map>
#include <string>
#include <vector>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/event.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/radiobox.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include "Common/Common.h"
#include "Core/Analytics.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/PowerPC/PowerPC.h"
#include "DolphinWX/WxEventUtils.h"

static const std::map<PowerPC::CPUCore, std::string> CPU_CORE_NAMES = {
    {PowerPC::CORE_INTERPRETER, _trans("Interpreter (slowest)")},
    {PowerPC::CORE_CACHEDINTERPRETER, _trans("Cached Interpreter (slower)")},
    {PowerPC::CORE_JIT64, _trans("JIT Recompiler (recommended)")},
    {PowerPC::CORE_JITARM64, _trans("JIT Arm64 (experimental)")},
};

GeneralConfigPane::GeneralConfigPane(wxWindow* parent, wxWindowID id) : wxPanel(parent, id)
{
  InitializeGUI();
  LoadGUIValues();
  BindEvents();
}

void GeneralConfigPane::InitializeGUI()
{
  m_throttler_array_string.Add(_("Unlimited"));
  for (int i = 10; i <= 200; i += 10)  // from 10% to 200%
  {
    if (i == 100)
      m_throttler_array_string.Add(wxString::Format(_("%i%% (Normal Speed)"), i));
    else
      m_throttler_array_string.Add(wxString::Format(_("%i%%"), i));
  }

  for (PowerPC::CPUCore cpu_core : PowerPC::AvailableCPUCores())
    m_cpu_engine_array_string.Add(wxGetTranslation(CPU_CORE_NAMES.at(cpu_core)));

  m_dual_core_checkbox = new wxCheckBox(this, wxID_ANY, _("Enable Dual Core (speedup)"));
  m_cheats_checkbox = new wxCheckBox(this, wxID_ANY, _("Enable Cheats"));
  m_force_ntscj_checkbox = new wxCheckBox(this, wxID_ANY, _("Force Console as NTSC-J"));
#if defined(USE_ANALYTICS) && USE_ANALYTICS
  m_analytics_checkbox = new wxCheckBox(this, wxID_ANY, _("Enable Usage Statistics Reporting"));
#ifdef __APPLE__
  m_analytics_new_id = new wxButton(this, wxID_ANY, _("Generate a New Statistics Identity"),
                                    wxDefaultPosition, wxSize(350, 25));
#else
  m_analytics_new_id = new wxButton(this, wxID_ANY, _("Generate a New Statistics Identity"));
#endif
#endif
  m_throttler_choice =
      new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_throttler_array_string);
  m_cpu_engine_radiobox =
      new wxRadioBox(this, wxID_ANY, _("CPU Emulation Engine"), wxDefaultPosition, wxDefaultSize,
                     m_cpu_engine_array_string, 0, wxRA_SPECIFY_ROWS);

  m_dual_core_checkbox->SetToolTip(
      _("Splits the CPU and GPU threads so they can be run on separate cores.\nProvides major "
        "speed improvements on most modern PCs, but can cause occasional crashes/glitches."));
  m_cheats_checkbox->SetToolTip(_("Enables the use of Action Replay and Gecko cheats."));
  m_force_ntscj_checkbox->SetToolTip(
      _("Forces NTSC-J mode for using the Japanese ROM font.\nIf left unchecked, Dolphin defaults "
        "to NTSC-U and automatically enables this setting when playing Japanese games."));
#if defined(USE_ANALYTICS) && USE_ANALYTICS
  m_analytics_checkbox->SetToolTip(
      _("Enables the collection and sharing of usage statistics data with the Dolphin development "
        "team. This data is used to improve the emulator and help us understand how our users "
        "interact with the system. No private data is ever collected."));
  m_analytics_new_id->SetToolTip(
      _("Usage statistics reporting uses a unique random per-machine identifier to distinguish "
        "users from one another. This button generates a new random identifier for this machine "
        "which is dissociated from the previous one."));
#endif

  m_throttler_choice->SetToolTip(_("Limits the emulation speed to the specified percentage.\nNote "
                                   "that raising or lowering the emulation speed will also raise "
                                   "or lower the audio pitch unless audio stretching is enabled."));

  const int space5 = FromDIP(5);

  wxBoxSizer* const throttler_sizer = new wxBoxSizer(wxHORIZONTAL);
  throttler_sizer->AddSpacer(space5);
  throttler_sizer->Add(new wxStaticText(this, wxID_ANY, _("Speed Limit:")), 0,
                       wxALIGN_CENTER_VERTICAL | wxBOTTOM, space5);
  throttler_sizer->AddSpacer(space5);
  throttler_sizer->Add(m_throttler_choice, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, space5);
  throttler_sizer->AddSpacer(space5);

  wxStaticBoxSizer* const basic_settings_sizer =
      new wxStaticBoxSizer(wxVERTICAL, this, _("Basic Settings"));
  basic_settings_sizer->AddSpacer(space5);
  basic_settings_sizer->Add(m_dual_core_checkbox, 0, wxLEFT | wxRIGHT, space5);
  basic_settings_sizer->AddSpacer(space5);
  basic_settings_sizer->Add(m_cheats_checkbox, 0, wxLEFT | wxRIGHT, space5);
  basic_settings_sizer->AddSpacer(space5);
  basic_settings_sizer->Add(throttler_sizer);

#if defined(USE_ANALYTICS) && USE_ANALYTICS
  wxStaticBoxSizer* const analytics_sizer =
      new wxStaticBoxSizer(wxVERTICAL, this, _("Usage Statistics Reporting Settings"));
  analytics_sizer->AddSpacer(space5);
  analytics_sizer->Add(m_analytics_checkbox, 0, wxLEFT | wxRIGHT, space5);
  analytics_sizer->AddSpacer(space5);
  analytics_sizer->Add(m_analytics_new_id, 0, wxLEFT | wxRIGHT, space5);
  analytics_sizer->AddSpacer(space5);
#endif

  wxStaticBoxSizer* const advanced_settings_sizer =
      new wxStaticBoxSizer(wxVERTICAL, this, _("Advanced Settings"));
  advanced_settings_sizer->AddSpacer(space5);
  advanced_settings_sizer->Add(m_cpu_engine_radiobox, 0, wxLEFT | wxRIGHT, space5);
  advanced_settings_sizer->AddSpacer(space5);
  advanced_settings_sizer->Add(m_force_ntscj_checkbox, 0, wxLEFT | wxRIGHT, space5);
  advanced_settings_sizer->AddSpacer(space5);

  wxBoxSizer* const main_sizer = new wxBoxSizer(wxVERTICAL);
  main_sizer->AddSpacer(space5);
  main_sizer->Add(basic_settings_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  main_sizer->AddSpacer(space5);
#if defined(USE_ANALYTICS) && USE_ANALYTICS
  main_sizer->Add(analytics_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
#endif
  main_sizer->AddSpacer(space5);
  main_sizer->Add(advanced_settings_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  main_sizer->AddSpacer(space5);

  SetSizer(main_sizer);
}

void GeneralConfigPane::LoadGUIValues()
{
  const SConfig& startup_params = SConfig::GetInstance();

  m_dual_core_checkbox->SetValue(startup_params.bCPUThread);
  m_cheats_checkbox->SetValue(startup_params.bEnableCheats);
  m_force_ntscj_checkbox->SetValue(startup_params.bForceNTSCJ);

#if defined(USE_ANALYTICS) && USE_ANALYTICS
  m_analytics_checkbox->SetValue(startup_params.m_analytics_enabled);
#endif

  u32 selection = std::lround(startup_params.m_EmulationSpeed * 10.0f);
  if (selection < m_throttler_array_string.size())
    m_throttler_choice->SetSelection(selection);

  const std::vector<PowerPC::CPUCore>& cpu_cores = PowerPC::AvailableCPUCores();
  for (size_t i = 0; i < cpu_cores.size(); ++i)
  {
    if (cpu_cores[i] == startup_params.iCPUCore)
      m_cpu_engine_radiobox->SetSelection(i);
  }
}

void GeneralConfigPane::BindEvents()
{
  m_dual_core_checkbox->Bind(wxEVT_CHECKBOX, &GeneralConfigPane::OnDualCoreCheckBoxChanged, this);
  m_dual_core_checkbox->Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreNotRunning);

  m_cheats_checkbox->Bind(wxEVT_CHECKBOX, &GeneralConfigPane::OnCheatCheckBoxChanged, this);
  m_cheats_checkbox->Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreNotRunning);

  m_force_ntscj_checkbox->Bind(wxEVT_CHECKBOX, &GeneralConfigPane::OnForceNTSCJCheckBoxChanged,
                               this);
  m_force_ntscj_checkbox->Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreNotRunning);

#if defined(USE_ANALYTICS) && USE_ANALYTICS
  m_analytics_checkbox->Bind(wxEVT_CHECKBOX, &GeneralConfigPane::OnAnalyticsCheckBoxChanged, this);

  m_analytics_new_id->Bind(wxEVT_BUTTON, &GeneralConfigPane::OnAnalyticsNewIdButtonClick, this);
#endif

  m_throttler_choice->Bind(wxEVT_CHOICE, &GeneralConfigPane::OnThrottlerChoiceChanged, this);

  m_cpu_engine_radiobox->Bind(wxEVT_RADIOBOX, &GeneralConfigPane::OnCPUEngineRadioBoxChanged, this);
  m_cpu_engine_radiobox->Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreNotRunning);
}

void GeneralConfigPane::OnDualCoreCheckBoxChanged(wxCommandEvent& event)
{
  if (Core::IsRunning())
    return;

  SConfig::GetInstance().bCPUThread = m_dual_core_checkbox->IsChecked();
}

void GeneralConfigPane::OnCheatCheckBoxChanged(wxCommandEvent& event)
{
  SConfig::GetInstance().bEnableCheats = m_cheats_checkbox->IsChecked();
}

void GeneralConfigPane::OnForceNTSCJCheckBoxChanged(wxCommandEvent& event)
{
  SConfig::GetInstance().bForceNTSCJ = m_force_ntscj_checkbox->IsChecked();
}

void GeneralConfigPane::OnThrottlerChoiceChanged(wxCommandEvent& event)
{
  if (m_throttler_choice->GetSelection() != wxNOT_FOUND)
    SConfig::GetInstance().m_EmulationSpeed = m_throttler_choice->GetSelection() * 0.1f;
}

void GeneralConfigPane::OnCPUEngineRadioBoxChanged(wxCommandEvent& event)
{
  SConfig::GetInstance().iCPUCore = PowerPC::AvailableCPUCores()[event.GetSelection()];
}

void GeneralConfigPane::OnAnalyticsCheckBoxChanged(wxCommandEvent& event)
{
  SConfig::GetInstance().m_analytics_enabled = m_analytics_checkbox->IsChecked();
  DolphinAnalytics::Instance()->ReloadConfig();
}

void GeneralConfigPane::OnAnalyticsNewIdButtonClick(wxCommandEvent& event)
{
  DolphinAnalytics::Instance()->GenerateNewIdentity();
  wxMessageBox(_("New identity generated."), _("Identity Generation"), wxICON_INFORMATION);
}
