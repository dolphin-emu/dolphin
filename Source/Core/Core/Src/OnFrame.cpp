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
#include "Thread.h"
#include "FileUtil.h"
#include "PowerPC/PowerPC.h"
#include "HW/SI.h"
#include "HW/Wiimote.h"
#include "IPC_HLE/WII_IPC_HLE_Device_usb.h"
#include "VideoBackendBase.h"

#ifdef WIN32
#include <io.h> //_chsize_s
#include <fcntl.h>
#include <share.h>
#include <sys/stat.h>
#else
#include <unistd.h> //truncate
#endif
#include "State.h"

std::mutex cs_frameSkip;

namespace Frame {

bool g_bFrameStep = false;
bool g_bFrameStop = false;
bool g_bReadOnly = true;
u32 g_rerecords = 0;
PlayMode g_playMode = MODE_NONE;

unsigned int g_framesToSkip = 0, g_frameSkipCounter = 0;

int g_numPads = 0;
ControllerState g_padState;
FILE *g_recordfd = NULL;

u64 g_frameCounter = 0, g_lagCounter = 0, g_totalFrameCount = 0;
bool g_bRecordingFromSaveState = false;
bool g_bPolled = false;

int g_numRerecords = 0;
std::string g_recordFile = "0.dtm";
std::string g_tmpRecordFile = "1.dtm";

std::string g_InputDisplay[4];

std::string GetInputDisplay()
{
	std::string inputDisplay = "";
	for (int i = 0; i < 4; ++i)
		inputDisplay.append(g_InputDisplay[i]);
	return inputDisplay; 
}

void FrameUpdate()
{
	g_frameCounter++;
	if (IsRecordingInput())
		g_totalFrameCount = g_frameCounter;

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
	std::lock_guard<std::mutex> lk(cs_frameSkip);
	
	g_framesToSkip = framesToSkip;
	g_frameSkipCounter = 0;
	
	// Don't forget to re-enable rendering in case it wasn't...
	// as this won't be changed anymore when frameskip is turned off
	if (framesToSkip == 0)
		g_video_backend->Video_SetRendering(true);
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

void SetReadOnly(bool bEnabled)
{
	g_bReadOnly = bEnabled;
}

void FrameSkipping()
{
	// Frameskipping will desync movie playback
	if (!IsPlayingInput() && !IsRecordingInput())
	{
		std::lock_guard<std::mutex> lk(cs_frameSkip);

		g_frameSkipCounter++;
		if (g_frameSkipCounter > g_framesToSkip || Core::report_slow(g_frameSkipCounter) == false)
			g_frameSkipCounter = 0;
		
		g_video_backend->Video_SetRendering(!g_frameSkipCounter);
	}
}

bool IsRecordingInput()
{
	return (g_playMode == MODE_RECORDING);
}

bool IsRecordingInputFromSaveState()
{
	return g_bRecordingFromSaveState;
}

bool IsPlayingInput()
{
	return (g_playMode == MODE_PLAYING);
}

bool IsUsingPad(int controller)
{
	return ((g_numPads & (1 << controller)) != 0);
}

bool IsUsingWiimote(int wiimote)
{
	return ((g_numPads & (1 << (wiimote + 4))) != 0);
}

void ChangePads(bool instantly)
{
	if (Core::GetState() != Core::CORE_UNINITIALIZED)
	{
		for (int i = 0; i < 4; i++)
			if (instantly) // Changes from savestates need to be instantaneous
				SerialInterface::AddDevice(IsUsingPad(i) ? SI_GC_CONTROLLER : SI_NONE, i);
			else
				SerialInterface::ChangeDevice(IsUsingPad(i) ? SI_GC_CONTROLLER : SI_NONE, i);
	}
}

void ChangeWiiPads()
{
	for (int i = 0; i < 4; i++)
	{
		g_wiimote_sources[i] = IsUsingWiimote(i) ? WIIMOTE_SRC_EMU : WIIMOTE_SRC_NONE;
		GetUsbPointer()->AccessWiiMote(i | 0x100)->Activate(IsUsingWiimote(i));
	}
}

bool BeginRecordingInput(int controllers)
{
	if(g_playMode != MODE_NONE || controllers == 0 || g_recordfd != NULL)
		return false;

	if(File::Exists(g_recordFile))
		File::Delete(g_recordFile);

	if (Core::isRunning())
	{
		const std::string stateFilename = g_recordFile + ".sav";
		if(File::Exists(stateFilename))
			File::Delete(stateFilename);
		State_SaveAs(stateFilename.c_str());
		g_bRecordingFromSaveState = true;
	}

	g_recordfd = fopen(g_recordFile.c_str(), "wb");
	if(!g_recordfd) {
		PanicAlertT("Error opening file %s for recording", g_recordFile.c_str());
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

void SetInputDisplayString(ControllerState padState, int controllerID)
{
	char inp[70];
	sprintf(inp, "%dP:", controllerID + 1);
	g_InputDisplay[controllerID] = inp;

	sprintf(inp, " X: %d Y: %d rX: %d rY: %d L: %d R: %d", 
		g_padState.AnalogStickX,
		g_padState.AnalogStickY,
		g_padState.CStickX,
		g_padState.CStickY,
		g_padState.TriggerL,
		g_padState.TriggerR);
	g_InputDisplay[controllerID].append(inp);

	if(g_padState.A)
	{
		g_InputDisplay[controllerID].append(" A");
	}
	if(g_padState.B)
	{
		g_InputDisplay[controllerID].append(" B");
	}
	if(g_padState.X)
	{
		g_InputDisplay[controllerID].append(" X");
	}
	if(g_padState.Y)
	{
		g_InputDisplay[controllerID].append(" Y");
	}
	if(g_padState.Z)
	{
		g_InputDisplay[controllerID].append(" Z");
	}
	if(g_padState.Start)
	{
		g_InputDisplay[controllerID].append(" START");
	}

	if(g_padState.DPadUp)
	{
		g_InputDisplay[controllerID].append(" UP");
	}
	if(g_padState.DPadDown)
	{
		g_InputDisplay[controllerID].append(" DOWN");
	}
	if(g_padState.DPadLeft)
	{
		g_InputDisplay[controllerID].append(" LEFT");
	}
	if(g_padState.DPadRight)
	{
		g_InputDisplay[controllerID].append(" RIGHT");
	}

	if(g_padState.L)
	{
		g_InputDisplay[controllerID].append(" L");
	}
	if(g_padState.R)
	{
		g_InputDisplay[controllerID].append(" R");
	}

	g_InputDisplay[controllerID].append("\n");
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
	
	g_padState.L = ((PadStatus->button & PAD_TRIGGER_L) != 0);
	g_padState.R = ((PadStatus->button & PAD_TRIGGER_R) != 0);
	g_padState.TriggerL = PadStatus->triggerLeft;
	g_padState.TriggerR = PadStatus->triggerRight;
	
	g_padState.AnalogStickX = PadStatus->stickX;
	g_padState.AnalogStickY = PadStatus->stickY;
	
	g_padState.CStickX = PadStatus->substickX;
	g_padState.CStickY = PadStatus->substickY;
	
	fwrite(&g_padState, sizeof(ControllerState), 1, g_recordfd);

	SetInputDisplayString(g_padState, controllerID);
}

void RecordWiimote(int wiimote, u8 *data, s8 size)
{
	if(!IsRecordingInput() || !IsUsingWiimote(wiimote))
		return;

	fwrite(&size, 1, 1, g_recordfd);
	fwrite(data, 1, size, g_recordfd);
}

bool PlayInput(const char *filename)
{
	if(!filename || g_playMode != MODE_NONE || g_recordfd)
		return false;
	
	if(!File::Exists(filename))
		return false;
	
	DTMHeader header;
	
	File::Delete(g_recordFile);
	File::Copy(filename, g_recordFile);
	
	g_recordfd = fopen(g_recordFile.c_str(), "r+b");
	if(!g_recordfd)
		return false;
	
	fread(&header, sizeof(DTMHeader), 1, g_recordfd);
	
	if(header.filetype[0] != 'D' || header.filetype[1] != 'T' || header.filetype[2] != 'M' || header.filetype[3] != 0x1A) {
		PanicAlertT("Invalid recording file");
		goto cleanup;
	}
	
	// Load savestate (and skip to frame data)
	if(header.bFromSaveState)
	{
		const std::string stateFilename = std::string(filename) + ".sav";
		if(File::Exists(stateFilename))
			Core::SetStateFileName(stateFilename);
		g_bRecordingFromSaveState = true;
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
	g_totalFrameCount = header.frameCount;
	
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
	
	if(header.filetype[0] != 'D' || header.filetype[1] != 'T' || header.filetype[2] != 'M' || header.filetype[3] != 0x1A)
	{
		PanicAlertT("Savestate movie %s is corrupted, movie recording stopping...", filename);
		EndPlayInput(false);
		return;
	}
	
	if (g_rerecords == 0)
		g_rerecords = header.numRerecords;
	
	g_frameCounter = header.frameCount;
	g_totalFrameCount = header.frameCount;
	
	g_numPads = header.numControllers;
	
	ChangePads(true);

	if (Core::g_CoreStartupParameter.bWii)
		ChangeWiiPads();
	
	if (g_recordfd)
		fclose(g_recordfd);
	
	File::Delete(g_recordFile);
	File::Copy(filename, g_recordFile);
	
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
	
	PadStatus->triggerLeft = g_padState.TriggerL;
	PadStatus->triggerRight = g_padState.TriggerR;

	PadStatus->stickX = g_padState.AnalogStickX;
	PadStatus->stickY = g_padState.AnalogStickY;

	PadStatus->substickX = g_padState.CStickX;
	PadStatus->substickY = g_padState.CStickY;

	PadStatus->button |= PAD_USE_ORIGIN;
	
	if(g_padState.A)
	{
		PadStatus->button |= PAD_BUTTON_A;
		PadStatus->analogA = 0xFF;
	}
	if(g_padState.B)
	{
		PadStatus->button |= PAD_BUTTON_B;
		PadStatus->analogB = 0xFF;
	}
	if(g_padState.X)
	{
		PadStatus->button |= PAD_BUTTON_X;
	}
	if(g_padState.Y)
	{
		PadStatus->button |= PAD_BUTTON_Y;
	}
	if(g_padState.Z)
	{
		PadStatus->button |= PAD_TRIGGER_Z;
	}
	if(g_padState.Start)
	{
		PadStatus->button |= PAD_BUTTON_START;
	}
	
	if(g_padState.DPadUp)
	{
		PadStatus->button |= PAD_BUTTON_UP;
	}
	if(g_padState.DPadDown)
	{
		PadStatus->button |= PAD_BUTTON_DOWN;
	}
	if(g_padState.DPadLeft)
	{
		PadStatus->button |= PAD_BUTTON_LEFT;
	}
	if(g_padState.DPadRight)
	{
		PadStatus->button |= PAD_BUTTON_RIGHT;
	}
	
	if(g_padState.L)
	{
		PadStatus->button |= PAD_TRIGGER_L;
	}
	if(g_padState.R)
	{
		PadStatus->button |= PAD_TRIGGER_R;
	}

	SetInputDisplayString(g_padState, controllerID);

	if(feof(g_recordfd) || g_frameCounter >= g_totalFrameCount)
	{
		Core::DisplayMessage("Movie End", 2000);
		EndPlayInput(!g_bReadOnly);
	}
}

bool PlayWiimote(int wiimote, u8 *data, s8 &size)
{
	s8 count = 0;
	if(!IsPlayingInput() || !IsUsingWiimote(wiimote))
		return false;

	fread(&count, 1, 1, g_recordfd);
	size = (count > size) ? size : count;
	fread(data, 1, size, g_recordfd);
	
	// TODO: merge this with the above so that there's no duplicate code
	if(feof(g_recordfd) || g_frameCounter >= g_totalFrameCount)
	{
		Core::DisplayMessage("Movie End", 2000);
		EndPlayInput(!g_bReadOnly);
	}
	return true;
}

void EndPlayInput(bool cont) 
{
	if (cont && g_recordfd)
	{
		// The save and restore here is to resume rerecording
		// from the exact point in playback we're at now
		// if playback ends before the end of the file.
		SaveRecording(g_tmpRecordFile.c_str());
		fclose(g_recordfd);
		File::Delete(g_recordFile);
		File::Copy(g_tmpRecordFile, g_recordFile);
		g_recordfd = fopen(g_recordFile.c_str(), "r+b");
		fseeko(g_recordfd, 0, SEEK_END);
		g_playMode = MODE_RECORDING;
		Core::DisplayMessage("Resuming movie recording", 2000);
	}
	else
	{
		if (g_recordfd)
			fclose(g_recordfd);
		g_recordfd = NULL;
		g_numPads = g_rerecords = 0;
		g_totalFrameCount = g_frameCounter = g_lagCounter = 0;
		g_playMode = MODE_NONE;
	}
}

void SaveRecording(const char *filename)
{
	off_t size = ftello(g_recordfd);

	// NOTE: Eventually this will not happen in
	// read-only mode, but we need a way for the save state to
	// store the current point in the file first.
	// if (!g_bReadOnly)
	{
		rewind(g_recordfd);
	
		// Create the real header now and write it
		DTMHeader header;
		memset(&header, 0, sizeof(DTMHeader));
		
		header.filetype[0] = 'D'; header.filetype[1] = 'T'; header.filetype[2] = 'M'; header.filetype[3] = 0x1A;
		strncpy((char *)header.gameID, Core::g_CoreStartupParameter.GetUniqueID().c_str(), 6);
		header.bWii = Core::g_CoreStartupParameter.bWii;
		header.numControllers = g_numPads & (Core::g_CoreStartupParameter.bWii ? 0xFF : 0x0F);
		
		header.bFromSaveState = g_bRecordingFromSaveState;
		header.frameCount = g_frameCounter;
		header.lagCount = g_lagCounter; 
		header.numRerecords = g_rerecords;
		
		// TODO
		header.uniqueID = 0; 
		// header.author;
		// header.videoBackend; 
		// header.audioEmulator;
		
		fwrite(&header, sizeof(DTMHeader), 1, g_recordfd);
	}

	bool success = false;
	fclose(g_recordfd);
	File::Delete(filename);
	success = File::Copy(g_recordFile, filename);

	if (success && g_bRecordingFromSaveState)
	{
		std::string tmpStateFilename = g_recordFile;
		tmpStateFilename.append(".sav");
		std::string stateFilename = filename;
		stateFilename.append(".sav");
		success = File::Copy(tmpStateFilename, stateFilename);
	}

	if (success /* && !g_bReadOnly*/)
	{
#ifdef WIN32
		int fd;
		if (!_sopen_s(&fd, filename, _O_RDWR, _SH_DENYNO, _S_IREAD | _S_IWRITE))
		{
			success = (_chsize_s(fd, size) == 0);
			_close(fd);
		}
		else
		{
			success = false;
		}
#else
		success = !truncate(filename, size);			
#endif
	}
	
	if (success)
		Core::DisplayMessage(StringFromFormat("DTM %s saved", filename).c_str(), 2000);
	else
		Core::DisplayMessage(StringFromFormat("Failed to save %s", filename).c_str(), 2000);
	
	g_recordfd = fopen(g_recordFile.c_str(), "r+b");
	fseeko(g_recordfd, size, SEEK_SET);
}
};
