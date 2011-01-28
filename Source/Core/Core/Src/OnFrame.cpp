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
#include "PowerPC/PowerPC.h"
#include "HW/SI.h"

Common::CriticalSection cs_frameSkip;

namespace Frame {

bool g_bFrameStep = false;
bool g_bFrameStop = false;
u32 g_rerecords = 0;
bool g_bFirstKey = true;
PlayMode g_playMode = MODE_NONE;

unsigned int g_framesToSkip = 0, g_frameSkipCounter = 0;

int g_numPads = 0;
ControllerState g_padState;
FILE *g_recordfd = NULL;

u64 g_frameCounter = 0, g_lagCounter = 0;
bool g_bPolled = false;

int g_numRerecords = 0;
std::string g_recordFile = "0.dtm";

void FrameUpdate()
{
	g_frameCounter++;

	if(!g_bPolled) 
		g_lagCounter++;
	
	if (g_bFrameStep)
		Core::SetState(Core::CORE_PAUSE);
	
	// ("framestop") the only purpose of this is to cause interpreter/jit Run() to return temporarily.
	// after that we set it back to CPU_RUNNING and continue as normal.
	if (g_bFrameStop)
		*PowerPC::GetStatePtr() = PowerPC::CPU_STEPPING;

	if(g_framesToSkip)
		FrameSkipping();
	
	g_bPolled = false;
}

void SetFrameSkipping(unsigned int framesToSkip)
{
	cs_frameSkip.Enter();
	
	g_framesToSkip = framesToSkip;
	g_frameSkipCounter = 0;
	
	// Don't forget to re-enable rendering in case it wasn't...
	// as this won't be changed anymore when frameskip is turned off
	if (framesToSkip == 0)
		CPluginManager::GetInstance().GetVideo()->Video_SetRendering(true);
	
	cs_frameSkip.Leave();
}

int FrameSkippingFactor()
{
	return g_framesToSkip;
}

void SetPolledDevice()
{
	g_bPolled = true;
}

void SetFrameStepping(bool bEnabled)
{
	g_bFrameStep = bEnabled;
}

void SetFrameStopping(bool bEnabled)
{
	g_bFrameStop = bEnabled;
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

bool IsUsingPad(int controller)
{
	switch (controller)
	{
	case 0:
		return (g_numPads & 0x01) != 0;
	case 1:
		return (g_numPads & 0x02) != 0;
	case 2:
		return (g_numPads & 0x04) != 0;
	case 3:
		return (g_numPads & 0x08) != 0;
	default:
		return false;
	}
}

void ChangePads()
{
	if (Core::GetState() != Core::CORE_UNINITIALIZED) {
		SerialInterface::ChangeDevice(IsUsingPad(0) ? SI_GC_CONTROLLER : SI_NONE, 0);
		SerialInterface::ChangeDevice(IsUsingPad(1) ? SI_GC_CONTROLLER : SI_NONE, 1);
		SerialInterface::ChangeDevice(IsUsingPad(2) ? SI_GC_CONTROLLER : SI_NONE, 2);
		SerialInterface::ChangeDevice(IsUsingPad(3) ? SI_GC_CONTROLLER : SI_NONE, 3);
	}
}

// TODO: Add BeginRecordingFromSavestate
bool BeginRecordingInput(int controllers)
{
	if(g_playMode != MODE_NONE || controllers == 0 || g_recordfd != NULL)
		return false;
	
	const char *filename = g_recordFile.c_str();
	
	if(File::Exists(filename))
		File::Delete(filename);
	
	g_recordfd = fopen(filename, "wb");
	if(!g_recordfd) {
		PanicAlertT("Error opening file %s for recording", filename);
		return false;
	}
	
	// Write initial empty header
	DTMHeader dummy;
	fwrite(&dummy, sizeof(DTMHeader), 1, g_recordfd);
	
	g_numPads = controllers;
	
	g_frameCounter = 0;
	g_lagCounter = 0;
	
	g_playMode = MODE_RECORDING;
	
	Core::DisplayMessage("Starting movie recording", 2000);
	
	return true;
}

void RecordInput(SPADStatus *PadStatus, int controllerID)
{
	if(!IsRecordingInput() || !IsUsingPad(controllerID))
		return;

	g_padState.A		 = ((PadStatus->button & PAD_BUTTON_A) != 0);
	g_padState.B		 = ((PadStatus->button & PAD_BUTTON_B) != 0);
	g_padState.X		 = ((PadStatus->button & PAD_BUTTON_X) != 0);
	g_padState.Y		 = ((PadStatus->button & PAD_BUTTON_Y) != 0);
	g_padState.Z		 = ((PadStatus->button & PAD_TRIGGER_Z) != 0);
	g_padState.Start     = ((PadStatus->button & PAD_BUTTON_START) != 0);
	
	g_padState.DPadUp	 = ((PadStatus->button & PAD_BUTTON_UP) != 0);
	g_padState.DPadDown  = ((PadStatus->button & PAD_BUTTON_DOWN) != 0);
	g_padState.DPadLeft  = ((PadStatus->button & PAD_BUTTON_LEFT) != 0);
	g_padState.DPadRight = ((PadStatus->button & PAD_BUTTON_RIGHT) != 0);
	
	g_padState.L = PadStatus->triggerLeft;
	g_padState.R = PadStatus->triggerRight;
	
	g_padState.AnalogStickX = PadStatus->stickX;
	g_padState.AnalogStickY = PadStatus->stickY;
	
	g_padState.CStickX = PadStatus->substickX;
	g_padState.CStickY = PadStatus->substickY;
	
	fwrite(&g_padState, sizeof(ControllerState), 1, g_recordfd);
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
		PanicAlertT("Invalid recording file");
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
	g_numRerecords = header.numRerecords;
	
	ChangePads();
	
	g_playMode = MODE_PLAYING;
	
	return true;
	
cleanup:
	fclose(g_recordfd);
	g_recordfd = NULL;
	return false;
}

void LoadInput(const char *filename)
{
	FILE *t_record = fopen(filename, "rb");
	
	DTMHeader header;
	
	fread(&header, sizeof(DTMHeader), 1, t_record);
	fclose(t_record);
	
	if(header.filetype[0] != 'D' || header.filetype[1] != 'T' || header.filetype[2] != 'M' || header.filetype[3] != 0x1A) {
		PanicAlertT("Savestate movie %s is corrupted, movie recording stopping...", filename);
		EndPlayInput();
		return;
	}
	
	if (g_rerecords == 0)
		g_rerecords = header.numRerecords;
	
	g_numPads = header.numControllers;
	
	ChangePads();
	
	if (g_recordfd)
		fclose(g_recordfd);
	
	File::Delete(g_recordFile.c_str());
	File::Copy(filename, g_recordFile.c_str());
	
	g_recordfd = fopen(g_recordFile.c_str(), "r+b");
	fseeko(g_recordfd, 0, SEEK_END);
	
	g_rerecords++;
	
	Core::DisplayMessage("Resuming movie recording", 2000);
	
	g_playMode = MODE_RECORDING;
}

void PlayController(SPADStatus *PadStatus, int controllerID)
{
	// Correct playback is entirely dependent on the emulator polling the controllers
	// in the same order done during recording
	if(!IsPlayingInput() || !IsUsingPad(controllerID))
		return;
	
	memset(PadStatus, 0, sizeof(SPADStatus));
	fread(&g_padState, sizeof(ControllerState), 1, g_recordfd);
	
	PadStatus->button |= PAD_USE_ORIGIN;
	
	if(g_padState.A) {
		PadStatus->button |= PAD_BUTTON_A;
		PadStatus->analogA = 0xFF;
	}
	if(g_padState.B) {
		PadStatus->button |= PAD_BUTTON_B;
		PadStatus->analogB = 0xFF;
	}
	if(g_padState.X)
		PadStatus->button |= PAD_BUTTON_X;
	if(g_padState.Y)
		PadStatus->button |= PAD_BUTTON_Y;
	if(g_padState.Z)
		PadStatus->button |= PAD_TRIGGER_Z;
	if(g_padState.Start)
		PadStatus->button |= PAD_BUTTON_START;
	
	if(g_padState.DPadUp)
		PadStatus->button |= PAD_BUTTON_UP;
	if(g_padState.DPadDown)
		PadStatus->button |= PAD_BUTTON_DOWN;
	if(g_padState.DPadLeft)
		PadStatus->button |= PAD_BUTTON_LEFT;
	if(g_padState.DPadRight)
		PadStatus->button |= PAD_BUTTON_RIGHT;
	
	PadStatus->triggerLeft = g_padState.L;
	if(PadStatus->triggerLeft > 230)
		PadStatus->button |= PAD_TRIGGER_L;
	PadStatus->triggerRight = g_padState.R;
	if(PadStatus->triggerRight > 230)
		PadStatus->button |= PAD_TRIGGER_R;
	
	PadStatus->stickX = g_padState.AnalogStickX;
	PadStatus->stickY = g_padState.AnalogStickY;
	
	PadStatus->substickX = g_padState.CStickX;
	PadStatus->substickY = g_padState.CStickY;
	
	if(feof(g_recordfd))
	{
		Core::DisplayMessage("Movie End", 2000);
		// TODO: read-only mode
		//EndPlayInput();
		g_playMode = MODE_RECORDING;
	}
}

void EndPlayInput() {
	if (g_recordfd)
		fclose(g_recordfd);
	g_recordfd = NULL;
	g_numPads = g_rerecords = 0;
	g_frameCounter = g_lagCounter = 0;
	g_playMode = MODE_NONE;
}

void SaveRecording(const char *filename)
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
	header.numRerecords = g_rerecords;
	
	// TODO
	header.uniqueID = 0; 
	// header.author;
	// header.videoPlugin; 
	// header.audioPlugin;
	
	fwrite(&header, sizeof(DTMHeader), 1, g_recordfd);
	fclose(g_recordfd);
	
	if (File::Copy(g_recordFile.c_str(), filename))
		Core::DisplayMessage(StringFromFormat("DTM %s saved", filename).c_str(), 2000);
	else
		Core::DisplayMessage(StringFromFormat("Failed to save %s", filename).c_str(), 2000);
	
	g_recordfd = fopen(g_recordFile.c_str(), "r+b");
	fseeko(g_recordfd, 0, SEEK_END);
}
};
