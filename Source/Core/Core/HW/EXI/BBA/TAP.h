// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <thread>

#ifdef _WIN32
#include <windows.h>

#include <vector>
#endif

#include "Common/Flag.h"
#include "Core/HW/EXI/EXI_DeviceEthernet.h"

namespace ExpansionInterface
{

class TAPNetworkInterface : public CEXIETHERNET::NetworkInterface
{
public:
  explicit TAPNetworkInterface(CEXIETHERNET* eth_ref) : NetworkInterface(eth_ref) {}

  bool Activate() override;
  void Deactivate() override;
  bool IsActivated() override;
  bool SendFrame(const u8* frame, u32 size) override;
  bool RecvInit() override;
  void RecvStart() override;
  void RecvStop() override;

protected:
#if defined(WIN32) || defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) ||          \
    defined(__OpenBSD__)
  std::thread readThread;
  Common::Flag readEnabled;
  Common::Flag readThreadShutdown;
  static void ReadThreadHandler(TAPNetworkInterface* self);
#endif
#if defined(_WIN32)
  HANDLE mHAdapter = INVALID_HANDLE_VALUE;
  OVERLAPPED mReadOverlapped = {};
  OVERLAPPED mWriteOverlapped = {};
  std::vector<u8> mWriteBuffer;
  bool mWritePending = false;
#elif defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
  int fd = -1;
#endif
};

}  // namespace ExpansionInterface
