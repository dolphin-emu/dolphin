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

void *Initialise();
#define NUM_BLOCKS 10
u32 aram_blocks[NUM_BLOCKS];
u32 start_addr = -1;


int main(int argc, char **argv) {

	xfb = Initialise();
	start_addr = AR_Init(aram_blocks, NUM_BLOCKS);
	while(1) {
		PAD_ScanPads();
		VIDEO_ClearFrameBuffer(rmode, xfb, 0);
		
		std::cout << "Aram is " << (AR_GetDMAStatus() ? "In Progress" : "Idle") << std::endl;
		std::cout << "Aram Start is 0x" << std::setbase(16) << start_addr << ", Allocated 0x" << AR_GetSize() << " Of memory" << std::setbase(10) << std::endl;
		std::cout << "Internal Size is 0x" << std::setbase(16) << AR_GetInternalSize() << std::setbase(10) << std::endl;
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
