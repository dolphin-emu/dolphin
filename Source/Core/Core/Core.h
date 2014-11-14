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
