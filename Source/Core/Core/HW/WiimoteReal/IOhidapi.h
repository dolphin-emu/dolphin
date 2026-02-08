// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef HAVE_HIDAPI
#include <hidapi.h>

#include "Core/HW/WiimoteReal/WiimoteReal.h"

namespace WiimoteReal
{
class WiimoteHidapi final : public Wiimote
{
public:
  explicit WiimoteHidapi(std::string device_path);
  ~WiimoteHidapi() override;
  std::string GetId() const override { return m_device_path; }

protected:
  bool ConnectInternal() override;
  void DisconnectInternal() override;
  bool IsConnected() const override;
  void IOWakeup() override {}
  int IORead(u8* buf) override;
  int IOWrite(const u8* buf, size_t len) override;

private:
  const std::string m_device_path;
  hid_device* m_handle = nullptr;
};

class WiimoteScannerHidapi final : public WiimoteScannerBackend
{
public:
  WiimoteScannerHidapi();
  ~WiimoteScannerHidapi() override;
  bool IsReady() const override;

  FindResults FindAttachedWiimotes() override;

  void Update() override {}                // not needed for hidapi
  void RequestStopSearching() override {}  // not needed for hidapi
};
}  // namespace WiimoteReal

#else
#include "Core/HW/WiimoteReal/IODummy.h"
namespace WiimoteReal
{
using WiimoteScannerHidapi = WiimoteScannerDummy;
}
#endif
