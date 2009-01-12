#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#ifndef _WIN32
//#include <curses.h>
#else
#endif


#include "Globals.h"
#include "Host.h"
#include "Common.h"
#include "ISOFile.h"
#include "CPUDetect.h"
#include "cmdline.h"
#include "Thread.h"
#include "PowerPC/PowerPC.h"

#if defined(HAVE_COCOA) && HAVE_COCOA
#include "cocoaApp.h"
#endif

#include "BootManager.h"
void* g_pCodeWindow = NULL;
void* main_frame = NULL;
bool wxPanicAlert(const char* text, bool /*yes_no*/)
{
	return(true);
}


// OK, this thread boundary is DANGEROUS on linux
// wxPostEvent / wxAddPendingEvent is the solution.
void Host_NotifyMapLoaded(){}


void Host_UpdateLogDisplay(){}


void Host_UpdateDisasmDialog(){}


Common::Event updateMainFrameEvent;
void Host_UpdateMainFrame()
{
	updateMainFrameEvent.Set();
}

void Host_UpdateBreakPointView(){}


void Host_UpdateMemoryView(){}


void Host_SetDebugMode(bool){}


void Host_SetWaitCursor(bool enable){}


void Host_UpdateStatusBar(const char* _pText){}

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
	fprintf(stderr, msg);
}

void Host_UpdateLeds(int led_bits)
{
}
void Host_UpdateSpeakerStatus(int index, int speaker_bits)
{
}
void Host_UpdateStatus()
{
}

void Host_SetWiiMoteConnectionState(int _State) {}


//for cocoa we need to hijack the main to get event
#if defined(HAVE_COCOA) && HAVE_COCOA
int appleMain(int argc, char *argv[]);

int main(int argc, char *argv[])
{

	cocoaCreateApp();
	return appleMain(argc, argv);

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

	if (cmdline_parser (argc, argv, &args_info) != 0)
		return(1);

	if (args_info.inputs_num < 1)
	{
		fprintf(stderr, "Please supply at least one argument - the ISO to boot.\n");
		return(1);
	}
	std::string bootFile(args_info.inputs[0]);

	updateMainFrameEvent.Init();
	DetectCPU();
	BootManager::BootCore(bootFile);
	while (PowerPC::state != PowerPC::CPU_POWERDOWN)
	{
		updateMainFrameEvent.Wait();
	}

	cmdline_parser_free (&args_info);
	return(0);
}
