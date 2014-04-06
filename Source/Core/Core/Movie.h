// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/ChunkFile.h"
#include "Common/Common.h"

#include "InputCommon/GCPadStatus.h"

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

// Gamecube Controller State
#pragma pack(push,1)
struct ControllerState
{
	bool Start:1, A:1, B:1, X:1, Y:1, Z:1; // Binary buttons, 6 bits
	bool DPadUp:1, DPadDown:1,             // Binary D-Pad buttons, 4 bits
	    DPadLeft:1, DPadRight:1;
	bool L:1, R:1;                         // Binary triggers, 2 bits
	bool disc:1;                           // Checks for disc being changed
	bool reserved:3;                       // Reserved bits used for padding, 4 bits

	u8   TriggerL, TriggerR;               // Triggers, 16 bits
	u8   AnalogStickX, AnalogStickY;       // Main Stick, 16 bits
	u8   CStickX, CStickY;                 // Sub-Stick, 16 bits

}; // Total: 60 + 4 = 64 bits per frame
static_assert(sizeof(ControllerState) == 8, "ControllerState should be 8 bytes");
#pragma pack(pop)

// Global declarations
extern bool g_bFrameStep, g_bPolled, g_bReadOnly, g_bDiscChange, g_bClearSave;
extern PlayMode g_playMode;
extern u64 g_titleID;

extern u32 g_framesToSkip, g_frameSkipCounter;

extern u8 g_numPads;
extern ControllerState *g_padStates;
extern char g_playingFile[256];
extern std::string g_recordFile;

extern u64 g_currentByte, g_totalBytes;
extern u64 g_currentFrame, g_totalFrames;
extern u64 g_currentLagCount, g_totalLagCount;
extern u64 g_currentInputCount, g_totalInputCount;
extern std::string g_discChange;

extern u32 g_rerecords;

#pragma pack(push,1)
struct DTMHeader
{
	u8 filetype[4];         // Unique Identifier (always "DTM"0x1A)

	u8 gameID[6];           // The Game ID
	bool bWii;              // Wii game

	u8  numControllers;     // The number of connected controllers (1-4)

	bool bFromSaveState;    // false indicates that the recording started from bootup, true for savestate
	u64 frameCount;         // Number of frames in the recording
	u64 inputCount;         // Number of input frames in recording
	u64 lagCount;           // Number of lag frames in the recording
	u64 uniqueID;           // (not implemented) A Unique ID comprised of: md5(time + Game ID)
	u32 numRerecords;       // Number of rerecords/'cuts' of this TAS
	u8  author[32];         // Author's name (encoded in UTF-8)

	u8  videoBackend[16];   // UTF-8 representation of the video backend
	u8  audioEmulator[16];  // UTF-8 representation of the audio emulator
	unsigned char  md5[16]; // MD5 of game iso

	u64 recordingStartTime; // seconds since 1970 that recording started (used for RTC)

	bool bSaveConfig;       // Loads the settings below on startup if true
	bool bSkipIdle;
	bool bDualCore;
	bool bProgressive;
	bool bDSPHLE;
	bool bFastDiscSpeed;
	u8  CPUCore;            // 0 = interpreter, 1 = JIT, 2 = JITIL
	bool bEFBAccessEnable;
	bool bEFBCopyEnable;
	bool bCopyEFBToTexture;
	bool bEFBCopyCacheEnable;
	bool bEFBEmulateFormatChanges;
	bool bUseXFB;
	bool bUseRealXFB;
	bool bMemcard;
	bool bClearSave;        // Create a new memory card when playing back a movie if true
	u8 bongos;
	bool bSyncGPU;
	bool bNetPlay;
	u8 reserved[13];        // Padding for any new config options
	u8 discChange[40];      // Name of iso file to switch to, for two disc games.
	u8 revision[20];        // Git hash
	u32 DSPiromHash;
	u32 DSPcoefHash;
	u8 reserved2[19];       // Make heading 256 bytes, just because we can
};
static_assert(sizeof(DTMHeader) == 256, "DTMHeader should be 256 bytes");

#pragma pack(pop)

void FrameUpdate();
void InputUpdate();
void Init();

void SetPolledDevice();

bool IsRecordingInput();
bool IsRecordingInputFromSaveState();
bool IsJustStartingRecordingInputFromSaveState();
bool IsJustStartingPlayingInputFromSaveState();
bool IsPlayingInput();
bool IsReadOnly();
u64 GetRecordingStartTime();

bool IsConfigSaved();
bool IsDualCore();
bool IsProgressive();
bool IsSkipIdle();
bool IsDSPHLE();
bool IsFastDiscSpeed();
int GetCPUMode();
bool IsStartingFromClearSave();
bool IsUsingMemcard();
bool IsSyncGPU();
void SetGraphicsConfig();
void GetSettings();
bool IsNetPlayRecording();

bool IsUsingPad(int controller);
bool IsUsingWiimote(int wiimote);
bool IsUsingBongo(int controller);
void ChangePads(bool instantly = false);
void ChangeWiiPads(bool instantly = false);

void DoFrameStep();
void SetFrameStopping(bool bEnabled);
void SetReadOnly(bool bEnabled);

void SetFrameSkipping(unsigned int framesToSkip);
void FrameSkipping();

bool BeginRecordingInput(int controllers);
void RecordInput(SPADStatus *PadStatus, int controllerID);
void RecordWiimote(int wiimote, u8 *data, u8 size);

bool PlayInput(const std::string& filename);
void LoadInput(const std::string& filename);
void ReadHeader();
void PlayController(SPADStatus *PadStatus, int controllerID);
bool PlayWiimote(int wiimote, u8* data, const struct WiimoteEmu::ReportFeatures& rptf, int irMode);
void EndPlayInput(bool cont);
void SaveRecording(const std::string& filename);
void DoState(PointerWrap &p);
void CheckMD5();
void GetMD5();
void Shutdown();
void CheckPadStatus(SPADStatus *PadStatus, int controllerID);
void CheckWiimoteStatus(int wiimote, u8* data, const struct WiimoteEmu::ReportFeatures& rptf, int irMode);

std::string GetInputDisplay();

// Done this way to avoid mixing of core and gui code
typedef void(*ManipFunction)(SPADStatus *, int);

void SetInputManip(ManipFunction);
void CallInputManip(SPADStatus *PadStatus, int controllerID);
};
