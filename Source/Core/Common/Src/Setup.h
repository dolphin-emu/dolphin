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

#ifndef _SETUP_H_
#define _SETUP_H_

// -----------------------------------------------------------------------------------------------------
// File description:
// Compilation settings. I avoid placing this in Common.h or some place where lots of files needs
// to be rebuilt if any of these settings are changed. I'd rather have it in as few files as possible.
// This file can be kept on the ignore list in your SVN program. It allows local optional settings
// depending on what works on your computer.
// -----------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------
// Settings:

// This may remove sound artifacts in Wario Land Shake It and perhaps other games
//#define SETUP_AVOID_SOUND_ARTIFACTS

/* This may fix a problem with Stop and Start that I described in the comments to revision 2,139,
   and in the comments in the File Description for PluginManager.cpp */
//#define SETUP_FREE_VIDEO_PLUGIN_ON_BOOT
//#define SETUP_FREE_DSP_PLUGIN_ON_BOOT
//#define SETUP_DONT_FREE_PLUGIN_ON_STOP

/* This will avoid deleting the g_EmuThread after Stop, that may hang when we are rendering to a child
   window, however, I didn't seem to need this any more */
//#define SETUP_AVOID_CHILD_WINDOW_RENDERING_HANG

// Build with playback rerecording options
//#define SETUP_AVOID_OPENGL_SCREEN_MESSAGE_HANG

// Use a timer to wait for threads for stop instead of WaitForEternity()
/* I tried that this worked with these options
		SETUP_FREE_VIDEO_PLUGIN_ON_BOOT
		SETUP_DONT_FREE_PLUGIN_ON_STOP
   then the Confirm on Close message box doesn't hang, and we have a controlled Shutdown process
   without any hanged threads. The downside is a few error messages in the ShutDown() of the
   OpenGL plugin, so I still need FreeLibrary() to clean it, even with this option. */
//#define SETUP_TIMER_WAITING

// Build with playback rerecording options
//#define RERECORDING

// -----------------------------------------------------------------------------------------------------

#endif // _SETUP_H_

