// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.


// Core

// The external interface to the emulator core. Plus some extras.
// This is another part of the emu that needs cleaning - Core.cpp really has
// too much random junk inside.

#pragma once

#include <functional>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

namespace Core
{

// TODO: ugly, remove
extern bool g_aspect_wide;

extern bool g_want_determinism;

bool GetIsThrottlerTempDisabled();
void SetIsThrottlerTempDisabled(bool disable);

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

void DeclareAsCPUThread();
void UndeclareAsCPUThread();

std::string StopMessage(bool, const std::string&);

bool IsRunning();
bool IsRunningAndStarted(); // is running and the CPU loop has been entered
bool IsRunningInCurrentThread(); // this tells us whether we are running in the CPU thread.
bool IsCPUThread(); // this tells us whether we are the CPU thread.
bool IsGPUThread();

// [NOT THREADSAFE] For use by Host only
void SetState(EState state);
EState GetState();

void SaveScreenShot();
void SaveScreenShot(const std::string& name);

void Callback_WiimoteInterruptChannel(int _number, u16 _channelID, const void* _pData, u32 _Size);

// This displays messages in a user-visible way.
void DisplayMessage(const std::string& message, int time_in_ms);

std::string GetStateFileName();
void SetStateFileName(const std::string& val);

void SetBlockStart(u32 addr);

void FrameUpdateOnCPUThread();

bool ShouldSkipFrame(int skipped);
void VideoThrottle();
void RequestRefreshInfo();

void UpdateTitle();

// waits until all systems are paused and fully idle, and acquires a lock on that state.
// or, if doLock is false, releases a lock on that state and optionally unpauses.
// calls must be balanced (once with doLock true, then once with doLock false) but may be recursive.
// the return value of the first call should be passed in as the second argument of the second call.
// [NOT THREADSAFE] Host only
bool PauseAndLock(bool doLock, bool unpauseOnUnlock = true);

// for calling back into UI code without introducing a dependency on it in core
typedef void(*StoppedCallbackFunc)(void);
void SetOnStoppedCallback(StoppedCallbackFunc callback);

// Run on the Host thread when the factors change. [NOT THREADSAFE]
void UpdateWantDeterminism(bool initial = false);

// Queue an arbitrary function to asynchronously run once on the Host thread later.
// Threadsafe. Can be called by any thread, including the Host itself.
// Jobs will be executed in RELATIVE order. If you queue 2 jobs from the same thread
// then they will be executed in the order they were queued; however, there is no
// global order guarantee across threads - jobs from other threads may execute in
// between.
// NOTE: Make sure the jobs check the global state instead of assuming everything is
//   still the same as when the job was queued.
// NOTE: Jobs that are not set to run during stop will be discarded instead.
void QueueHostJob(std::function<void()> job, bool run_during_stop = false);

// Should be called periodically by the Host to run pending jobs.
// WM_USER_JOB_DISPATCH will be sent when something is added to the queue.
void HostDispatchJobs();

}  // namespace
