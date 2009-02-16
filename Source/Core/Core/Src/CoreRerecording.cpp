// Copyright (C) 2003-2008 Dolphin Project.
 
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
 
 
#include "Setup.h"
#ifdef RERECORDING


//////////////////////////////////////////////////////////////////////////////////////////
// Include
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯
#ifdef _WIN32
	#include <windows.h>
#endif

#include "Thread.h" // Common

#include "Timer.h"
#include "Common.h"
#include "ConsoleWindow.h"

#include "Console.h"
#include "Core.h"
#include "CPUDetect.h"
#include "CoreTiming.h"
#include "Boot/Boot.h"
#include "PatchEngine.h"
 
#include "HW/Memmap.h"
#include "HW/PeripheralInterface.h"
#include "HW/GPFifo.h"
#include "HW/CPU.h"
#include "HW/CPUCompare.h"
#include "HW/HW.h"
#include "HW/DSP.h"
#include "HW/GPFifo.h"
#include "HW/AudioInterface.h"
#include "HW/VideoInterface.h"
#include "HW/CommandProcessor.h"
#include "HW/PixelEngine.h"
#include "HW/SystemTimers.h"
 
#include "PowerPC/PowerPC.h"
 
#include "PluginManager.h"
#include "ConfigManager.h"
 
#include "MemTools.h"
#include "Host.h"
#include "LogManager.h"
////////////////////////////////////////



///////////////////////////////////////////////////////////////////////////////////////////
// File description: Rerecording Functions
/* ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯

How the timer works: We measure the time between drawn frames, not when the game is paused. So time
should be a fairly comparable measure of the time it took to play the game. However the time it takes
to draw a frame will be lower on a fast computer. Therefore we could perhaps measure time as an
internal game time that is adjusted by the average time it takes to draw a frame. Also if it only takes
ten or twenty milliseconds to draw a frame I'm not certain about how accurate the mmsystem timers are for
such short periods.

//////////////////////////////////////*/



namespace Core
{


///////////////////////////////////////////////////////////////////////////////////////////
// Declarations and definitions
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
int g_FrameCounter = 0;
bool g_FrameStep = false;
Common::Timer ReRecTimer;
////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////////////
// Control Run, Pause, Stop and the Timer.
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯

// Subtract the paused time when we run again
void Run()
{
	ReRecTimer.AddTimeDifference();
}
// Update the time
void Pause()
{
	ReRecTimer.Update();
}

// Start the timer when a game is booted
void RerecordingStart()
{
	g_FrameCounter == 0;
	ReRecTimer.Start();
}

// Reset the frame counter
void RerecordingStop()
{
	// Write the final time and Stop the timer
	ReRecTimer.Stop();

	// Update status bar
	WriteStatus();
}
////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////////////
// Frame advance
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void FrameAdvance()
{
	// Update status bar
	WriteStatus();

	// If a game is not started, return
	if(Core::GetState() == Core::CORE_UNINITIALIZED) return;

	// Play to the next frame
	if(g_FrameStep)
	{
		Run();
		Core::SetState(Core::CORE_RUN);	
	}

}
// Turn on frame stepping
void FrameStepOnOff()
{
	/* Turn frame step on or off. If a game is running and we turn this on it means that the game
	   will pause after the next frame update */
	g_FrameStep = !g_FrameStep;

	// Update status bar
	WriteStatus();

	// If a game is not started, return
	if(Core::GetState() == Core::CORE_UNINITIALIZED) return;

	// Run the emulation if we turned off framestepping
	if (!g_FrameStep)
	{
		Run();
		Core::SetState(Core::CORE_RUN);
	}
}
////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////////////
// General functions
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯

// Write to the status bar
void WriteStatus()
{
	std::string TmpStr = "Time: " + ReRecTimer.GetTimeElapsedFormatted();
	TmpStr += StringFromFormat("  Frame: %s", ThS(g_FrameCounter).c_str());
	// The FPS is the total average since the game was booted
	TmpStr += StringFromFormat("  FPS: %i", (g_FrameCounter * 1000) / ReRecTimer.GetTimeElapsed());
	TmpStr += StringFromFormat("  FrameStep: %s", g_FrameStep ? "On" : "Off");
	Host_UpdateStatusBar(TmpStr.c_str(), 1);	
}


// When a new frame is drawn
void FrameUpdate()
{
	// Write to the status bar
	WriteStatus();

	// Pause if frame stepping is on
	if(g_FrameStep)
	{
		Pause();
		Core::SetState(Core::CORE_PAUSE);
	}

	// Count one frame
	g_FrameCounter++;
}
////////////////////////////////////////


} // Core


#endif // RERECORDING