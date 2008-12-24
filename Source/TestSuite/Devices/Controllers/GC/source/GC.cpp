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


static u32* xfb = NULL;

u32 first_frame = 1;
GXRModeObj *rmode;
vu16 oldstate;
vu16 keystate;
vu16 keydown;
vu16 keyup;
PADStatus pad[4];

int main() {

	VIDEO_Init();
	
	rmode = VIDEO_GetPreferredMode(NULL);
	
	PAD_Init();

	
	xfb = (u32*)MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
		
	VIDEO_Configure(rmode);
		
	VIDEO_SetNextFramebuffer(xfb);
	
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) 
		VIDEO_WaitVSync();
	
	
	console_init(xfb,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*2);


	time_t gc_time;
	gc_time = time(NULL);

	srand(gc_time);

	while(1) {

		gc_time = time(NULL);
		VIDEO_ClearFrameBuffer(rmode, xfb, 0);
		DrawPixel(0,0, 255, 0, 0);
		PAD_Read(pad);
		for(int a = 0; a < 4;a ++)
		{
			if(pad[a].err & PAD_ERR_NO_CONTROLLER)
				continue; // Controller not connected
			printf("pad[%d] Sticks, Main[ %d, %d ] Sub[ %d, %d ]\n", a,  pad[a].stickX, pad[a].stickY, pad[a].substickX, pad[a].substickY);
			printf("pad[%d] LTrigger is %s, LTrigger Analog %d\n", a, pad[a].button & PAD_TRIGGER_L ? "pressed" : "not pressed", pad[a].triggerL);
			printf("pad[%d] RTrigger is %s, RTrigger Analog %d\n", a, pad[a].button & PAD_TRIGGER_R ? "pressed" : "not pressed", pad[a].triggerR);
			printf("pad[%d] Trigger Z is %s\n",  a, pad[a].button & PAD_TRIGGER_Z ? "pressed" : "not pressed");
			printf("pad[%d] Button A %s, Analog A %d\n", a, pad[a].button & PAD_BUTTON_A ? "pressed" : "not pressed", pad[a].analogA);
			printf("pad[%d] Button B %s, Analog B %d\n", a, pad[a].button & PAD_BUTTON_B ? "pressed" : "not pressed", pad[a].analogB);
			printf("pad[%d] Button X is %s\n",  a, pad[a].button & PAD_BUTTON_X ? "pressed" : "not pressed");
			printf("pad[%d] Button Y is %s\n",  a, pad[a].button & PAD_BUTTON_Y ? "pressed" : "not pressed");
			printf("pad[%d] Button Start is %s\n",  a, pad[a].button & PAD_BUTTON_START ? "pressed" : "not pressed");
			printf("pad[%d] DPad is [up, down, left, right](%d, %d, %d, %d)\n",  a, pad[a].button & PAD_BUTTON_UP ? 1 : 0, pad[a].button & PAD_BUTTON_DOWN ? 1 : 0, pad[a].button & PAD_BUTTON_LEFT ? 1 : 0, pad[a].button & PAD_BUTTON_RIGHT ? 1 : 0);
		}
		VIDEO_WaitVSync();

		int buttonsDown = PAD_ButtonsDown(0);
		
		if (buttonsDown & PAD_BUTTON_START) {
			exit(0);
		}
	}
	
}
