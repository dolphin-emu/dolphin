// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <utility>

#include <wx/clntdata.h>
#include <wx/dialog.h>
#include <wx/timer.h>

#include "Common/CommonTypes.h"

class wxListBox;
class wxSizer;
class wxTextCtrl;

class USBPassthroughDeviceEntry final : public wxClientData
{
public:
  explicit USBPassthroughDeviceEntry(const std::pair<u16, u16> pair)
      : m_vid(pair.first), m_pid(pair.second)
  {
  }
  const u16 m_vid;
  const u16 m_pid;
};

// This dialog is used to add a new USB device to the USB passthrough whitelist,
// either by selecting a connected USB device or by entering the PID/VID manually.
class AddUSBDeviceDiag final : public wxDialog
{
public:
  explicit AddUSBDeviceDiag(wxWindow* parent);

private:
  static constexpr int DEVICE_REFRESH_INTERVAL_MS = 100;

  void InitControls();
  void RefreshDeviceList();
  wxSizer* CreateManualControlsSizer();
  wxSizer* CreateDeviceListSizer();

  void OnRefreshDevicesTimer(wxTimerEvent&);
  void OnDeviceSelection(wxCommandEvent&);
  void OnSave(wxCommandEvent&);

  std::map<std::pair<u16, u16>, std::string> m_shown_devices;
  wxTimer m_refresh_devices_timer{this};

  wxTextCtrl* m_new_device_vid_ctrl;
  wxTextCtrl* m_new_device_pid_ctrl;
  wxListBox* m_inserted_devices_listbox;
};
