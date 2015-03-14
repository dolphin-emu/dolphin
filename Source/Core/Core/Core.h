// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


// Core

// The external interface to the emulator core. Plus some extras.
// This is another part of the emu that needs cleaning - Core.cpp really has
// too much random junk inside.

#pragma once

#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/CoreParameter.h"

// TODO: ugly, remove
extern bool g_aspect_wide;

namespace Core
{

extern bool g_want_determinism;

bool GetIsFramelimiterTempDisabled();
void SetIsFramelimiterTempDisabled(bool disable);

void Callback_VideoCopiedToXFB(bool video_update);

// Action Replay culling code brute-forcing by penkamaster
// take photo
extern int ch_take_screenshot;
// current code
extern int ch_current_position;
// move on to next code?
extern bool ch_next_code;
// start searching
extern bool ch_begin_search;
extern bool ch_first_search;
// number of windows messages without saving a screenshot
extern int ch_cycles_without_snapshot;
// search last
extern bool ch_last_search;
// emulator is in action replay culling code brute-forcing mode
extern bool ch_bruteforce;

extern std::vector<std::string> ch_map;
extern std::string ch_title_id;
extern std::string ch_code;


enum EState
{
	CORE_UNINITIALIZED,
	CORE_PAUSE,
	CORE_RUN,
	CORE_STOPPING
};

bool Init();
void Stop();
void Shutdown();
void KillDolphinAndRestart();

std::string StopMessage(bool, std::string);

bool IsRunning();
bool IsRunningAndStarted(); // is running and the CPU loop has been entered
bool IsRunningInCurrentThread(); // this tells us whether we are running in the CPU thread.
bool IsCPUThread(); // this tells us whether we are the CPU thread.
bool IsGPUThread();

void SetState(EState _State);
EState GetState();

void SaveScreenShot();

void Callback_WiimoteInterruptChannel(int _number, u16 _channelID, const void* _pData, u32 _Size);

// This displays messages in a user-visible way.
void DisplayMessage(const std::string& message, int time_in_ms);

std::string GetStateFileName();
void SetStateFileName(std::string val);

void SetBlockStart(u32 addr);

bool ShouldSkipFrame(int skipped);
bool ShouldAddTimewarpFrame();
void VideoThrottle();
void RequestRefreshInfo();

void UpdateTitle();

// waits until all systems are paused and fully idle, and acquires a lock on that state.
// or, if doLock is false, releases a lock on that state and optionally unpauses.
// calls must be balanced (once with doLock true, then once with doLock false) but may be recursive.
// the return value of the first call should be passed in as the second argument of the second call.
bool PauseAndLock(bool doLock, bool unpauseOnUnlock=true);

// for calling back into UI code without introducing a dependency on it in core
typedef void(*StoppedCallbackFunc)(void);
void SetOnStoppedCallback(StoppedCallbackFunc callback);

// Run on the GUI thread when the factors change.
void UpdateWantDeterminism(bool initial = false);

}  // namespace
