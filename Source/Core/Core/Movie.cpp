// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <polarssl/md5.h>

#include "Common/ChunkFile.h"
#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/Hash.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"
#include "Common/Timer.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/Movie.h"
#include "Core/NetPlayProto.h"
#include "Core/State.h"
#include "Core/DSP/DSPCore.h"
#include "Core/HW/DVDInterface.h"
#include "Core/HW/EXI_Device.h"
#include "Core/HW/SI.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/HW/WiimoteEmu/WiimoteHid.h"
#include "Core/HW/WiimoteEmu/Attachment/Classic.h"
#include "Core/HW/WiimoteEmu/Attachment/Nunchuk.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb.h"
#include "Core/PowerPC/PowerPC.h"
#include "InputCommon/GCPadStatus.h"
#include "VideoCommon/VideoConfig.h"

// The chunk to allocate movie data in multiples of.
#define DTM_BASE_LENGTH (1024)

static std::mutex cs_frameSkip;

namespace Movie {

static bool s_bFrameStep = false;
static bool s_bFrameStop = false;
static bool s_bReadOnly = true;
static u32 s_rerecords = 0;
static PlayMode s_playMode = MODE_NONE;

static u32 s_framesToSkip = 0, s_frameSkipCounter = 0;

static u8 s_numPads = 0;
static ControllerState s_padState;
static DTMHeader tmpHeader;
static u8* tmpInput = nullptr;
static size_t tmpInputAllocated = 0;
static u64 s_currentByte = 0, s_totalBytes = 0;
u64 g_currentFrame = 0, g_totalFrames = 0; // VI
u64 g_currentLagCount = 0;
static u64 s_totalLagCount = 0; // just stats
u64 g_currentInputCount = 0, g_totalInputCount = 0; // just stats
static u64 s_totalTickCount = 0, s_tickCountAtLastInput = 0; // just stats
static u64 s_recordingStartTime; // seconds since 1970 that recording started
static bool s_bSaveConfig = false, s_bSkipIdle = false, s_bDualCore = false, s_bProgressive = false, s_bDSPHLE = false, s_bFastDiscSpeed = false;
static bool s_bSyncGPU = false, s_bNetPlay = false;
static std::string s_videoBackend = "unknown";
static int s_iCPUCore = 1;
bool g_bClearSave = false;
bool g_bDiscChange = false;
static std::string s_author = "";
std::string g_discChange = "";
u64 g_titleID = 0;
static u8 s_MD5[16];
static u8 s_bongos, s_memcards;
static u8 s_revision[20];
static u32 s_DSPiromHash = 0;
static u32 s_DSPcoefHash = 0;

static bool s_bRecordingFromSaveState = false;
static bool s_bPolled = false;

static std::string tmpStateFilename = File::GetUserPath(D_STATESAVES_IDX) + "dtm.sav";

static std::string s_InputDisplay[8];

static ManipFunction mfunc = nullptr;

static void EnsureTmpInputSize(size_t bound)
{
	if (tmpInputAllocated >= bound)
		return;
	// The buffer expands in powers of two of DTM_BASE_LENGTH
	// (standard exponential buffer growth).
	size_t newAlloc = DTM_BASE_LENGTH;
	while (newAlloc < bound)
		newAlloc *= 2;

	u8* newTmpInput = new u8[newAlloc];
	tmpInputAllocated = newAlloc;
	if (tmpInput != nullptr)
	{
		if (s_totalBytes > 0)
			memcpy(newTmpInput, tmpInput, (size_t)s_totalBytes);
		delete[] tmpInput;
	}
	tmpInput = newTmpInput;
}

static bool IsMovieHeader(u8 magic[4])
{
	return magic[0] == 'D' &&
	       magic[1] == 'T' &&
	       magic[2] == 'M' &&
	       magic[3] == 0x1A;
}

std::string GetInputDisplay()
{
	if (!IsMovieActive())
	{
		s_numPads = 0;
		for (int i = 0; i < 4; ++i)
		{
			if (SerialInterface::GetDeviceType(i) != SIDEVICE_NONE)
				s_numPads |= (1 << i);
			if (g_wiimote_sources[i] != WIIMOTE_SRC_NONE)
				s_numPads |= (1 << (i + 4));
		}
	}

	std::string inputDisplay = "";
	for (int i = 0; i < 8; ++i)
		if ((s_numPads & (1 << i)) != 0)
			inputDisplay.append(s_InputDisplay[i]);

	return inputDisplay;
}

void FrameUpdate()
{
	g_currentFrame++;
	if (!s_bPolled)
		g_currentLagCount++;

	if (IsRecordingInput())
	{
		g_totalFrames = g_currentFrame;
		s_totalLagCount = g_currentLagCount;
	}
	if (s_bFrameStep)
	{
		Core::SetState(Core::CORE_PAUSE);
		s_bFrameStep = false;
	}

	// ("framestop") the only purpose of this is to cause interpreter/jit Run() to return temporarily.
	// after that we set it back to CPU_RUNNING and continue as normal.
	if (s_bFrameStop)
		*PowerPC::GetStatePtr() = PowerPC::CPU_STEPPING;

	if (s_framesToSkip)
		FrameSkipping();

	s_bPolled = false;
}

// called when game is booting up, even if no movie is active,
// but potentially after BeginRecordingInput or PlayInput has been called.
void Init()
{
	s_bPolled = false;
	s_bFrameStep = false;
	s_bFrameStop = false;
	s_bSaveConfig = false;
	s_iCPUCore = SConfig::GetInstance().m_LocalCoreStartupParameter.iCPUCore;
	if (IsPlayingInput())
	{
		ReadHeader();
		std::thread md5thread(CheckMD5);
		md5thread.detach();
		if (strncmp((char *)tmpHeader.gameID, SConfig::GetInstance().m_LocalCoreStartupParameter.GetUniqueID().c_str(), 6))
		{
			PanicAlertT("The recorded game (%s) is not the same as the selected game (%s)", tmpHeader.gameID, SConfig::GetInstance().m_LocalCoreStartupParameter.GetUniqueID().c_str());
			EndPlayInput(false);
		}
	}

	if (IsRecordingInput())
	{
		GetSettings();
		std::thread md5thread(GetMD5);
		md5thread.detach();
		s_tickCountAtLastInput = 0;
	}

	s_frameSkipCounter = s_framesToSkip;
	memset(&s_padState, 0, sizeof(s_padState));
	if (!tmpHeader.bFromSaveState || !IsPlayingInput())
		Core::SetStateFileName("");

	for (auto& disp : s_InputDisplay)
		disp.clear();

	if (!IsMovieActive())
	{
		s_bRecordingFromSaveState = false;
		s_rerecords = 0;
		s_currentByte = 0;
		g_currentFrame = 0;
		g_currentLagCount = 0;
		g_currentInputCount = 0;
	}
}

void InputUpdate()
{
	g_currentInputCount++;
	if (IsRecordingInput())
	{
		g_totalInputCount = g_currentInputCount;
		s_totalTickCount += CoreTiming::GetTicks() - s_tickCountAtLastInput;
		s_tickCountAtLastInput = CoreTiming::GetTicks();
	}

	if (IsPlayingInput() && g_currentInputCount == (g_totalInputCount - 1) && SConfig::GetInstance().m_PauseMovie)
		Core::SetState(Core::CORE_PAUSE);
}

void SetFrameSkipping(unsigned int framesToSkip)
{
	std::lock_guard<std::mutex> lk(cs_frameSkip);

	s_framesToSkip = framesToSkip;
	s_frameSkipCounter = 0;

	// Don't forget to re-enable rendering in case it wasn't...
	// as this won't be changed anymore when frameskip is turned off
	if (framesToSkip == 0)
		g_video_backend->Video_SetRendering(true);
}

void SetPolledDevice()
{
	s_bPolled = true;
}

void DoFrameStep()
{
	if (Core::GetState() == Core::CORE_PAUSE)
	{
		// if already paused, frame advance for 1 frame
		Core::SetState(Core::CORE_RUN);
		Core::RequestRefreshInfo();
		s_bFrameStep = true;
	}
	else if (!s_bFrameStep)
	{
		// if not paused yet, pause immediately instead
		Core::SetState(Core::CORE_PAUSE);
	}
}

void SetFrameStopping(bool bEnabled)
{
	s_bFrameStop = bEnabled;
}

void SetReadOnly(bool bEnabled)
{
	if (s_bReadOnly != bEnabled)
		Core::DisplayMessage(bEnabled ? "Read-only mode." :  "Read+Write mode.", 1000);

	s_bReadOnly = bEnabled;
}

void FrameSkipping()
{
	// Frameskipping will desync movie playback
	if (!IsMovieActive() || NetPlay::IsNetPlayRunning())
	{
		std::lock_guard<std::mutex> lk(cs_frameSkip);

		s_frameSkipCounter++;
		if (s_frameSkipCounter > s_framesToSkip || Core::ShouldSkipFrame(s_frameSkipCounter) == false)
			s_frameSkipCounter = 0;

		g_video_backend->Video_SetRendering(!s_frameSkipCounter);
	}
}

bool IsRecordingInput()
{
	return (s_playMode == MODE_RECORDING);
}

bool IsRecordingInputFromSaveState()
{
	return s_bRecordingFromSaveState;
}

bool IsJustStartingRecordingInputFromSaveState()
{
	return IsRecordingInputFromSaveState() && g_currentFrame == 0;
}

bool IsJustStartingPlayingInputFromSaveState()
{
	return IsRecordingInputFromSaveState() && g_currentFrame == 1 && IsPlayingInput();
}

bool IsPlayingInput()
{
	return (s_playMode == MODE_PLAYING);
}

bool IsMovieActive()
{
	return s_playMode != MODE_NONE;
}

bool IsReadOnly()
{
	return s_bReadOnly;
}

u64 GetRecordingStartTime()
{
	return s_recordingStartTime;
}

bool IsUsingPad(int controller)
{
	return ((s_numPads & (1 << controller)) != 0);
}

bool IsUsingBongo(int controller)
{
	return ((s_bongos & (1 << controller)) != 0);
}

bool IsUsingWiimote(int wiimote)
{
	return ((s_numPads & (1 << (wiimote + 4))) != 0);
}

bool IsConfigSaved()
{
	return s_bSaveConfig;
}
bool IsDualCore()
{
	return s_bDualCore;
}

bool IsProgressive()
{
	return s_bProgressive;
}

bool IsSkipIdle()
{
	return s_bSkipIdle;
}

bool IsDSPHLE()
{
	return s_bDSPHLE;
}

bool IsFastDiscSpeed()
{
	return s_bFastDiscSpeed;
}

int GetCPUMode()
{
	return s_iCPUCore;
}

bool IsStartingFromClearSave()
{
	return g_bClearSave;
}

bool IsUsingMemcard(int memcard)
{
	return (s_memcards & (1 << memcard)) != 0;
}
bool IsSyncGPU()
{
	return s_bSyncGPU;
}

bool IsNetPlayRecording()
{
	return s_bNetPlay;
}

void ChangePads(bool instantly)
{
	if (!Core::IsRunning())
		return;

	int controllers = 0;

	for (int i = 0; i < MAX_SI_CHANNELS; ++i)
		if (SConfig::GetInstance().m_SIDevice[i] == SIDEVICE_GC_CONTROLLER || SConfig::GetInstance().m_SIDevice[i] == SIDEVICE_GC_TARUKONGA)
			controllers |= (1 << i);

	if (instantly && (s_numPads & 0x0F) == controllers)
		return;

	for (int i = 0; i < MAX_SI_CHANNELS; ++i)
		if (instantly) // Changes from savestates need to be instantaneous
			SerialInterface::AddDevice(IsUsingPad(i) ? (IsUsingBongo(i) ? SIDEVICE_GC_TARUKONGA : SIDEVICE_GC_CONTROLLER) : SIDEVICE_NONE, i);
		else
			SerialInterface::ChangeDevice(IsUsingPad(i) ? (IsUsingBongo(i) ? SIDEVICE_GC_TARUKONGA : SIDEVICE_GC_CONTROLLER) : SIDEVICE_NONE, i);
}

void ChangeWiiPads(bool instantly)
{
	int controllers = 0;

	for (int i = 0; i < MAX_WIIMOTES; ++i)
		if (g_wiimote_sources[i] != WIIMOTE_SRC_NONE)
			controllers |= (1 << i);

	// This is important for Wiimotes, because they can desync easily if they get re-activated
	if (instantly && (s_numPads >> 4) == controllers)
		return;

	for (int i = 0; i < MAX_WIIMOTES; ++i)
	{
		g_wiimote_sources[i] = IsUsingWiimote(i) ? WIIMOTE_SRC_EMU : WIIMOTE_SRC_NONE;
		GetUsbPointer()->AccessWiiMote(i | 0x100)->Activate(IsUsingWiimote(i));
	}
}

bool BeginRecordingInput(int controllers)
{
	if (s_playMode != MODE_NONE || controllers == 0)
		return false;

	bool was_unpaused = Core::PauseAndLock(true);

	s_numPads = controllers;
	g_currentFrame = g_totalFrames = 0;
	g_currentLagCount = s_totalLagCount = 0;
	g_currentInputCount = g_totalInputCount = 0;
	s_totalTickCount = s_tickCountAtLastInput = 0;
	s_bongos = 0;
	s_memcards = 0;
	if (NetPlay::IsNetPlayRunning())
	{
		s_bNetPlay = true;
		s_recordingStartTime = NETPLAY_INITIAL_GCTIME;
	}
	else
	{
		s_recordingStartTime = Common::Timer::GetLocalTimeSinceJan1970();
	}

	s_rerecords = 0;

	for (int i = 0; i < MAX_SI_CHANNELS; ++i)
		if (SConfig::GetInstance().m_SIDevice[i] == SIDEVICE_GC_TARUKONGA)
			s_bongos |= (1 << i);

	if (Core::IsRunningAndStarted())
	{
		if (File::Exists(tmpStateFilename))
			File::Delete(tmpStateFilename);

		State::SaveAs(tmpStateFilename);
		s_bRecordingFromSaveState = true;

		// This is only done here if starting from save state because otherwise we won't have the titleid. Otherwise it's set in WII_IPC_HLE_Device_es.cpp.
		// TODO: find a way to GetTitleDataPath() from Movie::Init()
		if (SConfig::GetInstance().m_LocalCoreStartupParameter.bWii)
		{
			if (File::Exists(Common::GetTitleDataPath(g_titleID) + "banner.bin"))
				Movie::g_bClearSave = false;
			else
				Movie::g_bClearSave = true;
		}
		std::thread md5thread(GetMD5);
		md5thread.detach();
		GetSettings();
	}
	s_playMode = MODE_RECORDING;
	s_author = SConfig::GetInstance().m_strMovieAuthor;
	EnsureTmpInputSize(1);

	s_currentByte = s_totalBytes = 0;

	Core::UpdateWantDeterminism();

	Core::PauseAndLock(false, was_unpaused);

	Core::DisplayMessage("Starting movie recording", 2000);
	return true;
}

static std::string Analog2DToString(u8 x, u8 y, const std::string& prefix, u8 range = 255)
{
	u8 center = range / 2 + 1;
	if ((x <= 1 || x == center || x >= range) &&
	    (y <= 1 || y == center || y >= range))
	{
		if (x != center || y != center)
		{
			if (x != center && y != center)
			{
				return StringFromFormat("%s:%s,%s", prefix.c_str(), x < center ? "LEFT" : "RIGHT", y < center ? "DOWN" : "UP");
			}
			else if (x != center)
			{
				return StringFromFormat("%s:%s", prefix.c_str(), x < center ? "LEFT" : "RIGHT");
			}
			else
			{
				return StringFromFormat("%s:%s", prefix.c_str(), y < center ? "DOWN" : "UP");
			}
		}
		else
		{
			return "";
		}
	}
	else
	{
		return StringFromFormat("%s:%d,%d", prefix.c_str(), x, y);
	}
}

static std::string Analog1DToString(u8 v, const std::string& prefix, u8 range = 255)
{
	if (v > 0)
	{
		if (v == range)
		{
			return prefix;
		}
		else
		{
			return StringFromFormat("%s:%d", prefix.c_str(), v);
		}
	}
	else
	{
		return "";
	}
}

static void SetInputDisplayString(ControllerState padState, int controllerID)
{
	s_InputDisplay[controllerID] = StringFromFormat("P%d:", controllerID + 1);

	if (padState.A)
		s_InputDisplay[controllerID].append(" A");
	if (padState.B)
		s_InputDisplay[controllerID].append(" B");
	if (padState.X)
		s_InputDisplay[controllerID].append(" X");
	if (padState.Y)
		s_InputDisplay[controllerID].append(" Y");
	if (padState.Z)
		s_InputDisplay[controllerID].append(" Z");
	if (padState.Start)
		s_InputDisplay[controllerID].append(" START");

	if (padState.DPadUp)
		s_InputDisplay[controllerID].append(" UP");
	if (padState.DPadDown)
		s_InputDisplay[controllerID].append(" DOWN");
	if (padState.DPadLeft)
		s_InputDisplay[controllerID].append(" LEFT");
	if (padState.DPadRight)
		s_InputDisplay[controllerID].append(" RIGHT");

	s_InputDisplay[controllerID].append(Analog1DToString(padState.TriggerL, " L"));
	s_InputDisplay[controllerID].append(Analog1DToString(padState.TriggerR, " R"));
	s_InputDisplay[controllerID].append(Analog2DToString(padState.AnalogStickX, padState.AnalogStickY, " ANA"));
	s_InputDisplay[controllerID].append(Analog2DToString(padState.CStickX, padState.CStickY, " C"));
	s_InputDisplay[controllerID].append("\n");
}

static void SetWiiInputDisplayString(int remoteID, u8* const data, const WiimoteEmu::ReportFeatures& rptf, int ext, const wiimote_key key)
{
	int controllerID = remoteID + 4;

	s_InputDisplay[controllerID] = StringFromFormat("R%d:", remoteID + 1);

	u8* const coreData = rptf.core ? (data + rptf.core) : nullptr;
	u8* const accelData = rptf.accel ? (data + rptf.accel) : nullptr;
	u8* const irData = rptf.ir ? (data + rptf.ir) : nullptr;
	u8* const extData = rptf.ext ? (data + rptf.ext) : nullptr;

	if (coreData)
	{
		wm_core buttons = *(wm_core*)coreData;
		if (buttons & WiimoteEmu::Wiimote::PAD_LEFT)
			s_InputDisplay[controllerID].append(" LEFT");
		if (buttons & WiimoteEmu::Wiimote::PAD_RIGHT)
			s_InputDisplay[controllerID].append(" RIGHT");
		if (buttons & WiimoteEmu::Wiimote::PAD_DOWN)
			s_InputDisplay[controllerID].append(" DOWN");
		if (buttons & WiimoteEmu::Wiimote::PAD_UP)
			s_InputDisplay[controllerID].append(" UP");
		if (buttons & WiimoteEmu::Wiimote::BUTTON_A)
			s_InputDisplay[controllerID].append(" A");
		if (buttons & WiimoteEmu::Wiimote::BUTTON_B)
			s_InputDisplay[controllerID].append(" B");
		if (buttons & WiimoteEmu::Wiimote::BUTTON_PLUS)
			s_InputDisplay[controllerID].append(" +");
		if (buttons & WiimoteEmu::Wiimote::BUTTON_MINUS)
			s_InputDisplay[controllerID].append(" -");
		if (buttons & WiimoteEmu::Wiimote::BUTTON_ONE)
			s_InputDisplay[controllerID].append(" 1");
		if (buttons & WiimoteEmu::Wiimote::BUTTON_TWO)
			s_InputDisplay[controllerID].append(" 2");
		if (buttons & WiimoteEmu::Wiimote::BUTTON_HOME)
			s_InputDisplay[controllerID].append(" HOME");
	}

	if (accelData)
	{
		wm_accel* dt = (wm_accel*)accelData;
		std::string accel = StringFromFormat(" ACC:%d,%d,%d", dt->x, dt->y, dt->z);
		s_InputDisplay[controllerID].append(accel);
	}

	if (irData)
	{
		u16 x = irData[0] | ((irData[2] >> 4 & 0x3) << 8);
		u16 y = irData[1] | ((irData[2] >> 2 & 0x3) << 8);
		std::string ir = StringFromFormat(" IR:%d,%d", x, y);
		s_InputDisplay[controllerID].append(ir);
	}

	// Nunchuck
	if (extData && ext == 1)
	{
		wm_extension nunchuck;
		memcpy(&nunchuck, extData, sizeof(wm_extension));
		WiimoteDecrypt(&key, (u8*)&nunchuck, 0, sizeof(wm_extension));
		nunchuck.bt = nunchuck.bt ^ 0xFF;

		std::string accel = StringFromFormat(" N-ACC:%d,%d,%d", nunchuck.ax, nunchuck.ay, nunchuck.az);

		if (nunchuck.bt & WiimoteEmu::Nunchuk::BUTTON_C)
			s_InputDisplay[controllerID].append(" C");
		if (nunchuck.bt & WiimoteEmu::Nunchuk::BUTTON_Z)
			s_InputDisplay[controllerID].append(" Z");
		s_InputDisplay[controllerID].append(accel);
		s_InputDisplay[controllerID].append(Analog2DToString(nunchuck.jx, nunchuck.jy, " ANA"));
	}

	// Classic controller
	if (extData && ext == 2)
	{
		wm_classic_extension cc;
		memcpy(&cc, extData, sizeof(wm_classic_extension));
		WiimoteDecrypt(&key, (u8*)&cc, 0, sizeof(wm_classic_extension));
		cc.bt = cc.bt ^ 0xFFFF;

		if (cc.bt & WiimoteEmu::Classic::PAD_LEFT)
			s_InputDisplay[controllerID].append(" LEFT");
		if (cc.bt & WiimoteEmu::Classic::PAD_RIGHT)
			s_InputDisplay[controllerID].append(" RIGHT");
		if (cc.bt & WiimoteEmu::Classic::PAD_DOWN)
			s_InputDisplay[controllerID].append(" DOWN");
		if (cc.bt & WiimoteEmu::Classic::PAD_UP)
			s_InputDisplay[controllerID].append(" UP");
		if (cc.bt & WiimoteEmu::Classic::BUTTON_A)
			s_InputDisplay[controllerID].append(" A");
		if (cc.bt & WiimoteEmu::Classic::BUTTON_B)
			s_InputDisplay[controllerID].append(" B");
		if (cc.bt & WiimoteEmu::Classic::BUTTON_X)
			s_InputDisplay[controllerID].append(" X");
		if (cc.bt & WiimoteEmu::Classic::BUTTON_Y)
			s_InputDisplay[controllerID].append(" Y");
		if (cc.bt & WiimoteEmu::Classic::BUTTON_ZL)
			s_InputDisplay[controllerID].append(" ZL");
		if (cc.bt & WiimoteEmu::Classic::BUTTON_ZR)
			s_InputDisplay[controllerID].append(" ZR");
		if (cc.bt & WiimoteEmu::Classic::BUTTON_PLUS)
			s_InputDisplay[controllerID].append(" +");
		if (cc.bt & WiimoteEmu::Classic::BUTTON_MINUS)
			s_InputDisplay[controllerID].append(" -");
		if (cc.bt & WiimoteEmu::Classic::BUTTON_HOME)
			s_InputDisplay[controllerID].append(" HOME");

		s_InputDisplay[controllerID].append(Analog1DToString(cc.lt1 | (cc.lt2 << 3), " L", 31));
		s_InputDisplay[controllerID].append(Analog1DToString(cc.rt, " R", 31));
		s_InputDisplay[controllerID].append(Analog2DToString(cc.lx, cc.ly, " ANA", 63));
		s_InputDisplay[controllerID].append(Analog2DToString(cc.rx1 | (cc.rx2 << 1) | (cc.rx3 << 3), cc.ry, " R-ANA", 31));
	}

	s_InputDisplay[controllerID].append("\n");
}

void CheckPadStatus(GCPadStatus* PadStatus, int controllerID)
{
	s_padState.A         = ((PadStatus->button & PAD_BUTTON_A) != 0);
	s_padState.B         = ((PadStatus->button & PAD_BUTTON_B) != 0);
	s_padState.X         = ((PadStatus->button & PAD_BUTTON_X) != 0);
	s_padState.Y         = ((PadStatus->button & PAD_BUTTON_Y) != 0);
	s_padState.Z         = ((PadStatus->button & PAD_TRIGGER_Z) != 0);
	s_padState.Start     = ((PadStatus->button & PAD_BUTTON_START) != 0);

	s_padState.DPadUp    = ((PadStatus->button & PAD_BUTTON_UP) != 0);
	s_padState.DPadDown  = ((PadStatus->button & PAD_BUTTON_DOWN) != 0);
	s_padState.DPadLeft  = ((PadStatus->button & PAD_BUTTON_LEFT) != 0);
	s_padState.DPadRight = ((PadStatus->button & PAD_BUTTON_RIGHT) != 0);

	s_padState.L         = ((PadStatus->button & PAD_TRIGGER_L) != 0);
	s_padState.R         = ((PadStatus->button & PAD_TRIGGER_R) != 0);
	s_padState.TriggerL  = PadStatus->triggerLeft;
	s_padState.TriggerR  = PadStatus->triggerRight;

	s_padState.AnalogStickX = PadStatus->stickX;
	s_padState.AnalogStickY = PadStatus->stickY;

	s_padState.CStickX   = PadStatus->substickX;
	s_padState.CStickY   = PadStatus->substickY;

	SetInputDisplayString(s_padState, controllerID);
}

void RecordInput(GCPadStatus* PadStatus, int controllerID)
{
	if (!IsRecordingInput() || !IsUsingPad(controllerID))
		return;

	CheckPadStatus(PadStatus, controllerID);

	if (g_bDiscChange)
	{
		s_padState.disc = g_bDiscChange;
		g_bDiscChange = false;
	}

	EnsureTmpInputSize((size_t)(s_currentByte + 8));
	memcpy(&(tmpInput[s_currentByte]), &s_padState, 8);
	s_currentByte += 8;
	s_totalBytes = s_currentByte;
}

void CheckWiimoteStatus(int wiimote, u8 *data, const WiimoteEmu::ReportFeatures& rptf, int ext, const wiimote_key key)
{
	u8 size = rptf.size;
	SetWiiInputDisplayString(wiimote, data, rptf, ext, key);

	if (IsRecordingInput())
		RecordWiimote(wiimote, data, size);
}

void RecordWiimote(int wiimote, u8 *data, u8 size)
{
	if (!IsRecordingInput() || !IsUsingWiimote(wiimote))
		return;

	InputUpdate();
	EnsureTmpInputSize((size_t)(s_currentByte + size + 1));
	tmpInput[s_currentByte++] = size;
	memcpy(&(tmpInput[s_currentByte]), data, size);
	s_currentByte += size;
	s_totalBytes = s_currentByte;
}

void ReadHeader()
{
	s_numPads = tmpHeader.numControllers;
	s_recordingStartTime = tmpHeader.recordingStartTime;
	if (s_rerecords < tmpHeader.numRerecords)
		s_rerecords = tmpHeader.numRerecords;

	if (tmpHeader.bSaveConfig)
	{
		s_bSaveConfig = true;
		s_bSkipIdle = tmpHeader.bSkipIdle;
		s_bDualCore = tmpHeader.bDualCore;
		s_bProgressive = tmpHeader.bProgressive;
		s_bDSPHLE = tmpHeader.bDSPHLE;
		s_bFastDiscSpeed = tmpHeader.bFastDiscSpeed;
		s_iCPUCore = tmpHeader.CPUCore;
		g_bClearSave = tmpHeader.bClearSave;
		s_memcards = tmpHeader.memcards;
		s_bongos = tmpHeader.bongos;
		s_bSyncGPU = tmpHeader.bSyncGPU;
		s_bNetPlay = tmpHeader.bNetPlay;
		memcpy(s_revision, tmpHeader.revision, ArraySize(s_revision));
	}
	else
	{
		GetSettings();
	}

	s_videoBackend = (char*) tmpHeader.videoBackend;
	g_discChange = (char*) tmpHeader.discChange;
	s_author = (char*) tmpHeader.author;
	memcpy(s_MD5, tmpHeader.md5, 16);
	s_DSPiromHash = tmpHeader.DSPiromHash;
	s_DSPcoefHash = tmpHeader.DSPcoefHash;
}

bool PlayInput(const std::string& filename)
{
	if (s_playMode != MODE_NONE)
		return false;

	if (!File::Exists(filename))
		return false;

	File::IOFile g_recordfd;

	if (!g_recordfd.Open(filename, "rb"))
		return false;

	g_recordfd.ReadArray(&tmpHeader, 1);

	if (!IsMovieHeader(tmpHeader.filetype))
	{
		PanicAlertT("Invalid recording file");
		g_recordfd.Close();
		return false;
	}

	ReadHeader();
	g_totalFrames = tmpHeader.frameCount;
	s_totalLagCount = tmpHeader.lagCount;
	g_totalInputCount = tmpHeader.inputCount;
	s_totalTickCount = tmpHeader.tickCount;
	g_currentFrame = 0;
	g_currentLagCount = 0;
	g_currentInputCount = 0;

	s_playMode = MODE_PLAYING;

	Core::UpdateWantDeterminism();

	s_totalBytes = g_recordfd.GetSize() - 256;
	EnsureTmpInputSize((size_t)s_totalBytes);
	g_recordfd.ReadArray(tmpInput, (size_t)s_totalBytes);
	s_currentByte = 0;
	g_recordfd.Close();

	// Load savestate (and skip to frame data)
	if (tmpHeader.bFromSaveState)
	{
		const std::string stateFilename = filename + ".sav";
		if (File::Exists(stateFilename))
			Core::SetStateFileName(stateFilename);
		s_bRecordingFromSaveState = true;
		Movie::LoadInput(filename);
	}

	return true;
}

void DoState(PointerWrap &p)
{
	// many of these could be useful to save even when no movie is active,
	// and the data is tiny, so let's just save it regardless of movie state.
	p.Do(g_currentFrame);
	p.Do(s_currentByte);
	p.Do(g_currentLagCount);
	p.Do(g_currentInputCount);
	p.Do(s_bPolled);
	p.Do(s_tickCountAtLastInput);
	// other variables (such as s_totalBytes and g_totalFrames) are set in LoadInput
}

void LoadInput(const std::string& filename)
{
	File::IOFile t_record;
	if (!t_record.Open(filename, "r+b"))
	{
		PanicAlertT("Failed to read %s", filename.c_str());
		EndPlayInput(false);
		return;
	}

	t_record.ReadArray(&tmpHeader, 1);

	if (!IsMovieHeader(tmpHeader.filetype))
	{
		PanicAlertT("Savestate movie %s is corrupted, movie recording stopping...", filename.c_str());
		EndPlayInput(false);
		return;
	}
	ReadHeader();
	if (!s_bReadOnly)
	{
		s_rerecords++;
		tmpHeader.numRerecords = s_rerecords;
		t_record.Seek(0, SEEK_SET);
		t_record.WriteArray(&tmpHeader, 1);
	}

	ChangePads(true);
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bWii)
		ChangeWiiPads(true);

	u64 totalSavedBytes = t_record.GetSize() - 256;

	bool afterEnd = false;
	// This can only happen if the user manually deletes data from the dtm.
	if (s_currentByte > totalSavedBytes)
	{
		PanicAlertT("Warning: You loaded a save whose movie ends before the current frame in the save (byte %u < %u) (frame %u < %u). You should load another save before continuing.", (u32)totalSavedBytes+256, (u32)s_currentByte+256, (u32)tmpHeader.frameCount, (u32)g_currentFrame);
		afterEnd = true;
	}

	if (!s_bReadOnly || tmpInput == nullptr)
	{
		g_totalFrames = tmpHeader.frameCount;
		s_totalLagCount = tmpHeader.lagCount;
		g_totalInputCount = tmpHeader.inputCount;
		s_totalTickCount = s_tickCountAtLastInput = tmpHeader.tickCount;

		EnsureTmpInputSize((size_t)totalSavedBytes);
		s_totalBytes = totalSavedBytes;
		t_record.ReadArray(tmpInput, (size_t)s_totalBytes);
	}
	else if (s_currentByte > 0)
	{
		if (s_currentByte > totalSavedBytes)
		{
		}
		else if (s_currentByte > s_totalBytes)
		{
			afterEnd = true;
			PanicAlertT("Warning: You loaded a save that's after the end of the current movie. (byte %u > %u) (frame %u > %u). You should load another save before continuing, or load this state with read-only mode off.", (u32)s_currentByte+256, (u32)s_totalBytes+256, (u32)g_currentFrame, (u32)g_totalFrames);
		}
		else if (s_currentByte > 0 && s_totalBytes > 0)
		{
			// verify identical from movie start to the save's current frame
			u32 len = (u32)s_currentByte;
			u8* movInput = new u8[len];
			t_record.ReadArray(movInput, (size_t)len);
			for (u32 i = 0; i < len; ++i)
			{
				if (movInput[i] != tmpInput[i])
				{
					// this is a "you did something wrong" alert for the user's benefit.
					// we'll try to say what's going on in excruciating detail, otherwise the user might not believe us.
					if (IsUsingWiimote(0))
					{
						// TODO: more detail
						PanicAlertT("Warning: You loaded a save whose movie mismatches on byte %d (0x%X). You should load another save before continuing, or load this state with read-only mode off. Otherwise you'll probably get a desync.", i+256, i+256);
						memcpy(tmpInput, movInput, s_currentByte);
					}
					else
					{
						int frame = i / 8;
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

	s_bSaveConfig = tmpHeader.bSaveConfig;

	if (!afterEnd)
	{
		if (s_bReadOnly)
		{
			if (s_playMode != MODE_PLAYING)
			{
				s_playMode = MODE_PLAYING;
				Core::DisplayMessage("Switched to playback", 2000);
			}
		}
		else
		{
			if (s_playMode != MODE_RECORDING)
			{
				s_playMode = MODE_RECORDING;
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
	if (g_currentFrame > g_totalFrames || s_currentByte >= s_totalBytes || (CoreTiming::GetTicks() > s_totalTickCount && !IsRecordingInputFromSaveState()))
	{
		EndPlayInput(!s_bReadOnly);
	}
}

void PlayController(GCPadStatus* PadStatus, int controllerID)
{
	// Correct playback is entirely dependent on the emulator polling the controllers
	// in the same order done during recording
	if (!IsPlayingInput() || !IsUsingPad(controllerID) || tmpInput == nullptr)
		return;

	if (s_currentByte + 8 > s_totalBytes)
	{
		PanicAlertT("Premature movie end in PlayController. %u + 8 > %u", (u32)s_currentByte, (u32)s_totalBytes);
		EndPlayInput(!s_bReadOnly);
		return;
	}

	// dtm files don't save the mic button or error bit. not sure if they're actually used, but better safe than sorry
	signed char e = PadStatus->err;
	memset(PadStatus, 0, sizeof(GCPadStatus));
	PadStatus->err = e;


	memcpy(&s_padState, &(tmpInput[s_currentByte]), 8);
	s_currentByte += 8;

	PadStatus->triggerLeft = s_padState.TriggerL;
	PadStatus->triggerRight = s_padState.TriggerR;

	PadStatus->stickX = s_padState.AnalogStickX;
	PadStatus->stickY = s_padState.AnalogStickY;

	PadStatus->substickX = s_padState.CStickX;
	PadStatus->substickY = s_padState.CStickY;

	PadStatus->button |= PAD_USE_ORIGIN;

	if (s_padState.A)
	{
		PadStatus->button |= PAD_BUTTON_A;
		PadStatus->analogA = 0xFF;
	}
	if (s_padState.B)
	{
		PadStatus->button |= PAD_BUTTON_B;
		PadStatus->analogB = 0xFF;
	}
	if (s_padState.X)
		PadStatus->button |= PAD_BUTTON_X;
	if (s_padState.Y)
		PadStatus->button |= PAD_BUTTON_Y;
	if (s_padState.Z)
		PadStatus->button |= PAD_TRIGGER_Z;
	if (s_padState.Start)
		PadStatus->button |= PAD_BUTTON_START;

	if (s_padState.DPadUp)
		PadStatus->button |= PAD_BUTTON_UP;
	if (s_padState.DPadDown)
		PadStatus->button |= PAD_BUTTON_DOWN;
	if (s_padState.DPadLeft)
		PadStatus->button |= PAD_BUTTON_LEFT;
	if (s_padState.DPadRight)
		PadStatus->button |= PAD_BUTTON_RIGHT;

	if (s_padState.L)
		PadStatus->button |= PAD_TRIGGER_L;
	if (s_padState.R)
		PadStatus->button |= PAD_TRIGGER_R;
	if (s_padState.disc)
	{
		// This implementation assumes the disc change will only happen once. Trying to change more than that will cause
		// it to load the last disc every time. As far as i know though, there are no 3+ disc games, so this should be fine.
		Core::SetState(Core::CORE_PAUSE);
		bool found = false;
		std::string path;
		for (size_t i = 0; i < SConfig::GetInstance().m_ISOFolder.size(); ++i)
		{
			path = SConfig::GetInstance().m_ISOFolder[i];
			if (File::Exists(path + '/' + g_discChange))
			{
				found = true;
				break;
			}
		}
		if (found)
		{
			DVDInterface::ChangeDisc(path + '/' + g_discChange);
			Core::SetState(Core::CORE_RUN);
		}
		else
		{
			PanicAlertT("Change the disc to %s", g_discChange.c_str());
		}
	}

	SetInputDisplayString(s_padState, controllerID);
	CheckInputEnd();
}

bool PlayWiimote(int wiimote, u8 *data, const WiimoteEmu::ReportFeatures& rptf, int ext, const wiimote_key key)
{
	if (!IsPlayingInput() || !IsUsingWiimote(wiimote) || tmpInput == nullptr)
		return false;

	if (s_currentByte > s_totalBytes)
	{
		PanicAlertT("Premature movie end in PlayWiimote. %u > %u", (u32)s_currentByte, (u32)s_totalBytes);
		EndPlayInput(!s_bReadOnly);
		return false;
	}

	u8 size = rptf.size;

	u8 sizeInMovie = tmpInput[s_currentByte];

	if (size != sizeInMovie)
	{
		PanicAlertT("Fatal desync. Aborting playback. (Error in PlayWiimote: %u != %u, byte %u.)%s", (u32)sizeInMovie, (u32)size, (u32)s_currentByte,
					(s_numPads & 0xF)?" Try re-creating the recording with all GameCube controllers disabled (in Configure > GameCube > Device Settings)." : "");
		EndPlayInput(!s_bReadOnly);
		return false;
	}

	s_currentByte++;

	if (s_currentByte + size > s_totalBytes)
	{
		PanicAlertT("Premature movie end in PlayWiimote. %u + %d > %u", (u32)s_currentByte, size, (u32)s_totalBytes);
		EndPlayInput(!s_bReadOnly);
		return false;
	}

	memcpy(data, &(tmpInput[s_currentByte]), size);
	s_currentByte += size;

	SetWiiInputDisplayString(wiimote, data, rptf, ext, key);

	g_currentInputCount++;

	CheckInputEnd();
	return true;
}

void EndPlayInput(bool cont)
{
	if (cont)
	{
		s_playMode = MODE_RECORDING;
		Core::DisplayMessage("Reached movie end. Resuming recording.", 2000);
	}
	else if (s_playMode != MODE_NONE)
	{
		s_rerecords = 0;
		s_currentByte = 0;
		s_playMode = MODE_NONE;
		Core::UpdateWantDeterminism();
		Core::DisplayMessage("Movie End.", 2000);
		s_bRecordingFromSaveState = false;
		// we don't clear these things because otherwise we can't resume playback if we load a movie state later
		//g_totalFrames = s_totalBytes = 0;
		//delete tmpInput;
		//tmpInput = nullptr;
	}
}

void SaveRecording(const std::string& filename)
{
	File::IOFile save_record(filename, "wb");
	// Create the real header now and write it
	DTMHeader header;
	memset(&header, 0, sizeof(DTMHeader));

	header.filetype[0] = 'D'; header.filetype[1] = 'T'; header.filetype[2] = 'M'; header.filetype[3] = 0x1A;
	strncpy((char *)header.gameID, SConfig::GetInstance().m_LocalCoreStartupParameter.GetUniqueID().c_str(), 6);
	header.bWii = SConfig::GetInstance().m_LocalCoreStartupParameter.bWii;
	header.numControllers = s_numPads & (SConfig::GetInstance().m_LocalCoreStartupParameter.bWii ? 0xFF : 0x0F);

	header.bFromSaveState = s_bRecordingFromSaveState;
	header.frameCount = g_totalFrames;
	header.lagCount = s_totalLagCount;
	header.inputCount = g_totalInputCount;
	header.numRerecords = s_rerecords;
	header.recordingStartTime = s_recordingStartTime;

	header.bSaveConfig = true;
	header.bSkipIdle = s_bSkipIdle;
	header.bDualCore = s_bDualCore;
	header.bProgressive = s_bProgressive;
	header.bDSPHLE = s_bDSPHLE;
	header.bFastDiscSpeed = s_bFastDiscSpeed;
	strncpy((char *)header.videoBackend, s_videoBackend.c_str(),ArraySize(header.videoBackend));
	header.CPUCore = s_iCPUCore;
	header.bEFBAccessEnable = g_ActiveConfig.bEFBAccessEnable;
	header.bEFBCopyEnable = g_ActiveConfig.bEFBCopyEnable;
	header.bCopyEFBToTexture = g_ActiveConfig.bCopyEFBToTexture;
	header.bEFBCopyCacheEnable = g_ActiveConfig.bEFBCopyCacheEnable;
	header.bEFBEmulateFormatChanges = g_ActiveConfig.bEFBEmulateFormatChanges;
	header.bUseXFB = g_ActiveConfig.bUseXFB;
	header.bUseRealXFB = g_ActiveConfig.bUseRealXFB;
	header.memcards = s_memcards;
	header.bClearSave = g_bClearSave;
	header.bSyncGPU = s_bSyncGPU;
	header.bNetPlay = s_bNetPlay;
	strncpy((char *)header.discChange, g_discChange.c_str(),ArraySize(header.discChange));
	strncpy((char *)header.author, s_author.c_str(),ArraySize(header.author));
	memcpy(header.md5,s_MD5,16);
	header.bongos = s_bongos;
	memcpy(header.revision, s_revision, ArraySize(header.revision));
	header.DSPiromHash = s_DSPiromHash;
	header.DSPcoefHash = s_DSPcoefHash;
	header.tickCount = s_totalTickCount;

	// TODO
	header.uniqueID = 0;
	// header.audioEmulator;

	save_record.WriteArray(&header, 1);

	bool success = save_record.WriteArray(tmpInput, (size_t)s_totalBytes);

	if (success && s_bRecordingFromSaveState)
	{
		std::string stateFilename = filename + ".sav";
		success = File::Copy(tmpStateFilename, stateFilename);
	}

	if (success)
		Core::DisplayMessage(StringFromFormat("DTM %s saved", filename.c_str()), 2000);
	else
		Core::DisplayMessage(StringFromFormat("Failed to save %s", filename.c_str()), 2000);
}

void SetInputManip(ManipFunction func)
{
	mfunc = func;
}

void CallInputManip(GCPadStatus* PadStatus, int controllerID)
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

void GetSettings()
{
	s_bSaveConfig = true;
	s_bSkipIdle = SConfig::GetInstance().m_LocalCoreStartupParameter.bSkipIdle;
	s_bDualCore = SConfig::GetInstance().m_LocalCoreStartupParameter.bCPUThread;
	s_bProgressive = SConfig::GetInstance().m_LocalCoreStartupParameter.bProgressive;
	s_bDSPHLE = SConfig::GetInstance().m_LocalCoreStartupParameter.bDSPHLE;
	s_bFastDiscSpeed = SConfig::GetInstance().m_LocalCoreStartupParameter.bFastDiscSpeed;
	s_videoBackend = g_video_backend->GetName();
	s_bSyncGPU = SConfig::GetInstance().m_LocalCoreStartupParameter.bSyncGPU;
	s_iCPUCore = SConfig::GetInstance().m_LocalCoreStartupParameter.iCPUCore;
	s_bNetPlay = NetPlay::IsNetPlayRunning();
	if (!SConfig::GetInstance().m_LocalCoreStartupParameter.bWii)
		g_bClearSave = !File::Exists(SConfig::GetInstance().m_strMemoryCardA);
	s_memcards |= (SConfig::GetInstance().m_EXIDevice[0] == EXIDEVICE_MEMORYCARD) << 0;
	s_memcards |= (SConfig::GetInstance().m_EXIDevice[1] == EXIDEVICE_MEMORYCARD) << 1;
	unsigned int tmp;
	for (int i = 0; i < 20; ++i)
	{
		sscanf(&scm_rev_git_str[2 * i], "%02x", &tmp);
		s_revision[i] = tmp;
	}
	if (!s_bDSPHLE)
	{
		std::string irom_file = File::GetUserPath(D_GCUSER_IDX) + DSP_IROM;
		std::string coef_file = File::GetUserPath(D_GCUSER_IDX) + DSP_COEF;

		if (!File::Exists(irom_file))
			irom_file = File::GetSysDirectory() + GC_SYS_DIR DIR_SEP DSP_IROM;
		if (!File::Exists(coef_file))
			coef_file = File::GetSysDirectory() + GC_SYS_DIR DIR_SEP DSP_COEF;
		std::vector<u16> irom(DSP_IROM_SIZE);
		File::IOFile file_irom(irom_file, "rb");

		file_irom.ReadArray(irom.data(), DSP_IROM_SIZE);
		file_irom.Close();
		for (int i = 0; i < DSP_IROM_SIZE; ++i)
			irom[i] = Common::swap16(irom[i]);

		std::vector<u16> coef(DSP_COEF_SIZE);
		File::IOFile file_coef(coef_file, "rb");

		file_coef.ReadArray(coef.data(), DSP_COEF_SIZE);
		file_coef.Close();
		for (int i = 0; i < DSP_COEF_SIZE; ++i)
			coef[i] = Common::swap16(coef[i]);
		s_DSPiromHash = HashAdler32((u8*)irom.data(), DSP_IROM_BYTE_SIZE);
		s_DSPcoefHash = HashAdler32((u8*)coef.data(), DSP_COEF_BYTE_SIZE);
	}
	else
	{
		s_DSPiromHash = 0;
		s_DSPcoefHash = 0;
	}
}

void CheckMD5()
{
	for (int i = 0, n = 0; i < 16; ++i)
	{
		if (tmpHeader.md5[i] != 0)
			continue;
		n++;
		if (n == 16)
			return;
	}
	Core::DisplayMessage("Verifying checksum...", 2000);

	unsigned char gameMD5[16];
	char game[255];
	memcpy(game, SConfig::GetInstance().m_LocalCoreStartupParameter.m_strFilename.c_str(), SConfig::GetInstance().m_LocalCoreStartupParameter.m_strFilename.size());
	md5_file(game, gameMD5);

	if (memcmp(gameMD5,s_MD5,16) == 0)
		Core::DisplayMessage("Checksum of current game matches the recorded game.", 2000);
	else
		Core::DisplayMessage("Checksum of current game does not match the recorded game!", 3000);
}

void GetMD5()
{
	Core::DisplayMessage("Calculating checksum of game file...", 2000);
	memset(s_MD5, 0, sizeof(s_MD5));
	char game[255];
	memcpy(game, SConfig::GetInstance().m_LocalCoreStartupParameter.m_strFilename.c_str(),SConfig::GetInstance().m_LocalCoreStartupParameter.m_strFilename.size());
	md5_file(game, s_MD5);
	Core::DisplayMessage("Finished calculating checksum.", 2000);
}

void Shutdown()
{
	g_currentInputCount = g_totalInputCount = g_totalFrames = s_totalBytes = s_tickCountAtLastInput = 0;
	delete [] tmpInput;
	tmpInput = nullptr;
	tmpInputAllocated = 0;
}
};
