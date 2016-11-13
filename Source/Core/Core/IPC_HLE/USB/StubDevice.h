// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "Core/IPC_HLE/USB/Common.h"

class StubDevice : public Device
{
public:
  StubDevice(u8 interface) : Device(interface) {}
  ~StubDevice() = default;
  bool AttachDevice() override { return false; }
  int CancelTransfer(u8 endpoint) override { return -1; }
  int SetAltSetting(u8 alt_setting) override { return -1; }
  int SubmitTransfer(std::unique_ptr<CtrlMessage> message) override { return -1; }
  int SubmitTransfer(std::unique_ptr<BulkMessage> message) override { return -1; }
  int SubmitTransfer(std::unique_ptr<IntrMessage> message) override { return -1; }
  int SubmitTransfer(std::unique_ptr<IsoMessage> message) override { return -1; }
  std::vector<u8> GetIOSDescriptors() override { return {}; }
};
