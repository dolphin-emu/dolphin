#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ogcsys.h>
#include <gccore.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>
#include <wiiuse/wpad.h>

// Pull in the assembly functions.
extern "C" {
void TestFRES1(u32 *fpscr, float *result, float *result2);
};

int doreload=0, dooff=0;
void reload() {	doreload=1; }
void shutdown() { dooff=1; }


void Compare(const char *a, const char *b) {
	if (!strcmp(a, b)) {
		printf("SUCCESS - %s\n", a);
	} else {
		printf("FAIL - %s != \n"
			   "       %s\n", a, b);
	}
}

void TestDivision() {
	double a, b, c, d, e;
	a = 1.0;
	b = 0.0;
	c = a / b;
	d = b / a;
	e = sqrt(-1);
	char temp[100];
	sprintf(temp, "%1.1f %1.1f %1.1f %1.1f %1.1f", a, b, c, d, e);
	Compare(temp, "1.0 0.0 inf 0.0 nan");
}

void TestFres() {
	u32 fpscr;
	float out, out2;
	TestFRES1(&fpscr, &out, &out2);
	char temp[100];
	sprintf(temp, "%08x %1.1f %1.1f", fpscr, out, out2);
	Compare(temp, "ffc00004 inf 0.0");
}

void TestNormalize() {
	//float a[3] = {2,2,2};
	//d_guVecNormalize(a);
	//printf("%f %f %f\n", a[0], a[1], a[2]);
}


int main(int argc, char **argv) {
	void *xfb[2];
	int fbi = 0;
	GXRModeObj *rmode = NULL;

	VIDEO_Init();
	PAD_Init();
	WPAD_Init();

	rmode = VIDEO_GetPreferredMode(NULL);

	// double buffering, prevents flickering (is it needed for LCD TV? i don't have one to test)
	xfb[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	xfb[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb[0]);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (rmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();

	SYS_SetResetCallback(reload);
	SYS_SetPowerCallback(shutdown);

	WPAD_SetDataFormat(0, WPAD_FMT_BTNS_ACC_IR);
	WPAD_SetVRes(0, rmode->fbWidth, rmode->xfbHeight);

	CON_Init(xfb[fbi],0,0,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);

	printf(" ");
	printf("Tests\n\n");

	TestDivision();
	TestFres();

	while (!doreload && !dooff) {
		WPAD_ScanPads();
		if (WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME)
			exit(0);

		VIDEO_SetNextFramebuffer(xfb[fbi]);
		VIDEO_Flush();
		VIDEO_WaitVSync();
	}
	if(doreload) return 0;
	if(dooff) SYS_ResetSystem(SYS_SHUTDOWN,0,0);

	return 0;
}
