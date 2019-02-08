// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <thread>

#include "Common/Flag.h"
#include "EXI_DeviceEthernetBase.h"

namespace sf { class TcpSocket; }

namespace ExpansionInterface
{
class CEXIEthernetTCP : public CEXIEthernetBase
{
public:
    ~CEXIEthernetTCP() override;

protected:
  bool Activate() override;
  bool IsActivated() const override;
  bool SendFrame(const u8* frame, u32 size) override;
  bool RecvInit() override;
  void RecvStart() override;
  void RecvStop() override;

private:
  static void ReadThreadHandler(CEXIEthernetTCP* self);

  std::unique_ptr<sf::TcpSocket> m_socket;

  std::thread m_read_thread;
  Common::Flag m_read_enabled;
  Common::Flag m_read_thread_shutdown;
};
}  // namespace ExpansionInterface
