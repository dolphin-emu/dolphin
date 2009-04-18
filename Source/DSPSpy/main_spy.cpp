// This is a test program for running code on the Wii DSP, with full control over input
// and automatic compare with output. VERY useful for figuring out what those little
// ops actually do.
// It's very unpolished though
// Use Dolphin's dsptool to generate a new dsp_code.h.
// Originally written by FIRES?

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

u16 opcode[4] = {
	0x0000, 0x0000, 0x0000, 0x0000,
};


u16 dspreg_in[32] = {
	0x0410, 0x0510, 0x0610, 0x0710, 0x0810, 0x0910, 0x0a10, 0x0b10, 
	0x0411, 0x0522, 0x0633, 0x0744, 0x0855, 0x0966, 0x0a77, 0x0b88,
	0x0014, 0xfff5, 0x00ff, 0x2200, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0003, 0x0004, 0x8000, 0x000C, 0x0007, 0x0008, 0x0009, 0x000a,
}; ///            ax_h_1   ax_h_1

/* ttt

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
0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x03d1, 0x0000, 0x0418, 0x0002,     // r08 must have a value ... no idea why
0x0000, 0x0000, 0x00ff, 0x1804, 0xdb70, 0x4ddb, 0x0000, 0x0000,
0x0000, 0x0000, 0x0000, 0xde6d, 0x0000, 0x0000, 0x0000, 0x004e,
};*/ 


u16 dspreg_out[1000][32];
u16 dsp_steps = 0;
u16 show_step = 0;

#include "mem_dump.h"

// #include "dsp_test.cpp"
u32 padding[1024];

s32 cursor_x = 1;
s32 cursor_y = 1;

s32 old_cur_x;
s32 old_cur_y;

s32 small_cursor_x;

u32 ui_mode;

#define UIM_SEL			1
#define UIM_EDIT_REG	2
#define	UIM_EDIT_BIN	4

PADStatus gpad;
PADStatus opad;
u16 *reg_value;

void ds_text_out(int xpos, int ypos, const char *str);
void ds_set_colour(int f, int b);
void ds_init(void *framebuffer,int xstart,int ystart,int xres,int yres,int stride);
void ds_underline(int xpos, int ypos, int len, int col);
void ds_printf(int x, int y, const char *fmt, ...);
void ds_clear(void);

volatile int regs_refreshed = false;


static void my__dsp_handler(u32 nIrq,void *pCtx)
{
	// volatile u32 mail;
	_dspReg[5] = (_dspReg[5]&~(DSPCR_AIINT|DSPCR_ARINT))|DSPCR_DSPINT;
	/*
	while(!DSP_CheckMailFrom());
	mail = DSP_ReadMailFrom();

	if (mail == 0x8888beef)
	{
	DSP_SendMailTo((u32)dspbuf | 0x80000000);
	while(DSP_CheckMailTo());
	regs_refreshed = false;
	return;
	}
	else if (mail == 0x8888beeb)
	{
	regs_refreshed = true;
	return;
	}
	*/
	//	printf("Mail: %08x\n", mail);
}



const int r_off_x = 2;
const int r_off_y = 2;

vu16 val_x = 0x1234;

void print_regs(u16 _step)
{
	int i, j;

	for(j = 0 ; j < 4 ; j++)
	{
		for(i = 0 ; i < 8 ; i++)
		{
			int reg = j * 8 + i;
			ds_set_colour(COLOR_GREEN, COLOR_BLACK);
			ds_printf(0 + j * 8, i + 2, "%02x_", reg);
			ds_set_colour(COLOR_WHITE, COLOR_BLACK);
			if (_step == 0)
				ds_printf(3 + j * 8, i + 2, "%04x", dspreg_in[reg]);
			else
				ds_printf(3 + j * 8, i + 2, "%04x", dspreg_out[_step-1][reg]);
		}
	}

	//	for(i = 0 ; i < 4 ; i++)
	{
		ds_set_colour(COLOR_WHITE, COLOR_BLACK);
		ds_printf(4, 11, "%03i / %03i", _step+1, dsp_steps);
		/*		ds_set_colour(COLOR_WHITE, COLOR_BLACK);
		int j;
		for(j = 0 ; j < 16 ; j++)
		ds_printf(10 + j, i + 11, "%d", (opcode[i] >> (15 - j)) & 0x1);*/
	}

	for(j = 0 ; j < 4 ; j++)
	{
		for(i = 0 ; i < 8 ; i++)
		{
			char tmpbuf1[20];
			int reg = j * 8 + i;
			sprintf(tmpbuf1, "%02x_", reg);
			ds_set_colour(COLOR_GREEN, COLOR_BLACK);
			ds_text_out(33 + j * 8, i + 2, tmpbuf1);
			sprintf(tmpbuf1, "%04x", dspreg_out[_step][reg]);

			bool Red = true;
			if (_step == 0)
				Red = dspreg_in[reg] != dspreg_out[_step][reg];
			else
				Red = dspreg_out[_step-1][reg] != dspreg_out[_step][reg];

			if (Red)
				ds_set_colour(COLOR_RED, COLOR_BLACK);
			else
				ds_set_colour(COLOR_WHITE, COLOR_BLACK);
			ds_text_out(36 + j * 8, i + 2, tmpbuf1);
		}
	}

	//ds_printf(4, 15, "DMA: %04x %04x %04x", _dspReg[16], _dspReg[17], _dspReg[18]);
	ds_printf(4, 15, "DICR: %04x ",val_x);
	ds_printf(4, 25, "         ");
	static int count = 0;
	int x, y;
	y = 16;
	x = 0;
	if (count > 2)
		ds_clear();
	count = 0;
	ds_set_colour(COLOR_WHITE, COLOR_BLACK);
	for (i = 0x0; i < 0xf70 ; i++)
	{
		if (dspbufC[i] != mem_dump[i])
		{
			ds_printf(x, y, "%04x=%04x", i, dspbufC[i]);
			count++;
			x+=10; if (x >= 60) { x=0 ; y++;};
		}
	}
	ds_printf(4, 25, "%08x", count);
}



void hide_cursor(void)
{
	if (old_cur_y < 8)
	{
		ds_underline(old_cur_x * 8 + 3, old_cur_y + 2, 4, COLOR_BLACK);
	}
	else
	{
		if (old_cur_x == 0) ds_underline(4, cursor_y + 3, 4, COLOR_BLACK);
		else ds_underline(10, cursor_y + 3, 16, COLOR_BLACK);
	}
}

void show_cursor(void)
{
	if (ui_mode == UIM_SEL)
	{
		if (cursor_y < 8)
		{
			ds_underline(cursor_x * 8 + 3, cursor_y + 2, 4, COLOR_WHITE);
		}
		else
		{
			if (cursor_x == 0) 
				ds_underline(4, cursor_y + 3, 4, COLOR_WHITE);
			else
				ds_underline(10, cursor_y + 3, 16, COLOR_WHITE);
		}
	}
	else 
	{
		if (cursor_y < 8)
		{
			ds_underline(cursor_x * 8 + 3 + small_cursor_x, cursor_y + 2, 1, COLOR_WHITE);
		}
		else
		{
			if (cursor_x == 0) 
				ds_underline(4 + small_cursor_x, cursor_y + 3, 1, COLOR_WHITE);
			else
				ds_underline(10 + small_cursor_x, cursor_y + 3, 1, COLOR_WHITE);
		}
	}
	old_cur_x = cursor_x;
	old_cur_y = cursor_y;
}


void check_pad(void)
{
	PADStatus pads[4];
	PAD_Read(pads);

	if (opad.button == pads[0].button)
	{
		gpad.button = 0;
		return;
	}
	opad.button = gpad.button = pads[0].button;
	return;
}


void ui_pad_sel(void)
{
	if (gpad.button & PAD_BUTTON_RIGHT)
	{
		cursor_x++;
	}
	if (gpad.button & PAD_BUTTON_LEFT)
	{
		cursor_x--;
	}
	if (gpad.button & PAD_BUTTON_UP)
	{
		cursor_y--;
	}
	if (gpad.button & PAD_BUTTON_DOWN)
	{
		cursor_y++;
		if (cursor_y == 8)
			cursor_x = 0;
	}
	if (cursor_y < 0)
	{
		cursor_y = 11;
		cursor_x = 0;
	}
	else if (cursor_y > 11)
		cursor_y = 0;

	if (cursor_y < 8)
		cursor_x &= 0x3;
	else
		cursor_x &= 0x1;

	if (gpad.button & PAD_BUTTON_A)
	{
		if (cursor_y < 8)
		{
			ui_mode = UIM_EDIT_REG;
			reg_value = &dspreg_in[cursor_y + cursor_x * 8];
		}
		else
		{
			if (cursor_x == 0)
				ui_mode = UIM_EDIT_REG;
			else
				ui_mode = UIM_EDIT_BIN;
			reg_value = &opcode[cursor_y-8];
		}
	}
}

void ui_pad_edit_bin(void)
{
	u8 pos;

	if (gpad.button & PAD_BUTTON_RIGHT)
	{
		small_cursor_x++;
	}
	if (gpad.button & PAD_BUTTON_LEFT)
	{
		small_cursor_x--;
	}
	small_cursor_x &= 0xf;
	if (gpad.button & PAD_BUTTON_UP)
	{
		pos = 0xf - small_cursor_x;
		*reg_value |= 1 << pos;
	}
	if (gpad.button & PAD_BUTTON_DOWN)
	{
		pos = 0xf - small_cursor_x;
		*reg_value &= ~(1 << pos);
	}
	if (gpad.button & PAD_BUTTON_A)
	{
		ui_mode = UIM_SEL;
	}
}

void ui_pad_edit_reg(void)
{
	if (gpad.button & PAD_BUTTON_RIGHT)
	{
		small_cursor_x++;
	}
	if (gpad.button & PAD_BUTTON_LEFT)
	{
		small_cursor_x--;
	}

	if (gpad.button & PAD_BUTTON_UP)
	{
		*reg_value += 0x1 << (4 * (3 - small_cursor_x));
	}
	if (gpad.button & PAD_BUTTON_DOWN)
	{
		*reg_value -= 0x1 << (4 * (3 - small_cursor_x));
	}
	small_cursor_x &= 0x3;
	if (gpad.button & PAD_BUTTON_A)
	{
		ui_mode = UIM_SEL;
	}
	if (gpad.button & PAD_BUTTON_X)
		*reg_value = 0;
	if (gpad.button & PAD_BUTTON_Y)
		*reg_value = 0xffff;
}

void init_video(void)
{
	VIDEO_Init();

	switch(VIDEO_GetCurrentTvMode())
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

	//xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	xfb = SYS_AllocateFramebuffer(rmode);

	VIDEO_Configure(rmode);

	VIDEO_SetNextFramebuffer(xfb);

	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	//if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();
	//VIDEO_SetPreRetraceCallback(ScanPads);
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
	int i, j;
	u32	mail;
	u32 level;

	{
		vu16 *dicr = ((vu16 *)0xcc002002);
		*dicr = 0x100;
		*dicr = 0x002;
		//		*dicr = 0x000;
		//		*dicr = 0x001;
		val_x = *dicr;
	}


	init_video();
	//console_init(xfb,20,64,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*2);
	ds_init(xfb,20,64,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*2);


	ui_mode = UIM_SEL;

	dspbufP = (u16 *)MEM_VIRTUAL_TO_PHYSICAL(dspbuffer);
	dspbufC = dspbuffer;
	dspbufU = (u32 *)(MEM_K0_TO_K1(dspbuffer));

	DCInvalidateRange(dspbuffer, 0x2000);
	for(j = 0 ; j < 0x800 ; j++)
		dspbufU[j] = 0xffffffff;

	_dspReg[5] = (_dspReg[5]&~(DSPCR_AIINT|DSPCR_ARINT|DSPCR_DSPINT))|DSPCR_DSPRESET;
	_dspReg[5] = (_dspReg[5]&~(DSPCR_HALT|DSPCR_AIINT|DSPCR_ARINT|DSPCR_DSPINT));

	_CPU_ISR_Disable(level);
	IRQ_Request(IRQ_DSP_DSP, my__dsp_handler,NULL);
	_CPU_ISR_Restore(level);	

#if ENABLE_OUT
	if_config("192.168.0.5", "192.168.0.1", "255.255.255.0", false);
	//printf("Network Intitalized\n");
#endif

	WPAD_Init();

	while(1)
	{
		if (DSP_CheckMailFrom())
		{
			mail = DSP_ReadMailFrom();
			ds_printf(2, 1, "Mail: %08x", mail);

			if (mail == 0x8071feed)
			{
				int n;
				for (n = 0 ; n < 32 ; n++)
					dspbufC[0x00 + n] = dspreg_in[n];
				DCFlushRange(dspbufC, 0x2000);
				/*
				for (n = 0 ; n < 600 ; n++)
				{
					if (((u16*)dsp_test)[n] == 0x1234)
					{
						((u16*)dsp_test)[n+0] = opcode[0];
						((u16*)dsp_test)[n+1] = opcode[1];
						((u16*)dsp_test)[n+2] = opcode[2];
						((u16*)dsp_test)[n+3] = opcode[3];
						break;
					}
				}
				*/
				DCFlushRange((void *)dsp_code, 0x1000);
				my_send_task((void *)MEM_VIRTUAL_TO_PHYSICAL(dsp_code), 0, 4000, 0x10);
				((u16*)dsp_code)[n] = 0x1234;  // wtf?
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
				DCInvalidateRange(dspbufC, 0x2000);
				for(i = 0 ; i < 32 ; i++)
					dspreg_out[dsp_steps][i] = dspbufC[0xf80 + i];
				regs_refreshed = true;

				dsp_steps++;

				while(DSP_CheckMailTo());
				DSP_SendMailTo(0x8000DEAD);
				while(DSP_CheckMailTo());

				// dump_to_pc();

			}
		}

		VIDEO_WaitVSync();

		WPAD_ScanPads();
		if (WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME)
			exit(0);

		//ds_clear();
		//if (regs_refreshed)
		{
			print_regs(show_step);
			//regs_refreshed = false;

		}
		hide_cursor();

		switch(ui_mode)
		{
		case UIM_SEL:
			ui_pad_sel();
			show_cursor();
			break;
		case UIM_EDIT_REG:
			ui_pad_edit_reg();
			show_cursor();
			break;
		case UIM_EDIT_BIN:
			ui_pad_edit_bin();
			show_cursor();
			break;
		default:
			break;
		}
		DCFlushRange(xfb, 0x200000);


		if (WPAD_ButtonsDown(0) & WPAD_BUTTON_B) 
		{
			DCInvalidateRange(dspbufC, 0x2000);
			int n;
			for(n = 0 ; n < 0x2000 ; n++)
			{
				//	dspbufU[n/2] = 0; dspbufC[n] = 0;
			}
			DCFlushRange(dspbufC, 0x2000);
			_dspReg[5] = (_dspReg[5]&~(DSPCR_AIINT|DSPCR_ARINT|DSPCR_DSPINT))|DSPCR_DSPRESET;
			_dspReg[5] = (_dspReg[5]&~(DSPCR_HALT|DSPCR_AIINT|DSPCR_ARINT|DSPCR_DSPINT));
			_dspReg[5] |= DSPCR_RES;
			while(_dspReg[5]&DSPCR_RES);
			_dspReg[9] = 0x63;
		}

		if (WPAD_ButtonsDown(0) & WPAD_BUTTON_2) 
		{
			vu16 *dicr = ((vu16 *)0xcc002002);
			*dicr = 0x001;
			val_x = *dicr;
		}
		if (WPAD_ButtonsDown(0) & WPAD_BUTTON_A)
		{
			show_step++;
			if (show_step >= dsp_steps) 
				show_step = 0;
		}

	};

	// Reset the DSP
	_dspReg[5] = (_dspReg[5]&~(DSPCR_AIINT|DSPCR_ARINT|DSPCR_DSPINT))|DSPCR_DSPRESET;
	_dspReg[5] = (_dspReg[5]&~(DSPCR_HALT|DSPCR_AIINT|DSPCR_ARINT|DSPCR_DSPINT));
	reload();

	// Exit
	exit(0);
	return 0;
}
