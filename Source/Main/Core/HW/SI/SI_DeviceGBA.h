// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>

#include <SFML/Network.hpp>

#include "Common/CommonTypes.h"
#include "Core/HW/SI/SI_Device.h"

// GameBoy Advance "Link Cable"

namespace SerialInterface
{
void GBAConnectionWaiter_Shutdown();

class GBASockServer
{
public:
  GBASockServer();
  ~GBASockServer();

  bool Connect();
  bool IsConnected();
  void ClockSync();
  void Send(const u8* si_buffer);
  int Receive(u8* si_buffer);

private:
  void Disconnect();

  std::unique_ptr<sf::TcpSocket> m_client;
  std::unique_ptr<sf::TcpSocket> m_clock_sync;

  u64 m_last_time_slice = 0;
  bool m_booted = false;
};

class CSIDevice_GBA : public ISIDevice
{
public:
  CSIDevice_GBA(SIDevices device, int device_number);

  int RunBuffer(u8* buffer, int length) override;
  int TransferInterval() override;
  bool GetData(u32& hi, u32& low) override;
  void SendCommand(u32 command, u8 poll) override;

private:
  enum class NextAction
  {
    SendCommand,
    WaitTransferTime,
    ReceiveResponse
  };

  GBASockServer m_sock_server;
  NextAction m_next_action = NextAction::SendCommand;
  u8 m_last_cmd;
  u64 m_timestamp_sent = 0;
};
}  // namespace SerialInterface
