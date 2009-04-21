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
	WPAD_Init();

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
		u32 sentinel = 0;

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
				sentinel++;
			}
		}

		if (sentinel == 0)
			printf("empty\n");

		closedir(pdir);
		printf("\n");
	}
	else
	{
		printf("opendir() failure.\n");
	}
}

int main()
{
	bool canList = false;

	Initialise();

	printf("\x1b[10;0H");

	if(fatInitDefault())
	{
		printf("\nPress A to list dirs\n");
		canList = true;
	}
	else
		printf("\nfatInitDefault() failure.\n");

	while(1)
	{
		// Call WPAD_ScanPads each loop, this reads the latest controller states
		PAD_ScanPads();
		WPAD_ScanPads();

		// WPAD_ButtonsDown tells us which buttons were wpressed in this loop
		// this is a "one shot" state which will not fire again until the button has been released
		u32 pressed = PAD_ButtonsDown(0);
		u32 wpressed = WPAD_ButtonsDown(0);

		if ((wpressed & WPAD_BUTTON_A || pressed & PAD_BUTTON_A) && canList)
			dirlist("/");

		// We return to the launcher application via exit
		if (wpressed & WPAD_BUTTON_HOME || pressed & PAD_BUTTON_START)
			exit(0);

		// Wait for the next frame
		VIDEO_WaitVSync();
	}

	return 0;
}
