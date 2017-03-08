// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cctype>

#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/listbox.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/window.h>

#include "Common/StringUtil.h"
#include "Core/ConfigManager.h"
#include "DolphinWX/Config/AddUSBDeviceDiag.h"
#include "DolphinWX/WxUtils.h"
#include "UICommon/USBUtils.h"

AddUSBDeviceDiag::AddUSBDeviceDiag(wxWindow* const parent)
    : wxDialog(parent, wxID_ANY, _("Add New USB Device"))
{
  InitControls();

  RefreshDeviceList();
  Bind(wxEVT_TIMER, &AddUSBDeviceDiag::OnRefreshDevicesTimer, this,
       m_refresh_devices_timer.GetId());
  m_refresh_devices_timer.Start(DEVICE_REFRESH_INTERVAL_MS, wxTIMER_CONTINUOUS);

  auto* const btn_sizer = CreateStdDialogButtonSizer(wxOK | wxCANCEL);
  btn_sizer->GetAffirmativeButton()->SetLabel(_("Add"));
  Bind(wxEVT_BUTTON, &AddUSBDeviceDiag::OnSave, this, wxID_OK);

  auto* const sizer = new wxBoxSizer(wxVERTICAL);
  const int space5 = FromDIP(5);
  sizer->AddSpacer(FromDIP(10));
  sizer->Add(new wxStaticText(this, wxID_ANY, _("Enter USB device ID"), wxDefaultPosition,
                              wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL),
             0, wxEXPAND | wxBOTTOM, FromDIP(10));
  sizer->Add(CreateManualControlsSizer(), 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  sizer->Add(new wxStaticText(this, wxID_ANY, _("or select a device"), wxDefaultPosition,
                              wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL),
             0, wxEXPAND | wxTOP | wxBOTTOM, FromDIP(10));
  auto* const device_list_sizer = CreateDeviceListSizer();
  sizer->Add(device_list_sizer, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, space5);
  sizer->SetItemMinSize(device_list_sizer, FromDIP(350), FromDIP(150));
  sizer->Add(btn_sizer, 0, wxEXPAND);
  sizer->AddSpacer(space5);

  SetSizerAndFit(sizer);
  Center();
}

void AddUSBDeviceDiag::InitControls()
{
  m_new_device_vid_ctrl = new wxTextCtrl(this, wxID_ANY);
  m_new_device_pid_ctrl = new wxTextCtrl(this, wxID_ANY);
  // i18n: VID means Vendor ID (in the context of a USB device)
  m_new_device_vid_ctrl->SetHint(_("Device VID (e.g., 057e)"));
  // i18n: PID means Product ID (in the context of a USB device), not Process ID
  m_new_device_pid_ctrl->SetHint(_("Device PID (e.g., 0305)"));

  m_inserted_devices_listbox = new wxListBox(this, wxID_ANY);
  m_inserted_devices_listbox->Bind(wxEVT_LISTBOX, &AddUSBDeviceDiag::OnDeviceSelection, this);
  m_inserted_devices_listbox->Bind(wxEVT_LISTBOX_DCLICK, &AddUSBDeviceDiag::OnSave, this);
}

void AddUSBDeviceDiag::RefreshDeviceList()
{
  const auto& current_devices = USBUtils::GetInsertedDevices();
  if (current_devices == m_shown_devices)
    return;

  m_inserted_devices_listbox->Freeze();
  const auto selection_string = m_inserted_devices_listbox->GetStringSelection();
  m_inserted_devices_listbox->Clear();
  for (const auto& device : current_devices)
  {
    if (SConfig::GetInstance().IsUSBDeviceWhitelisted(device.first))
      continue;
    m_inserted_devices_listbox->Append(device.second, new USBPassthroughDeviceEntry(device.first));
  }
  if (!selection_string.empty())
    m_inserted_devices_listbox->SetStringSelection(selection_string);
  m_inserted_devices_listbox->Thaw();

  m_shown_devices = current_devices;
}

wxSizer* AddUSBDeviceDiag::CreateManualControlsSizer()
{
  const int space5 = FromDIP(5);
  auto* const sizer = new wxBoxSizer(wxHORIZONTAL);

  sizer->Add(m_new_device_vid_ctrl, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);
  sizer->Add(m_new_device_pid_ctrl, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);

  return sizer;
}

wxSizer* AddUSBDeviceDiag::CreateDeviceListSizer()
{
  const int space5 = FromDIP(5);
  auto* const sizer = new wxBoxSizer(wxVERTICAL);

  sizer->Add(m_inserted_devices_listbox, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);

  return sizer;
}

static bool IsValidUSBIDString(const std::string& string)
{
  if (string.empty() || string.length() > 4)
    return false;
  return std::all_of(string.begin(), string.end(),
                     [](const auto character) { return std::isxdigit(character) != 0; });
}

void AddUSBDeviceDiag::OnRefreshDevicesTimer(wxTimerEvent&)
{
  RefreshDeviceList();
}

void AddUSBDeviceDiag::OnDeviceSelection(wxCommandEvent&)
{
  const int index = m_inserted_devices_listbox->GetSelection();
  if (index == wxNOT_FOUND)
    return;
  auto* const entry = static_cast<const USBPassthroughDeviceEntry*>(
      m_inserted_devices_listbox->GetClientObject(index));
  m_new_device_vid_ctrl->SetValue(StringFromFormat("%04x", entry->m_vid));
  m_new_device_pid_ctrl->SetValue(StringFromFormat("%04x", entry->m_pid));
}

void AddUSBDeviceDiag::OnSave(wxCommandEvent&)
{
  const std::string vid_string = StripSpaces(WxStrToStr(m_new_device_vid_ctrl->GetValue()));
  const std::string pid_string = StripSpaces(WxStrToStr(m_new_device_pid_ctrl->GetValue()));
  if (!IsValidUSBIDString(vid_string))
  {
    // i18n: Here, VID means Vendor ID (for a USB device).
    WxUtils::ShowErrorDialog(_("The entered VID is invalid."));
    return;
  }
  if (!IsValidUSBIDString(pid_string))
  {
    // i18n: Here, PID means Product ID (for a USB device).
    WxUtils::ShowErrorDialog(_("The entered PID is invalid."));
    return;
  }

  const u16 vid = static_cast<u16>(std::stoul(vid_string, nullptr, 16));
  const u16 pid = static_cast<u16>(std::stoul(pid_string, nullptr, 16));

  if (SConfig::GetInstance().IsUSBDeviceWhitelisted({vid, pid}))
  {
    WxUtils::ShowErrorDialog(_("This USB device is already whitelisted."));
    return;
  }

  SConfig::GetInstance().m_usb_passthrough_devices.emplace(vid, pid);
  AcceptAndClose();
}
