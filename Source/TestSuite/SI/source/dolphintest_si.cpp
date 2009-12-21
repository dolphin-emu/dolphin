#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ogcsys.h>
#include <gccore.h>
#include <iostream>
#include <iomanip>
#include <unistd.h>
#include <fat.h>

#ifdef HW_RVL
#include <wiiuse/wpad.h>
#include <sdcard/wiisd_io.h>
#endif

static void *xfb;
static GXRModeObj *rmode;

void Initialise();
void (*reboot)() = (void(*)())0x80001800;
static u32* const SI_REG = (u32*)0xCD006400;
static bool haveInit = false;
static int counter = 0;
static bool logWritten = false;

void AppendSDLog()
{
#ifdef HW_RVL
	FILE *f = fopen("sd:/si_log.txt", "a");
	if (f)
	{
		fprintf(f, "\n-------------------------------------\n");
		for (int i = 0; i < 4; i++)
			fprintf(f, "%i\tstatus: %x\t type:%x\n", i, SI_GetStatus(i), SI_GetType(i));
		u32 x = 0;
		fprintf(f, "-------------------------------------\n");
		fprintf(f, "SI_CHANNEL_0_OUT\t%08x\n", SI_REG[x++]);
		fprintf(f, "SI_CHANNEL_0_IN_HI\t%08x\n", SI_REG[x++]);
		fprintf(f, "SI_CHANNEL_0_IN_LO\t%08x\n", SI_REG[x++]);
		fprintf(f, "SI_CHANNEL_1_OUT\t%08x\n", SI_REG[x++]);
		fprintf(f, "SI_CHANNEL_1_IN_HI\t%08x\n", SI_REG[x++]);
		fprintf(f, "SI_CHANNEL_1_IN_LO\t%08x\n", SI_REG[x++]);
		fprintf(f, "SI_CHANNEL_2_OUT\t%08x\n", SI_REG[x++]);
		fprintf(f, "SI_CHANNEL_2_IN_HI\t%08x\n", SI_REG[x++]);
		fprintf(f, "SI_CHANNEL_2_IN_LO\t%08x\n", SI_REG[x++]);
		fprintf(f, "SI_CHANNEL_3_OUT\t%08x\n", SI_REG[x++]);
		fprintf(f, "SI_CHANNEL_3_IN_HI\t%08x\n", SI_REG[x++]);
		fprintf(f, "SI_CHANNEL_3_IN_LO\t%08x\n", SI_REG[x++]);
		fprintf(f, "SI_POLL\t\t\t%08x\n", SI_REG[x++]);
		fprintf(f, "SI_COM_CSR\t\t\t%08x\n", SI_REG[x++]);
		fprintf(f, "SI_STATUS_REG\t\t%08x\n", SI_REG[x++]);
		fprintf(f, "SI_EXI_CLOCK_COUNT\t%08x\n", SI_REG[x++]);
		fprintf(f, "-------------------------------------\n");
		fclose(f);
	}
#endif
}

int main(int argc, char **argv)
{
	Initialise();

	while(1) {
		if (haveInit) PAD_ScanPads();

		VIDEO_ClearFrameBuffer(rmode, xfb, COLOR_BLACK);

		printf("\x1b[4;0H");

		for(int Chan = 0; Chan < 4; Chan++)
			printf("%i\tstatus: %x\t type:%x\n", Chan, SI_GetStatus(Chan), SI_GetType(Chan));

		printf("SI Regs: (cc006000)\n");
		for (u32 i = 0; i < 16/*num SI regs*/; ++i)
		{
			printf("%08x ", SI_REG[i]);
			if ((i+1)%8==0) printf("\n");
		}

		if (haveInit)
			printf("\nPAD_Init\n");

		VIDEO_WaitVSync();

		if (haveInit)
		{
			if (PAD_ButtonsDown(0) & PAD_BUTTON_START)
			{
				AppendSDLog();
			#ifdef HW_RVL
				fatUnmount("sd");
				__io_wiisd.shutdown();
			#endif
				reboot();
			}
		}

		counter++;
		AppendSDLog();
		if (counter > 5 && !haveInit)
		{
			PAD_Init();
			haveInit = true;
		}
		else if (haveInit && !logWritten)
		{
			logWritten = true;
		}
	}

	return 0;
}

void Initialise()
{
	// Initialise the video system
	VIDEO_Init();

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

#ifdef HW_RVL
	// Initialize FAT so we can write to SD.
	__io_wiisd.startup();
	fatMountSimple("sd", &__io_wiisd);
#endif
}
