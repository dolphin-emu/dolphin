#include <gccore.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <ogcsys.h>
#include <unistd.h>
#include <iostream>
#include <iomanip>

#include <debug.h>
#include <math.h>

static void *xfb = NULL;
u32 first_frame = 1;
GXRModeObj *rmode;

void Initialise();

int main()
{
	Initialise();

	while(1)
	{
		s32 Size;
		s32 SSize;
		u32 ID;
		s32 getIDerr;
		s32 CARDerr;
		s32 EXIerr;
		VIDEO_ClearFrameBuffer(rmode, xfb, 0);

		// Can't use printf since Dolphin overrides it
		std::cout<<"\x1b[0;0H"; // Position the cursor (at 0, 0)
		for (int channel = 0; channel < EXI_CHANNEL_MAX; ++channel)
			for (int device = 0; device < EXI_DEVICE_MAX; ++device)
			{
				if (getIDerr = EXI_GetID(channel, device, &ID) == 1)
				{
					std::cout<<"Channel "<<channel<<" Device "<<device<<"\tID = 0x"<<std::setbase(16)<<ID<<std::endl;
					if ((channel == 0 && device == 0)||(channel == 1 && device == 0))
					{
						// It's a memcard slot
						if (CARDerr = CARD_ProbeEx(channel, &Size, &SSize) >= 0)
						{
							std::cout<<"\tMemcard has a size of "<<std::setbase(10)<<Size<<" and a Sector Size of "<<std::setbase(10)<<SSize<<std::endl;
						}
						else if (CARDerr == -2)
							std::cout<<"\tNot a Memcard!"<<std::endl;
						else
							std::cout<<"\tCARD Error "<<CARDerr<<std::endl;
					}
					else
					{
						// It's not a memcard, what to do? - Just probe for now
						if (EXIerr = EXI_ProbeEx(channel)){}
						else
							std::cout<<"\tEXI Error "<<EXIerr<<std::endl;
					}
				}
				else
					std::cout<<"Channel "<<channel<<" Device "<<device<<std::endl<<"\tEXI_GetID Error "<<getIDerr<<std::endl;
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
