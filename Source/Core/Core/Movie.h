// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>

#include "Common/CommonTypes.h"

struct BootParameters;

struct GCPadStatus;
class PointerWrap;

namespace ExpansionInterface
{
enum class Slot : int;
}

namespace WiimoteCommon
{
class DataReportBuilder;
}

namespace WiimoteEmu
{
class EncryptionKey;
enum ExtensionNumber : u8;
}  // namespace WiimoteEmu

// Per-(video )Movie actions

namespace Movie
{
// Enumerations and structs
enum class ControllerType
{
  None = 0,
  GC,
  GBA,
};
using ControllerTypeArray = std::array<ControllerType, 4>;
using WiimoteEnabledArray = std::array<bool, 4>;

// GameCube Controller State
#pragma pack(push, 1)
struct ControllerState
{
  bool Start : 1, A : 1, B : 1, X : 1, Y : 1, Z : 1;  // Binary buttons, 6 bits
  bool DPadUp : 1, DPadDown : 1,                      // Binary D-Pad buttons, 4 bits
      DPadLeft : 1, DPadRight : 1;
  bool L : 1, R : 1;      // Binary triggers, 2 bits
  bool disc : 1;          // Checks for disc being changed
  bool reset : 1;         // Console reset button
  bool is_connected : 1;  // Should controller be treated as connected
  bool get_origin : 1;    // Special bit to indicate analog origin reset

  u8 TriggerL, TriggerR;          // Triggers, 16 bits
  u8 AnalogStickX, AnalogStickY;  // Main Stick, 16 bits
  u8 CStickX, CStickY;            // Sub-Stick, 16 bits
};
static_assert(sizeof(ControllerState) == 8, "ControllerState should be 8 bytes");
#pragma pack(pop)

// When making changes to the DTM format, keep in mind that there are programs other
// than Dolphin that parse DTM files. The format is expected to be relatively stable.
#pragma pack(push, 1)
struct DTMHeader
{
  std::string_view GetGameID() const
  {
    return {gameID.data(), strnlen(gameID.data(), gameID.size())};
  }

  std::array<u8, 4> filetype;  // Unique Identifier (always "DTM"0x1A)

  std::array<char, 6> gameID;  // The Game ID
  bool bWii;                   // Wii game

  u8 controllers;  // Controllers plugged in (from least to most significant,
                   // the bits are GC controllers 1-4 and Wiimotes 1-4)

  bool
      bFromSaveState;  // false indicates that the recording started from bootup, true for savestate
  u64 frameCount;      // Number of frames in the recording
  u64 inputCount;      // Number of input frames in recording
  u64 lagCount;        // Number of lag frames in the recording
  u64 uniqueID;        // (not implemented) A Unique ID comprised of: md5(time + Game ID)
  u32 numRerecords;    // Number of rerecords/'cuts' of this TAS
  std::array<char, 32> author;  // Author's name (encoded in UTF-8)

  std::array<char, 16> videoBackend;   // UTF-8 representation of the video backend
  std::array<char, 16> audioEmulator;  // UTF-8 representation of the audio emulator
  std::array<u8, 16> md5;              // MD5 of game iso

  u64 recordingStartTime;  // seconds since 1970 that recording started (used for RTC)

  bool bSaveConfig;  // Loads the settings below on startup if true
  bool bSkipIdle;
  bool bDualCore;
  bool bProgressive;
  bool bDSPHLE;
  bool bFastDiscSpeed;
  u8 CPUCore;  // Uses the values of PowerPC::CPUCore
  bool bEFBAccessEnable;
  bool bEFBCopyEnable;
  bool bSkipEFBCopyToRam;
  bool bEFBCopyCacheEnable;
  bool bEFBEmulateFormatChanges;
  bool bImmediateXFB;
  bool bSkipXFBCopyToRam;
  u8 memcards;      // Memcards inserted (from least to most significant, the bits are slot A and B)
  bool bClearSave;  // Create a new memory card when playing back a movie if true
  u8 bongos;        // Bongos plugged in (from least to most significant, the bits are ports 1-4)
  bool bSyncGPU;
  bool bNetPlay;
  bool bPAL60;
  u8 language;
  u8 reserved3;
  bool bFollowBranch;
  bool bUseFMA;
  u8 GBAControllers;                // GBA Controllers plugged in (the bits are ports 1-4)
  std::array<u8, 7> reserved;       // Padding for any new config options
  std::array<char, 40> discChange;  // Name of iso file to switch to, for two disc games.
  std::array<u8, 20> revision;      // Git hash
  u32 DSPiromHash;
  u32 DSPcoefHash;
  u64 tickCount;                 // Number of ticks in the recording
  std::array<u8, 11> reserved2;  // Make heading 256 bytes, just because we can
};
static_assert(sizeof(DTMHeader) == 256, "DTMHeader should be 256 bytes");

#pragma pack(pop)

void FrameUpdate();
void InputUpdate();
void Init(const BootParameters& boot);

void SetPolledDevice();

bool IsRecordingInput();
bool IsRecordingInputFromSaveState();
bool IsJustStartingRecordingInputFromSaveState();
bool IsJustStartingPlayingInputFromSaveState();
bool IsPlayingInput();
bool IsMovieActive();
bool IsReadOnly();
u64 GetRecordingStartTime();

u64 GetCurrentFrame();
u64 GetTotalFrames();
u64 GetCurrentInputCount();
u64 GetTotalInputCount();
u64 GetCurrentLagCount();
u64 GetTotalLagCount();

void SetClearSave(bool enabled);
void SignalDiscChange(const std::string& new_path);
void SetReset(bool reset);

bool IsConfigSaved();
bool IsStartingFromClearSave();
bool IsUsingMemcard(ExpansionInterface::Slot slot);
void SetGraphicsConfig();
bool IsNetPlayRecording();

bool IsUsingPad(int controller);
bool IsUsingWiimote(int wiimote);
bool IsUsingBongo(int controller);
bool IsUsingGBA(int controller);
void ChangePads();
void ChangeWiiPads(bool instantly = false);

void SetReadOnly(bool bEnabled);

bool BeginRecordingInput(const ControllerTypeArray& controllers,
                         const WiimoteEnabledArray& wiimotes);
void RecordInput(const GCPadStatus* PadStatus, int controllerID);
void RecordWiimote(int wiimote, const u8* data, u8 size);

bool PlayInput(const std::string& movie_path, std::optional<std::string>* savestate_path);
void LoadInput(const std::string& movie_path);
void ReadHeader();
void PlayController(GCPadStatus* PadStatus, int controllerID);
bool PlayWiimote(int wiimote, WiimoteCommon::DataReportBuilder& rpt,
                 WiimoteEmu::ExtensionNumber ext, const WiimoteEmu::EncryptionKey& key);
void EndPlayInput(bool cont);
void SaveRecording(const std::string& filename);
void DoState(PointerWrap& p);
void Shutdown();
void CheckPadStatus(const GCPadStatus* PadStatus, int controllerID);
void CheckWiimoteStatus(int wiimote, const WiimoteCommon::DataReportBuilder& rpt,
                        WiimoteEmu::ExtensionNumber ext, const WiimoteEmu::EncryptionKey& key);

std::string GetInputDisplay();
std::string GetRTCDisplay();
std::string GetRerecords();

}  // namespace Movie
