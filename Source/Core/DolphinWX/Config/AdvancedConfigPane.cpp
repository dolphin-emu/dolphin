// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/Config/AdvancedConfigPane.h"

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
#include "Core/HW/SystemTimers.h"
#include "DolphinWX/DolphinSlider.h"
#include "DolphinWX/WxEventUtils.h"

AdvancedConfigPane::AdvancedConfigPane(wxWindow* parent, wxWindowID id) : wxPanel(parent, id)
{
  InitializeGUI();
  LoadGUIValues();
  BindEvents();
}

void AdvancedConfigPane::InitializeGUI()
{
  m_clock_override_checkbox =
      new wxCheckBox(this, wxID_ANY, _("Enable Emulated CPU Clock Override"));
  m_clock_override_slider =
      new DolphinSlider(this, wxID_ANY, 100, 0, 150, wxDefaultPosition, FromDIP(wxSize(200, -1)));
  m_clock_override_text = new wxStaticText(this, wxID_ANY, "");

  m_custom_rtc_checkbox = new wxCheckBox(this, wxID_ANY, _("Enable Custom RTC"));
  m_custom_rtc_date_picker = new wxDatePickerCtrl(this, wxID_ANY);
  // The Wii System Menu only allows configuring a year between 2000 and 2035.
  // However, the GameCube main menu (IPL) allows setting a year between 2000 and 2099,
  // which is why we use that range here. The Wii still deals OK with dates beyond 2035
  // and simply clamps them to 2035-12-31.
  m_custom_rtc_date_picker->SetRange(wxDateTime(1, wxDateTime::Jan, 2000),
                                     wxDateTime(31, wxDateTime::Dec, 2099));
  m_custom_rtc_time_picker = new wxTimePickerCtrl(this, wxID_ANY);

  wxStaticText* const clock_override_description =
      new wxStaticText(this, wxID_ANY, _("Adjusts the emulated CPU's clock rate.\n\n"
                                         "Higher values may make variable-framerate games run "
                                         "at a higher framerate, at the expense of performance. "
                                         "Lower values may activate a game's internal "
                                         "frameskip, potentially improving performance.\n\n"
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
  clock_override_slider_sizer->Add(m_clock_override_slider, 1);
  clock_override_slider_sizer->Add(m_clock_override_text, 1, wxLEFT, space5);

  wxStaticBoxSizer* const cpu_options_sizer =
      new wxStaticBoxSizer(wxVERTICAL, this, _("CPU Options"));
  cpu_options_sizer->AddSpacer(space5);
  cpu_options_sizer->Add(m_clock_override_checkbox, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  cpu_options_sizer->AddSpacer(space5);
  cpu_options_sizer->Add(clock_override_slider_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
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
  int ocFactor =
      static_cast<int>(std::ceil(std::log2f(SConfig::GetInstance().m_OCFactor) * 25.f + 100.f));
  bool oc_enabled = SConfig::GetInstance().m_OCEnable;
  m_clock_override_checkbox->SetValue(oc_enabled);
  m_clock_override_slider->SetValue(ocFactor);
  m_clock_override_slider->Enable(oc_enabled);
  UpdateCPUClock();
  LoadCustomRTC();
}

void AdvancedConfigPane::BindEvents()
{
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

static wxDateTime GetCustomRTCDateTime()
{
  time_t timestamp = SConfig::GetInstance().m_customRTCValue;
  return wxDateTime(timestamp).ToUTC();
}

static wxDateTime CombineDateAndTime(const wxDateTime& date, const wxDateTime& time)
{
  wxDateTime datetime = date;
  datetime.SetHour(time.GetHour());
  datetime.SetMinute(time.GetMinute());
  datetime.SetSecond(time.GetSecond());

  return datetime;
}

void AdvancedConfigPane::OnCustomRTCCheckBoxChanged(wxCommandEvent& event)
{
  const bool checked = m_custom_rtc_checkbox->IsChecked();
  SConfig::GetInstance().bEnableCustomRTC = checked;
  m_custom_rtc_date_picker->Enable(checked);
  m_custom_rtc_time_picker->Enable(checked);
}

void AdvancedConfigPane::OnCustomRTCDateChanged(wxDateEvent& event)
{
  wxDateTime datetime = CombineDateAndTime(event.GetDate(), GetCustomRTCDateTime());
  UpdateCustomRTC(datetime);
}

void AdvancedConfigPane::OnCustomRTCTimeChanged(wxDateEvent& event)
{
  wxDateTime datetime = CombineDateAndTime(GetCustomRTCDateTime(), event.GetDate());
  UpdateCustomRTC(datetime);
}

void AdvancedConfigPane::UpdateCPUClock()
{
  int core_clock = SystemTimers::GetTicksPerSecond() / std::pow(10, 6);
  int percent = static_cast<int>(std::round(SConfig::GetInstance().m_OCFactor * 100.f));
  int clock = static_cast<int>(std::round(SConfig::GetInstance().m_OCFactor * core_clock));

  m_clock_override_text->SetLabel(SConfig::GetInstance().m_OCEnable ?
                                      wxString::Format("%d %% (%d MHz)", percent, clock) :
                                      wxString());
}

void AdvancedConfigPane::LoadCustomRTC()
{
  bool custom_rtc_enabled = SConfig::GetInstance().bEnableCustomRTC;
  m_custom_rtc_checkbox->SetValue(custom_rtc_enabled);

  wxDateTime datetime = GetCustomRTCDateTime();
  if (datetime.IsValid())
  {
    m_custom_rtc_date_picker->SetValue(datetime);
    m_custom_rtc_time_picker->SetValue(datetime);
  }

  // Make sure we have a valid custom RTC date and time
  // both when it was out of range as well as when it was invalid to begin with.
  datetime = CombineDateAndTime(m_custom_rtc_date_picker->GetValue(),
                                m_custom_rtc_time_picker->GetValue());
  UpdateCustomRTC(datetime);
}

void AdvancedConfigPane::UpdateCustomRTC(const wxDateTime& datetime)
{
  // We need GetValue() as GetTicks() only works up to 0x7ffffffe, which is in 2038.
  u32 timestamp = datetime.FromUTC().GetValue().GetValue() / 1000;
  SConfig::GetInstance().m_customRTCValue = timestamp;
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
