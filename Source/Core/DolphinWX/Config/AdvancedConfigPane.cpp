// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/Config/AdvancedConfigPane.h"

#include <cmath>

#include <wx/checkbox.h>
#include <wx/datectrl.h>
#include <wx/dateevt.h>
#include <wx/radiobox.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/time.h>
#include <wx/timectrl.h>

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/SystemTimers.h"
#include "Core/PowerPC/PowerPC.h"
#include "DolphinWX/DolphinSlider.h"
#include "DolphinWX/WxEventUtils.h"

AdvancedConfigPane::AdvancedConfigPane(wxWindow* parent, wxWindowID id) : wxPanel(parent, id)
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

void AdvancedConfigPane::InitializeGUI()
{
  for (const CPUCore& cpu_core : m_cpu_cores)
    m_cpu_engine_array_string.Add(cpu_core.name);
  m_cpu_engine_radiobox =
      new wxRadioBox(this, wxID_ANY, _("CPU Emulator Engine"), wxDefaultPosition, wxDefaultSize,
                     m_cpu_engine_array_string, 0, wxRA_SPECIFY_ROWS);

  m_force_ntscj_checkbox = new wxCheckBox(this, wxID_ANY, _("Force Console as NTSC-J"));
  m_force_ntscj_checkbox->SetToolTip(
      _("Forces NTSC-J mode for using the Japanese ROM font.\nIf left unchecked, Dolphin defaults "
        "to NTSC-U and automatically enables this setting when playing Japanese games."));

  m_clock_override_checkbox = new wxCheckBox(this, wxID_ANY, _("Enable CPU Clock Override"));
  m_clock_override_slider =
      new DolphinSlider(this, wxID_ANY, 100, 0, 150, wxDefaultPosition, FromDIP(wxSize(200, -1)));
  m_clock_override_text = new wxStaticText(this, wxID_ANY, "");

  m_custom_rtc_checkbox = new wxCheckBox(this, wxID_ANY, _("Enable Custom RTC"));
  m_custom_rtc_date_picker = new wxDatePickerCtrl(this, wxID_ANY);
  m_custom_rtc_time_picker = new wxTimePickerCtrl(this, wxID_ANY);

  wxStaticText* const clock_override_description =
      new wxStaticText(this, wxID_ANY, _("Higher values can make variable-framerate games "
                                         "run at a higher framerate, at the expense of CPU. "
                                         "Lower values can make variable-framerate games "
                                         "run at a lower framerate, saving CPU.\n\n"
                                         "WARNING: Changing this from the default (100%) "
                                         "can and will break games and cause glitches. "
                                         "Do so at your own risk. Please do not report "
                                         "bugs that occur with a non-default clock. "));

  wxStaticText* const custom_rtc_description = new wxStaticText(
      this, wxID_ANY,
      _("This setting allows you to set a custom real time clock (RTC) separate "
        "from your current system time.\n\nIf you're unsure, leave this disabled."));

#ifdef __APPLE__
  clock_override_description->Wrap(550);
  custom_rtc_description->Wrap(550);
#else
  clock_override_description->Wrap(FromDIP(400));
  custom_rtc_description->Wrap(FromDIP(400));
#endif

  const int space5 = FromDIP(5);

  wxStaticBoxSizer* const advanced_settings_sizer =
      new wxStaticBoxSizer(wxVERTICAL, this, _("Advanced Settings"));
  advanced_settings_sizer->AddSpacer(space5);
  advanced_settings_sizer->Add(m_cpu_engine_radiobox, 0, wxLEFT | wxRIGHT, space5);
  advanced_settings_sizer->AddSpacer(space5);
  advanced_settings_sizer->Add(m_force_ntscj_checkbox, 0, wxLEFT | wxRIGHT, space5);
  advanced_settings_sizer->AddSpacer(space5);

  wxBoxSizer* const clock_override_slider_sizer = new wxBoxSizer(wxHORIZONTAL);
  clock_override_slider_sizer->Add(m_clock_override_slider, 1);
  clock_override_slider_sizer->Add(m_clock_override_text, 1, wxLEFT, space5);

  wxStaticBoxSizer* const cpu_clock_override_sizer =
      new wxStaticBoxSizer(wxVERTICAL, this, _("CPU Clock Override"));
  cpu_clock_override_sizer->AddSpacer(space5);
  cpu_clock_override_sizer->Add(m_clock_override_checkbox, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  cpu_clock_override_sizer->AddSpacer(space5);
  cpu_clock_override_sizer->Add(clock_override_slider_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT,
                                space5);
  cpu_clock_override_sizer->AddSpacer(space5);
  cpu_clock_override_sizer->Add(clock_override_description, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  cpu_clock_override_sizer->AddSpacer(space5);

  wxFlexGridSizer* const custom_rtc_date_time_sizer =
      new wxFlexGridSizer(2, wxSize(space5, space5));
  custom_rtc_date_time_sizer->Add(m_custom_rtc_date_picker, 0, wxEXPAND);
  custom_rtc_date_time_sizer->Add(m_custom_rtc_time_picker, 0, wxEXPAND);

  wxStaticBoxSizer* const custom_rtc_sizer =
      new wxStaticBoxSizer(wxVERTICAL, this, _("Custom RTC Options"));
  custom_rtc_sizer->AddSpacer(space5);
  custom_rtc_sizer->Add(m_custom_rtc_checkbox, 0, wxLEFT | wxRIGHT, space5);
  custom_rtc_sizer->AddSpacer(space5);
  custom_rtc_sizer->Add(custom_rtc_date_time_sizer, 0, wxLEFT | wxRIGHT, space5);
  custom_rtc_sizer->AddSpacer(space5);
  custom_rtc_sizer->Add(custom_rtc_description, 0, wxLEFT | wxRIGHT, space5);
  custom_rtc_sizer->AddSpacer(space5);

  wxBoxSizer* const main_sizer = new wxBoxSizer(wxVERTICAL);
  main_sizer->AddSpacer(space5);
  main_sizer->Add(advanced_settings_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  main_sizer->AddSpacer(space5);
  main_sizer->Add(cpu_clock_override_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  main_sizer->AddSpacer(space5);
  main_sizer->Add(custom_rtc_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  main_sizer->AddSpacer(space5);

  SetSizer(main_sizer);
}

void AdvancedConfigPane::LoadGUIValues()
{
  const SConfig& startup_params = SConfig::GetInstance();

  for (size_t i = 0; i < m_cpu_cores.size(); ++i)
  {
    if (m_cpu_cores[i].CPUid == startup_params.iCPUCore)
      m_cpu_engine_radiobox->SetSelection(i);
  }
  m_force_ntscj_checkbox->SetValue(startup_params.bForceNTSCJ);

  int ocFactor = (int)(std::log2f(startup_params.m_OCFactor) * 25.f + 100.f + 0.5f);
  bool oc_enabled = startup_params.m_OCEnable;
  m_clock_override_checkbox->SetValue(oc_enabled);
  m_clock_override_slider->SetValue(ocFactor);
  m_clock_override_slider->Enable(oc_enabled);

  UpdateCPUClock();
  LoadCustomRTC();
}

void AdvancedConfigPane::BindEvents()
{
  m_cpu_engine_radiobox->Bind(wxEVT_RADIOBOX, &AdvancedConfigPane::OnCPUEngineRadioBoxChanged,
                              this);
  m_cpu_engine_radiobox->Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreNotRunning);

  m_force_ntscj_checkbox->Bind(wxEVT_CHECKBOX, &AdvancedConfigPane::OnForceNTSCJCheckBoxChanged,
                               this);
  m_force_ntscj_checkbox->Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreNotRunning);

  m_clock_override_checkbox->Bind(wxEVT_CHECKBOX,
                                  &AdvancedConfigPane::OnClockOverrideCheckBoxChanged, this);
  m_clock_override_checkbox->Bind(wxEVT_UPDATE_UI, &AdvancedConfigPane::OnUpdateCPUClockControls,
                                  this);

  m_clock_override_slider->Bind(wxEVT_SLIDER, &AdvancedConfigPane::OnClockOverrideSliderChanged,
                                this);
  m_clock_override_slider->Bind(wxEVT_UPDATE_UI, &AdvancedConfigPane::OnUpdateCPUClockControls,
                                this);

  m_custom_rtc_checkbox->Bind(wxEVT_CHECKBOX, &AdvancedConfigPane::OnCustomRTCCheckBoxChanged,
                              this);
  m_custom_rtc_checkbox->Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreNotRunning);

  m_custom_rtc_date_picker->Bind(wxEVT_DATE_CHANGED, &AdvancedConfigPane::OnCustomRTCDateChanged,
                                 this);
  m_custom_rtc_date_picker->Bind(wxEVT_UPDATE_UI, &AdvancedConfigPane::OnUpdateRTCDateTimeEntries,
                                 this);

  m_custom_rtc_time_picker->Bind(wxEVT_TIME_CHANGED, &AdvancedConfigPane::OnCustomRTCTimeChanged,
                                 this);
  m_custom_rtc_time_picker->Bind(wxEVT_UPDATE_UI, &AdvancedConfigPane::OnUpdateRTCDateTimeEntries,
                                 this);
}

void AdvancedConfigPane::OnCPUEngineRadioBoxChanged(wxCommandEvent& event)
{
  SConfig::GetInstance().iCPUCore = m_cpu_cores.at(event.GetSelection()).CPUid;
}

void AdvancedConfigPane::OnForceNTSCJCheckBoxChanged(wxCommandEvent& event)
{
  SConfig::GetInstance().bForceNTSCJ = m_force_ntscj_checkbox->IsChecked();
}

void AdvancedConfigPane::OnClockOverrideCheckBoxChanged(wxCommandEvent& event)
{
  SConfig::GetInstance().m_OCEnable = m_clock_override_checkbox->IsChecked();
  m_clock_override_slider->Enable(SConfig::GetInstance().m_OCEnable);
  UpdateCPUClock();
}

void AdvancedConfigPane::OnClockOverrideSliderChanged(wxCommandEvent& event)
{
  // Vaguely exponential scaling?
  SConfig::GetInstance().m_OCFactor =
      std::exp2f((m_clock_override_slider->GetValue() - 100.f) / 25.f);
  UpdateCPUClock();
}

static u32 ToSeconds(wxDateTime date)
{
  return static_cast<u32>(date.GetValue().GetValue() / 1000);
}

void AdvancedConfigPane::OnCustomRTCCheckBoxChanged(wxCommandEvent& event)
{
  const bool checked = m_custom_rtc_checkbox->IsChecked();
  SConfig::GetInstance().bEnableCustomRTC = checked;
  m_custom_rtc_date_picker->Enable(checked);
  m_custom_rtc_time_picker->Enable(checked);
}

void AdvancedConfigPane::OnCustomRTCDateChanged(wxCommandEvent& event)
{
  m_temp_date = ToSeconds(m_custom_rtc_date_picker->GetValue());
  UpdateCustomRTC(m_temp_date, m_temp_time);
}

void AdvancedConfigPane::OnCustomRTCTimeChanged(wxCommandEvent& event)
{
  m_temp_time = ToSeconds(m_custom_rtc_time_picker->GetValue()) - m_temp_date;
  UpdateCustomRTC(m_temp_date, m_temp_time);
}

void AdvancedConfigPane::UpdateCPUClock()
{
  int core_clock = SystemTimers::GetTicksPerSecond() / pow(10, 6);
  int percent = static_cast<int>(std::round(SConfig::GetInstance().m_OCFactor * 100.f));
  int clock = static_cast<int>(std::round(SConfig::GetInstance().m_OCFactor * core_clock));

  m_clock_override_text->SetLabel(
      SConfig::GetInstance().m_OCEnable ? wxString::Format("%d %% (%d MHz)", percent, clock) : "");
}

void AdvancedConfigPane::LoadCustomRTC()
{
  wxDateTime custom_rtc(static_cast<time_t>(SConfig::GetInstance().m_customRTCValue));
  custom_rtc = custom_rtc.ToUTC();
  bool custom_rtc_enabled = SConfig::GetInstance().bEnableCustomRTC;
  m_custom_rtc_checkbox->SetValue(custom_rtc_enabled);
  if (custom_rtc.IsValid())
  {
    m_custom_rtc_date_picker->SetValue(custom_rtc);
    m_custom_rtc_time_picker->SetValue(custom_rtc);
  }
  m_temp_date = ToSeconds(m_custom_rtc_date_picker->GetValue());
  m_temp_time = ToSeconds(m_custom_rtc_time_picker->GetValue()) - m_temp_date;
  // Limit dates to a valid range (Jan 1/2000 to Dec 31/2099)
  m_custom_rtc_date_picker->SetRange(wxDateTime(1, wxDateTime::Jan, 2000),
                                     wxDateTime(31, wxDateTime::Dec, 2099));
}

void AdvancedConfigPane::UpdateCustomRTC(time_t date, time_t time)
{
  wxDateTime custom_rtc(date + time);
  SConfig::GetInstance().m_customRTCValue = ToSeconds(custom_rtc.FromUTC());
  m_custom_rtc_date_picker->SetValue(custom_rtc);
  m_custom_rtc_time_picker->SetValue(custom_rtc);
}

void AdvancedConfigPane::OnUpdateCPUClockControls(wxUpdateUIEvent& event)
{
  if (!Core::IsRunning())
  {
    event.Enable(true);
    return;
  }

  event.Enable(!Core::WantsDeterminism());
}

void AdvancedConfigPane::OnUpdateRTCDateTimeEntries(wxUpdateUIEvent& event)
{
  event.Enable(!Core::IsRunning() && m_custom_rtc_checkbox->IsChecked());
}
