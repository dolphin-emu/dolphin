// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#define DSP_REG_AR0 0x00  // address registers
#define DSP_REG_AR1 0x01
#define DSP_REG_AR2 0x02
#define DSP_REG_AR3 0x03

#define DSP_REG_IX0 0x04  // indexing registers (actually, mostly used as increments)
#define DSP_REG_IX1 0x05
#define DSP_REG_IX2 0x06
#define DSP_REG_IX3 0x07

#define DSP_REG_WR0                                                                                \
  0x08  // address wrapping registers. should be initialized to 0xFFFF if not used.
#define DSP_REG_WR1 0x09
#define DSP_REG_WR2 0x0a
#define DSP_REG_WR3 0x0b

#define DSP_REG_ST0 0x0c  // stacks.
#define DSP_REG_ST1 0x0d
#define DSP_REG_ST2 0x0e
#define DSP_REG_ST3 0x0f

#define DSP_REG_CR 0x12  // Seems to be the top 8 bits of LRS/SRS.
#define DSP_REG_SR 0x13

#define DSP_REG_PRODL 0x14  // product.
#define DSP_REG_PRODM 0x15
#define DSP_REG_PRODH 0x16
#define DSP_REG_PRODM2 0x17

#define DSP_REG_AXL0 0x18
#define DSP_REG_AXL1 0x19
#define DSP_REG_AXH0 0x1a
#define DSP_REG_AXH1 0x1b

#define DSP_REG_ACC0 0x1c  // accumulator (global)
#define DSP_REG_ACC1 0x1d

#define DSP_REG_ACL0 0x1c  // Low accumulator
#define DSP_REG_ACL1 0x1d
#define DSP_REG_ACM0 0x1e  // Mid accumulator
#define DSP_REG_ACM1 0x1f
#define DSP_REG_ACH0 0x10  // Sign extended 8 bit register 0
#define DSP_REG_ACH1 0x11  // Sign extended 8 bit register 1
