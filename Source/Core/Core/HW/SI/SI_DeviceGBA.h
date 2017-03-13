// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include <SFML/Network.hpp>

#include "Common/CommonTypes.h"
#include "Core/HW/SI/SI_Device.h"

// GameBoy Advance "Link Cable"

u8 GetNumConnected();
int GetTransferTime(u8 cmd);
void GBAConnectionWaiter_Shutdown();

class GBASockServer
{
public:
  GBASockServer(int _iDeviceNumber);
  ~GBASockServer();

  void Disconnect();

  void ClockSync();

  void Send(const u8* si_buffer);
  int Receive(u8* si_buffer);

private:
  std::unique_ptr<sf::TcpSocket> client;
  std::unique_ptr<sf::TcpSocket> clock_sync;
  char send_data[5] = {};
  char recv_data[5] = {};

  u64 time_cmd_sent = 0;
  u64 last_time_slice = 0;
  int device_number;
  u8 cmd = 0;
  bool booted = false;
};

class CSIDevice_GBA : public ISIDevice, private GBASockServer
{
public:
  CSIDevice_GBA(SIDevices device, int _iDeviceNumber);
  ~CSIDevice_GBA();

  int RunBuffer(u8* _pBuffer, int _iLength) override;
  int TransferInterval() override;

  bool GetData(u32& _Hi, u32& _Low) override { return false; }
  void SendCommand(u32 _Cmd, u8 _Poll) override {}
private:
  u8 send_data[5] = {};
  int num_data_received = 0;
  u64 timestamp_sent = 0;
  bool waiting_for_response = false;
};
