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

#ifndef _HOST_H
#define _HOST_H


// Host - defines an interface for the emulator core to communicate back to the
// OS-specific layer

// The emulator core is abstracted from the OS using 2 interfaces:
// Common and Host.

// Common simply provides OS-neutral implementations of things like threads, mutexes,
// INI file manipulation, memory mapping, etc. 

// Host is an abstract interface for communicating things back to the host. The emulator
// core is treated as a library, not as a main program, because it is far easier to
// write GUI interfaces that control things than to squash GUI into some model that wasn't
// designed for it.

// The host can be just a command line app that opens a window, or a full blown debugger
// interface.

bool Host_RendererHasFocus();
void Host_ConnectWiimote(int wm_idx, bool connect);
bool Host_GetKeyState(int keycode);
void Host_GetRenderWindowSize(int& x, int& y, int& width, int& height);
void Host_Message(int Id);
void Host_NotifyMapLoaded();
void Host_RefreshDSPDebuggerWindow();
void Host_RequestRenderWindowSize(int width, int height);
void Host_SetStartupDebuggingParameters();
void Host_SetWiiMoteConnectionState(int _State);
void Host_ShowJitResults(unsigned int address);
void Host_SysMessage(const char *fmt, ...);
void Host_UpdateBreakPointView();
void Host_UpdateDisasmDialog();
void Host_UpdateLogDisplay();
void Host_UpdateMainFrame();
void Host_UpdateStatusBar(const char* _pText, int Filed = 0);
void Host_UpdateTitle(const char* title);

// TODO (neobrain): Remove these from host!
void* Host_GetInstance();
void* Host_GetRenderHandle();

#endif
