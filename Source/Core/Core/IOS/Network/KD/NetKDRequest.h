// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include "Core/IOS/Device.h"
#include "Core/IOS/Network/KD/NWC24Config.h"

namespace IOS::HLE
{
// KD is the IOS module responsible for implementing WiiConnect24 functionality.
// It can perform HTTPS downloads, send and receive mail via SMTP, and execute a
// JavaScript-like language while the Wii is in standby mode.
class NetKDRequestDevice : public Device
{
public:
  NetKDRequestDevice(Kernel& ios, const std::string& device_name);
  ~NetKDRequestDevice() override;

  std::optional<IPCReply> IOCtl(const IOCtlRequest& request) override;

private:
  NWC24::NWC24Config config;
};
}  // namespace IOS::HLE
