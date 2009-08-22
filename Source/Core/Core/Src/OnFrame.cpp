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

#include "OnFrame.h"

#include "Core.h"
#include "PluginManager.h"
#include "Thread.h"
#include "FileUtil.h"

Common::CriticalSection cs_frameSkip;

namespace Frame {

bool g_bFrameStep = false;
bool g_bAutoFire = false;
u32 g_autoFirstKey = 0, g_autoSecondKey = 0;
bool g_bFirstKey = true;
PlayMode g_playMode = MODE_NONE;

int g_framesToSkip = 0, g_frameSkipCounter = 0;

int g_numPads = 0;
ControllerState *g_padStates;
FILE *g_recordfd = NULL;

u64 g_frameCounter = 0, g_lagCounter = 0;
bool g_bPolled = false;

void FrameUpdate() {
	g_frameCounter++;

	if(!g_bPolled) 
		g_lagCounter++;

	if (g_bFrameStep)
		Core::SetState(Core::CORE_PAUSE);

	if(g_framesToSkip)
		FrameSkipping();

	if (g_bAutoFire)
		g_bFirstKey = !g_bFirstKey;
	
	// Dump/Read all controllers' states for this frame
	if(IsRecordingInput())
		fwrite(g_padStates, sizeof(ControllerState), g_numPads, g_recordfd); 
	else if(IsPlayingInput()) {
		fread(g_padStates, sizeof(ControllerState), g_numPads, g_recordfd); 	
			
		// End of recording
		if(feof(g_recordfd))
			EndPlayInput();
	}


	g_bPolled = false;
}

void SetFrameSkipping(unsigned int framesToSkip) {
	cs_frameSkip.Enter();

	g_framesToSkip = (int)framesToSkip;
	g_frameSkipCounter = 0;

	cs_frameSkip.Leave();
}

int FrameSkippingFactor() {
	return g_framesToSkip;
}

void SetPolledDevice() {
	g_bPolled = true;
}

void SetAutoHold(bool bEnabled, u32 keyToHold)
{
	g_bAutoFire = bEnabled;
	if (bEnabled)
		g_autoFirstKey = g_autoSecondKey = keyToHold;
	else
		g_autoFirstKey = g_autoSecondKey = 0;
}

void SetAutoFire(bool bEnabled, u32 keyOne, u32 keyTwo)
{
	g_bAutoFire = bEnabled;
	if (bEnabled) {
		g_autoFirstKey = keyOne;
		g_autoSecondKey = keyTwo;
	} else
		g_autoFirstKey = g_autoSecondKey = 0;

	g_bFirstKey = true;
}

bool IsAutoFiring() {
	return g_bAutoFire;
}

void SetFrameStepping(bool bEnabled) {
	g_bFrameStep = bEnabled;
}

void ModifyController(SPADStatus *PadStatus, int controllerID)
{
	if(controllerID < 0)
		return;

	u32 keyToPress = (g_bFirstKey) ? g_autoFirstKey : g_autoSecondKey;
	
	if (!keyToPress)
		return;
	
	PadStatus->button |= keyToPress;

	switch(keyToPress) {
		default:
			return;

		case PAD_BUTTON_A:
			PadStatus->analogA = 255;
			break;
		case PAD_BUTTON_B:
			PadStatus->analogB = 255;
			break;
		
		case PAD_TRIGGER_L:
			PadStatus->triggerLeft = 255;
			break;
		case PAD_TRIGGER_R:
			PadStatus->triggerRight = 255;
			break;
	}

}

void FrameSkipping()
{
	cs_frameSkip.Enter();

	g_frameSkipCounter++;
	if (g_frameSkipCounter > g_framesToSkip || Core::report_slow(g_frameSkipCounter) == false)
		g_frameSkipCounter = 0;

	CPluginManager::GetInstance().GetVideo()->Video_SetRendering(!g_frameSkipCounter);

	cs_frameSkip.Leave();
}

bool IsRecordingInput()
{
	return (g_playMode == MODE_RECORDING);
}

bool IsPlayingInput()
{
	return (g_playMode == MODE_PLAYING);
}

// TODO: Add BeginRecordingFromSavestate
bool BeginRecordingInput(const char *filename, int controllers)
{
	if(!filename || g_playMode != MODE_NONE || g_recordfd)
		return false;

	if(File::Exists(filename))
		File::Delete(filename);

	g_recordfd = fopen(filename, "wb");
	if(!g_recordfd) {
		PanicAlert("Error opening file %s for recording", filename);
		return false;
	}

	// Write initial empty header
	DTMHeader dummy;
	fwrite(&dummy, sizeof(DTMHeader), 1, g_recordfd);

	g_numPads = controllers;
	g_padStates = new ControllerState[controllers];

	g_frameCounter = 0;
	g_lagCounter = 0;

	g_playMode = MODE_RECORDING;

	return true;
}

void EndRecordingInput()
{
	rewind(g_recordfd);

	// Create the real header now and write it
	DTMHeader header;
	memset(&header, 0, sizeof(DTMHeader));

	header.filetype[0] = 'D'; header.filetype[1] = 'T'; header.filetype[2] = 'M'; header.filetype[3] = 0x1A;
	strncpy((char *)header.gameID, Core::g_CoreStartupParameter.GetUniqueID().c_str(), 6);
	header.bWii = Core::g_CoreStartupParameter.bWii;
    header.numControllers = g_numPads;

    header.bFromSaveState = false; // TODO: add the case where it's true
	header.frameCount = g_frameCounter;
	header.lagCount = g_lagCounter; 

    // TODO
    header.uniqueID = 0; 
    header.numRerecords = 0;
    header.author;
    header.videoPlugin; 
    header.audioPlugin;
    header.padPlugin;
    

	fwrite(&header, sizeof(DTMHeader), 1, g_recordfd);

	fclose(g_recordfd);
	g_recordfd = NULL;

	delete[] g_padStates;

	g_playMode = MODE_NONE;
}

void RecordInput(SPADStatus *PadStatus, int controllerID)
{
	if(!IsRecordingInput() || controllerID >= g_numPads || controllerID < 0)
		return;
	
	g_padStates[controllerID].A     = ((PadStatus->button & PAD_BUTTON_A) != 0);
	g_padStates[controllerID].B     = ((PadStatus->button & PAD_BUTTON_B) != 0);
	g_padStates[controllerID].X     = ((PadStatus->button & PAD_BUTTON_X) != 0);
	g_padStates[controllerID].Y     = ((PadStatus->button & PAD_BUTTON_Y) != 0);
	g_padStates[controllerID].Z     = ((PadStatus->button & PAD_TRIGGER_Z) != 0);
	g_padStates[controllerID].Start = ((PadStatus->button & PAD_BUTTON_START) != 0);

	g_padStates[controllerID].DPadUp    = ((PadStatus->button & PAD_BUTTON_UP) != 0);
	g_padStates[controllerID].DPadDown  = ((PadStatus->button & PAD_BUTTON_DOWN) != 0);
	g_padStates[controllerID].DPadLeft  = ((PadStatus->button & PAD_BUTTON_LEFT) != 0);
	g_padStates[controllerID].DPadRight = ((PadStatus->button & PAD_BUTTON_RIGHT) != 0);

	g_padStates[controllerID].L = PadStatus->triggerLeft;
	g_padStates[controllerID].R = PadStatus->triggerRight;

	g_padStates[controllerID].AnalogStickX = PadStatus->stickX;
	g_padStates[controllerID].AnalogStickY = PadStatus->stickY;

	g_padStates[controllerID].CStickX = PadStatus->substickX;
	g_padStates[controllerID].CStickY = PadStatus->substickY;

	PlayController(PadStatus, controllerID);
}

bool PlayInput(const char *filename)
{
	if(!filename || g_playMode != MODE_NONE || g_recordfd)
		return false;

	if(!File::Exists(filename))
		return false;

	DTMHeader header;

	g_recordfd = fopen(filename, "rb");
	if(!g_recordfd)
		return false;

	fread(&header, sizeof(DTMHeader), 1, g_recordfd);

	if(header.filetype[0] != 'D' || header.filetype[1] != 'T' || header.filetype[2] != 'M' || header.filetype[3] != 0x1A) {
		PanicAlert("Invalid recording file");
		goto cleanup;
	}

	// Load savestate (and skip to frame data)
	if(header.bFromSaveState) {
		// TODO
	}
	
	/* TODO: Put this verification somewhere we have the gameID of the played game
	// TODO: Replace with Unique ID
	if(header.uniqueID != 0) {
		PanicAlert("Recording Unique ID Verification Failed");
		goto cleanup;
	}

	if(strncmp((char *)header.gameID, Core::g_CoreStartupParameter.GetUniqueID().c_str(), 6)) {
		PanicAlert("The recorded game (%s) is not the same as the selected game (%s)", header.gameID, Core::g_CoreStartupParameter.GetUniqueID().c_str());
		goto cleanup;
	}
	*/

	g_numPads = header.numControllers;
	g_padStates = new ControllerState[g_numPads];

	g_playMode = MODE_PLAYING;

	return true;

cleanup:
	fclose(g_recordfd);
	g_recordfd = NULL;
	return false;
}

void PlayController(SPADStatus *PadStatus, int controllerID)
{
	if(!IsPlayingInput() || controllerID >= g_numPads || controllerID < 0)
		return;

	memset(PadStatus, 0, sizeof(SPADStatus));

	PadStatus->button |= PAD_USE_ORIGIN;

	if(g_padStates[controllerID].A) {
		PadStatus->button |= PAD_BUTTON_A;
		PadStatus->analogA = 0xFF;
	}
	if(g_padStates[controllerID].B) {
		PadStatus->button |= PAD_BUTTON_B;
		PadStatus->analogB = 0xFF;
	}
	if(g_padStates[controllerID].X)
		PadStatus->button |= PAD_BUTTON_X;
	if(g_padStates[controllerID].Y)
		PadStatus->button |= PAD_BUTTON_Y;
	if(g_padStates[controllerID].Z)
		PadStatus->button |= PAD_TRIGGER_Z;
	if(g_padStates[controllerID].Start)
		PadStatus->button |= PAD_BUTTON_START;

	if(g_padStates[controllerID].DPadUp)
		PadStatus->button |= PAD_BUTTON_UP;
	if(g_padStates[controllerID].DPadDown)
		PadStatus->button |= PAD_BUTTON_DOWN;
	if(g_padStates[controllerID].DPadLeft)
		PadStatus->button |= PAD_BUTTON_LEFT;
	if(g_padStates[controllerID].DPadRight)
		PadStatus->button |= PAD_BUTTON_RIGHT;

	PadStatus->triggerLeft = g_padStates[controllerID].L;
	if(PadStatus->triggerLeft > 230)
		PadStatus->button |= PAD_TRIGGER_L;
	PadStatus->triggerRight = g_padStates[controllerID].R;
	if(PadStatus->triggerRight > 230)
		PadStatus->button |= PAD_TRIGGER_R;

	PadStatus->stickX = g_padStates[controllerID].AnalogStickX;
	PadStatus->stickY = g_padStates[controllerID].AnalogStickY;

	PadStatus->substickX = g_padStates[controllerID].CStickX;
	PadStatus->substickY = g_padStates[controllerID].CStickY;
}

void EndPlayInput() {
	fclose(g_recordfd);
	g_recordfd = NULL;
	g_numPads = 0;
	delete[] g_padStates;
	g_playMode = MODE_NONE;
}

};
