// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Core/HW/MagCard/MagneticCardReader.h"
#include "Core/HW/SI/SI.h"
#include "Core/HW/SI/SI_Device.h"
#include "Core/HW/Triforce/IOPorts.h"

namespace Triforce
{
class JVSIOBoard;
class SerialDevice;
}  // namespace Triforce

namespace SerialInterface
{

// Triforce (GC-AM) baseboard
class CSIDevice_AMBaseboard final : public ISIDevice
{
public:
  CSIDevice_AMBaseboard(Core::System& system, SIDevices device, int device_number);
  ~CSIDevice_AMBaseboard() override;

  int RunBuffer(u8* buffer, int request_length) override;

  DataResponse GetData(u32& hi, u32& low) override;

  void SendCommand(u32 command, u8 poll) override;

  void DoState(PointerWrap&) override;

private:
  static constexpr u32 RESPONSE_SIZE = SerialInterfaceManager::BUFFER_SIZE;

  // Reply has to be delayed due a bug in the parser
  std::array<std::array<u8, RESPONSE_SIZE>, 2> m_response_buffers{};
  u8 m_current_response_buffer_index = 0;

  Triforce::IOPorts m_io_ports;

  std::unique_ptr<Triforce::JVSIOBoard> m_jvs_io_board;

  // Magnetic Card Reader
  MagCard::MagneticCardReader::Settings m_mag_card_settings;

  // Serial A
  std::unique_ptr<Triforce::SerialDevice> m_serial_device_a;

  // Serial B
  std::unique_ptr<Triforce::SerialDevice> m_serial_device_b;
};

}  // namespace SerialInterface
