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
	g_FrameCounter = 0;
	ReRecTimer.Start();

	// Logging
	//Console::Print("RerecordingStart: %i\n", g_FrameCounter);
}

// Reset the frame counter
void RerecordingStop()
{
	// Write the final time and Stop the timer
	ReRecTimer.Stop();

	// Update status bar
	WriteStatus();
}

/* Wind back the frame counter when a save state is loaded. Currently we don't know what that means in
   time so we just guess that the time is proportional the the number of frames
   
   Todo: There are many assumptions here: We probably want to replace the time here by the actual time
   that we save together with the save state or the input recording for example. And have it adjusted
   for full speed playback (whether it's 30 fps or 60 fps or some other speed that the game is natively
   capped at). Also the input interrupts do not occur as often as the frame renderings, they occur more
   often. So we may want to move the input recording to fram updates, or perhaps sync the input interrupts
   to frame updates.
   */
void WindBack(int Counter)
{
	/* Counter should be smaller than g_FrameCounter, however it currently updates faster than the
	   frames so currently it may not be the same. Therefore I use the abs() function. */
	int AbsoluteFrameDifference = abs(g_FrameCounter - Counter);
	float FractionalFrameDifference = (float) AbsoluteFrameDifference / (float) g_FrameCounter;

	// Update the frame counter
	g_FrameCounter = Counter;

	// Approximate a time to wind back the clock to
	// Get the current time
	u64 CurrentTimeMs = ReRecTimer.GetTimeElapsed();
	// Save the current time in seconds in a new double
	double CurrentTimeSeconds = (double) (CurrentTimeMs / 1000);
	// Reduce it by the same proportion as the counter was wound back
	CurrentTimeSeconds = CurrentTimeSeconds * FractionalFrameDifference;
	// Update the clock
	ReRecTimer.WindBackStartingTime((u64)CurrentTimeSeconds * 1000);

	// Logging
	Console::Print("WindBack: %i %u\n", Counter, (u64)CurrentTimeSeconds);
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
	/* I don't think the frequent update has any material speed inpact at all, but should it
	   have you can controls the update speed by changing the "% 10" in this line */
	//if (g_FrameCounter % 10 == 0) WriteStatus();

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