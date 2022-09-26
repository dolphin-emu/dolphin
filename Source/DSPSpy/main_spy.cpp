// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// This is a test program for running code on the Wii DSP, with full control over input
// and automatic compare with output. VERY useful for figuring out what those little
// ops actually do.
// It's very unpolished though
// Use Dolphin's dsptool to generate a new dsp_code.h.
// Originally written by duddie and modified by FIRES. Then further modified by ector.

#include <array>
#include <debug.h>
#include <fat.h>
#include <fcntl.h>
#include <gccore.h>
#include <malloc.h>
#include <network.h>
#include <ogcsys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <ogc/color.h>
#include <ogc/consol.h>
#include <unistd.h>

#ifdef _MSC_VER
// Just for easy looking :)
#define HW_RVL  // HW_DOL
#endif

#ifdef HW_RVL
#include <sdcard/wiisd_io.h>
#include <wiiuse/wpad.h>
#else
#include <sdcard/gcsd.h>
#endif

#include "ConsoleHelper.h"

#include "dspregs.h"

// This is where the DSP binary is.
#include "dsp_code.h"
#include "mem_dump.h"

// Communication with the real DSP and with the DSP emulator.
#include "dsp_interface.h"
#include "real_dsp.h"
// #include "virtual_dsp.h"

// Used for communications with the DSP, such as dumping registers etc.
u16 dspbuffer[16 * 1024] __attribute__((aligned(0x4000)));

static void* xfb = nullptr;
void (*reboot)() = (void (*)())0x80001800;
GXRModeObj* rmode;

static vu16* const _dspReg = (u16*)0xCC005000;

u16* dspbufP;
u16* dspbufC;
u32* dspbufU;

u16 dspreg_in[32] = {
    0x0410, 0x0510, 0x0610, 0x0710, 0x0810, 0x0910, 0x0a10, 0x0b10, 0xFFFF, 0xFFFF, 0xFFFF,
    0xFFFF, 0x0855, 0x0966, 0x0a77, 0x0b88, 0x0014, 0xfff5, 0x00ff, 0x2200, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0003, 0x0004, 0x8000, 0x000C, 0x0007, 0x0008, 0x0009, 0x000a,
};  ///            ax_h_1   ax_h_1

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
0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x03d1, 0x0000, 0x0418, 0x0002,     // r08 must have a value ... no
idea why (ector: it's the looped addressing regs)
0x0000, 0x0000, 0x00ff, 0x1804, 0xdb70, 0x4ddb, 0x0000, 0x0000,
0x0000, 0x0000, 0x0000, 0xde6d, 0x0000, 0x0000, 0x0000, 0x004e,
};*/

u16 dspreg_out[1000][32];

/*
// gba ucode dmas result here
u32 SecParams_out[2] __attribute__ ((aligned (0x20))) = {
  0x11223344, // key
  0x55667788 // bootinfo
};

// ripped from demo
u32 SecParams_in[8] __attribute__ ((aligned (0x20))) = {
  0xDB967E0F, // key from gba
  0x00000002,
  0x00000002,
  0x00001078,
  (u32)SecParams_out, //0x80075060, // ptr to receiving buffer
  // padding?
  0x00000000,
  0x00000000,
  0x00000000
};
*/

// UI (interactive register editing)
u32 ui_mode;

enum
{
  UIM_SEL = 1,
  UIM_EDIT_REG = 2,
  UIM_EDIT_BIN = 4,
};

// Currently selected register.
s32 cursor_reg = 0;
// Currently selected digit.
s32 small_cursor_x;
// Value currently being edited.
u16* reg_value;

RealDSP real_dsp;

// Currently running microcode
int curUcode = 0, runningUcode = 1;

int dsp_steps = 0;

constexpr std::array reg_names = {
    "ar0", "ar1", "ar2", "ar3", "ix0", "ix1", "ix2", "ix3", "wr0", "wr1", "wr2",
    "wr3", "st0", "st1", "st2", "st3", "c0h", "c1h", "cr ", "sr ", "pl ", "pm1",
    "ph ", "pm2", "x0l", "x1l", "x0h", "x1h", "c0l", "c1l", "c0m", "c1m",
};

void print_reg_block(int x, int y, int sel, const u16* regs, const u16* compare_regs)
{
  for (int j = 0; j < 4; j++)
  {
    for (int i = 0; i < 8; i++)
    {
      const int reg = j * 8 + i;
      u8 color1 = regs[reg] == compare_regs[reg] ? CON_BRIGHT_WHITE : CON_BRIGHT_RED;
      CON_SetColor(sel == reg ? CON_BRIGHT_YELLOW : CON_GREEN);
      CON_Printf(x + j * 9, i + y, "%s ", reg_names[reg]);
      for (int k = 0; k < 4; k++)
      {
        if (sel == reg && k == small_cursor_x && ui_mode == UIM_EDIT_REG)
          CON_SetColor(CON_BRIGHT_CYAN);
        else
          CON_SetColor(color1);
        CON_Printf(x + 4 + j * 9 + k, i + y, "%01x", (regs[reg] >> ((3 - k) * 4)) & 0xf);
      }
    }
  }
  CON_SetColor(CON_WHITE);

  CON_Printf(x + 2, y + 9, "ACC0: %02x %04x %04x", regs[DSP_REG_ACH0] & 0xff, regs[DSP_REG_ACM0],
             regs[DSP_REG_ACL0]);
  CON_Printf(x + 2, y + 10, "ACC1: %02x %04x %04x", regs[DSP_REG_ACH1] & 0xff, regs[DSP_REG_ACM1],
             regs[DSP_REG_ACL1]);
  CON_Printf(x + 2, y + 11, "AX0:     %04x %04x", regs[DSP_REG_AXH0], regs[DSP_REG_AXL0]);
  CON_Printf(x + 2, y + 12, "AX1:     %04x %04x", regs[DSP_REG_AXH1], regs[DSP_REG_AXL1]);

  u64 prod = (u64(regs[DSP_REG_PRODH]) << 32) + (u64(regs[DSP_REG_PRODM]) << 16) +
             (u64(regs[DSP_REG_PRODM2]) << 16) + u64(regs[DSP_REG_PRODL]);
  u8 prod_h = (prod >> 32) & 0xff;
  u16 prod_m = (prod >> 16) & 0xffff;
  u16 prod_l = prod & 0xffff;
  CON_Printf(x + 2, y + 13, "PROD: %02x %04x %04x", prod_h, prod_m, prod_l);

  CON_Printf(x + 2, y + 14, "SR:");
  for (int i = 0; i < 16; ++i)
    CON_Printf(x + 6 + i + i / 4, y + 14, "%c", regs[DSP_REG_SR] & (1 << (15 - i)) ? '1' : '0');
  CON_Printf(x + 21, y + 15, "SZOC");
}

void print_regs(int _step, int _dsp_steps)
{
  const u16* regs = _step == 0 ? dspreg_in : dspreg_out[_step - 1];
  const u16* regs2 = dspreg_out[_step];

  print_reg_block(0, 2, _step == 0 ? cursor_reg : -1, regs, regs2);
  print_reg_block(38, 2, -1, regs2, regs);

  CON_SetColor(CON_WHITE);
  CON_Printf(33, 17, "%i / %i      ", _step + 1, _dsp_steps);

  return;

  static int count = 0;
  int x = 0, y = 16;
  if (count > 2)
    CON_Clear();
  count = 0;
  CON_SetColor(CON_WHITE);
  for (int i = 0x0; i < 0xf70; i++)
  {
    if (dspbufC[i] != mem_dump[i])
    {
      CON_Printf(x, y, "%04x=%04x", i, dspbufC[i]);
      count++;
      x += 10;
      if (x >= 60)
      {
        x = 0;
        y++;
      }
    }
  }
  CON_Printf(4, 25, "%08x", count);
}

void UpdateLastMessage(const char* msg)
{
  CON_PrintRow(4, 24, msg);
}

void DumpDSP_ROMs(const u16* rom, const u16* coef)
{
  char filename[260] = {0};
  sprintf(filename, "sd:/dsp_rom.bin");
  FILE* fROM = fopen(filename, "wb");
  sprintf(filename, "sd:/dsp_coef.bin");
  FILE* fCOEF = fopen(filename, "wb");

  if (fROM && fCOEF)
  {
    fwrite(MEM_PHYSICAL_TO_K0(rom), 0x2000, 1, fROM);
    fwrite(MEM_PHYSICAL_TO_K0(coef), 0x1000, 1, fCOEF);

    UpdateLastMessage("DSP ROMs dumped to SD");
  }
  else
  {
    UpdateLastMessage("SD Write Error");
  }

  if (fROM)
    fclose(fROM);
  if (fCOEF)
    fclose(fCOEF);
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
#endif
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
#endif
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
}

void handle_dsp_mail(void)
{
  // Should put a loop around this too.
  if (DSP_CheckMailFrom())
  {
    u32 mail = DSP_ReadMailFrom();

    if (mail == 0x8071feed)
    {
      // DSP ready for task. Let's send one.
      // First, prepare data.
      for (int n = 0; n < 32; n++)
        dspbufC[0x00 + n] = dspreg_in[n];
      DCFlushRange(dspbufC, 0x2000);
      // Then send the code.
      DCFlushRange((void*)dsp_code[curUcode], 0x2000);
      // DMA ucode to iram base, entry point is just after exception vectors...0x10
      // NOTE: for any ucode made by dsptool, the block length will be 8191
      real_dsp.SendTask((void*)MEM_VIRTUAL_TO_PHYSICAL(dsp_code[curUcode]), 0,
                        sizeof(dsp_code[curUcode]) - 1, 0x10);

      runningUcode = curUcode + 1;

      // Clear exception status since we've loaded a new ucode
      CON_BlankRow(25);
    }
    else if ((mail & 0xffff0000) == 0x8bad0000)
    {
      // dsp_base.inc is reporting an exception happened
      CON_PrintRow(4, 25, "%s caused exception %x at step %i", UCODE_NAMES[curUcode], mail & 0xff,
                   dsp_steps);
    }
    else if (mail == 0x8888dead)
    {
      // Send memory dump (DSP DRAM from someone's GameCube?)
      // not really sure why this is important - I guess just to try to keep tests predictable
      u16* tmpBuf = (u16*)MEM_VIRTUAL_TO_PHYSICAL(mem_dump);

      while (real_dsp.CheckMailTo())
        ;
      real_dsp.SendMailTo((u32)tmpBuf);
      while (real_dsp.CheckMailTo())
        ;
    }
    else if (mail == 0x8888beef)
    {
      // Provide register base to DSP (if using dsp_base.inc, it will DMA them to the correct place)
      while (real_dsp.CheckMailTo())
        ;
      real_dsp.SendMailTo((u32)dspbufP);
      while (real_dsp.CheckMailTo())
        ;
    }
    else if (mail == 0x8888feeb)
    {
      // We got a stepful of registers.
      DCInvalidateRange(dspbufC, 0x2000);
      for (int i = 0; i < 32; i++)
        dspreg_out[dsp_steps][i] = dspbufC[0xf80 + i];

      dsp_steps++;

      while (real_dsp.CheckMailTo())
        ;
      real_dsp.SendMailTo(0x8000dead);
      while (real_dsp.CheckMailTo())
        ;
    }
    else if (mail == 0x80050000)
    {
      CON_PrintRow(4, 25, "ACCOV at step %i", dsp_steps);
    }

    // ROM dumping mails
    else if (mail == 0x8888c0de)
    {
      // DSP has copied irom to its DRAM...send address so it can dma it back
      while (real_dsp.CheckMailTo())
        ;
      real_dsp.SendMailTo((u32)dspbufP);
      while (real_dsp.CheckMailTo())
        ;
    }
    else if (mail == 0x8888da7a)
    {
      // DSP has copied coef to its DRAM...send address so it can DMA it back
      while (real_dsp.CheckMailTo())
        ;
      real_dsp.SendMailTo((u32)&dspbufP[0x1000]);
      while (real_dsp.CheckMailTo())
        ;

      // Now we can do something useful with the buffer :)
      DumpDSP_ROMs(dspbufP, &dspbufP[0x1000]);
    }

    // SDK status mails
    /*
    // GBA ucode
    else if (mail == 0xdcd10000) // DSP_INIT
    {
      real_dsp.SendMailTo(0xabba0000);
      while (real_dsp.CheckMailTo());
      DCFlushRange(SecParams_in, sizeof(SecParams_in));
      CON_PrintRow(4, 25, "SecParams_out = %x", SecParams_in[4]);
      real_dsp.SendMailTo((u32)SecParams_in);
      while (real_dsp.CheckMailTo());
    }
    else if (mail == 0xdcd10003) // DSP_DONE
    {
      real_dsp.SendMailTo(0xcdd1babe); // custom mail to tell DSP to halt (calls end_of_test)
      while (real_dsp.CheckMailTo());

      DCInvalidateRange(SecParams_out, sizeof(SecParams_out));
      CON_PrintRow(4, 26, "SecParams_out: %08x %08x",
      SecParams_out[0], SecParams_out[1]);
    }
    */

    CON_PrintRow(2, 1, "UCode: %d/%d %s, Last mail: %08x", curUcode + 1, NUM_UCODES,
                 UCODE_NAMES[curUcode], mail);
  }
}

void dump_all_ucodes(bool fastmode)
{
  char filename[260] = {0};
  char temp[100];
  u32 written;

  sprintf(filename, "sd:/dsp_dump_all.bin");
  FILE* f2 = fopen(filename, "wb");
  fclose(f2);

  for (int UCodeToDump = 0; UCodeToDump < NUM_UCODES; UCodeToDump++)
  {
    // First, change the microcode
    dsp_steps = 0;
    curUcode = UCodeToDump;
    runningUcode = 0;

    DCInvalidateRange(dspbufC, 0x2000);
    DCFlushRange(dspbufC, 0x2000);

    real_dsp.Reset();

    // Loop over handling mail until we've stopped stepping
    // dsp_steps-3 compensates for mails to setup the ucode
    for (int steps_cache = dsp_steps - 3; steps_cache <= dsp_steps; steps_cache++)
    {
      VIDEO_WaitVSync();
      handle_dsp_mail();
    }
    VIDEO_WaitVSync();

    sprintf(filename, "sd:/dsp_dump_all.bin");
    FILE* f2 = fopen(filename, "ab");

    if (fastmode == false)
    {
      // Then write microcode dump to file
      sprintf(filename, "sd:/dsp_dump%d.bin", UCodeToDump);
      FILE* f = fopen(filename, "wb");
      if (f)
      {
        // First write initial regs
        written = fwrite(dspreg_in, 1, 32 * 2, f);

        // Then write all the dumps.
        written += fwrite(dspreg_out, 1, dsp_steps * 32 * 2, f);
        fclose(f);
      }
      else
      {
        UpdateLastMessage("SD Write Error");
        break;
      }
    }

    if (f2)  // all in 1 dump file (extra)
    {
      if (UCodeToDump == 0)
      {
        // First write initial regs
        written = fwrite(dspreg_in, 1, 32 * 2, f2);
        written += fwrite(dspreg_out, 1, dsp_steps * 32 * 2, f2);
      }
      else
      {
        written = fwrite(dspreg_out, 1, dsp_steps * 32 * 2, f2);
      }

      fclose(f2);

      if (UCodeToDump < NUM_UCODES - 1)
      {
        sprintf(temp, "Dump %d Successful. Wrote %d bytes, steps: %d", UCodeToDump + 1, written,
                dsp_steps);
        UpdateLastMessage(temp);
      }
      else
      {
        UpdateLastMessage("DUMPING DONE!");
      }
    }
    else
    {
      UpdateLastMessage("SD Write Error");
      break;
    }
  }
}

// Shove common, un-dsp-ish init things here
void InitGeneral()
{
  // Initialize the video system
  VIDEO_Init();

  // This function initializes the attached controllers
  PAD_Init();
#ifdef HW_RVL
  WPAD_Init();
#endif

  // Obtain the preferred video mode from the system
  // This will correspond to the settings in the Wii Menu
  rmode = VIDEO_GetPreferredMode(nullptr);

  // Allocate memory for the display in the uncached region
  xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

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
  if (rmode->viTVMode & VI_NON_INTERLACE)
    VIDEO_WaitVSync();

  // Initialize the console, required for printf
  CON_Init(xfb, 20, 64, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth * VI_DISPLAY_PIX_SZ);

#ifdef HW_RVL
  // Initialize FAT so we can write to SD.
  __io_wiisd.startup();
  fatMountSimple("sd", &__io_wiisd);
#else
  // Initialize FAT so we can write to SD Gecko in slot B.
  fatMountSimple("sd", &__io_gcsdb);

  // Init debug over BBA...change IPs to suite your needs
  tcp_localip = "192.168.1.103";
  tcp_netmask = "255.255.255.0";
  tcp_gateway = "192.168.1.2";
  DEBUG_Init(GDBSTUB_DEVICE_TCP, GDBSTUB_DEF_TCPPORT);
#endif
}

void ExitToLoader()
{
  fatUnmount("sd");
#ifdef HW_RVL
  __io_wiisd.shutdown();
#endif

  UpdateLastMessage("Exiting...");
  real_dsp.Reset();
  reboot();
}

int main()
{
  InitGeneral();

  ui_mode = UIM_SEL;

  dspbufP = (u16*)MEM_VIRTUAL_TO_PHYSICAL(dspbuffer);  // physical
  dspbufC = dspbuffer;                                 // cached
  dspbufU = (u32*)(MEM_K0_TO_K1(dspbuffer));           // uncached

  DCInvalidateRange(dspbuffer, 0x2000);
  for (int j = 0; j < 0x800; j++)
    dspbufU[j] = 0xffffffff;

  // Initialize DSP.
  real_dsp.Init();

  int show_step = 0;
  while (true)
  {
    handle_dsp_mail();

    VIDEO_WaitVSync();

    PAD_ScanPads();
    if (PAD_ButtonsDown(0) & PAD_BUTTON_START)
      ExitToLoader();
#ifdef HW_RVL
    WPAD_ScanPads();
    if (WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME)
      ExitToLoader();

    CON_Printf(2, 18, "Controls:");
    CON_Printf(4, 19, "+/- (GC:'L'/'R') to move");
    CON_Printf(4, 20, "A (GC:'A') to edit register; B (GC:'B') to start over");
    CON_Printf(4, 21, "1 (GC:'Z') to move next microcode");
    CON_Printf(4, 22,
               "2 (GC:'X') dump results to SD; UP (GC:'Y') dump results to SD (SINGLE FILE)");
    CON_Printf(4, 23, "Home (GC:'START') to exit");
#else
    CON_Printf(2, 18, "Controls:");
    CON_Printf(4, 19, "L/R to move");
    CON_Printf(4, 20, "A to edit register, B to start over");
    CON_Printf(4, 21, "Z to move to next microcode");
    CON_Printf(4, 22, "Start to exit");
#endif

    print_regs(show_step, dsp_steps);

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
      dsp_steps =
          0;  // Let's not add the new steps after the original ones. That was just annoying.

      DCInvalidateRange(dspbufC, 0x2000);
      DCFlushRange(dspbufC, 0x2000);

      // Reset the DSP.
      real_dsp.Reset();
      UpdateLastMessage("OK");
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
      UpdateLastMessage("OK");
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
      UpdateLastMessage("OK");
    }

#ifdef HW_RVL
    if ((WPAD_ButtonsDown(0) & WPAD_BUTTON_1) || (PAD_ButtonsDown(0) & PAD_TRIGGER_Z))
#else
    if (PAD_ButtonsDown(0) & PAD_TRIGGER_Z)
#endif
    {
      curUcode++;
      if (curUcode == NUM_UCODES)
        curUcode = 0;

      // Reset step counters since we're in a new ucode.
      show_step = 0;
      dsp_steps = 0;

      DCInvalidateRange(dspbufC, 0x2000);
      for (int n = 0; n < 0x2000; n++)
      {
        //	dspbufU[n/2] = 0; dspbufC[n] = 0;
      }
      DCFlushRange(dspbufC, 0x2000);

      // Reset the DSP.
      real_dsp.Reset();
      UpdateLastMessage("OK");

      // Waiting for video to synchronize (enough time to set our new microcode)
      VIDEO_WaitVSync();
    }

#ifdef HW_RVL
    // Probably could offer to save to sd gecko or something on gc...
    // The future is web-based reporting ;)
    if ((WPAD_ButtonsDown(0) & WPAD_BUTTON_2) || (PAD_ButtonsDown(0) & PAD_BUTTON_X))
    {
      dump_all_ucodes(false);
    }

    // Dump all results into 1 file (skip file per ucode part) = FAST because of LIBFAT filecreate
    // bug
    if ((WPAD_ButtonsDown(0) & WPAD_BUTTON_UP) || (PAD_ButtonsDown(0) & PAD_BUTTON_Y))
    {
      dump_all_ucodes(true);
    }
#endif

  }  // end main loop

  ExitToLoader();

  // Will never reach here, but just to be sure..
  exit(0);
  return 0;
}
