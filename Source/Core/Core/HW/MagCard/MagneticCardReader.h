// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Based on: https://github.com/GXTX/YACardEmu
// Copyright (C) 2020-2023 wutno (https://github.com/GXTX)
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"

namespace MagCard
{

class MagneticCardReader
{
public:
  static constexpr std::size_t TRACK_SIZE = 69;  // A nice amount of data.
  static constexpr std::size_t NUM_TRACKS = 3;

  // This data is not part of the card reader state.
  struct Settings
  {
    std::string card_name;
    std::string card_path;

    bool insert_card_when_possible = true;
    bool report_dispenser_empty = false;
  };

  explicit MagneticCardReader(Settings* settings);
  virtual ~MagneticCardReader();

  MagneticCardReader(const MagneticCardReader&) = delete;
  MagneticCardReader& operator=(const MagneticCardReader&) = delete;
  MagneticCardReader(MagneticCardReader&&) = delete;
  MagneticCardReader& operator=(MagneticCardReader&&) = delete;

  // TODO: This std:vector buffer interface is a bit funky..

  //  read = MagCard <-- Baseboard.
  // write = Magcard --> Baseboard.
  void Process(std::vector<u8>* read, std::vector<u8>* write);

  virtual void DoState(PointerWrap& p);

protected:
  // Status bytes:
  enum class R : u8
  {
    // Note: Derived classes define the real values in GetPositionValue().
    NO_CARD,
    READ_WRITE_HEAD,
    THERMAL_HEAD,
    DISPENSER_THERMAL,
    ENTRY_SLOT,

    MAX_POSITIONS,
  };
  enum class P : u8
  {
    NO_ERR = 0x30,
    READ_ERR = 0x31,
    WRITE_ERR = 0x32,
    CARD_JAM = 0x33,
    MOTOR_ERR = 0x34,  // transport system motor error
    PRINT_ERR = 0x35,
    ILLEGAL_ERR = 0x38,  // generic error
    BATTERY_ERR = 0x40,  // low battery voltage
    SYSTEM_ERR = 0x41,   // reader/writer system err
    TRACK_1_READ_ERR = 0x51,
    TRACK_2_READ_ERR = 0x52,
    TRACK_3_READ_ERR = 0x53,
    TRACK_1_AND_2_READ_ERR = 0x54,
    TRACK_1_AND_3_READ_ERR = 0x55,
    TRACK_2_AND_3_READ_ERR = 0x56,
  };
  enum class S : u8
  {
    NO_JOB = 0x30,
    ILLEGAL_COMMAND = 0x32,
    RUNNING_COMMAND = 0x33,
    WAITING_FOR_CARD = 0x34,
    DISPENSER_EMPTY = 0x35,
    NO_DISPENSER = 0x36,
    CARD_FULL = 0x37,
  };

  struct Status
  {
    R r = R::NO_CARD;
    P p = P::NO_ERR;
    S s = S::NO_JOB;

    void SoftReset()
    {
      p = P::NO_ERR;
      s = S::NO_JOB;
    }
  };

  enum class InitMode : u8
  {
    Standard = 0x30,
    EjectAfter = 0x31,
    ResetSpecifications = 0x32,
  };

  bool ReceivePacket(std::span<const u8> packet);
  void BuildPacket(std::vector<u8>& write_buffer);

  void StepStateMachine(DT elapsed_time);
  void StepStatePerson(DT elapsed_time);

  // This is what actually maps the "R" position value to a number.
  virtual u8 GetPositionValue() = 0;

  void SetPError(P error_code);
  void SetSError(S error_code);

  void ClearCardInMemory();

  // Read/Write with host filesystem.
  void ReadCardFile();
  void WriteCardFile();

  bool IsRunningCommand() const;
  void FinishCommand();

  u8 GetCurrentCommand() const;

  // Is a card anywhere the machine can sense it ?
  bool IsCardPresent() const;

  // Is a card beyond the "ENTRY_SLOT" position ?
  bool IsCardLoaded() const;

  virtual bool IsReadyForCard();

  // Move the card through the machine.
  void MoveCard(R new_position);

  // Unloads the card and moves it to the "ENTRY_SLOT" position.
  void EjectCard();

  // Feeds a new card into the machine from an internal hopper.
  void DispenseCard();

  // Commands
  virtual void Command_10_Initialize();
  void Command_20_ReadStatus();
  void Command_33_ReadData();  // Multi-track read.
  void Command_35_GetData();   // Read the entire card.
  void Command_40_Cancel();
  void Command_53_WriteData();  // Multi-track write.
  void Command_78_PrintSettings();
  void Command_7A_RegisterFont();
  void Command_7B_PrintImage();
  void Command_7C_PrintLine();
  void Command_7D_Erase();  // Erase the printed image.
  void Command_7E_PrintBarcode();
  void Command_80_EjectCard();
  void Command_A0_Clean();
  void Command_B0_DispenseCard();
  void Command_C0_ControlLED();
  void Command_C1_SetPrintRetry();
  virtual void Command_D0_ControlShutter();
  void Command_F0_GetVersion();

  Settings* const m_card_settings;

  // R P S values that are sent in every ENQUIRY response.
  Status m_status{};

  // A byte sent in every ENQUIRY response.
  // TODO: What's the default value on just powered up hardware ?
  u8 m_current_command = 0;

  // The data included in every ENQUIRY response.
  std::vector<u8> m_command_payload;

  // The card track data in memory.
  using TrackData = std::optional<std::array<u8, TRACK_SIZE>>;
  std::array<TrackData, NUM_TRACKS> m_card_data;

  // The parameter data of the command being processed.
  std::vector<u8> m_current_packet;

  // The progress state of the current command.
  // 0 == not running a command.
  // TODO: Change this to emulated milliseconds or something.
  s32 m_current_step = 0;

  // Used to delay re-insertion of cards after ejection.
  // This avoids having to expose card "Take"/"Insert" actions.
  DT m_time_since_machine_moved_card{};
  DT m_time_since_person_moved_card{};
};

}  // namespace MagCard
