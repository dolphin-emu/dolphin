// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dsp_interface.h"

void IDSP::SendTask(void* addr, u16 iram_addr, u16 len, u16 start)
{
  // addr			main ram addr			4byte aligned (1 Gekko word)
  // iram_addr	dsp addr				4byte aligned (2 DSP words)
  // len			block length in bytes	multiple of 4 (wtf? if you want to fill whole iram, you need
  // 8191)
  //											(8191 % 4 = 3) wtffff
  // start		dsp iram entry point
  while (CheckMailTo())
    ;
  SendMailTo(0x80F3A001);
  while (CheckMailTo())
    ;
  SendMailTo((u32)addr);
  while (CheckMailTo())
    ;
  SendMailTo(0x80F3C002);
  while (CheckMailTo())
    ;
  SendMailTo(iram_addr);
  while (CheckMailTo())
    ;
  SendMailTo(0x80F3A002);
  while (CheckMailTo())
    ;
  SendMailTo(len);
  while (CheckMailTo())
    ;
  SendMailTo(0x80F3B002);
  while (CheckMailTo())
    ;
  SendMailTo(0);
  while (CheckMailTo())
    ;
  SendMailTo(0x80F3D001);
  while (CheckMailTo())
    ;
  SendMailTo(start);
  while (CheckMailTo())
    ;
}
