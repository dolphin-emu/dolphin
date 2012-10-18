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
#include "HW/WiimoteEmu/WiimoteEmu.h"
#include "HW/WiimoteEmu/WiimoteHid.h"
#include "IPC_HLE/WII_IPC_HLE_Device_usb.h"
#include "VideoBackendBase.h"
#include "State.h"
#include "Timer.h"
#include "VideoConfig.h"
#include "HW/EXI.h"
#include "HW/EXI_Device.h"
#include "HW/EXI_Channel.h"

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
u8* tmpInput = NULL;
u64 g_currentByte = 0, g_totalBytes = 0;
u64 g_currentFrame = 0, g_totalFrames = 0; // VI
u64 g_currentLagCount = 0, g_totalLagCount = 0; // just stats
u64 g_currentInputCount = 0, g_totalInputCount = 0; // just stats
u64 g_recordingStartTime; // seconds since 1970 that recording started
bool g_bSaveConfig = false;
bool g_bSkipIdle = false;
bool g_bDualCore = false;
bool g_bProgressive = false;
bool g_bDSPHLE = false;
bool g_bFastDiscSpeed = false;
std::string g_videoBackend = "opengl";
int g_CPUCore = 1;
bool g_bMemcard;
bool g_bBlankMC = false;

bool g_bRecordingFromSaveState = false;
bool g_bPolled = false;
int g_currentSaveVersion = 0;

std::string tmpStateFilename = "dtm.sav";

std::string g_InputDisplay[8];

ManipFunction mfunc = NULL;


std::string GetInputDisplay()
{
	std::string inputDisplay = "";
	for (int i = 0; i < 8; ++i)
		if ((g_numPads & (1 << i)) != 0)
			inputDisplay.append(g_InputDisplay[i]);
	return inputDisplay; 
}

void FrameUpdate()
{
	g_currentFrame++;
	if(!g_bPolled) 
		g_currentLagCount++;

	if (IsRecordingInput())
	{
		g_totalFrames = g_currentFrame;
		g_totalLagCount = g_currentLagCount;
	}

	if (g_bFrameStep)
	{
		Core::SetState(Core::CORE_PAUSE);
		g_bFrameStep = false;
	}
	
	// ("framestop") the only purpose of this is to cause interpreter/jit Run() to return temporarily.
	// after that we set it back to CPU_RUNNING and continue as normal.
	if (g_bFrameStop)
		*PowerPC::GetStatePtr() = PowerPC::CPU_STEPPING;

	if(g_framesToSkip)
		FrameSkipping();
	
	g_bPolled = false;
}

// called when game is booting up, even if no movie is active,
// but potentially after BeginRecordingInput or PlayInput has been called.
void Init()
{
	g_bPolled = false;
	g_bFrameStep = false;
	g_bFrameStop = false;
	g_bSaveConfig = false;
	g_CPUCore = SConfig::GetInstance().m_LocalCoreStartupParameter.iCPUCore;
	g_bMemcard = SConfig::GetInstance().m_EXIDevice[0] == EXIDEVICE_MEMORYCARD;
	if (IsRecordingInput() || (!tmpHeader.bSaveConfig && IsPlayingInput()))
	{
		g_bSkipIdle = SConfig::GetInstance().m_LocalCoreStartupParameter.bSkipIdle;
		g_bDualCore = SConfig::GetInstance().m_LocalCoreStartupParameter.bCPUThread;
		g_bProgressive = SConfig::GetInstance().m_LocalCoreStartupParameter.bProgressive;
		g_bDSPHLE = SConfig::GetInstance().m_LocalCoreStartupParameter.bDSPHLE;
		g_bFastDiscSpeed = SConfig::GetInstance().m_LocalCoreStartupParameter.bFastDiscSpeed;
		g_videoBackend = SConfig::GetInstance().m_LocalCoreStartupParameter.m_strVideoBackend;
		g_bBlankMC = !File::Exists(SConfig::GetInstance().m_strMemoryCardA);
	}
	else if (IsPlayingInput() && tmpHeader.bSaveConfig)
	{
		g_bSaveConfig = tmpHeader.bSaveConfig;
		g_bSkipIdle = tmpHeader.bSkipIdle;
		g_bDualCore = tmpHeader.bDualCore;
		g_bProgressive = tmpHeader.bProgressive;
		g_bDSPHLE = tmpHeader.bDSPHLE;
		g_bFastDiscSpeed = tmpHeader.bFastDiscSpeed;
		g_CPUCore = tmpHeader.CPUCore;
		g_bBlankMC = tmpHeader.bBlankMC;
	}
	g_frameSkipCounter = g_framesToSkip;
	memset(&g_padState, 0, sizeof(g_padState));
	if (!tmpHeader.bFromSaveState)
		Core::SetStateFileName("");
	for (int i = 0; i < 8; ++i)
		g_InputDisplay[i].clear();

	if (!IsPlayingInput() && !IsRecordingInput())
	{
		g_bRecordingFromSaveState = false;
		g_rerecords = 0;
		g_numPads = 0;
		g_currentByte = 0;
		g_currentFrame = 0;
		g_currentLagCount = 0;
		g_currentInputCount = 0;
		// we don't clear these things because otherwise we can't resume playback if we load a movie state later
		//g_totalFrames = g_totalBytes = 0;
		//delete tmpInput;
		//tmpInput = NULL;
	}
}

void InputUpdate()
{
	g_currentInputCount++;
	if (IsRecordingInput())
		g_totalInputCount = g_currentInputCount;
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

void DoFrameStep()
{
	if(Core::GetState() == Core::CORE_PAUSE)
	{
		// if already paused, frame advance for 1 frame
		Core::SetState(Core::CORE_RUN);
		Core::RequestRefreshInfo();
		g_bFrameStep = true;
	}
	else if(!g_bFrameStep)
	{
		// if not paused yet, pause immediately instead
		Core::SetState(Core::CORE_PAUSE);
	}
}

void SetFrameStopping(bool bEnabled)
{
	g_bFrameStop = bEnabled;
}

void SetReadOnly(bool bEnabled)
{
	if (g_bReadOnly != bEnabled)
		Core::DisplayMessage(bEnabled ? "Read-only mode." :  "Read+Write mode.", 1000);

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

bool IsJustStartingRecordingInputFromSaveState()
{
	return IsRecordingInputFromSaveState() && g_currentFrame == 0;
}

bool IsJustStartingPlayingInputFromSaveState()
{
	return tmpHeader.bFromSaveState && g_currentFrame == 1;
}

bool IsPlayingInput()
{
	return (g_playMode == MODE_PLAYING);
}

bool IsReadOnly()
{
	return g_bReadOnly;
}

u64 GetRecordingStartTime()
{
	return g_recordingStartTime;
}

bool IsUsingPad(int controller)
{
	return ((g_numPads & (1 << controller)) != 0);
}

bool IsUsingWiimote(int wiimote)
{
	return ((g_numPads & (1 << (wiimote + 4))) != 0);
}

bool IsConfigSaved()
{
	return g_bSaveConfig;
}
bool IsDualCore()
{
	return g_bDualCore;
}

bool IsProgressive()
{
	return g_bProgressive;
}

bool IsSkipIdle()
{
	return g_bSkipIdle;
}

bool IsDSPHLE()
{
	return g_bDSPHLE;
}

bool IsFastDiscSpeed()
{
	return g_bFastDiscSpeed;
}

int GetCPUMode()
{
	return g_CPUCore;
}

void ChangePads(bool instantly)
{
	if (Core::GetState() == Core::CORE_UNINITIALIZED)
		return;

	int controllers = 0;

	for (int i = 0; i < 4; i++)
		if (SConfig::GetInstance().m_SIDevice[i] == SIDEVICE_GC_CONTROLLER)
			controllers |= (1 << i);

	if (instantly && (g_numPads & 0x0F) == controllers)
		return;

	for (int i = 0; i < 4; i++)
		if (instantly) // Changes from savestates need to be instantaneous
			SerialInterface::AddDevice(IsUsingPad(i) ? SIDEVICE_GC_CONTROLLER : SIDEVICE_NONE, i);
		else
			SerialInterface::ChangeDevice(IsUsingPad(i) ? SIDEVICE_GC_CONTROLLER : SIDEVICE_NONE, i);
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

	g_numPads = controllers;
	g_currentFrame = g_totalFrames = 0;
	g_currentLagCount = g_totalLagCount = 0;
	g_currentInputCount = g_totalInputCount = 0;
	g_recordingStartTime = Common::Timer::GetLocalTimeSinceJan1970();
	g_rerecords = 0;

	if (Core::IsRunning())
	{
		if(File::Exists(tmpStateFilename))
			File::Delete(tmpStateFilename);

		State::SaveAs(tmpStateFilename.c_str());
		g_bRecordingFromSaveState = true;
	}
	g_playMode = MODE_RECORDING;

	g_bSkipIdle = SConfig::GetInstance().m_LocalCoreStartupParameter.bSkipIdle;
	g_bDualCore = SConfig::GetInstance().m_LocalCoreStartupParameter.bCPUThread;
	g_bProgressive = SConfig::GetInstance().m_LocalCoreStartupParameter.bProgressive;
	g_bDSPHLE = SConfig::GetInstance().m_LocalCoreStartupParameter.bDSPHLE;
	g_bFastDiscSpeed = SConfig::GetInstance().m_LocalCoreStartupParameter.bFastDiscSpeed;
	g_videoBackend = SConfig::GetInstance().m_LocalCoreStartupParameter.m_strVideoBackend;
	g_CPUCore = SConfig::GetInstance().m_LocalCoreStartupParameter.iCPUCore;

	delete [] tmpInput;
	tmpInput = new u8[MAX_DTM_LENGTH];
	g_currentByte = g_totalBytes = 0;

	Core::DisplayMessage("Starting movie recording", 2000);
	return true;
}

static void Analog2DToString(u8 x, u8 y, const char* prefix, char* str)
{
	if((x <= 1 || x == 128 || x >= 255)
	&& (y <= 1 || y == 128 || y >= 255))
	{
		if(x != 128 || y != 128)
		{
			if(x != 128 && y != 128)
			{
				sprintf(str, "%s:%s,%s", prefix, x<128?"LEFT":"RIGHT", y<128?"DOWN":"UP");
			}
			else if(x != 128)
			{
				sprintf(str, "%s:%s", prefix, x<128?"LEFT":"RIGHT");
			}
			else
			{
				sprintf(str, "%s:%s", prefix, y<128?"DOWN":"UP");
			}
		}
		else
		{
			str[0] = '\0';
		}
	}
	else
	{
		sprintf(str, "%s:%d,%d", prefix, x, y);
	}
}

static void Analog1DToString(u8 v, const char* prefix, char* str)
{
	if(v > 0)
	{
		if(v == 255)
		{
			strcpy(str, prefix);
		}
		else
		{
			sprintf(str, "%s:%d", prefix, v);
		}
	}
	else
	{
		str[0] = '\0';
	}
}

void SetInputDisplayString(ControllerState padState, int controllerID)
{
	char inp[70];
	sprintf(inp, "P%d:", controllerID + 1);
	g_InputDisplay[controllerID] = inp;

	if(g_padState.A)
		g_InputDisplay[controllerID].append(" A");
	if(g_padState.B)
		g_InputDisplay[controllerID].append(" B");
	if(g_padState.X)
		g_InputDisplay[controllerID].append(" X");
	if(g_padState.Y)
		g_InputDisplay[controllerID].append(" Y");
	if(g_padState.Z)
		g_InputDisplay[controllerID].append(" Z");
	if(g_padState.Start)
		g_InputDisplay[controllerID].append(" START");

	if(g_padState.DPadUp)
		g_InputDisplay[controllerID].append(" UP");
	if(g_padState.DPadDown)
		g_InputDisplay[controllerID].append(" DOWN");
	if(g_padState.DPadLeft)
		g_InputDisplay[controllerID].append(" LEFT");
	if(g_padState.DPadRight)
		g_InputDisplay[controllerID].append(" RIGHT");

	//if(g_padState.L)
	//{
	//	g_InputDisplay[controllerID].append(" L");
	//}
	//if(g_padState.R)
	//{
	//	g_InputDisplay[controllerID].append(" R");
	//}

	Analog1DToString(g_padState.TriggerL, " L", inp);
	g_InputDisplay[controllerID].append(inp);

	Analog1DToString(g_padState.TriggerR, " R", inp);
	g_InputDisplay[controllerID].append(inp);

	Analog2DToString(g_padState.AnalogStickX, g_padState.AnalogStickY, " ANA", inp);
	g_InputDisplay[controllerID].append(inp);

	Analog2DToString(g_padState.CStickX, g_padState.CStickY, " C", inp);
	g_InputDisplay[controllerID].append(inp);

	g_InputDisplay[controllerID].append("\n");
}

void SetWiiInputDisplayString(int remoteID, u8* const coreData, u8* const accelData, u8* const irData)
{
	int controllerID = remoteID + 4;

	char inp[70];
	sprintf(inp, "R%d:", remoteID + 1);
	g_InputDisplay[controllerID] = inp;

	if(coreData)
	{
		wm_core buttons = *(wm_core*)coreData;
		if(buttons & WiimoteEmu::Wiimote::PAD_LEFT)
			g_InputDisplay[controllerID].append(" LEFT");
		if(buttons & WiimoteEmu::Wiimote::PAD_RIGHT)
			g_InputDisplay[controllerID].append(" RIGHT");
		if(buttons & WiimoteEmu::Wiimote::PAD_DOWN)
			g_InputDisplay[controllerID].append(" DOWN");
		if(buttons & WiimoteEmu::Wiimote::PAD_UP)
			g_InputDisplay[controllerID].append(" UP");
		if(buttons & WiimoteEmu::Wiimote::BUTTON_A)
			g_InputDisplay[controllerID].append(" A");
		if(buttons & WiimoteEmu::Wiimote::BUTTON_B)
			g_InputDisplay[controllerID].append(" B");
		if(buttons & WiimoteEmu::Wiimote::BUTTON_PLUS)
			g_InputDisplay[controllerID].append(" +");
		if(buttons & WiimoteEmu::Wiimote::BUTTON_MINUS)
			g_InputDisplay[controllerID].append(" -");
		if(buttons & WiimoteEmu::Wiimote::BUTTON_ONE)
			g_InputDisplay[controllerID].append(" 1");
		if(buttons & WiimoteEmu::Wiimote::BUTTON_TWO)
			g_InputDisplay[controllerID].append(" 2");
		if(buttons & WiimoteEmu::Wiimote::BUTTON_HOME)
			g_InputDisplay[controllerID].append(" HOME");
	}

	if(accelData)
	{
		wm_accel* dt = (wm_accel*)accelData;
		sprintf(inp, " ACC:%d,%d,%d", dt->x, dt->y, dt->z);
		g_InputDisplay[controllerID].append(inp);
	}

	if(irData) // incomplete
	{
		sprintf(inp, " IR:%d,%d", ((u8*)irData)[0], ((u8*)irData)[1]);
		g_InputDisplay[controllerID].append(inp);
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


	memcpy(&(tmpInput[g_currentByte]), &g_padState, 8);
	g_currentByte += 8;
	g_totalBytes = g_currentByte;

	SetInputDisplayString(g_padState, controllerID);
}

void RecordWiimote(int wiimote, u8 *data, const WiimoteEmu::ReportFeatures& rptf, int irMode)
{
	if(!IsRecordingInput() || !IsUsingWiimote(wiimote))
		return;

	u8* const coreData = rptf.core?(data+rptf.core):NULL;
	u8* const accelData = rptf.accel?(data+rptf.accel):NULL;
	u8* const irData = rptf.ir?(data+rptf.ir):NULL;
	u8 size = rptf.size;

	InputUpdate();
	tmpInput[g_currentByte++] = size;
	memcpy(&(tmpInput[g_currentByte]), data, size);
	g_currentByte += size;
	g_totalBytes = g_currentByte;
	SetWiiInputDisplayString(wiimote, coreData, accelData, irData);
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
		Movie::LoadInput(filename);
		g_currentFrame = 0;
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

	g_totalFrames = tmpHeader.frameCount;
	g_totalLagCount = tmpHeader.lagCount;
	g_totalInputCount = tmpHeader.inputCount;
	g_recordingStartTime = tmpHeader.recordingStartTime;

	g_bSaveConfig = tmpHeader.bSaveConfig;
	g_bSkipIdle = tmpHeader.bSkipIdle;
	g_bDualCore = tmpHeader.bDualCore;
	g_bProgressive = tmpHeader.bProgressive;
	g_bDSPHLE = tmpHeader.bDSPHLE;
	g_bFastDiscSpeed = tmpHeader.bFastDiscSpeed;
	g_CPUCore = tmpHeader.CPUCore;

	g_currentFrame = 0;
	g_currentLagCount = 0;
	g_currentInputCount = 0;

	g_playMode = MODE_PLAYING;
	
	g_totalBytes = g_recordfd.GetSize() - 256;
	delete tmpInput;
	tmpInput = new u8[MAX_DTM_LENGTH];
	g_recordfd.ReadArray(tmpInput, (size_t)g_totalBytes);
	g_currentByte = 0;
	g_recordfd.Close();
	
	return true;
	
cleanup:
	g_recordfd.Close();
	return false;
}

void DoState(PointerWrap &p)
{
	static const int MOVIE_STATE_VERSION = 1;
	g_currentSaveVersion = MOVIE_STATE_VERSION;
	p.Do(g_currentSaveVersion);
	// many of these could be useful to save even when no movie is active,
	// and the data is tiny, so let's just save it regardless of movie state.
	p.Do(g_currentFrame);
	p.Do(g_currentByte);
	p.Do(g_currentLagCount);
	p.Do(g_currentInputCount);
	p.Do(g_bPolled);
	// other variables (such as g_totalBytes and g_totalFrames) are set in LoadInput
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
	{
		if (g_rerecords > tmpHeader.numRerecords)
		{
			tmpHeader.numRerecords = g_rerecords;
		}
		tmpHeader.numRerecords++;
		t_record.Seek(0, SEEK_SET);
		t_record.WriteArray(&tmpHeader, 1);
	}
	
	g_numPads = tmpHeader.numControllers;
	ChangePads(true);
	if (Core::g_CoreStartupParameter.bWii)
		ChangeWiiPads(true);

	u64 totalSavedBytes = t_record.GetSize() - 256;

	bool afterEnd = false;
	if (g_currentByte > totalSavedBytes)
	{
		//PanicAlertT("Warning: You loaded a save whose movie ends before the current frame in the save (byte %u < %u) (frame %u < %u). You should load another save before continuing.", (u32)totalSavedBytes+256, (u32)g_currentByte+256, (u32)tmpHeader.frameCount, (u32)g_currentFrame);
		afterEnd = true;
	}

	if (!g_bReadOnly || tmpInput == NULL)
	{
		g_totalFrames = tmpHeader.frameCount;
		g_totalLagCount = tmpHeader.lagCount;
		g_totalInputCount = tmpHeader.inputCount;

		g_totalBytes = totalSavedBytes;
		delete [] tmpInput;
		tmpInput = new u8[MAX_DTM_LENGTH];
		t_record.ReadArray(tmpInput, (size_t)g_totalBytes);
	}
	else if (g_currentByte > 0)
	{
		if (g_currentByte > totalSavedBytes)
		{
		}
		else if (g_currentByte > g_totalBytes)
		{
			PanicAlertT("Warning: You loaded a save that's after the end of the current movie. (byte %u > %u) (frame %u > %u). You should load another save before continuing, or load this state with read-only mode off.", (u32)g_currentByte+256, (u32)g_totalBytes+256, (u32)g_currentFrame, (u32)g_totalFrames);
		}
		else if(g_currentByte > 0 && g_totalBytes > 0)
		{
			// verify identical from movie start to the save's current frame
			u32 len = (u32)g_currentByte;
			u8* movInput = new u8[len];
			t_record.ReadArray(movInput, (size_t)len);
			for (u32 i = 0; i < len; ++i)
			{
				if (movInput[i] != tmpInput[i])
				{
					// this is a "you did something wrong" alert for the user's benefit.
					// we'll try to say what's going on in excruciating detail, otherwise the user might not believe us.
					if(Core::g_CoreStartupParameter.bWii)
					{ 
						// TODO: more detail
						PanicAlertT("Warning: You loaded a save whose movie mismatches on byte %d (0x%X). You should load another save before continuing, or load this state with read-only mode off. Otherwise you'll probably get a desync.", i+256, i+256);
					}
					else
					{
						int frame = i/8;
						ControllerState curPadState;
						memcpy(&curPadState, &(tmpInput[frame*8]), 8);
						ControllerState movPadState;
						memcpy(&movPadState, &(movInput[frame*8]), 8);
						PanicAlertT("Warning: You loaded a save whose movie mismatches on frame %d. You should load another save before continuing, or load this state with read-only mode off. Otherwise you'll probably get a desync.\n\n"
							"More information: The current movie is %d frames long and the savestate's movie is %d frames long.\n\n"
							"On frame %d, the current movie presses:\n"
							"Start=%d, A=%d, B=%d, X=%d, Y=%d, Z=%d, DUp=%d, DDown=%d, DLeft=%d, DRight=%d, L=%d, R=%d, LT=%d, RT=%d, AnalogX=%d, AnalogY=%d, CX=%d, CY=%d"
							"\n\n"
							"On frame %d, the savestate's movie presses:\n"
							"Start=%d, A=%d, B=%d, X=%d, Y=%d, Z=%d, DUp=%d, DDown=%d, DLeft=%d, DRight=%d, L=%d, R=%d, LT=%d, RT=%d, AnalogX=%d, AnalogY=%d, CX=%d, CY=%d",
							(int)frame,
							(int)g_totalFrames, (int)tmpHeader.frameCount,
							(int)frame,
							(int)curPadState.Start, (int)curPadState.A, (int)curPadState.B, (int)curPadState.X, (int)curPadState.Y, (int)curPadState.Z, (int)curPadState.DPadUp, (int)curPadState.DPadDown, (int)curPadState.DPadLeft, (int)curPadState.DPadRight, (int)curPadState.L, (int)curPadState.R, (int)curPadState.TriggerL, (int)curPadState.TriggerR, (int)curPadState.AnalogStickX, (int)curPadState.AnalogStickY, (int)curPadState.CStickX, (int)curPadState.CStickY,
							(int)frame,
							(int)movPadState.Start, (int)movPadState.A, (int)movPadState.B, (int)movPadState.X, (int)movPadState.Y, (int)movPadState.Z, (int)movPadState.DPadUp, (int)movPadState.DPadDown, (int)movPadState.DPadLeft, (int)movPadState.DPadRight, (int)movPadState.L, (int)movPadState.R, (int)movPadState.TriggerL, (int)movPadState.TriggerR, (int)movPadState.AnalogStickX, (int)movPadState.AnalogStickY, (int)movPadState.CStickX, (int)movPadState.CStickY);
					}
					break;
				}
			}
			delete [] movInput;
		}
	}
	t_record.Close();

	g_rerecords = tmpHeader.numRerecords;
	g_bSaveConfig = tmpHeader.bSaveConfig;

	if (!afterEnd)
	{
		if (g_bReadOnly)
		{
			if(g_playMode != MODE_PLAYING)
			{
				g_playMode = MODE_PLAYING;
				Core::DisplayMessage("Switched to playback", 2000);
			}
		}
		else
		{
			if(g_playMode != MODE_RECORDING)
			{
				g_playMode = MODE_RECORDING;
				Core::DisplayMessage("Switched to recording", 2000);
			}
		}
	}
	else
	{
		EndPlayInput(false);
	}
}

static void CheckInputEnd()
{
	if (g_currentFrame > g_totalFrames || g_currentByte >= g_totalBytes)
	{
		EndPlayInput(!g_bReadOnly);
	}
}

void PlayController(SPADStatus *PadStatus, int controllerID)
{
	if (IsPlayingInput() && IsConfigSaved())
	{
		SetGraphicsConfig();
	}
	if (g_currentFrame == 1)
	{
		if (tmpHeader.bMemcard)
		{
			ExpansionInterface::ChangeDevice(0, EXIDEVICE_MEMORYCARD, 0);
		}
		else if (!tmpHeader.bMemcard)
		{
			ExpansionInterface::ChangeDevice(0, EXIDEVICE_NONE, 0);
		}
	}
	// Correct playback is entirely dependent on the emulator polling the controllers
	// in the same order done during recording
	if (!IsPlayingInput() || !IsUsingPad(controllerID) || tmpInput == NULL)
		return;

	if (g_currentByte + 8 > g_totalBytes)
	{
		PanicAlertT("Premature movie end in PlayController. %u + 8 > %u", (u32)g_currentByte, (u32)g_totalBytes);
		EndPlayInput(!g_bReadOnly);
		return;
	}

	// dtm files don't save the mic button or error bit. not sure if they're actually
	// used, but better safe than sorry
	signed char e = PadStatus->err;
	memset(PadStatus, 0, sizeof(SPADStatus));
	PadStatus->err = e;


	memcpy(&g_padState, &(tmpInput[g_currentByte]), 8);
	g_currentByte += 8;
	
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
	
	if(g_padState.L)
		PadStatus->button |= PAD_TRIGGER_L;
	if(g_padState.R)
		PadStatus->button |= PAD_TRIGGER_R;

	SetInputDisplayString(g_padState, controllerID);

	CheckInputEnd();
}

bool PlayWiimote(int wiimote, u8 *data, const WiimoteEmu::ReportFeatures& rptf, int irMode)
{
	if(!IsPlayingInput() || !IsUsingWiimote(wiimote) || tmpInput == NULL)
		return false;

	if (g_currentByte > g_totalBytes)
	{
		PanicAlertT("Premature movie end in PlayWiimote. %u > %u", (u32)g_currentByte, (u32)g_totalBytes);
		EndPlayInput(!g_bReadOnly);
		return false;
	}

	u8* const coreData = rptf.core?(data+rptf.core):NULL;
	u8* const accelData = rptf.accel?(data+rptf.accel):NULL;
	u8* const irData = rptf.ir?(data+rptf.ir):NULL;
	u8 size = rptf.size;

	u8 sizeInMovie = tmpInput[g_currentByte];

	if (size != sizeInMovie)
	{
		PanicAlertT("Fatal desync. Aborting playback. (Error in PlayWiimote: %u != %u, byte %u.)%s", (u32)sizeInMovie, (u32)size, (u32)g_currentByte, (g_numPads & 0xF)?" Try re-creating the recording with all GameCube controllers disabled (in Configure > Gamecube > Device Settings).":"");
		EndPlayInput(!g_bReadOnly);
		return false;
	}

	g_currentByte++;

	if (g_currentByte + size > g_totalBytes)
	{
		PanicAlertT("Premature movie end in PlayWiimote. %u + %d > %u", (u32)g_currentByte, size, (u32)g_totalBytes);
		EndPlayInput(!g_bReadOnly);
		return false;
	}
	
	memcpy(data, &(tmpInput[g_currentByte]), size);
	g_currentByte += size;
	
	SetWiiInputDisplayString(wiimote, coreData, accelData, irData);

	g_currentInputCount++;
	
	CheckInputEnd();
	return true;
}

void EndPlayInput(bool cont) 
{
	if (cont)
	{
		g_playMode = MODE_RECORDING;
		Core::DisplayMessage("Reached movie end. Resuming recording.", 2000);
	}
	else if(g_playMode != MODE_NONE)
	{
		g_numPads = g_rerecords = 0;
		g_currentByte = 0;
		g_playMode = MODE_NONE;
		Core::DisplayMessage("Movie End.", 2000);
		tmpHeader.bFromSaveState = 0;
		// we don't clear these things because otherwise we can't resume playback if we load a movie state later
		//g_totalFrames = g_totalBytes = 0;
		//delete tmpInput;
		//tmpInput = NULL;
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
	header.frameCount = g_totalFrames;
	header.lagCount = g_totalLagCount;
	header.inputCount = g_totalInputCount;
	header.numRerecords = g_rerecords;
	header.recordingStartTime = g_recordingStartTime;

	header.bSaveConfig = true;
	header.bSkipIdle = g_bSkipIdle;
	header.bDualCore = g_bDualCore;
	header.bProgressive = g_bProgressive;
	header.bDSPHLE = g_bDSPHLE;
	header.bFastDiscSpeed = g_bFastDiscSpeed;
	strncpy((char *)header.videoBackend, g_videoBackend.c_str(),ARRAYSIZE(header.videoBackend));
	header.CPUCore = g_CPUCore;
	header.bEFBAccessEnable = g_ActiveConfig.bEFBAccessEnable;
	header.bEFBCopyEnable = g_ActiveConfig.bEFBCopyEnable;
	header.bCopyEFBToTexture = g_ActiveConfig.bCopyEFBToTexture;
	header.bEFBCopyCacheEnable = g_ActiveConfig.bEFBCopyCacheEnable;
	header.bEFBEmulateFormatChanges = g_ActiveConfig.bEFBEmulateFormatChanges;
	header.bUseXFB = g_ActiveConfig.bUseXFB;
	header.bUseRealXFB = g_ActiveConfig.bUseRealXFB;
	header.bMemcard = g_bMemcard;
	header.bBlankMC = g_bBlankMC;

	// TODO
	header.uniqueID = 0; 
	// header.author;
	// header.audioEmulator;
	
	save_record.WriteArray(&header, 1);

	bool success = save_record.WriteArray(tmpInput, (size_t)g_totalBytes);

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

void SetGraphicsConfig()
{
	g_Config.bEFBAccessEnable = tmpHeader.bEFBAccessEnable;
	g_Config.bEFBCopyEnable = tmpHeader.bEFBCopyEnable;
	g_Config.bCopyEFBToTexture = tmpHeader.bCopyEFBToTexture;
	g_Config.bEFBCopyCacheEnable = tmpHeader.bEFBCopyCacheEnable;
	g_Config.bEFBEmulateFormatChanges = tmpHeader.bEFBEmulateFormatChanges;
	g_Config.bUseXFB = tmpHeader.bUseXFB;
	g_Config.bUseRealXFB = tmpHeader.bUseRealXFB;
}
};
