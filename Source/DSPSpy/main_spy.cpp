// This is a test program for running code on the Wii DSP, with full control over input
// and automatic compare with output. VERY useful for figuring out what those little
// ops actually do.
// It's very unpolished though
// Use Dolphin's dsptool to generate a new dsp_code.h.
// Originally written by duddie and modified by FIRES.

#include <gccore.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ogcsys.h>
#include <time.h>
#include <fat.h>
#include <fcntl.h>
#include <wiiuse/wpad.h>

#include "color.h"
#include "network.h"
#include "dsp.h"
#include "asm.h"
#include "processor.h"
#include "irq.h"
#include "dsp.h"

// Pull in some constants etc from DSPCore.
#include "../Core/DSPCore/Src/gdsp_registers.h"

// This is where the DSP binary is.
#include "dsp_code.h"

// DSPCR bits
#define DSPCR_DSPRESET      0x0800        // Reset DSP
#define DSPCR_ARDMA         0x0200        // ARAM dma in progress, if set
#define DSPCR_DSPINTMSK     0x0100        // * interrupt mask   (RW)
#define DSPCR_DSPINT        0x0080        // * interrupt active (RWC)
#define DSPCR_ARINTMSK      0x0040
#define DSPCR_ARINT         0x0020
#define DSPCR_AIINTMSK      0x0010
#define DSPCR_AIINT         0x0008
#define DSPCR_HALT          0x0004        // halt DSP
#define DSPCR_PIINT         0x0002        // assert DSP PI interrupt
#define DSPCR_RES           0x0001        // reset DSP

// Used for communications with the DSP, such as dumping registers etc.
u16 dspbuffer[16 * 1024] __attribute__ ((aligned (0x4000))); 

// #define ENABLE_OUT
#undef ENABLE_OUT

static void *xfb = NULL;
void (*reload)() = (void(*)())0x80001800;
GXRModeObj *rmode;

static vu16* const _dspReg = (u16*)0xCC005000;

u16 *dspbufP;
u16 *dspbufC;
u32 *dspbufU;

u16 dspreg_in[32] = {
	0x0410, 0x0510, 0x0610, 0x0710, 0x0810, 0x0910, 0x0a10, 0x0b10, 
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x0855, 0x0966, 0x0a77, 0x0b88,
	0x0014, 0xfff5, 0x00ff, 0x2200, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0003, 0x0004, 0x8000, 0x000C, 0x0007, 0x0008, 0x0009, 0x000a,
}; ///            ax_h_1   ax_h_1

/* ttt ?

u16 dspreg_in[32] = {
0x0e4c, 0x03c0, 0x0bd9, 0x06a3, 0x0c06, 0x0240, 0x0010, 0x0ecc, 
0x0000, 0x0000, 0x0000, 0x0000, 0x0322, 0x0000, 0x0000, 0x0000,
0x0000, 0x0000, 0x00ff, 0x1b41, 0x0000, 0x0040, 0x00ff, 0x0000,
0x1000, 0x96cc, 0x0000, 0x0000, 0x3fc0, 0x96cc, 0x0000, 0x0000,
}; */

// if i set bit 0x4000 of SR my tests crashes :(               

/*
// zelda 0x00da
u16 dspreg_in[32] = {
0x0a50, 0x0ca2, 0x04f8, 0x0ab0, 0x8039, 0x0000, 0x0000, 0x0000, 
0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x03d1, 0x0000, 0x0418, 0x0002,     // r08 must have a value ... no idea why (ector: it's the looped addressing regs)
0x0000, 0x0000, 0x00ff, 0x1804, 0xdb70, 0x4ddb, 0x0000, 0x0000,
0x0000, 0x0000, 0x0000, 0xde6d, 0x0000, 0x0000, 0x0000, 0x004e,
};*/ 

#include "mem_dump.h"

void ds_text_out(int xpos, int ypos, const char *str);
void ds_set_colour(int f, int b);
void ds_init(void *framebuffer, int xstart, int ystart, int xres, int yres, int stride);
void ds_underline(int xpos, int ypos, int len, int col);
void ds_printf(int x, int y, const char *fmt, ...);
void ds_clear(void);

u16 dspreg_out[1000][32];

u32 padding[1024];

// UI (interactive register editing)
u32 ui_mode;
#define UIM_SEL			1
#define UIM_EDIT_REG	2
#define	UIM_EDIT_BIN	4

// Currently selected register.
s32 cursor_reg = 0;
// Currently selected digit.
s32 small_cursor_x;
// Value currently being edited.
u16 *reg_value;  

// Got regs to draw. Dunno why we need this.
volatile int regs_refreshed = false;


// Handler for DSP interrupt.
static void my__dsp_handler(u32 nIrq,void *pCtx)
{
	// Acknowledge interrupt?
	_dspReg[5] = (_dspReg[5]&~(DSPCR_AIINT|DSPCR_ARINT)) | DSPCR_DSPINT;
}


// When comparing regs, ignore the loop stack registers.
bool regs_equal(int reg, u16 value1, u16 value2) {
	if (reg >= DSP_REG_ST0 && reg <= DSP_REG_ST3)
		return true;
	else
		return value1 == value2;
}

void print_reg_block(int x, int y, int sel, const u16 *regs, const u16 *compare_regs)
{
	for (int j = 0; j < 4 ; j++)
	{
		for (int i = 0; i < 8 ; i++)
		{
			// Do not even display the loop stack registers.
			if (j != 1 || i < 4)
			{
				const int reg = j * 8 + i;
				ds_set_colour(sel == reg ? COLOR_YELLOW : COLOR_GREEN, COLOR_BLACK);
				ds_printf(x + j * 8, i + y, "%02x ", reg);
				ds_set_colour(regs_equal(reg, regs[reg], compare_regs[reg]) ? COLOR_WHITE : COLOR_RED, COLOR_BLACK);
				ds_printf(x + 3 + j * 8, i + y, "%04x", regs[reg]);
			}
		}
	}
	ds_set_colour(COLOR_WHITE, COLOR_BLACK);

	ds_printf(x+2, y+9,  "ACC0: %02x %04x %04x", regs[DSP_REG_ACH0]&0xff, regs[DSP_REG_ACM0], regs[DSP_REG_ACL0]);
	ds_printf(x+2, y+10, "ACC1: %02x %04x %04x", regs[DSP_REG_ACH1]&0xff, regs[DSP_REG_ACM1], regs[DSP_REG_ACL1]);
	ds_printf(x+2, y+11, "AX0: %04x %04x", regs[DSP_REG_AXH0], regs[DSP_REG_AXL0]);
	ds_printf(x+2, y+12, "AX1: %04x %04x", regs[DSP_REG_AXH1], regs[DSP_REG_AXL1]);
}

void print_regs(int _step, int _dsp_steps)
{
	const u16 *regs = _step == 0 ? dspreg_in : dspreg_out[_step - 1];
	const u16 *regs2 = dspreg_out[_step];

	print_reg_block(0, 2, cursor_reg, regs, regs2);
	print_reg_block(33, 2, cursor_reg, regs2, regs);

	ds_set_colour(COLOR_WHITE, COLOR_BLACK);
	ds_printf(33, 17, "%i / %i      ", _step + 1, _dsp_steps);

	return;

	static int count = 0;
	int x = 0, y = 16;
	if (count > 2)
		ds_clear();
	count = 0;
	ds_set_colour(COLOR_WHITE, COLOR_BLACK);
	for (int i = 0x0; i < 0xf70 ; i++)
	{
		if (dspbufC[i] != mem_dump[i])
		{
			ds_printf(x, y, "%04x=%04x", i, dspbufC[i]);
			count++;
			x += 10;
			if (x >= 60) {
				x = 0;
				y++;
			}
		}
	}
	ds_printf(4, 25, "%08x", count);
}

void ui_pad_sel(void)
{
	if (WPAD_ButtonsDown(0) & WPAD_BUTTON_RIGHT)
	{
		cursor_reg+=8;
	}
	if (WPAD_ButtonsDown(0) & WPAD_BUTTON_LEFT)
	{
		cursor_reg-=8;
	}
	if (WPAD_ButtonsDown(0) & WPAD_BUTTON_UP)
	{
		cursor_reg--;
	}
	if (WPAD_ButtonsDown(0) & WPAD_BUTTON_DOWN)
	{
		cursor_reg++;
	}
	cursor_reg &= 0x1f;
	if (WPAD_ButtonsDown(0) & WPAD_BUTTON_A)
	{
		ui_mode = UIM_EDIT_REG;
		reg_value = &dspreg_in[cursor_reg * 8];
	}
}

/*
void ui_pad_edit_bin(void)
{
	u8 pos;
	if (WPAD_ButtonsDown(0) & WPAD_BUTTON_RIGHT)
	{
		small_cursor_x++;
	}
	if (WPAD_ButtonsDown(0) & WPAD_BUTTON_LEFT)
	{
		small_cursor_x--;
	}
	small_cursor_x &= 0xf;
	if (WPAD_ButtonsDown(0) & WPAD_BUTTON_UP)
	{
		pos = 0xf - small_cursor_x;
		*reg_value |= 1 << pos;
	}
	if (WPAD_ButtonsDown(0) & WPAD_BUTTON_DOWN)
	{
		pos = 0xf - small_cursor_x;
		*reg_value &= ~(1 << pos);
	}
	if (WPAD_ButtonsDown(0) & WPAD_BUTTON_A)
	{
		ui_mode = UIM_SEL;
	}
}*/

void ui_pad_edit_reg(void)
{
	if (WPAD_ButtonsDown(0) & WPAD_BUTTON_RIGHT)
	{
		small_cursor_x++;
	}
	if (WPAD_ButtonsDown(0) & WPAD_BUTTON_LEFT)
	{
		small_cursor_x--;
	}
	small_cursor_x &= 0x3;

	if (WPAD_ButtonsDown(0) & WPAD_BUTTON_UP)
	{
		*reg_value += 0x1 << (4 * (3 - small_cursor_x));
	}
	if (WPAD_ButtonsDown(0) & WPAD_BUTTON_DOWN)
	{
		*reg_value -= 0x1 << (4 * (3 - small_cursor_x));
	}
	if (WPAD_ButtonsDown(0) & WPAD_BUTTON_A)
	{
		ui_mode = UIM_SEL;
	}
	if (WPAD_ButtonsDown(0) & WPAD_BUTTON_1)
		*reg_value = 0;
	if (WPAD_ButtonsDown(0) & WPAD_BUTTON_2)
		*reg_value = 0xffff;
}

void init_video(void)
{
	VIDEO_Init();
	switch (VIDEO_GetCurrentTvMode())
	{
	case VI_NTSC:
		rmode = &TVNtsc480IntDf;
		break;
	case VI_PAL:
		rmode = &TVPal528IntDf;
		break;
	case VI_MPAL:
		rmode = &TVMpal480IntDf;
		break;
	default:
		rmode = &TVNtsc480IntDf;
		break;
	}

	PAD_Init();
	xfb = SYS_AllocateFramebuffer(rmode);

	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
}

void my_send_task(void *addr, u16 iram_addr, u16 len, u16 start)
{
	while(DSP_CheckMailTo());
	DSP_SendMailTo(0x80F3A001);
	while(DSP_CheckMailTo());
	DSP_SendMailTo((u32)addr);
	while(DSP_CheckMailTo());
	DSP_SendMailTo(0x80F3C002);
	while(DSP_CheckMailTo());
	DSP_SendMailTo(iram_addr);
	while(DSP_CheckMailTo());
	DSP_SendMailTo(0x80F3A002);
	while(DSP_CheckMailTo());
	DSP_SendMailTo(len);
	while(DSP_CheckMailTo());
	DSP_SendMailTo(0x80F3B002);
	while(DSP_CheckMailTo());
	DSP_SendMailTo(0);
	while(DSP_CheckMailTo());
	DSP_SendMailTo(0x80F3D001);
	while(DSP_CheckMailTo());
	DSP_SendMailTo(start);
	while(DSP_CheckMailTo());
}

int main()
{
	init_video();
	ds_init(xfb, 20, 64, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth * 2);

	ui_mode = UIM_SEL;

	dspbufP = (u16 *)MEM_VIRTUAL_TO_PHYSICAL(dspbuffer);
	dspbufC = dspbuffer;
	dspbufU = (u32 *)(MEM_K0_TO_K1(dspbuffer));

	DCInvalidateRange(dspbuffer, 0x2000);
	for (int j = 0 ; j < 0x800; j++)
		dspbufU[j] = 0xffffffff;

	_dspReg[5] = (_dspReg[5]&~(DSPCR_AIINT|DSPCR_ARINT|DSPCR_DSPINT))|DSPCR_DSPRESET;
	_dspReg[5] = (_dspReg[5]&~(DSPCR_HALT|DSPCR_AIINT|DSPCR_ARINT|DSPCR_DSPINT));

	u32 level;
	_CPU_ISR_Disable(level);
	IRQ_Request(IRQ_DSP_DSP, my__dsp_handler,NULL);
	_CPU_ISR_Restore(level);	

#if ENABLE_OUT
	if_config("192.168.0.5", "192.168.0.1", "255.255.255.0", false);
	//printf("Network Intitalized\n");
#endif

	WPAD_Init();

	int dsp_steps = 0;
	int show_step = 0;

	while (true)
	{
		// Should put a loop around this too.
		if (DSP_CheckMailFrom())
		{
			u32 mail = DSP_ReadMailFrom();
			ds_printf(2, 1, "Last mail: %08x", mail);

			if (mail == 0x8071feed)
			{
				// DSP ready for task. Let's send one.
				// First, prepare data.
				for (int n = 0 ; n < 32 ; n++)
					dspbufC[0x00 + n] = dspreg_in[n];
				DCFlushRange(dspbufC, 0x2000);
				// Then send the code.
				DCFlushRange((void *)dsp_code, 0x1000);
				my_send_task((void *)MEM_VIRTUAL_TO_PHYSICAL(dsp_code), 0, 4000, 0x10);
			}
			else if (mail == 0x8888dead)
			{
				u16* tmpBuf = (u16 *)MEM_VIRTUAL_TO_PHYSICAL(mem_dump);

				while (DSP_CheckMailTo());
				DSP_SendMailTo((u32)tmpBuf);
				while (DSP_CheckMailTo());
				regs_refreshed = false;
			}			
			else if (mail == 0x8888beef)
			{
				while (DSP_CheckMailTo());
				DSP_SendMailTo((u32)dspbufP);
				while (DSP_CheckMailTo());
				regs_refreshed = false;
			}
			else if (mail == 0x8888feeb)
			{
				// We got a stepful of registers.
				DCInvalidateRange(dspbufC, 0x2000);
				for (int i = 0 ; i < 32 ; i++)
					dspreg_out[dsp_steps][i] = dspbufC[0xf80 + i];
				regs_refreshed = true;

				dsp_steps++;

				while (DSP_CheckMailTo());
				DSP_SendMailTo(0x8000DEAD);
				while (DSP_CheckMailTo());
			}
		}

		VIDEO_WaitVSync();

		WPAD_ScanPads();
		if (WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME)
			exit(0);

		print_regs(show_step, dsp_steps);

		ds_printf(2, 18, "Controls:");
		ds_printf(4, 19, "+/- to move");
		ds_printf(4, 20, "B to start over");
		ds_printf(4, 21, "Home to exit");

		switch (ui_mode)
		{
		case UIM_SEL:
			ui_pad_sel();
			break;
		case UIM_EDIT_REG:
			ui_pad_edit_reg();
			break;
		case UIM_EDIT_BIN:
			// ui_pad_edit_bin();
			break;
		default:
			break;
		}
		DCFlushRange(xfb, 0x200000);

		// Use B to start over.
		if (WPAD_ButtonsDown(0) & WPAD_BUTTON_B) 
		{
			dsp_steps = 0;  // Let's not add the new steps after the original ones. That was just annoying.

			DCInvalidateRange(dspbufC, 0x2000);
			for (int n = 0 ; n < 0x2000 ; n++)
			{
				//	dspbufU[n/2] = 0; dspbufC[n] = 0;
			}
			DCFlushRange(dspbufC, 0x2000);

			// Reset the DSP.
			_dspReg[5] = (_dspReg[5] & ~(DSPCR_AIINT|DSPCR_ARINT|DSPCR_DSPINT)) | DSPCR_DSPRESET;
			_dspReg[5] = (_dspReg[5] & ~(DSPCR_HALT|DSPCR_AIINT|DSPCR_ARINT|DSPCR_DSPINT));
			_dspReg[5] |= DSPCR_RES;
			while (_dspReg[5] & DSPCR_RES)
				;
			_dspReg[9] = 0x63;
		}

		// Navigate between results using + and - buttons.

		if (WPAD_ButtonsDown(0) & WPAD_BUTTON_PLUS)
		{
			show_step++;
			if (show_step >= dsp_steps) 
				show_step = 0;
		}

		if (WPAD_ButtonsDown(0) & WPAD_BUTTON_MINUS)
		{
			show_step--;
			if (show_step < 0) 
				show_step = dsp_steps - 1;
		}
	}

	// Reset the DSP
	_dspReg[5] = (_dspReg[5]&~(DSPCR_AIINT|DSPCR_ARINT|DSPCR_DSPINT))|DSPCR_DSPRESET;
	_dspReg[5] = (_dspReg[5]&~(DSPCR_HALT|DSPCR_AIINT|DSPCR_ARINT|DSPCR_DSPINT));
	reload();

	// Exit
	exit(0);
	return 0;
}
