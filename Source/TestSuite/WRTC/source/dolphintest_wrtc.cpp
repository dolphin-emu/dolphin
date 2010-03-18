#include <gccore.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <ogcsys.h>
#include <time.h>
#include <sys/time.h>
#include <ogc/lwp_watchdog.h>
#include <iostream>
#include <debug.h>
#include <math.h>

static void *xfb = NULL;

u32 first_frame = 1;
GXRModeObj *rmode;

inline void Initialise()
{
	// Initialise the video system
	VIDEO_Init();

	// This function initialises the attached controllers
	PAD_Init();

	// Obtain the preferred video mode from the system
	// This will correspond to the settings in the Wii menu
	rmode = VIDEO_GetPreferredMode(NULL);

	// Allocate memory for the display in the uncached region
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

	// Initialise the console, required for printf
	console_init(xfb,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);

	// Set up the video registers with the chosen mode
	VIDEO_Configure(rmode);

	// Tell the video hardware where our display memory is
	VIDEO_SetNextFramebuffer(xfb);

	// Make the display visible
	VIDEO_SetBlack(FALSE);

	// Flush the video register changes to the hardware
	VIDEO_Flush();

	// Wait for Video setup to complete
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();
}

int main()
{
	Initialise();

	time_t wii_time;
//settime(secs_to_ticks(time(NULL) - 0x386D4380));
	wii_time = time(NULL);

	std::cout << "\x1b[20;0HWii RTC time is " <<ctime(&wii_time) <<" "<< wii_time;

	while(1)
	{
		wii_time = time(NULL);
		std::cout << "\x1b[10;0HWii RTC time is " << ctime(&wii_time) <<" "<< wii_time;

		VIDEO_WaitVSync();
	}
}
