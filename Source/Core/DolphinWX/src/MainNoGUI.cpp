#include <stdio.h>
#include <stdlib.h>
#ifndef _WIN32
//#include <curses.h>
#else
#endif

#include "Globals.h"
#include "Common.h"
#include "ISOFile.h"
#include "CPUDetect.h"

#include "BootManager.h"
void* g_pCodeWindow = NULL;
void* main_frame = NULL;
bool wxPanicAlert(const char* text, bool /*yes_no*/)
{
	return(true);
}


void Host_BootingStarted(){}


void Host_BootingEnded(){}


// OK, this thread boundary is DANGEROUS on linux
// wxPostEvent / wxAddPendingEvent is the solution.
void Host_NotifyMapLoaded(){}


void Host_UpdateLogDisplay(){}


void Host_UpdateDisasmDialog(){}


void Host_UpdateMainFrame(){}

void Host_UpdateBreakPointView(){}


void Host_UpdateMemoryView(){}


void Host_SetDebugMode(bool){}


void Host_SetWaitCursor(bool enable){}


void Host_CreateDisplay(){}


void Host_CloseDisplay(){}

void Host_UpdateStatusBar(const char* _pText){}

// Include SDL header so it can hijack main().
#include <SDL.h>

int main(int argc, char* argv[])
{
	if (argc != 2)
	{
		puts("Please supply at least one argument - the ISO to boot.\n");
		return(1);
	}
	std::string bootFile(argv[1]);

	DetectCPU();
	BootManager::BootCore(bootFile);
	usleep(2000 * 1000 * 1000);
//	while (!getch()) {
	//	usleep(20);
//	}
	return(0);
}


