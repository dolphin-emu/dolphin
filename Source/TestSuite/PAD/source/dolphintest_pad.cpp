#include <gccore.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <ogcsys.h>

#include <iostream>
#include <debug.h>
#include <math.h>


static void* xfb = NULL;

u32 first_frame = 1;
GXRModeObj *rmode;
vu16 oldstate;
vu16 keystate;
vu16 keydown;
vu16 keyup;
PADStatus pad[4];

void Initialise();


int main()
{
	Initialise();

	while(1)
	{
		VIDEO_ClearFrameBuffer(rmode, xfb, 0);
		std::cout<<"\x1b[0;0H"; // Position the cursor (at 0, 0)
		PAD_Read(pad);
		for(int a = 0; a < 4;a ++)
		{
			if(pad[a].err & PAD_ERR_NO_CONTROLLER)
			{
				std::cout<<"pad["<<a<<"] Not Connected\n";
				continue;
			}
			std::cout<<"pad["<<a<<"] Sticks: Main[ "<<(int)pad[a].stickX<<", "<<(int)pad[a].stickY<<" ] Sub[ "<<(int)pad[a].substickX<<", "<<(int)pad[a].substickX<<" ]\n";
			std::cout<<"pad["<<a<<"] Analog Triggers: Left "<<(int)pad[a].triggerL<<" Right "<<(int)pad[a].triggerL<<"\n";
			std::cout<<"pad["<<a<<"] Buttons: "<<
				(pad[a].button & PAD_BUTTON_START? "Start " : "")<<
				(pad[a].button & PAD_BUTTON_A ? "A " : "")<<
				(pad[a].button & PAD_BUTTON_B ? "B " : "")<<
				(pad[a].button & PAD_BUTTON_X ? "X " : "")<<
				(pad[a].button & PAD_BUTTON_Y ? "Y " : "")<<
				(pad[a].button & PAD_TRIGGER_Z? "Z " : "")<<
				(pad[a].button & PAD_TRIGGER_L? "L " : "")<<
				(pad[a].button & PAD_TRIGGER_R? "R " : "")<<std::endl;
			std::cout<<"pad["<<a<<"] DPad: "<<
				(pad[a].button & PAD_BUTTON_UP ? "Up " : "")<<
				(pad[a].button & PAD_BUTTON_DOWN ? "Down " : "")<<
				(pad[a].button & PAD_BUTTON_LEFT ? "Left " : "")<<
				(pad[a].button & PAD_BUTTON_RIGHT ? "Right " : "")<<std::endl;
			}
		VIDEO_WaitVSync();

		int buttonsDown = PAD_ButtonsDown(0);
	}
	
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
