// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <string>

#include "Common/CommonTypes.h"

struct BootParameters;

struct GCPadStatus;
class PointerWrap;
struct wiimote_key;

namespace WiimoteEmu
{
struct ReportFeatures;
}

// Per-(video )Movie actions

namespace Movie
{
// Enumerations and structs
enum PlayMode
{
  MODE_NONE = 0,
  MODE_RECORDING,
  MODE_PLAYING
};

// GameCube Controller State
#pragma pack(push, 1)
struct ControllerState
{
  bool Start : 1, A : 1, B : 1, X : 1, Y : 1, Z : 1;  // Binary buttons, 6 bits
  bool DPadUp : 1, DPadDown : 1,                      // Binary D-Pad buttons, 4 bits
      DPadLeft : 1, DPadRight : 1;
  bool L : 1, R : 1;  // Binary triggers, 2 bits
  bool disc : 1;      // Checks for disc being changed
  bool reset : 1;     // Console reset button
  bool reserved : 2;  // Reserved bits used for padding, 2 bits

  u8 TriggerL, TriggerR;          // Triggers, 16 bits
  u8 AnalogStickX, AnalogStickY;  // Main Stick, 16 bits
  u8 CStickX, CStickY;            // Sub-Stick, 16 bits
};
static_assert(sizeof(ControllerState) == 8, "ControllerState should be 8 bytes");
#pragma pack(pop)

#pragma pack(push, 1)
struct DTMHeader
{
  u8 filetype[4];  // Unique Identifier (always "DTM"0x1A)

  char gameID[6];  // The Game ID
  bool bWii;       // Wii game

  u8 controllers;  // Controllers plugged in (from least to most significant,
                   // the bits are GC controllers 1-4 and Wiimotes 1-4)

  bool
      bFromSaveState;  // false indicates that the recording started from bootup, true for savestate
  u64 frameCount;      // Number of frames in the recording
  u64 inputCount;      // Number of input frames in recording
  u64 lagCount;        // Number of lag frames in the recording
  u64 uniqueID;        // (not implemented) A Unique ID comprised of: md5(time + Game ID)
  u32 numRerecords;    // Number of rerecords/'cuts' of this TAS
  u8 author[32];       // Author's name (encoded in UTF-8)

  u8 videoBackend[16];   // UTF-8 representation of the video backend
  u8 audioEmulator[16];  // UTF-8 representation of the audio emulator
  u8 md5[16];            // MD5 of game iso

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
  bool bUseXFB;
  bool bUseRealXFB;
  u8 memcards;      // Memcards inserted (from least to most significant, the bits are slot A and B)
  bool bClearSave;  // Create a new memory card when playing back a movie if true
  u8 bongos;        // Bongos plugged in (from least to most significant, the bits are ports 1-4)
  bool bSyncGPU;
  bool bNetPlay;
  bool bPAL60;
  u8 language;
  u8 reserved[11];    // Padding for any new config options
  u8 discChange[40];  // Name of iso file to switch to, for two disc games.
  u8 revision[20];    // Git hash
  u32 DSPiromHash;
  u32 DSPcoefHash;
  u64 tickCount;     // Number of ticks in the recording
  u8 reserved2[11];  // Make heading 256 bytes, just because we can
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
bool IsUsingMemcard(int memcard);
void SetGraphicsConfig();
bool IsNetPlayRecording();

bool IsUsingPad(int controller);
bool IsUsingWiimote(int wiimote);
bool IsUsingBongo(int controller);
void ChangePads(bool instantly = false);
void ChangeWiiPads(bool instantly = false);

void DoFrameStep();
void SetReadOnly(bool bEnabled);

bool BeginRecordingInput(int controllers);
void RecordInput(GCPadStatus* PadStatus, int controllerID);
void RecordWiimote(int wiimote, u8* data, u8 size);

bool PlayInput(const std::string& filename);
void LoadInput(const std::string& filename);
void ReadHeader();
void PlayController(GCPadStatus* PadStatus, int controllerID);
bool PlayWiimote(int wiimote, u8* data, const struct WiimoteEmu::ReportFeatures& rptf, int ext,
                 const wiimote_key key);
void EndPlayInput(bool cont);
void SaveRecording(const std::string& filename);
void DoState(PointerWrap& p);
void Shutdown();
void CheckPadStatus(GCPadStatus* PadStatus, int controllerID);
void CheckWiimoteStatus(int wiimote, u8* data, const struct WiimoteEmu::ReportFeatures& rptf,
                        int ext, const wiimote_key key);

std::string GetInputDisplay();
std::string GetRTCDisplay();

// Done this way to avoid mixing of core and gui code
using GCManipFunction = std::function<void(GCPadStatus*, int)>;
using WiiManipFunction =
    std::function<void(u8*, WiimoteEmu::ReportFeatures, int, int, wiimote_key)>;

void SetGCInputManip(GCManipFunction);
void SetWiiInputManip(WiiManipFunction);
void CallGCInputManip(GCPadStatus* PadStatus, int controllerID);
void CallWiiInputManip(u8* core, WiimoteEmu::ReportFeatures rptf, int controllerID, int ext,
                       const wiimote_key key);
}
