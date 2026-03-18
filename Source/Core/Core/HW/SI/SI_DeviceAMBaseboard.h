// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Core/HW/MagCard/MagneticCardReader.h"
#include "Core/HW/SI/SI.h"
#include "Core/HW/SI/SI_Device.h"
#include "Core/HW/Triforce/IOPorts.h"

namespace Triforce
{
class SerialDevice;
}

namespace SerialInterface
{

// "JAMMA Video Standard" I/O
class JVSIOMessage
{
public:
  void Start(int node);
  void AddData(const u8* dst, std::size_t len, int sync);
  void AddData(const void* data, std::size_t len);
  void AddData(const char* data);
  void AddData(u32 n);
  void End();

  u32 m_pointer = 0;
  std::array<u8, 0x80> m_message;

private:
  u32 m_last_start = 0;
  u32 m_checksum = 0;
};

// Triforce (GC-AM) baseboard
class CSIDevice_AMBaseboard : public ISIDevice
{
public:
  CSIDevice_AMBaseboard(Core::System& system, SIDevices device, int device_number);

  // run the SI Buffer
  int RunBuffer(u8* buffer, int request_length) override;

  DataResponse GetData(u32& hi, u32& low) override;

  void SendCommand(u32 command, u8 poll) override;

  void DoState(PointerWrap&) override;

private:
  enum GCAMCommand
  {
    StatusSwitches = 0x10,
    SerialNumber = 0x11,
    Unknown_12 = 0x12,
    Unknown_14 = 0x14,
    FirmVersion = 0x15,
    FPGAVersion = 0x16,
    RegionSettings = 0x1F,

    Unknown_21 = 0x21,
    Unknown_22 = 0x22,
    Unknown_23 = 0x23,
    Unknown_24 = 0x24,

    SerialA = 0x31,
    SerialB = 0x32,

    JVSIOA = 0x40,
    JVSIOB = 0x41,

    Unknown_60 = 0x60,
  };

  enum JVSIOCommand
  {
    IOID = 0x10,
    CommandRevision = 0x11,
    JVRevision = 0x12,
    CommunicationVersion = 0x13,
    CheckFunctionality = 0x14,
    MainID = 0x15,

    SwitchesInput = 0x20,
    CoinInput = 0x21,
    AnalogInput = 0x22,
    RotaryInput = 0x23,
    KeyCodeInput = 0x24,
    PositionInput = 0x25,
    GeneralSwitchInput = 0x26,

    PayoutRemain = 0x2E,
    Retrans = 0x2F,
    CoinSubOutput = 0x30,
    PayoutAddOutput = 0x31,
    GeneralDriverOutput = 0x32,
    AnalogOutput = 0x33,
    CharacterOutput = 0x34,
    CoinAddOutput = 0x35,
    PayoutSubOutput = 0x36,
    GeneralDriverOutput2 = 0x37,
    GeneralDriverOutput3 = 0x38,

    NAMCOCommand = 0x70,

    Reset = 0xF0,
    SetAddress = 0xF1,
    ChangeComm = 0xF2,
  };

  enum JVSIOStatusCode
  {
    StatusOkay = 1,
    UnsupportedCommand = 2,
    ChecksumError = 3,
    AcknowledgeOverflow = 4,
  };

  static constexpr u32 RESPONSE_SIZE = SerialInterfaceManager::BUFFER_SIZE;

  // This value prevents F-Zero AX mag card breakage.
  // It's now used for serial port reads in general.
  // TODO: Verify how the hardware actually works.
  static constexpr u32 SERIAL_PORT_MAX_READ_SIZE = 0x1f;

  // Reply has to be delayed due a bug in the parser
  std::array<std::array<u8, RESPONSE_SIZE>, 2> m_response_buffers{};
  u8 m_current_response_buffer_index = 0;

  Triforce::IOPorts m_io_ports;

  std::array<u16, 2> m_coin{};
  std::array<u32, 2> m_coin_pressed{};

  // Magnetic Card Reader
  MagCard::MagneticCardReader::Settings m_mag_card_settings;

  // Serial A
  std::unique_ptr<Triforce::SerialDevice> m_serial_device_a;

  // Serial B
  std::unique_ptr<Triforce::SerialDevice> m_serial_device_b;

  u32 m_wheel_init = 0;

  u32 m_motor_init = 0;
  u8 m_motor_reply[64] = {};
  s16 m_motor_force_y = 0;

  // F-Zero AX (DX)
  bool m_fzdx_seatbelt = true;
  bool m_fzdx_motion_stop = false;
  bool m_fzdx_sensor_right = false;
  bool m_fzdx_sensor_left = false;
  u8 m_rx_reply = 0xF0;

  // F-Zero AX (CyCraft)
  bool m_fzcc_seatbelt = true;
  bool m_fzcc_sensor = false;
  bool m_fzcc_emergency = false;
  bool m_fzcc_service = false;

  u32 m_dip_switch_1 = 0xFE;
  u32 m_dip_switch_0 = 0xFF;

  int m_delay = 0;
};

}  // namespace SerialInterface
