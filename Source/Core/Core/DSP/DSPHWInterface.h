// Copyright 2008 Dolphin Emulator Project
// Copyright 2004 Duddie & Tratax
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

#define GDSP_MBOX_CPU   0
#define GDSP_MBOX_DSP   1

u32  gdsp_mbox_peek(u8 mbx);
void gdsp_mbox_write_h(u8 mbx, u16 val);
void gdsp_mbox_write_l(u8 mbx, u16 val);
u16  gdsp_mbox_read_h(u8 mbx);
u16  gdsp_mbox_read_l(u8 mbx);

void gdsp_ifx_init();

void gdsp_ifx_write(u32 addr, u32 val);
u16  gdsp_ifx_read(u16 addr);
