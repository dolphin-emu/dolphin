#include <stdio.h>
#include <stdlib.h>
#include <gccore.h>
#include <sdcard/wiisd_io.h>
#include <fat.h>
#include <dirent.h>
#include <wiiuse/wpad.h>
#include <unistd.h>
#include <string.h>

#include "Init.h"

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;
void die(char *msg) {
	if (f!=NULL) fclose(f);
	printf(msg);
	sleep(5);
	fatUnmount("sd");
	__io_wiisd.shutdown();
	exit(0);
}

void initialise_fat() {
	__io_wiisd.startup();
	if (!fatInitDefault())
		die("Unable to initialise FAT subsystem, exiting.\n");
	fatMountSimple("sd", &__io_wiisd);
	DIR_ITER *root = diropen("/");
	if (!root)
		die("Cannot open root dir, exiting.\n");
	dirclose(root);
	if (chdir("/"))
		die("Could not change to root directory, exiting.\n");
}

void init_crap() {
	VIDEO_Init();
	WPAD_Init();
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

void end() {
	int columns = 0, rows = 0;
	CON_GetMetrics(&columns, &rows);
	printf("\x1b[%i;0H",rows);
	printf("File written... press Home/Start to exit.");
	while(1) {
		WPAD_ScanPads(); PAD_ScanPads();
		if ((WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME) || (PAD_ButtonsDown(0) & PAD_BUTTON_START))
			exit(0);
		VIDEO_WaitVSync();
	}
}
