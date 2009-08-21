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
#include "pluginspecs_pad.h"

// Per-(video )Frame actions

namespace Frame {
	

enum PlayMode {
	MODE_NONE = 0,
	MODE_RECORDING,
	MODE_PLAYING
};

// Gamecube Controller State
typedef struct {
	bool Start, A, B, X, Y, Z;                  // Binary buttons, 6 bits
	bool DPadUp, DPadDown, DPadLeft, DPadRight; // Binary D-Pad buttons, 4 bits
	u8   L, R;                                  // Triggers, 16 bits
	u8   AnalogStickX, AnalogStickY;            // Main Stick, 16 bits
	u8   CStickX, CStickY;                      // Sub-Stick, 16 bits
	
	bool reserved1, reserved2;                  // Reserved bits, 2 bits
} ControllerState; // Total: 58 + 2 = 60 bits per frame

typedef struct {
    bool bWii;              // Wii game
    u8 gameID[6];           // The Game ID

    u8  numControllers;     // The number of connected controllers (1-4)

    bool bFromSaveState;    // false indicates that the recording started from bootup, true for savestate
    u64 frameCount;         // Number of frames in the recording
    u64 lagCount;           // Number of lag frames in the recording
    u64 uniqueID;           // A Unique ID comprised of: md5(time + Game ID)
    u32 numRerecords;       // Number of rerecords/'cuts' of this TAS
    u8  author[32];         // Author's name (encoded in UTF-8)
    
    u8  videoPlugin[16];    // UTF-8 representation of the video plugin
    u8  audioPlugin[16];    // UTF-8 representation of the audio plugin
    u8  padPlugin[16];      // UTF-8 representation of the input plugin
    

    bool padding[102];      // Padding to align the header to 1024 bits

    u8  reserved[128];      // Increasing size from 128 bytes to 256 bytes, just because we can
} DTMHeader;


void FrameUpdate();

void SetPolledDevice();

bool IsAutoFiring();
bool IsRecordingInput();
bool IsPlayingInput();

void SetAutoHold(bool bEnabled, u32 keyToHold = 0);
void SetAutoFire(bool bEnabled, u32 keyOne = 0, u32 keyTwo = 0);

void SetFrameStepping(bool bEnabled);

void SetFrameSkipping(unsigned int framesToSkip);
int FrameSkippingFactor();
void FrameSkipping();

void ModifyController(SPADStatus *PadStatus, int controllerID);

bool BeginRecordingInput(const char *filename, int controllers);
void RecordInput(SPADStatus *PadStatus, int controllerID);
void EndRecordingInput();

bool PlayInput(const char *filename);
void PlayController(SPADStatus *PadStatus, int controllerID);
void EndPlayInput();

};

#endif // __FRAME_H
