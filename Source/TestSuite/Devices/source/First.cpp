#include <gccore.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <ogcsys.h>
#include <time.h>
#include <sys/time.h>
#include <iostream>
#include <iomanip>

#include <iostream>
#include <debug.h>
#include <math.h>

static void *xfb = NULL;
u32 first_frame = 1;
GXRModeObj *rmode;

int main()
{
	VIDEO_Init();
	
	rmode = VIDEO_GetPreferredMode(NULL);
	
	PAD_Init();

	
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
		
	VIDEO_Configure(rmode);
		
	VIDEO_SetNextFramebuffer(xfb);
	
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();
	
	
	console_init(xfb,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*2);


	time_t gc_time;
	gc_time = time(NULL);

	srand(gc_time);
	while(1)
	{
		s32 Size;
		s32 SSize;
		u32 ID;
		s32 error;
		VIDEO_ClearFrameBuffer(rmode, xfb, 0);
			
		EXI_GetID(0, 0, &ID);
		error = CARD_ProbeEx(0, &Size, &SSize);
		// Can't use printf since Dolphin overrides it
		if(error >= 0) // All is good
			std::cout << "Card A has a size of " << Size << " and a Sector Size of " << SSize;
		else if( error == -2) // Device isn't Memory card
				std::cout << "Device isn't Mem card in Slot A";
		else
			std::cout << "Got an error of " << error << " with slot A";
		std::cout << " ID of " << std::setbase(16) << ID << std::endl;
			
		std::cout << std::setbase(10);
		EXI_GetID(1, 0, &ID);
		error = CARD_ProbeEx(1, &Size, &SSize);
		if(error >= 0) // All is good
			std::cout << "Card B has a size of " << Size << " and a Sector Size of " << SSize;
		else if( error == -2) // Device isn't Memory card
				std::cout << "Device isn't Mem card in Slot B";
		else
			std::cout << "Got an error of " << error << " with slot B";
		std::cout << " ID of " << std::setbase(16) << ID << std::endl;
		VIDEO_WaitVSync();
	}
	return 0;
}
