// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <gccore.h>

// DSPCR bits
#define DSPCR_DSPRESET 0x0800   // Reset DSP
#define DSPCR_ARDMA 0x0200      // ARAM DMA in progress, if set
#define DSPCR_DSPINTMSK 0x0100  // * interrupt mask   (RW)
#define DSPCR_DSPINT 0x0080     // * interrupt active (RWC)
#define DSPCR_ARINTMSK 0x0040
#define DSPCR_ARINT 0x0020
#define DSPCR_AIINTMSK 0x0010
#define DSPCR_AIINT 0x0008
#define DSPCR_HALT 0x0004   // halt DSP
#define DSPCR_PIINT 0x0002  // assert DSP PI interrupt
#define DSPCR_RES 0x0001    // reset DSP

class IDSP
{
public:
  virtual ~IDSP() {}
  virtual void Init() = 0;
  virtual void Reset() = 0;
  virtual u32 CheckMailTo() = 0;
  virtual void SendMailTo(u32 mail) = 0;

  // Yeah, yeah, having a method here makes this not a pure interface - but
  // the implementation does nothing but calling the virtual methods above.
  void SendTask(void* addr, u16 iram_addr, u16 len, u16 start);
};
