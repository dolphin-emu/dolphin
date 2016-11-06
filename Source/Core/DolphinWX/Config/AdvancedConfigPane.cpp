// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cmath>

#include <wx/checkbox.h>
#include <wx/datectrl.h>
#include <wx/dateevt.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/time.h>
#include <wx/timectrl.h>

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "DolphinWX/Config/AdvancedConfigPane.h"
#include "DolphinWX/DolphinSlider.h"

AdvancedConfigPane::AdvancedConfigPane(wxWindow* parent, wxWindowID id) : wxPanel(parent, id)
{
  InitializeGUI();
  LoadGUIValues();
  RefreshGUI();
}

void AdvancedConfigPane::InitializeGUI()
{
  m_cpu_clock_override_checkbox = new wxCheckBox(this, wxID_ANY, _("Enable CPU Clock Override"));
  m_cpu_clock_override_slider =
      new DolphinSlider(this, wxID_ANY, 100, 0, 150, wxDefaultPosition, FromDIP(wxSize(200, -1)));
  m_cpu_clock_override_text = new wxStaticText(this, wxID_ANY, "");

  m_cpu_clock_override_checkbox->Bind(wxEVT_CHECKBOX,
                                      &AdvancedConfigPane::OnClockOverrideCheckBoxChanged, this);
  m_cpu_clock_override_slider->Bind(wxEVT_SLIDER, &AdvancedConfigPane::OnClockOverrideSliderChanged,
                                    this);

  m_gpu_clock_override_checkbox = new wxCheckBox(this, wxID_ANY, _("Enable GPU Clock Override"));
  m_gpu_clock_override_slider =
      new DolphinSlider(this, wxID_ANY, 100, 0, 150, wxDefaultPosition, FromDIP(wxSize(200, -1)));
  m_gpu_clock_override_text = new wxStaticText(this, wxID_ANY, "");

  m_gpu_clock_override_checkbox->Bind(wxEVT_CHECKBOX,
                                      &AdvancedConfigPane::OnGPUClockOverrideCheckBoxChanged, this);
  m_gpu_clock_override_slider->Bind(wxEVT_SLIDER,
                                    &AdvancedConfigPane::OnGPUClockOverrideSliderChanged, this);

  m_sync_gpu_checkbox = new wxCheckBox(this, wxID_ANY, _("Enable Synchronous GPU"));

  m_sync_gpu_checkbox->Bind(wxEVT_CHECKBOX, &AdvancedConfigPane::OnSyncGPUCheckBoxChanged, this);

  m_custom_rtc_checkbox = new wxCheckBox(this, wxID_ANY, _("Enable Custom RTC"));
  m_custom_rtc_date_picker = new wxDatePickerCtrl(this, wxID_ANY);
  m_custom_rtc_time_picker = new wxTimePickerCtrl(this, wxID_ANY);

  m_custom_rtc_checkbox->Bind(wxEVT_CHECKBOX, &AdvancedConfigPane::OnCustomRTCCheckBoxChanged,
                              this);
  m_custom_rtc_date_picker->Bind(wxEVT_DATE_CHANGED, &AdvancedConfigPane::OnCustomRTCDateChanged,
                                 this);
  m_custom_rtc_time_picker->Bind(wxEVT_TIME_CHANGED, &AdvancedConfigPane::OnCustomRTCTimeChanged,
                                 this);

  wxStaticText* const clock_override_description =
      new wxStaticText(this, wxID_ANY, _("Higher values can make variable-framerate games "
                                         "run at a higher framerate, at the expense of CPU. "
                                         "Lower values can make variable-framerate games "
                                         "run at a lower framerate, saving CPU.\n\n"
                                         "WARNING: Changing this from the default (100%) "
                                         "can and will break games and cause glitches. "
                                         "Do so at your own risk. Please do not report "
                                         "bugs that occur with a non-default clock."));

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

  wxBoxSizer* const clock_override_slider_sizer = new wxBoxSizer(wxHORIZONTAL);
  clock_override_slider_sizer->Add(m_cpu_clock_override_slider, 1);
  clock_override_slider_sizer->Add(m_cpu_clock_override_text, 1, wxLEFT, space5);

  wxBoxSizer* const gpu_clock_override_slider_sizer = new wxBoxSizer(wxHORIZONTAL);
  gpu_clock_override_slider_sizer->Add(m_gpu_clock_override_slider, 1);
  gpu_clock_override_slider_sizer->Add(m_gpu_clock_override_text, 1, wxLEFT, space5);

  wxStaticBoxSizer* const cpu_options_sizer =
      new wxStaticBoxSizer(wxVERTICAL, this, _("Overclock Options"));
  cpu_options_sizer->AddSpacer(space5);
  cpu_options_sizer->Add(m_cpu_clock_override_checkbox, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  cpu_options_sizer->AddSpacer(space5);
  cpu_options_sizer->Add(clock_override_slider_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  cpu_options_sizer->AddSpacer(space5);
  cpu_options_sizer->Add(m_sync_gpu_checkbox, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  cpu_options_sizer->AddSpacer(space5);
  cpu_options_sizer->Add(m_gpu_clock_override_checkbox, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  cpu_options_sizer->AddSpacer(space5);
  cpu_options_sizer->Add(gpu_clock_override_slider_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  cpu_options_sizer->AddSpacer(space5);
  cpu_options_sizer->Add(clock_override_description, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  cpu_options_sizer->AddSpacer(space5);

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
  main_sizer->Add(cpu_options_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  main_sizer->AddSpacer(space5);
  main_sizer->Add(custom_rtc_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  main_sizer->AddSpacer(space5);

  SetSizer(main_sizer);
}

void AdvancedConfigPane::LoadGUIValues()
{
  LoadCPUOverclock();
  LoadGPUOverclock();
  UpdateCPUClock();
  UpdateGPUClock();
  LoadCustomRTC();
}

void AdvancedConfigPane::OnClockOverrideCheckBoxChanged(wxCommandEvent& event)
{
  SConfig::GetInstance().m_OCEnable = m_cpu_clock_override_checkbox->IsChecked();
  m_cpu_clock_override_slider->Enable(SConfig::GetInstance().m_OCEnable);
  UpdateCPUClock();
}

void AdvancedConfigPane::OnClockOverrideSliderChanged(wxCommandEvent& event)
{
  // Vaguely exponential scaling?
  SConfig::GetInstance().m_OCFactor =
      std::exp2f((m_cpu_clock_override_slider->GetValue() - 100.f) / 25.f);
  UpdateCPUClock();
}

void AdvancedConfigPane::OnGPUClockOverrideCheckBoxChanged(wxCommandEvent&)
{
  SConfig::GetInstance().m_GPUOCEnable = m_gpu_clock_override_checkbox->IsChecked();
  m_gpu_clock_override_slider->Enable(SConfig::GetInstance().m_GPUOCEnable);
  UpdateGPUClock();
}

void AdvancedConfigPane::OnGPUClockOverrideSliderChanged(wxCommandEvent&)
{
  // Vaguely exponential scaling?
  SConfig::GetInstance().fSyncGpuOverclock =
      std::exp2f((m_gpu_clock_override_slider->GetValue() - 100.f) / 25.f);
  UpdateGPUClock();
}

void AdvancedConfigPane::OnSyncGPUCheckBoxChanged(wxCommandEvent&)
{
  SConfig::GetInstance().bSyncGPU = m_sync_gpu_checkbox->IsChecked();
  m_gpu_clock_override_checkbox->Enable(SConfig::GetInstance().bSyncGPU);
  if (!SConfig::GetInstance().bSyncGPU)
  {
    // Disable GPU Overclocking
    m_gpu_clock_override_checkbox->SetValue(false);
    SConfig::GetInstance().m_GPUOCEnable = false;
  }
  m_gpu_clock_override_slider->Enable(m_gpu_clock_override_checkbox->IsChecked());
  UpdateGPUClock();
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
  bool wii = SConfig::GetInstance().bWii;
  int percent = static_cast<int>(std::roundf(SConfig::GetInstance().m_OCFactor * 100.f));
  int clock =
      static_cast<int>(std::roundf(SConfig::GetInstance().m_OCFactor * (wii ? 729.f : 486.f)));

  m_cpu_clock_override_text->SetLabel(
      SConfig::GetInstance().m_OCEnable ? wxString::Format("%d %% (%d mhz)", percent, clock) : "");
}

void AdvancedConfigPane::UpdateGPUClock()
{
  int percent = static_cast<int>(std::roundf(SConfig::GetInstance().fSyncGpuOverclock * 100.f));
  m_gpu_clock_override_text->SetLabel(
      SConfig::GetInstance().m_GPUOCEnable ? wxString::Format("%d %%", percent) : "");
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
  if (Core::IsRunning())
  {
    m_custom_rtc_checkbox->Enable(false);
    m_custom_rtc_date_picker->Enable(false);
    m_custom_rtc_time_picker->Enable(false);
  }
  else
  {
    m_custom_rtc_date_picker->Enable(custom_rtc_enabled);
    m_custom_rtc_time_picker->Enable(custom_rtc_enabled);
  }
}

void AdvancedConfigPane::UpdateCustomRTC(time_t date, time_t time)
{
  wxDateTime custom_rtc(date + time);
  SConfig::GetInstance().m_customRTCValue = ToSeconds(custom_rtc.FromUTC());
  m_custom_rtc_date_picker->SetValue(custom_rtc);
  m_custom_rtc_time_picker->SetValue(custom_rtc);
}

void AdvancedConfigPane::RefreshGUI()
{
  // Don't allow users to edit the RTC or toggle SyncGPU while the game is running
  if (Core::IsRunning())
  {
    m_custom_rtc_checkbox->Disable();
    m_custom_rtc_date_picker->Disable();
    m_custom_rtc_time_picker->Disable();
    m_sync_gpu_checkbox->Disable();
  }
  // Allow users to edit CPU/GPU clock speed in game, but not while needing determinism
  if (Core::IsRunning() && Core::g_want_determinism)
  {
    m_cpu_clock_override_checkbox->Disable();
    m_cpu_clock_override_slider->Disable();
    m_gpu_clock_override_checkbox->Disable();
    m_gpu_clock_override_slider->Disable();
  }
}

void AdvancedConfigPane::LoadCPUOverclock()
{
  int cpu_oc_factor =
      static_cast<int>(std::log2f(SConfig::GetInstance().m_OCFactor) * 25.f + 100.f + 0.5f);
  bool cpu_oc_enabled = SConfig::GetInstance().m_OCEnable;
  m_cpu_clock_override_checkbox->SetValue(cpu_oc_enabled);
  m_cpu_clock_override_slider->SetValue(cpu_oc_factor);
  m_cpu_clock_override_slider->Enable(cpu_oc_enabled);
}

void AdvancedConfigPane::LoadGPUOverclock()
{
  int gpu_oc_factor =
      static_cast<int>(std::log2f(SConfig::GetInstance().fSyncGpuOverclock) * 25.f + 100.f + 0.5f);
  bool gpu_oc_enabled = SConfig::GetInstance().m_GPUOCEnable;
  bool sync_gpu_enabled = SConfig::GetInstance().bSyncGPU;
  if (SConfig::GetInstance().bCPUThread)
  {
    m_sync_gpu_checkbox->Enable();
    m_sync_gpu_checkbox->SetValue(sync_gpu_enabled);
  }
  else  // SyncGPU is always enabled in single core mode
  {
    m_sync_gpu_checkbox->Disable();
    m_sync_gpu_checkbox->SetValue(true);
  }
  if (m_sync_gpu_checkbox->GetValue())
    m_gpu_clock_override_checkbox->Enable();
  else
    m_gpu_clock_override_checkbox->Disable();
  m_gpu_clock_override_checkbox->SetValue(gpu_oc_enabled);
  m_gpu_clock_override_slider->SetValue(gpu_oc_factor);
  m_gpu_clock_override_slider->Enable(gpu_oc_enabled);
}
