#include <aram.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ogcsys.h>
#include <gccore.h>
#include <iostream>
#include <iomanip>

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

void Initialise();
#define NUM_BLOCKS 10
u32 aram_blocks[NUM_BLOCKS];
u32 start_addr = -1;


int main(int argc, char **argv)
{
	Initialise();
	start_addr = AR_Init(aram_blocks, NUM_BLOCKS);
	while(1)
	{
		PAD_ScanPads();
		VIDEO_ClearFrameBuffer(rmode, xfb, 0);
		std::cout<<"\x1b[2;0H"; // Position the cursor (on 2nd row)

		std::cout << "Aram is " << (AR_GetDMAStatus() ? "In Progress" : "Idle") << std::endl;
		std::cout << "Aram Start is 0x" << std::setbase(16) << start_addr << ", Allocated 0x" << AR_GetSize() << " Of memory" << std::setbase(10) << std::endl;
		std::cout << "Internal Size is 0x" << std::setbase(16) << AR_GetInternalSize() << std::setbase(10) << std::endl;
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
