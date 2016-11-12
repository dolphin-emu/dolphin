// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#if defined(__LIBUSB__)
#include <cstddef>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/IPC_HLE/USB/Common.h"

struct libusb_device;
struct libusb_device_handle;
struct libusb_transfer;

class LibusbDevice final : public Device
{
public:
  LibusbDevice(libusb_device* device, s32 device_id, u8 interface);
  ~LibusbDevice();
  std::string GetErrorName(int error_code) const override;
  bool AttachDevice() override;
  int CancelTransfer(u8 endpoint) override;
  int ChangeInterface(u8 interface) override;
  int SetAltSetting(u8 alt_setting) override;
  int SubmitTransfer(std::unique_ptr<CtrlMessage> message) override;
  int SubmitTransfer(std::unique_ptr<BulkMessage> message) override;
  int SubmitTransfer(std::unique_ptr<IntrMessage> message) override;
  int SubmitTransfer(std::unique_ptr<IsoMessage> message) override;
  std::vector<u8> GetIOSDescriptors(size_t buffer_size, size_t* real_size, Version ver) override;

private:
  struct PendingTransfer
  {
    std::unique_ptr<TransferCommand> command;
    libusb_transfer* transfer;
  };

  libusb_device* m_device = nullptr;
  libusb_device_handle* m_handle = nullptr;

  std::mutex m_pending_transfers_mutex;
  std::map<u8 /* endpoint */, std::deque<PendingTransfer>> m_pending_transfers;
  static void CtrlTransferCallback(libusb_transfer* transfer);
  static void TransferCallback(libusb_transfer* transfer);

  int AttachInterface(u8 interface);
  int DetachInterface(u8 interface);
};

#else
#include "Core/IPC_HLE/USB/StubDevice.h"

using LibusbDevice = StubDevice;
#endif
