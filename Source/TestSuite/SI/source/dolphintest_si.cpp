#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ogcsys.h>
#include <gccore.h>
#include <iostream>
#include <iomanip>
#include <unistd.h>

static void *xfb;
static GXRModeObj *rmode;

void Initialise();


int main(int argc, char **argv)
{
	Initialise();

	while(1) {
		PAD_ScanPads();
		VIDEO_ClearFrameBuffer(rmode, xfb, 0);
		std::cout<<"\x1b[0;0H"; // Position the cursor (at 0, 0)
		for(int Chan = 0; Chan < 4; Chan++)
		{
			std::cout << "Chan " << Chan << std::endl;
			std::cout << "Status is " << SI_GetStatus(Chan) << std::endl;
			std::cout << "Type is 0x" << std::setbase(16) << SI_GetType(Chan) << std::setbase(10) << std::endl << std::endl;
		}
		VIDEO_WaitVSync();
	}

	return 0;
}

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
