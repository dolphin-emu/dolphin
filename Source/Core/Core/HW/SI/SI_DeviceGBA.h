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
u8 GetNumConnected();
int GetTransferTime(u8 cmd);
void GBAConnectionWaiter_Shutdown();

class GBASockServer
{
public:
  explicit GBASockServer(int device_number);
  ~GBASockServer();

  void Disconnect();

  void ClockSync();

  void Send(const u8* si_buffer);
  int Receive(u8* si_buffer);

private:
  std::unique_ptr<sf::TcpSocket> m_client;
  std::unique_ptr<sf::TcpSocket> m_clock_sync;
  std::array<char, 5> m_send_data{};
  std::array<char, 5> m_recv_data{};

  u64 m_time_cmd_sent = 0;
  u64 m_last_time_slice = 0;
  int m_device_number;
  u8 m_cmd = 0;
  bool m_booted = false;
};

class CSIDevice_GBA : public ISIDevice, private GBASockServer
{
public:
  CSIDevice_GBA(SIDevices device, int device_number);
  ~CSIDevice_GBA();

  int RunBuffer(u8* buffer, int length) override;
  int TransferInterval() override;
  bool GetData(u32& hi, u32& low) override;
  void SendCommand(u32 command, u8 poll) override;

private:
  std::array<u8, 5> m_send_data{};
  int m_num_data_received = 0;
  u64 m_timestamp_sent = 0;
  bool m_waiting_for_response = false;
};
}  // namespace SerialInterface
