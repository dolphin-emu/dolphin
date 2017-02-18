// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/Config/GeneralConfigPane.h"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/event.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/radiobox.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include "Core/Analytics.h"
#include "Core/Core.h"
#include "Core/PowerPC/PowerPC.h"
#include "DolphinWX/WxEventUtils.h"

GeneralConfigPane::GeneralConfigPane(wxWindow* parent, wxWindowID id) : wxPanel(parent, id)
{
  m_cpu_cores = {
      {PowerPC::CORE_INTERPRETER, _("Interpreter (slowest)")},
      {PowerPC::CORE_CACHEDINTERPRETER, _("Cached Interpreter (slower)")},
#ifdef _M_X86_64
      {PowerPC::CORE_JIT64, _("JIT Recompiler (recommended)")},
      {PowerPC::CORE_JITIL64, _("JITIL Recompiler (slow, experimental)")},
#elif defined(_M_ARM_64)
      {PowerPC::CORE_JITARM64, _("JIT Arm64 (experimental)")},
#endif
  };

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

  for (const CPUCore& cpu_core : m_cpu_cores)
    m_cpu_engine_array_string.Add(cpu_core.name);

  m_dual_core_checkbox = new wxCheckBox(this, wxID_ANY, _("Enable Dual Core (speedup)"));
  m_cheats_checkbox = new wxCheckBox(this, wxID_ANY, _("Enable Cheats"));
  m_force_ntscj_checkbox = new wxCheckBox(this, wxID_ANY, _("Force Console as NTSC-J"));
  m_analytics_checkbox = new wxCheckBox(this, wxID_ANY, _("Enable Usage Statistics Reporting"));
#ifdef __APPLE__
  m_analytics_new_id = new wxButton(this, wxID_ANY, _("Generate a New Statistics Identity"),
                                    wxDefaultPosition, wxSize(350, 25));
#else
  m_analytics_new_id = new wxButton(this, wxID_ANY, _("Generate a New Statistics Identity"));
#endif
  m_throttler_choice =
      new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_throttler_array_string);
  m_cpu_engine_radiobox =
      new wxRadioBox(this, wxID_ANY, _("CPU Emulator Engine"), wxDefaultPosition, wxDefaultSize,
                     m_cpu_engine_array_string, 0, wxRA_SPECIFY_ROWS);

  m_dual_core_checkbox->SetToolTip(
      _("Splits the CPU and GPU threads so they can be run on separate cores.\nProvides major "
        "speed improvements on most modern PCs, but can cause occasional crashes/glitches."));
  m_cheats_checkbox->SetToolTip(_("Enables the use of Action Replay and Gecko cheats."));
  m_force_ntscj_checkbox->SetToolTip(
      _("Forces NTSC-J mode for using the Japanese ROM font.\nIf left unchecked, Dolphin defaults "
        "to NTSC-U and automatically enables this setting when playing Japanese games."));
  m_analytics_checkbox->SetToolTip(
      _("Enables the collection and sharing of usage statistics data with the Dolphin development "
        "team. This data is used to improve the emulator and help us understand how our users "
        "interact with the system. No private data is ever collected."));
  m_analytics_new_id->SetToolTip(
      _("Usage statistics reporting uses a unique random per-machine identifier to distinguish "
        "users from one another. This button generates a new random identifier for this machine "
        "which is dissociated from the previous one."));
  m_throttler_choice->SetToolTip(_("Limits the emulation speed to the specified percentage.\nNote "
                                   "that raising or lowering the emulation speed will also raise "
                                   "or lower the audio pitch to prevent audio from stuttering."));

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

  wxStaticBoxSizer* const analytics_sizer =
      new wxStaticBoxSizer(wxVERTICAL, this, _("Usage Statistics Reporting Settings"));
  analytics_sizer->AddSpacer(space5);
  analytics_sizer->Add(m_analytics_checkbox, 0, wxLEFT | wxRIGHT, space5);
  analytics_sizer->AddSpacer(space5);
  analytics_sizer->Add(m_analytics_new_id, 0, wxLEFT | wxRIGHT, space5);
  analytics_sizer->AddSpacer(space5);

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
  main_sizer->Add(analytics_sizer, 0, wxEXPAND | wxLEFT | wxLEFT, space5);
  main_sizer->AddSpacer(space5);
  main_sizer->Add(advanced_settings_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  main_sizer->AddSpacer(space5);

  SetSizer(main_sizer);

  m_settings = {
      {m_dual_core_checkbox, {Config::System::Main, "Core", "CPUThread"}},
      {m_cheats_checkbox, {Config::System::Main, "Core", "EnableCheats"}},

      {m_force_ntscj_checkbox, {Config::System::Main, "Display", "ForceNTSCJ"}},

      {m_analytics_checkbox, {Config::System::Main, "Analytics", "Enabled"}},

      {m_throttler_choice, {Config::System::Main, "Core", "EmulationSpeed"}},
      {m_cpu_engine_radiobox, {Config::System::Main, "Core", "CPUCore"}},
  };
  Config::AddConfigChangedCallback([this]() { LoadGUIValues(); });
}

void GeneralConfigPane::LoadGUIValues()
{
  ConfigUtils::LoadValue(m_settings, m_dual_core_checkbox);
  ConfigUtils::LoadValue(m_settings, m_cheats_checkbox);
  ConfigUtils::LoadValue(m_settings, m_force_ntscj_checkbox);
  ConfigUtils::LoadValue(m_settings, m_analytics_checkbox);

  const float speed = ConfigUtils::Get<float>(m_settings, m_throttler_choice);
  u32 selection = std::lround(speed * 10.0f);
  if (selection < m_throttler_array_string.size())
    m_throttler_choice->SetSelection(selection);

  const int cpu_core = ConfigUtils::Get<int>(m_settings, m_cpu_engine_radiobox);
  for (size_t i = 0; i < m_cpu_cores.size(); ++i)
  {
    if (m_cpu_cores[i].CPUid == cpu_core)
      m_cpu_engine_radiobox->SetSelection(i);
  }
}

void GeneralConfigPane::BindEvents()
{
  ConfigUtils::SaveOnChange(m_settings, m_dual_core_checkbox);
  m_dual_core_checkbox->Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreNotRunning);

  ConfigUtils::SaveOnChange(m_settings, m_cheats_checkbox);
  m_cheats_checkbox->Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreNotRunning);

  ConfigUtils::SaveOnChange(m_settings, m_force_ntscj_checkbox);
  m_force_ntscj_checkbox->Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreNotRunning);

  ConfigUtils::SaveOnChange(m_settings, m_analytics_checkbox);

  m_analytics_new_id->Bind(wxEVT_BUTTON, &GeneralConfigPane::OnAnalyticsNewIdButtonClick, this);

  m_throttler_choice->Bind(wxEVT_CHOICE, &GeneralConfigPane::OnThrottlerChoiceChanged, this);

  m_cpu_engine_radiobox->Bind(wxEVT_RADIOBOX, &GeneralConfigPane::OnCPUEngineRadioBoxChanged, this);
  m_cpu_engine_radiobox->Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreNotRunning);
}

void GeneralConfigPane::OnThrottlerChoiceChanged(wxCommandEvent& event)
{
  if (m_throttler_choice->GetSelection() != wxNOT_FOUND)
    ConfigUtils::Set(m_settings, m_throttler_choice, m_throttler_choice->GetSelection() * 0.1f);
}

void GeneralConfigPane::OnCPUEngineRadioBoxChanged(wxCommandEvent& event)
{
  ConfigUtils::Set(m_settings, m_cpu_engine_radiobox, m_cpu_cores.at(event.GetSelection()).CPUid);
}

void GeneralConfigPane::OnAnalyticsNewIdButtonClick(wxCommandEvent& event)
{
  DolphinAnalytics::Instance()->GenerateNewIdentity();
  wxMessageBox(_("New identity generated."), _("Identity generation"), wxICON_INFORMATION);
}
