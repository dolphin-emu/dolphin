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

void *Initialise();
int main(int argc, char **argv) {

	xfb = Initialise();
	
	while(1) {
		PAD_ScanPads();
		VIDEO_ClearFrameBuffer(rmode, xfb, 0);
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

void * Initialise() {

	void *framebuffer;

	VIDEO_Init();
	PAD_Init();
	
	rmode = VIDEO_GetPreferredMode(NULL);

	framebuffer = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	console_init(framebuffer,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);
	
	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(framebuffer);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();

	return framebuffer;

}
