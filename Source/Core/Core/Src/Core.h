// Copyright (C) 2003-2009 Dolphin Project.

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


// Core

// The external interface to the emulator core. Plus some extras.
// This is another part of the emu that needs cleaning - Core.cpp really has
// too much random junk inside.

#ifndef _CORE_H
#define _CORE_H

#include <vector>
#include <string>

#include "Common.h"
#include "CoreParameter.h"

namespace Core
{
    enum EState
    {
        CORE_UNINITIALIZED,
        CORE_PAUSE,
        CORE_RUN,
    };

    // Init core
    bool Init();
    void Stop();

	bool isRunning();
    
    void SetState(EState _State);
    EState GetState();

	void ScreenShot(const std::string& name);
	void ScreenShot();
    
    // Get core parameters kill use SConfig instead
    const SCoreStartupParameter& GetStartupParameter();
    extern SCoreStartupParameter g_CoreStartupParameter; 

    void* GetWindowHandle();
    bool GetRealWiimote();
	void ReconnectWiimote();
	void ReconnectPad();

    extern bool bReadTrace;
    extern bool bWriteTrace;
    
    void StartTrace(bool write);
    void DisplayMessage(const std::string &message, int time_in_ms); // This displays messages in a user-visible way.
    void DisplayMessage(const char *message, int time_in_ms); // This displays messages in a user-visible way.
    
    int SyncTrace();
    void SetBlockStart(u32 addr);
    void StopTrace();

    // -----------------------------------------
	#ifdef SETUP_TIMER_WAITING
	// -----------------
		// Thread shutdown
		void VideoThreadEnd();
	#endif
	// ---------------------------

	// -----------------------------------------
	#ifdef RERECORDING
	// -----------------
		void FrameUpdate();
		void FrameAdvance();
		void FrameStepOnOff();
		void WriteStatus();
		void RerecordingStart();
		void RerecordingStop();
		void WindBack(int Counter);
		extern int g_FrameCounter;
		extern bool g_FrameStep;
	#endif
	// ---------------------------

}  // namespace

#endif

