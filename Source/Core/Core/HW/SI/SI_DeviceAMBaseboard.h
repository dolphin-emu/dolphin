// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Core/HW/GCPad.h"
#include "Core/HW/SI/SI_Device.h"
#include "InputCommon/GCPadStatus.h"

namespace Movie
{
class MovieManager;
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
  enum EButtonCombo
  {
    COMBO_NONE = 0,
    COMBO_ORIGIN,
    COMBO_RESET
  };

public:
  // constructor
  CSIDevice_AMBaseboard(Core::System& system, SIDevices device, int device_number);

  // run the SI Buffer
  int RunBuffer(u8* buffer, int request_length) override;

  // Reply has to be delayed due a bug in the parser
  void SwapBuffers(u8* buffer, u32* buffer_length);

  // return true on new data
  DataResponse GetData(u32& hi, u32& low) override;

  // send a command directly
  void SendCommand(u32 command, u8 poll) override;

  virtual GCPadStatus GetPadStatus();
  virtual u32 MapPadStatus(const GCPadStatus& pad_status);
  virtual EButtonCombo HandleButtonCombos(const GCPadStatus& pad_status);

  static void HandleMoviePadStatus(Movie::MovieManager& movie, int device_number,
                                   GCPadStatus* pad_status);

  // Send and Receive pad input from network
  static bool NetPlay_GetInput(int pad_num, GCPadStatus* status);
  static int NetPlay_InGamePadToLocalPad(int pad_num);

protected:
  struct SOrigin
  {
    u16 button;
    u8 origin_stick_x;
    u8 origin_stick_y;
    u8 substick_x;
    u8 substick_y;
    u8 trigger_left;
    u8 trigger_right;
    u8 unk_4;
    u8 unk_5;
  };

  // struct to compare input against
  // Set on connection to perfect neutral values
  // (standard pad only) Set on button combo to current input state
  SOrigin m_origin = {};

  // PADAnalogMode
  // Dunno if we need to do this, game/lib should set it?
  u8 m_mode = 0x3;

  // Timer to track special button combos:
  // y, X, start for 3 seconds updates origin with current status
  //   Technically, the above is only on standard pad, wavebird does not support it for example
  // b, x, start for 3 seconds triggers reset (PI reset button interrupt)
  u64 m_timer_button_combo_start = 0;
  // Type of button combo from the last/current poll
  EButtonCombo m_last_button_combo = COMBO_NONE;
  void SetOrigin(const GCPadStatus& pad_status);

private:
  enum BaseBoardCommand
  {
    GCAM_Reset = 0x00,
    GCAM_Command = 0x70,
  };

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

  enum CARDCommand
  {
    Init = 0x10,
    GetState = 0x20,
    Read = 0x33,
    IsPresent = 0x40,
    Write = 0x53,
    SetPrintParam = 0x78,
    RegisterFont = 0x7A,
    WriteInfo = 0x7C,
    Erase = 0x7D,
    Eject = 0x80,
    Clean = 0xA0,
    Load = 0xB0,
    SetShutter = 0xD0,
  };

  enum ICCARDCommand
  {
    GetStatus = 0x10,
    SetBaudrate = 0x11,
    FieldOn = 0x14,
    FieldOff = 0x15,
    InsertCheck = 0x20,
    AntiCollision = 0x21,
    SelectCard = 0x22,
    ReadPage = 0x24,
    WritePage = 0x25,
    DecreaseUseCount = 0x26,
    ReadUseCount = 0x33,
    ReadPages = 0x34,
    WritePages = 0x35,
  };

  enum ICCARDStatus
  {
    Okay = 0,
    NoCard = 0x8000,
    Unknown = 0x800E,
    BadCard = 0xFFFF,
  };

  enum CDReaderCommand
  {
    ShutterAuto = 0x61,
    BootVersion = 0x62,
    SensLock = 0x63,
    SensCard = 0x65,
    FirmwareUpdate = 0x66,
    ShutterGet = 0x67,
    CameraCheck = 0x68,
    ShutterCard = 0x69,
    ProgramChecksum = 0x6B,
    BootChecksum = 0x6D,
    ShutterLoad = 0x6F,
    ReadCard = 0x72,
    ShutterSave = 0x73,
    SelfTest = 0x74,
    ProgramVersion = 0x76,
  };

  union ICCommand
  {
    u8 data[81 + 4 + 4 + 4] = {};

    struct
    {
      u32 pktcmd : 8;
      u32 pktlen : 8;
      u32 fixed : 8;
      u32 command : 8;
      u32 flag : 8;
      u32 length : 8;
      u32 status : 16;

      u8 extdata[81];
      u32 extlen;
    };
  };

  u8 m_last[2][0x80] = {};
  u32 m_lastptr[2] = {};

  u16 m_coin[2] = {};
  u32 m_coin_pressed[2] = {};

  u8 m_ic_card_data[2048] = {};

  // Setup IC-card
  u16 m_ic_card_state = 0x20;
  u16 m_ic_card_status = ICCARDStatus::Okay;
  u16 m_ic_card_session = 0x23;

  u8 m_ic_write_buffer[512] = {};
  u32 m_ic_write_offset = 0;
  u32 m_ic_write_size = 0;

  u8 m_card_memory[0xD0] = {};
  u8 m_card_read_packet[0xDB] = {};
  u8 m_card_buffer[0x100] = {};

  // Setup CARD
  u32 m_card_memory_size = 0;
  bool m_card_is_inserted = false;

  u32 m_card_command = 0;
  u32 m_card_clean = 0;
  u32 m_card_write_length = 0;
  u32 m_card_wrote = 0;
  u32 m_card_read_length = 0;
  u32 m_card_read = 0;
  u32 m_card_bit = 0;
  bool m_card_shutter = true;  // Open
  u32 m_card_state_call_count = 0;
  u8 m_card_offset = 0;

  // Serial
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

  void ICCardSendReply(ICCommand* iccommand, u8* buffer, u32* length);
};

}  // namespace SerialInterface
