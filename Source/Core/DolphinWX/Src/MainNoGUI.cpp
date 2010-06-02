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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#ifndef _WIN32
#include <sys/param.h>
#else

#endif

#include "Common.h"
#include "FileUtil.h"

#if defined HAVE_X11 && HAVE_X11
#include <X11/keysym.h>
#include "State.h"
#include "X11Utils.h"
#endif

#if defined(HAVE_COCOA) && HAVE_COCOA
#import "cocoaApp.h"
#endif

#include "Core.h"
#include "Globals.h"
#include "Host.h"
#include "ISOFile.h"
#include "CPUDetect.h"
#include "cmdline.h"
#include "Thread.h"
#include "PowerPC/PowerPC.h"

#include "PluginManager.h"
#include "ConfigManager.h"
#include "LogManager.h"
#include "BootManager.h"

#if defined HAVE_X11 && HAVE_X11
bool running = true;
#endif

bool rendererHasFocus = true;

void Host_NotifyMapLoaded(){}

void Host_ShowJitResults(unsigned int address){}

Common::Event updateMainFrameEvent;
void Host_Message(int Id)
{
#if defined(HAVE_X11) && HAVE_X11
	switch (Id)
	{
		case WM_USER_STOP:
			running = false;
			break;
	}
#endif
}

void Host_UpdateTitle(const char* title){};

void Host_UpdateLogDisplay(){}


void Host_UpdateDisasmDialog(){}


void Host_UpdateMainFrame()
{
	updateMainFrameEvent.Set();
}

void Host_UpdateBreakPointView(){}


void Host_UpdateMemoryView(){}


void Host_SetDebugMode(bool){}

void Host_RequestWindowSize(int& x, int& y, int& width, int& height)
{
	x = SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowXPos;
	y = SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowYPos;
	width = SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowWidth;
	height = SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowHeight;
}

bool Host_RendererHasFocus()
{
	return rendererHasFocus;
}

void Host_SetWaitCursor(bool enable){}


void Host_UpdateStatusBar(const char* _pText, int Filed){}

void Host_SysMessage(const char *fmt, ...) 
{
	va_list list;
	char msg[512];

	va_start(list, fmt);
	vsprintf(msg, fmt, list);
	va_end(list);

	size_t len = strlen(msg);
	if (msg[len - 1] != '\n') {
		msg[len - 1] = '\n';
		msg[len] = '\0';
	}
	fprintf(stderr, "%s", msg);
}

void Host_SetWiiMoteConnectionState(int _State) {}

#if defined(HAVE_X11) && HAVE_X11
void X11_MainLoop()
{
	bool fullscreen = SConfig::GetInstance().m_LocalCoreStartupParameter.bFullscreen;
	while (Core::GetState() == Core::CORE_UNINITIALIZED)
		updateMainFrameEvent.Wait();

	Display *dpy = XOpenDisplay(0);
	Window win = *(Window *)Core::GetXWindow();
	XSelectInput(dpy, win, KeyPressMask | KeyReleaseMask | FocusChangeMask);

#if defined(HAVE_XRANDR) && HAVE_XRANDR
	X11Utils::XRRConfiguration *XRRConfig = new X11Utils::XRRConfiguration(dpy, win);
#endif

	Cursor blankCursor = NULL;
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bHideCursor)
	{
		// make a blank cursor
		Pixmap Blank;
		XColor DummyColor;
		char ZeroData[1] = {0};
		Blank = XCreateBitmapFromData (dpy, win, ZeroData, 1, 1);
		blankCursor = XCreatePixmapCursor(dpy, Blank, Blank, &DummyColor, &DummyColor, 0, 0);
		XFreePixmap (dpy, Blank);
		XDefineCursor(dpy, win, blankCursor);
	}

	if (fullscreen)
	{
		X11Utils::EWMH_Fullscreen(_NET_WM_STATE_TOGGLE);
#if defined(HAVE_XRANDR) && HAVE_XRANDR
		XRRConfig->ToggleDisplayMode(True);
#endif
	}

	// The actual loop
	while (running)
	{
		XEvent event;
		KeySym key;
		for (int num_events = XPending(dpy); num_events > 0; num_events--)
		{
			XNextEvent(dpy, &event);
			switch(event.type)
			{
				case KeyPress:
					key = XLookupKeysym((XKeyEvent*)&event, 0);
					if (key == XK_Escape)
					{
						if (Core::GetState() == Core::CORE_RUN)
						{
							if (SConfig::GetInstance().m_LocalCoreStartupParameter.bHideCursor)
								XUndefineCursor(dpy, win);
							Core::SetState(Core::CORE_PAUSE);
						}
						else
						{
							if (SConfig::GetInstance().m_LocalCoreStartupParameter.bHideCursor)
								XDefineCursor(dpy, win, blankCursor);
							Core::SetState(Core::CORE_RUN);
						}
					}
					else if ((key == XK_Return) && (event.xkey.state & Mod1Mask))
					{
						fullscreen = !fullscreen;
						X11Utils::EWMH_Fullscreen(_NET_WM_STATE_TOGGLE);
#if defined(HAVE_XRANDR) && HAVE_XRANDR
						XRRConfig->ToggleDisplayMode(fullscreen);
#endif
					}
					else if (key >= XK_F1 && key <= XK_F8)
					{
						int slot_number = key - XK_F1 + 1;
						if (event.xkey.state & ShiftMask)
							State_Save(slot_number);
						else
							State_Load(slot_number);
					}
					else if (key == XK_F9)
						Core::ScreenShot();
					else if (key == XK_F11)
						State_LoadLastSaved();
					else if (key == XK_F12)
					{
						if (event.xkey.state & ShiftMask)
							State_UndoLoadState();
						else
							State_UndoSaveState();
					}
					break;
				case FocusIn:
					rendererHasFocus = true;
					if (SConfig::GetInstance().m_LocalCoreStartupParameter.bHideCursor &&
							Core::GetState() != Core::CORE_PAUSE)
						XDefineCursor(dpy, win, blankCursor);
					break;
				case FocusOut:
					rendererHasFocus = false;
					if (SConfig::GetInstance().m_LocalCoreStartupParameter.bHideCursor)
						XUndefineCursor(dpy, win);
					break;
			}
		}
		if (!fullscreen)
		{
			Window winDummy;
			unsigned int borderDummy, depthDummy;
			XGetGeometry(dpy, win, &winDummy,
					&SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowXPos,
					&SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowYPos,
					(unsigned int *)&SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowWidth,
					(unsigned int *)&SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowHeight,
					&borderDummy, &depthDummy);
		}
		usleep(100000);
	}

#if defined(HAVE_XRANDR) && HAVE_XRANDR
	delete XRRConfig;
#endif
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bHideCursor)
		XFreeCursor(dpy, blankCursor);
	XCloseDisplay(dpy);
	Core::Stop();
}
#endif

//for cocoa we need to hijack the main to get event
#if defined(HAVE_COCOA) && HAVE_COCOA

@interface CocoaThread : NSObject
{
	NSThread *Thread;
}
- (void)cocoaThreadStart;
- (void)cocoaThreadRun:(id)sender;
- (void)cocoaThreadQuit:(NSNotification*)note;
- (bool)cocoaThreadRunning;
@end

static NSString *CocoaThreadHaveFinish = @"CocoaThreadHaveFinish";

int cocoaArgc;
char **cocoaArgv;
int appleMain(int argc, char *argv[]);

@implementation CocoaThread

- (void)cocoaThreadStart
{

	[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(cocoaThreadQuit:) name:CocoaThreadHaveFinish object:nil];
	[NSThread detachNewThreadSelector:@selector(cocoaThreadRun:) toTarget:self withObject:nil];

}

- (void)cocoaThreadRun:(id)sender
{

	NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
	Thread = [NSThread currentThread];
	//launch main
	appleMain(cocoaArgc,cocoaArgv);
	
	[[NSNotificationCenter defaultCenter] postNotificationName:CocoaThreadHaveFinish object:nil];

	[pool release];

}

- (void)cocoaThreadQuit:(NSNotification*)note
{

	[[NSNotificationCenter defaultCenter] removeObserver:self];

}

- (bool)cocoaThreadRunning
{
	if([Thread isFinished])
		return false;
	else 
		return true;
}

@end


int main(int argc, char *argv[])
{

	cocoaArgc = argc;
	cocoaArgv = argv;

	cocoaCreateApp();

	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

	CocoaThread *thread = [[CocoaThread alloc] init];
	NSEvent *event = [[NSEvent alloc] init];	
	
	[thread cocoaThreadStart];

	//cocoa event loop
	while(1)
	{
		event = [NSApp nextEventMatchingMask:NSAnyEventMask untilDate:[NSDate distantPast] inMode:NSDefaultRunLoopMode dequeue:YES ];
		if(cocoaSendEvent(event))
		{
			Core::Stop();
			break;
		}
		if(![thread cocoaThreadRunning])
			break;
	}	


	[event release];
	[thread release];
	[pool release];
}


int appleMain(int argc, char *argv[])
#else
// Include SDL header so it can hijack main().
#if defined(USE_SDL) && USE_SDL
#include <SDL.h>
#endif
int main(int argc, char* argv[])
#endif
{
	gengetopt_args_info args_info;

	if (cmdline_parser(argc, argv, &args_info) != 0)
		return(1);

	if (args_info.inputs_num < 1)
	{
		fprintf(stderr, "Please supply at least one argument - the ISO to boot.\n");
		return(1);
	}
	std::string bootFile(args_info.inputs[0]);

	updateMainFrameEvent.Init();

	LogManager::Init();
	EventHandler::Init();
	SConfig::Init();
	CPluginManager::Init();

	CPluginManager::GetInstance().ScanForPlugins();

#if defined HAVE_X11 && HAVE_X11
	XInitThreads();
#endif 

	if (BootManager::BootCore(bootFile)) //no use running the loop when booting fails
	{
#if defined(HAVE_X11) && HAVE_X11
		X11_MainLoop();
#else
		while (PowerPC::GetState() != PowerPC::CPU_POWERDOWN)
			updateMainFrameEvent.Wait();
#endif
	}
	updateMainFrameEvent.Shutdown();

	CPluginManager::Shutdown();
	SConfig::Shutdown();
	EventHandler::Shutdown();
	LogManager::Shutdown();

	cmdline_parser_free (&args_info);
	return(0);
}
