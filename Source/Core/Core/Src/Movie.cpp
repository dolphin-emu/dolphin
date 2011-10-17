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

#include "Movie.h"

#include "Core.h"
#include "ConfigManager.h"
#include "Thread.h"
#include "FileUtil.h"
#include "PowerPC/PowerPC.h"
#include "HW/SI.h"
#include "HW/Wiimote.h"
#include "IPC_HLE/WII_IPC_HLE_Device_usb.h"
#include "VideoBackendBase.h"
#include "State.h"

// large enough for just over 24 hours of single-player recording
#define MAX_DTM_LENGTH (40 * 1024 * 1024)

std::mutex cs_frameSkip;

namespace Movie {

bool g_bFrameStep = false;
bool g_bFrameStop = false;
bool g_bReadOnly = true;
u32 g_rerecords = 0;
PlayMode g_playMode = MODE_NONE;

u32 g_framesToSkip = 0, g_frameSkipCounter = 0;

u8 g_numPads = 0;
ControllerState g_padState;
DTMHeader tmpHeader;
u8 *tmpInput = NULL;
u64 inputOffset = 0, tmpLength = 0;

u64 g_frameCounter = 0, g_lagCounter = 0, g_totalFrameCount = 0, g_InputCounter = 0;
bool g_bRecordingFromSaveState = false;
bool g_bPolled = false;

std::string tmpStateFilename = "dtm.sav";

std::string g_InputDisplay[4];

ManipFunction mfunc = NULL;

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

void InputUpdate()
{
	g_InputCounter++;
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
		if (g_frameSkipCounter > g_framesToSkip || Core::ShouldSkipFrame(g_frameSkipCounter) == false)
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

bool IsReadOnly()
{
	return g_bReadOnly;
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
	if (Core::GetState() == Core::CORE_UNINITIALIZED)
		return;

	int controllers = 0;

	for (int i = 0; i < 4; i++)
		if (SConfig::GetInstance().m_SIDevice[i] == SI_GC_CONTROLLER)
			controllers |= (1 << i);

	if (instantly && (g_numPads & 0x0F) == controllers)
		return;

	for (int i = 0; i < 4; i++)
		if (instantly) // Changes from savestates need to be instantaneous
			SerialInterface::AddDevice(IsUsingPad(i) ? SI_GC_CONTROLLER : SI_NONE, i);
		else
			SerialInterface::ChangeDevice(IsUsingPad(i) ? SI_GC_CONTROLLER : SI_NONE, i);
}

void ChangeWiiPads(bool instantly)
{
	int controllers = 0;

	for (int i = 0; i < 4; i++)
		if (g_wiimote_sources[i] != WIIMOTE_SRC_NONE)
			controllers |= (1 << i);

	// This is important for Wiimotes, because they can desync easily if they get re-activated
	if (instantly && (g_numPads >> 4) == controllers)
		return;

	for (int i = 0; i < 4; i++)
	{
		g_wiimote_sources[i] = IsUsingWiimote(i) ? WIIMOTE_SRC_EMU : WIIMOTE_SRC_NONE;
		GetUsbPointer()->AccessWiiMote(i | 0x100)->Activate(IsUsingWiimote(i));
	}
}

bool BeginRecordingInput(int controllers)
{
	if(g_playMode != MODE_NONE || controllers == 0)
		return false;

	if (Core::IsRunning())
	{
		if(File::Exists(tmpStateFilename))
			File::Delete(tmpStateFilename);

		State::SaveAs(tmpStateFilename.c_str());
		g_bRecordingFromSaveState = true;
	}
	
	g_numPads = controllers;
	
	g_frameCounter = g_lagCounter = g_InputCounter = 0;
	g_rerecords = 0;
	g_playMode = MODE_RECORDING;
	inputOffset = 0;
	delete tmpInput;
	tmpInput = new u8[MAX_DTM_LENGTH];
	
	
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
	
	memcpy(&(tmpInput[inputOffset]), &g_padState, 8);
	inputOffset += 8;

	SetInputDisplayString(g_padState, controllerID);
}

void RecordWiimote(int wiimote, u8 *data, s8 size)
{
	if(!IsRecordingInput() || !IsUsingWiimote(wiimote))
		return;
	g_InputCounter++;
	tmpInput[inputOffset++] = (u8) size;
	memcpy(&(tmpInput[inputOffset]), data, size);
	inputOffset += size; 
}

bool PlayInput(const char *filename)
{
	if(!filename || g_playMode != MODE_NONE)
		return false;

	if(!File::Exists(filename))
		return false;
	
	File::IOFile g_recordfd;
	
	if (!g_recordfd.Open(filename, "rb"))
		return false;
	
	g_recordfd.ReadArray(&tmpHeader, 1);
	
	if(tmpHeader.filetype[0] != 'D' || tmpHeader.filetype[1] != 'T' || tmpHeader.filetype[2] != 'M' || tmpHeader.filetype[3] != 0x1A) {
		PanicAlertT("Invalid recording file");
		goto cleanup;
	}
	
	// Load savestate (and skip to frame data)
	if(tmpHeader.bFromSaveState)
	{
		const std::string stateFilename = std::string(filename) + ".sav";
		if(File::Exists(stateFilename))
			Core::SetStateFileName(stateFilename);
		g_bRecordingFromSaveState = true;
	}
	
	/* TODO: Put this verification somewhere we have the gameID of the played game
	// TODO: Replace with Unique ID
	if(tmpHeader.uniqueID != 0) {
		PanicAlert("Recording Unique ID Verification Failed");
		goto cleanup;
	}
	
	if(strncmp((char *)tmpHeader.gameID, Core::g_CoreStartupParameter.GetUniqueID().c_str(), 6)) {
		PanicAlert("The recorded game (%s) is not the same as the selected game (%s)", header.gameID, Core::g_CoreStartupParameter.GetUniqueID().c_str());
		goto cleanup;
	}
	*/
	
	g_numPads = tmpHeader.numControllers;
	g_rerecords = tmpHeader.numRerecords;
	g_totalFrameCount = tmpHeader.frameCount;
	
	g_playMode = MODE_PLAYING;
	
	tmpLength = g_recordfd.GetSize() - 256;
	delete tmpInput;
	tmpInput = new u8[MAX_DTM_LENGTH];
	g_recordfd.ReadArray(tmpInput, tmpLength);
	inputOffset = 0;
	g_recordfd.Close();
	
	return true;
	
cleanup:
	g_recordfd.Close();
	return false;
}

void LoadInput(const char *filename)
{
	File::IOFile t_record(filename, "r+b");
	
	t_record.ReadArray(&tmpHeader, 1);
	
	if(tmpHeader.filetype[0] != 'D' || tmpHeader.filetype[1] != 'T' || tmpHeader.filetype[2] != 'M' || tmpHeader.filetype[3] != 0x1A)
	{
		PanicAlertT("Savestate movie %s is corrupted, movie recording stopping...", filename);
		EndPlayInput(false);
		return;
	}

	if (!g_bReadOnly)
		tmpHeader.numRerecords++;
	
	t_record.Seek(0, SEEK_SET);
	t_record.WriteArray(&tmpHeader, 1);
	
	g_frameCounter = tmpHeader.frameCount;
	g_totalFrameCount = (tmpHeader.totalFrameCount ? tmpHeader.totalFrameCount : tmpHeader.frameCount);
	g_InputCounter = tmpHeader.InputCount;

	g_numPads = tmpHeader.numControllers;
	
	ChangePads(true);

	if (Core::g_CoreStartupParameter.bWii)
		ChangeWiiPads(true);

	inputOffset = t_record.GetSize() - 256;
	delete tmpInput;
	tmpInput = new u8[MAX_DTM_LENGTH];
	t_record.ReadArray(tmpInput, inputOffset);
	t_record.Close();
	g_rerecords = tmpHeader.numRerecords;

	if (g_bReadOnly)
	{
		tmpLength = inputOffset;
		inputOffset = tmpHeader.frameStart;
		Core::DisplayMessage("Resuming movie playback", 2000);
		g_playMode = MODE_PLAYING;
	}
	else
	{
		Core::DisplayMessage("Resuming movie recording", 2000);
		g_playMode = MODE_RECORDING;
	}
}

void PlayController(SPADStatus *PadStatus, int controllerID)
{
	// Correct playback is entirely dependent on the emulator polling the controllers
	// in the same order done during recording
	if (!IsPlayingInput() || !IsUsingPad(controllerID) || tmpInput == NULL)
		return;

	if (inputOffset + 8 > tmpLength)
	{
		EndPlayInput(!g_bReadOnly);
		return;
	}

	// dtm files don't save the mic button or error bit. not sure if they're actually
	// used, but better safe than sorry
	signed char e = PadStatus->err;
	memset(PadStatus, 0, sizeof(SPADStatus));
	PadStatus->err = e;
	
	memcpy(&g_padState, &(tmpInput[inputOffset]), 8);
	inputOffset += 8;
	
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

	if (g_frameCounter >= g_totalFrameCount)
	{
		EndPlayInput(!g_bReadOnly);
	}
}

bool PlayWiimote(int wiimote, u8 *data, s8 &size)
{
	s8 count = 0;
	
	if(!IsPlayingInput() || !IsUsingWiimote(wiimote) || tmpInput == NULL)
		return false;
	
	if (inputOffset > tmpLength)
	{
		EndPlayInput(!g_bReadOnly);
		return false;
	}
	
	count = (s8) (tmpInput[inputOffset++]);

	if (inputOffset + count > tmpLength)
	{
		EndPlayInput(!g_bReadOnly);
		return false;
	}
	
	memcpy(data, &(tmpInput[inputOffset]), count);
	inputOffset += count;
	size = (count > size) ? size : count;
	
	g_InputCounter++;
	
	// TODO: merge this with the above so that there's no duplicate code
	if (g_frameCounter >= g_totalFrameCount)
	{
		EndPlayInput(!g_bReadOnly);
	}
	return true;
}

void EndPlayInput(bool cont) 
{
	if (cont)
	{
		g_playMode = MODE_RECORDING;
		Core::DisplayMessage("Resuming movie recording", 2000);
	}
	else
	{
		g_numPads = g_rerecords = 0;
		g_totalFrameCount = g_frameCounter = g_lagCounter = 0;
		g_playMode = MODE_NONE;
		delete tmpInput;
		tmpInput = NULL;
		Core::DisplayMessage("Movie End", 2000);
	}
}

void SaveRecording(const char *filename)
{
	File::IOFile save_record(filename, "wb");
	
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
	header.InputCount = g_InputCounter;
	
	// TODO
	header.uniqueID = 0; 
	// header.author;
	// header.videoBackend; 
	// header.audioEmulator;
	
	if (g_bReadOnly)
	{
		header.frameStart = inputOffset;
		header.totalFrameCount = g_totalFrameCount;
	}
	
	save_record.WriteArray(&header, 1);

	bool success;

	if (g_bReadOnly)
		success = save_record.WriteArray(tmpInput, tmpLength);
	else
		success = save_record.WriteArray(tmpInput, inputOffset);

	if (success && g_bRecordingFromSaveState)
	{
		std::string stateFilename = filename;
		stateFilename.append(".sav");
		success = File::Copy(tmpStateFilename, stateFilename);
	}
	
	if (success)
		Core::DisplayMessage(StringFromFormat("DTM %s saved", filename).c_str(), 2000);
	else
		Core::DisplayMessage(StringFromFormat("Failed to save %s", filename).c_str(), 2000);
}

void SetInputManip(ManipFunction func)
{
	mfunc = func;
}

void CallInputManip(SPADStatus *PadStatus, int controllerID)
{
	if (mfunc)
		(*mfunc)(PadStatus, controllerID);
}
};
