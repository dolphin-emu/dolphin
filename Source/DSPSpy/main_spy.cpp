// Copyright (C) 2003-2009 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

// This is a test program for running code on the Wii DSP, with full control over input
// and automatic compare with output. VERY useful for figuring out what those little
// ops actually do.
// It's very unpolished though
// Use Dolphin's dsptool to generate a new dsp_code.h.
// Originally written by duddie and modified by FIRES. Then further modified by ector.

#include <gccore.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <network.h>
#include <ogcsys.h>
#include <time.h>
#include <fat.h>
#include <fcntl.h>

#include <ogc/color.h>
#include <ogc/consol.h>

#ifdef HW_RVL
#include <wiiuse/wpad.h>
#endif

#include "ConsoleHelper.h"

// Pull in some constants etc from DSPCore.
#include "../Core/DSPCore/Src/gdsp_registers.h"

// This is where the DSP binary is.
#include "dsp_code.h"
#include "mem_dump.h"

// Communication with the real DSP and with the DSP emulator.
#include "dsp_interface.h"
#include "real_dsp.h"
// #include "virtual_dsp.h"

// Used for communications with the DSP, such as dumping registers etc.
u16 dspbuffer[16 * 1024] __attribute__ ((aligned (0x4000))); 

static void *xfb = NULL;
void (*reboot)() = (void(*)())0x80001800;
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

char last_message[20] = "OK";

RealDSP real_dsp;

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
			const int reg = j * 8 + i;
			CON_SetColor(sel == reg ? CON_BRIGHT_YELLOW : CON_GREEN, CON_BLACK);
			CON_Printf(x + j * 8, i + y, "%02x ", reg);
			if (j != 1 || i < 4)
			{
				u8 color1 = regs_equal(reg, regs[reg], compare_regs[reg]) ? CON_BRIGHT_WHITE : CON_BRIGHT_RED;
				for (int k = 0; k < 4; k++)
				{
					if (sel == reg && k == small_cursor_x && ui_mode == UIM_EDIT_REG)
						CON_SetColor(CON_BLACK, color1);
					else
						CON_SetColor(color1, CON_BLACK);
					CON_Printf(x + 3 + j * 8 + k, i + y, "%01x", (regs[reg] >> ((3 - k) * 4)) & 0xf);
				}
			}
		}
	}
	CON_SetColor(CON_WHITE, CON_BLACK);

	CON_Printf(x+2, y+9, "ACC0: %02x %04x %04x", regs[DSP_REG_ACH0]&0xff, regs[DSP_REG_ACM0], regs[DSP_REG_ACL0]);
	CON_Printf(x+2, y+10, "ACC1: %02x %04x %04x", regs[DSP_REG_ACH1]&0xff, regs[DSP_REG_ACM1], regs[DSP_REG_ACL1]);
	CON_Printf(x+2, y+11, "AX0: %04x %04x", regs[DSP_REG_AXH0], regs[DSP_REG_AXL0]);
	CON_Printf(x+2, y+12, "AX1: %04x %04x", regs[DSP_REG_AXH1], regs[DSP_REG_AXL1]);
}

void print_regs(int _step, int _dsp_steps)
{
	const u16 *regs = _step == 0 ? dspreg_in : dspreg_out[_step - 1];
	const u16 *regs2 = dspreg_out[_step];

	print_reg_block(0, 2, _step == 0 ? cursor_reg : -1, regs, regs2);
	print_reg_block(33, 2, -1, regs2, regs);

	CON_SetColor(CON_WHITE, CON_BLACK);
	CON_Printf(33, 17, "%i / %i      ", _step + 1, _dsp_steps);

	return;

	static int count = 0;
	int x = 0, y = 16;
	if (count > 2)
		printf("\x1b[2J"); // Clear
	count = 0;
	CON_SetColor(CON_WHITE, CON_BLACK);
	for (int i = 0x0; i < 0xf70 ; i++)
	{
		if (dspbufC[i] != mem_dump[i])
		{
			CON_Printf(x, y, "%04x=%04x", i, dspbufC[i]);
			count++;
			x += 10;
			if (x >= 60) {
				x = 0;
				y++;
			}
		}
	}
	CON_Printf(4, 25, "%08x", count);
}

void ui_pad_sel(void)
{
#ifdef HW_RVL
	if (WPAD_ButtonsDown(0) & WPAD_BUTTON_RIGHT)
		cursor_reg += 8;
	if (WPAD_ButtonsDown(0) & WPAD_BUTTON_LEFT)
		cursor_reg -= 8;
	if (WPAD_ButtonsDown(0) & WPAD_BUTTON_UP)
		cursor_reg--;
	if (WPAD_ButtonsDown(0) & WPAD_BUTTON_DOWN)
		cursor_reg++;
	cursor_reg &= 0x1f;
	if (WPAD_ButtonsDown(0) & WPAD_BUTTON_A)
	{
		ui_mode = UIM_EDIT_REG;
		reg_value = &dspreg_in[cursor_reg];
	}
#else
	if (PAD_ButtonsDown(0) & PAD_BUTTON_RIGHT)
		cursor_reg += 8;
	if (PAD_ButtonsDown(0) & PAD_BUTTON_LEFT)
		cursor_reg -= 8;
	if (PAD_ButtonsDown(0) & PAD_BUTTON_UP)
		cursor_reg--;
	if (PAD_ButtonsDown(0) & PAD_BUTTON_DOWN)
		cursor_reg++;
	cursor_reg &= 0x1f;
	if (PAD_ButtonsDown(0) & PAD_BUTTON_A)
	{
		ui_mode = UIM_EDIT_REG;
		reg_value = &dspreg_in[cursor_reg];
	}
#endif
}

void ui_pad_edit_reg(void)
{
#ifdef HW_RVL
	if (WPAD_ButtonsDown(0) & WPAD_BUTTON_RIGHT)
		small_cursor_x++;
	if (WPAD_ButtonsDown(0) & WPAD_BUTTON_LEFT)
		small_cursor_x--;
	small_cursor_x &= 0x3;

	if (WPAD_ButtonsDown(0) & WPAD_BUTTON_UP)
		*reg_value += 0x1 << (4 * (3 - small_cursor_x));
	if (WPAD_ButtonsDown(0) & WPAD_BUTTON_DOWN)
		*reg_value -= 0x1 << (4 * (3 - small_cursor_x));
	if (WPAD_ButtonsDown(0) & WPAD_BUTTON_A)
		ui_mode = UIM_SEL;
	if (WPAD_ButtonsDown(0) & WPAD_BUTTON_1)
		*reg_value = 0;
	if (WPAD_ButtonsDown(0) & WPAD_BUTTON_2)
		*reg_value = 0xffff;
#else
	if (PAD_ButtonsDown(0) & PAD_BUTTON_RIGHT)
		small_cursor_x++;
	if (PAD_ButtonsDown(0) & PAD_BUTTON_LEFT)
		small_cursor_x--;
	small_cursor_x &= 0x3;

	if (PAD_ButtonsDown(0) & PAD_BUTTON_UP)
		*reg_value += 0x1 << (4 * (3 - small_cursor_x));
	if (PAD_ButtonsDown(0) & PAD_BUTTON_DOWN)
		*reg_value -= 0x1 << (4 * (3 - small_cursor_x));
	if (PAD_ButtonsDown(0) & PAD_BUTTON_A)
		ui_mode = UIM_SEL;
	if (PAD_ButtonsDown(0) & PAD_BUTTON_X)
		*reg_value = 0;
	if (PAD_ButtonsDown(0) & PAD_BUTTON_Y)
		*reg_value = 0xffff;
#endif
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

	xfb = SYS_AllocateFramebuffer(rmode);

	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
}


int main()
{
	init_video();
	CON_Init(xfb, 20, 64, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth * 2);

	ui_mode = UIM_SEL;

	dspbufP = (u16 *)MEM_VIRTUAL_TO_PHYSICAL(dspbuffer);
	dspbufC = dspbuffer;
	dspbufU = (u32 *)(MEM_K0_TO_K1(dspbuffer));

	DCInvalidateRange(dspbuffer, 0x2000);
	for (int j = 0; j < 0x800; j++)
		dspbufU[j] = 0xffffffff;

	// Initialize DSP.
	real_dsp.Init();

	// Initialize FAT so we can write to SD.
	fatInit(8, false);

	// Both GC and Wii controls.
	PAD_Init();
#ifdef HW_RVL
	WPAD_Init();
#endif

	int dsp_steps = 0;
	int show_step = 0;
	while (true)
	{
		// Should put a loop around this too.
		if (DSP_CheckMailFrom())
		{
			u32 mail = DSP_ReadMailFrom();
			CON_Printf(2, 1, "Last mail: %08x", mail);

			if (mail == 0x8071feed)
			{
				// DSP ready for task. Let's send one.
				// First, prepare data.
				for (int n = 0 ; n < 32 ; n++)
					dspbufC[0x00 + n] = dspreg_in[n];
				DCFlushRange(dspbufC, 0x2000);
				// Then send the code.
				DCFlushRange((void *)dsp_code, 0x2000);
				real_dsp.SendTask((void *)MEM_VIRTUAL_TO_PHYSICAL(dsp_code), 0, 4000, 0x10);
			}
			else if (mail == 0x8888dead)
			{
				u16* tmpBuf = (u16 *)MEM_VIRTUAL_TO_PHYSICAL(mem_dump);

				while (DSP_CheckMailTo());
				DSP_SendMailTo((u32)tmpBuf);
				while (DSP_CheckMailTo());
			}			
			else if (mail == 0x8888beef)
			{
				while (DSP_CheckMailTo());
				DSP_SendMailTo((u32)dspbufP);
				while (DSP_CheckMailTo());
			}
			else if (mail == 0x8888feeb)
			{
				// We got a stepful of registers.
				DCInvalidateRange(dspbufC, 0x2000);
				for (int i = 0 ; i < 32 ; i++)
					dspreg_out[dsp_steps][i] = dspbufC[0xf80 + i];

				dsp_steps++;

				while (DSP_CheckMailTo());
				DSP_SendMailTo(0x8000DEAD);
				while (DSP_CheckMailTo());
			}
		}

		VIDEO_WaitVSync();

		PAD_ScanPads();
		if (PAD_ButtonsDown(0) & PAD_BUTTON_START)
			exit(0);
#ifdef HW_RVL
		WPAD_ScanPads();
		if ((WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME) || (PAD_ButtonsDown(0) & PAD_BUTTON_START))
			exit(0);

		CON_Printf(2, 18, "Controls:");
		CON_Printf(4, 19, "+/- to move");
		CON_Printf(4, 20, "B to start over");
		CON_Printf(4, 21, "Home to exit");
		CON_Printf(4, 22, "2 to dump results to SD");
#else
		CON_Printf(2, 18, "Controls:");
		CON_Printf(4, 19, "L/R to move");
		CON_Printf(4, 20, "B to start over");
		CON_Printf(4, 21, "Start to exit");
#endif

		print_regs(show_step, dsp_steps);

		CON_Printf(4, 24, last_message);

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
#ifdef HW_RVL
		if ((WPAD_ButtonsDown(0) & WPAD_BUTTON_B) || (PAD_ButtonsDown(0) & PAD_BUTTON_B))
#else
		if (PAD_ButtonsDown(0) & PAD_BUTTON_B)
#endif
		{
			dsp_steps = 0;  // Let's not add the new steps after the original ones. That was just annoying.

			DCInvalidateRange(dspbufC, 0x2000);
			for (int n = 0 ; n < 0x2000 ; n++)
			{
				//	dspbufU[n/2] = 0; dspbufC[n] = 0;
			}
			DCFlushRange(dspbufC, 0x2000);

			// Reset the DSP.
			real_dsp.Reset();
			strcpy(last_message, "OK");
		}

		// Navigate between results using + and - buttons.
#ifdef HW_RVL
		if ((WPAD_ButtonsDown(0) & WPAD_BUTTON_PLUS) || (PAD_ButtonsDown(0) & PAD_TRIGGER_R))
#else
		if (PAD_ButtonsDown(0) & PAD_TRIGGER_R)
#endif
		{
			show_step++;
			if (show_step >= dsp_steps) 
				show_step = 0;
			strcpy(last_message, "OK");
		}
#ifdef HW_RVL
		if ((WPAD_ButtonsDown(0) & WPAD_BUTTON_MINUS) || (PAD_ButtonsDown(0) & PAD_TRIGGER_L))
#else
		if (PAD_ButtonsDown(0) & PAD_TRIGGER_L)
#endif
		{
			show_step--;
			if (show_step < 0) 
				show_step = dsp_steps - 1;
			strcpy(last_message, "OK");
		}

#ifdef HW_RVL
		// Probably could offer to save to memcard (sd gecko) but i dont have one so meh
		if (WPAD_ButtonsDown(0) & WPAD_BUTTON_2)
		{
			FILE *f = fopen("sd:/dsp_dump.bin", "wb");
			if (f) 
			{
				// First write initial regs
				fwrite(dspreg_in, 1, 32 * 2, f);

				// Then write all the dumps.
				fwrite(dspreg_out, 1, dsp_steps * 32 * 2, f);
				fclose(f);
				strcpy(last_message, "Dump Successful.");
			}
			else
			{
				strcpy(last_message, "SD Write Error");
			}
		}
#endif
	}

	// Reset the DSP
	real_dsp.Reset();

	// Reboot back to Homebrew Channel or whatever started this binary.
	reboot();

	// Will never reach here, but just to be sure..
	exit(0);
	return 0;
}
