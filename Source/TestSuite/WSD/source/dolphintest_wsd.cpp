// Thanks to:
// SD Card Directory Listing Demo
// Updated 12/19/2008 by PunMaster

#include <gccore.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <ogcsys.h>
#include <time.h>
#include <sys/time.h>
#include <wiiuse/wpad.h>
#include <fat.h>
#include <dirent.h>

#include <iostream>
#include <debug.h>
#include <math.h>

static void *xfb = NULL;

u32 first_frame = 1;
GXRModeObj *rmode;

void Initialise()
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

void dirlist(char* path)
{
	DIR* pdir = opendir(path);

	if (pdir != NULL)
	{
		while(true) 
		{
			struct dirent* pent = readdir(pdir);
			if(pent == NULL) break;

			if(strcmp(".", pent->d_name) != 0 && strcmp("..", pent->d_name) != 0)
			{
				char dnbuf[260];
				sprintf(dnbuf, "%s/%s", path, pent->d_name);

				struct stat statbuf;
				stat(dnbuf, &statbuf);

				if(S_ISDIR(statbuf.st_mode))
				{
					printf("%s <DIR>\n", dnbuf);
					dirlist(dnbuf);
				}
				else
				{
					printf("%s (%d)\n", dnbuf, (int)statbuf.st_size);
				}

			}
		}

		closedir(pdir);
	}
	else
	{
		printf("opendir() failure.\n");
	}
}

int main()
{
	Initialise();

	if(fatInitDefault())
		dirlist("/");
	else
		printf("fatInitDefault() failure.\n");

	while(1)
	{
		// Call WPAD_ScanPads each loop, this reads the latest controller states
		WPAD_ScanPads();

		// WPAD_ButtonsDown tells us which buttons were pressed in this loop
		// this is a "one shot" state which will not fire again until the button has been released
		u32 pressed = WPAD_ButtonsDown(0);

		// We return to the launcher application via exit
		if (pressed & WPAD_BUTTON_HOME)
			exit(0);

		// Wait for the next frame
		VIDEO_WaitVSync();
	}

	return 0;
}
