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

#ifdef __APPLE__
#include <sys/param.h>
#endif

#if defined HAVE_X11 && HAVE_X11
#include <X11/Xlib.h>
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

void Host_NotifyMapLoaded(){}

void Host_ShowJitResults(unsigned int address){}

Common::Event updateMainFrameEvent;
void Host_Message(int Id)
{
#if defined(HAVE_X11) && HAVE_X11
	if (Id == WM_USER_STOP)
		updateMainFrameEvent.Set();
#endif
}

void Host_UpdateLogDisplay(){}


void Host_UpdateDisasmDialog(){}


void Host_UpdateMainFrame()
{
	updateMainFrameEvent.Set();
}

void Host_UpdateBreakPointView(){}


void Host_UpdateMemoryView(){}


void Host_SetDebugMode(bool){}


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
		while (Core::GetState() == Core::CORE_UNINITIALIZED)
			updateMainFrameEvent.Wait();
		updateMainFrameEvent.Wait();
		Core::Stop();
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
