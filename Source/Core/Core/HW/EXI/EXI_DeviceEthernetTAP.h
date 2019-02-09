// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <thread>

#ifdef _WIN32
#include <vector>
#include <Windows.h>
#endif

#include "Common/Flag.h"
#include "Core/HW/EXI/EXI_DeviceEthernetBase.h"

namespace ExpansionInterface
{
class CEXIEthernetTAP : public CEXIEthernetBase
{
public:
    ~CEXIEthernetTAP() override;

protected:
    bool Activate() override;
    bool IsActivated() const override;
    bool SendFrame(const u8* frame, u32 size) override;
    bool RecvInit() override;
    void RecvStart() override;
    void RecvStop() override;

private:
  static void ReadThreadHandler(CEXIEthernetTAP* self);

#ifdef _WIN32
  HANDLE mHAdapter = INVALID_HANDLE_VALUE;
  OVERLAPPED mReadOverlapped = {};
  OVERLAPPED mWriteOverlapped = {};
  std::vector<u8> mWriteBuffer;
  bool mWritePending = false;
#elif defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
  int fd = -1;
#endif

#if defined(WIN32) || defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) ||          \
    defined(__OpenBSD__)
  std::thread m_read_thread;
  Common::Flag m_read_enabled;
  Common::Flag m_read_thread_shutdown;
#endif
};
}  // namespace ExpansionInterface
