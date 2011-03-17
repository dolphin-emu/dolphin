// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#ifndef __FRAME_H
#define __FRAME_H

#include "Common.h"
#include "FileUtil.h"
#include "../../InputCommon/Src/GCPadStatus.h"

#include <string>

// Per-(video )Frame actions

namespace Frame {

// Enumerations and structs
enum PlayMode {
	MODE_NONE = 0,
	MODE_RECORDING,
	MODE_PLAYING
};

// Gamecube Controller State
#pragma pack(push,1)
struct ControllerState {
	bool Start:1, A:1, B:1, X:1, Y:1, Z:1;		// Binary buttons, 6 bits
	bool DPadUp:1, DPadDown:1,					// Binary D-Pad buttons, 4 bits
		DPadLeft:1, DPadRight:1;
	bool L:1, R:1;					// Binary triggers, 2 bits
	bool reserved:4;							// Reserved bits used for padding, 4 bits

	u8   TriggerL, TriggerR;									// Triggers, 16 bits
	u8   AnalogStickX, AnalogStickY;			// Main Stick, 16 bits
	u8   CStickX, CStickY;						// Sub-Stick, 16 bits
	
}; // Total: 58 + 6 = 64 bits per frame
#pragma pack(pop)

// Global declarations
extern bool g_bFrameStep, g_bPolled, g_bReadOnly;
extern PlayMode g_playMode;

extern unsigned int g_framesToSkip, g_frameSkipCounter;

extern int g_numPads;
extern ControllerState *g_padStates;
extern char g_playingFile[256];
extern File::IOFile g_recordfd;
extern std::string g_recordFile;

extern u32 g_frameCounter, g_lagCounter, g_InputCounter;

extern int g_numRerecords;

#pragma pack(push,1)
struct DTMHeader {
	u8 filetype[4];			// Unique Identifier (always "DTM"0x1A)

	u8 gameID[6];			// The Game ID
	bool bWii;				// Wii game

    u8  numControllers;		// The number of connected controllers (1-4)

    bool bFromSaveState;	// false indicates that the recording started from bootup, true for savestate
    u64 frameCount;			// Number of frames in the recording
    u64 lagCount;			// Number of lag frames in the recording
    u64 uniqueID;			// A Unique ID comprised of: md5(time + Game ID)
    u32 numRerecords;		// Number of rerecords/'cuts' of this TAS
    u8  author[32];			// Author's name (encoded in UTF-8)
    
    u8  videoBackend[16];	// UTF-8 representation of the video backend
    u8  audioEmulator[16];	// UTF-8 representation of the audio emulator
    u8  padBackend[16];		// UTF-8 representation of the input backend

    u8	padding[7];		// Padding to align the header to 1024 bits

    u8  reserved[128];		// Increasing size from 128 bytes to 256 bytes, just because we can
};
#pragma pack(pop)

void FrameUpdate();
void InputUpdate();

void SetPolledDevice();

bool IsAutoFiring();
bool IsRecordingInput();
bool IsRecordingInputFromSaveState();
bool IsPlayingInput();

bool IsUsingPad(int controller);
bool IsUsingWiimote(int wiimote);
void ChangePads(bool instantly = false);
void ChangeWiiPads();

void SetFrameStepping(bool bEnabled);
void SetFrameStopping(bool bEnabled);
void SetReadOnly(bool bEnabled);

void SetFrameSkipping(unsigned int framesToSkip);
int FrameSkippingFactor();
void FrameSkipping();

bool BeginRecordingInput(int controllers);
void RecordInput(SPADStatus *PadStatus, int controllerID);
void RecordWiimote(int wiimote, u8* data, s8 size);

bool PlayInput(const char *filename);
void LoadInput(const char *filename);
void PlayController(SPADStatus *PadStatus, int controllerID);
bool PlayWiimote(int wiimote, u8* data, s8 &size);
void EndPlayInput(bool cont);
void SaveRecording(const char *filename);

std::string GetInputDisplay();
};

#endif // __FRAME_H
