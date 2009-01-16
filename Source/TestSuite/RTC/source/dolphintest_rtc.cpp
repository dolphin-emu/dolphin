#include <gccore.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <ogcsys.h>
#include <time.h>
#include <sys/time.h>

#include <iostream>
#include <debug.h>
#include <math.h>


static void *xfb = NULL;

u32 first_frame = 1;
GXRModeObj *rmode;


int main() {

	VIDEO_Init();
	
	rmode = VIDEO_GetPreferredMode(NULL);

	
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

	while(1) {

		gc_time = time(NULL);
		printf("\x1b[10;0HRTC time is %s     ",ctime(&gc_time));

		VIDEO_WaitVSync();
	}
	
}
