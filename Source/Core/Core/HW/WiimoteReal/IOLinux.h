// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#if defined(__linux__) && HAVE_BLUEZ
#include <bluetooth/bluetooth.h>

#include "Core/HW/WiimoteReal/WiimoteReal.h"

namespace WiimoteReal
{
class WiimoteLinux final : public Wiimote
{
public:
  WiimoteLinux(bdaddr_t bdaddr);
  ~WiimoteLinux() override;
  std::string GetId() const override
  {
    char bdaddr_str[18] = {};
    ba2str(&m_bdaddr, bdaddr_str);
    return bdaddr_str;
  }

protected:
  bool ConnectInternal() override;
  void DisconnectInternal() override;
  bool IsConnected() const override;
  void IOWakeup() override;
  int IORead(u8* buf) override;
  int IOWrite(u8 const* buf, size_t len) override;

private:
  bdaddr_t m_bdaddr;  // Bluetooth address
  int m_cmd_sock;     // Command socket
  int m_int_sock;     // Interrupt socket
  int m_wakeup_pipe_w;
  int m_wakeup_pipe_r;
};

class WiimoteScannerLinux final : public WiimoteScannerBackend
{
public:
  WiimoteScannerLinux();
  ~WiimoteScannerLinux() override;
  bool IsReady() const override;
  void FindWiimotes(std::vector<Wiimote*>&, Wiimote*&) override;
  void Update() override {}  // not needed on Linux
private:
  int m_device_id;
  int m_device_sock;
};
}  // namespace WiimoteReal

#else
#include "Core/HW/WiimoteReal/IODummy.h"
namespace WiimoteReal
{
using WiimoteScannerLinux = WiimoteScannerDummy;
}
#endif
