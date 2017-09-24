// Copyright 2008 Dolphin Emulator Project
// Copyright 2004 Duddie & Tratax
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

namespace DSP
{
enum Mailbox
{
  MAILBOX_CPU,
  MAILBOX_DSP
};

u32 gdsp_mbox_peek(Mailbox mbx);
void gdsp_mbox_write_h(Mailbox mbx, u16 val);
void gdsp_mbox_write_l(Mailbox mbx, u16 val);
u16 gdsp_mbox_read_h(Mailbox mbx);
u16 gdsp_mbox_read_l(Mailbox mbx);

void gdsp_ifx_init();
void gdsp_ifx_write(u32 addr, u16 val);
u16 gdsp_ifx_read(u16 addr);
}  // namespace DSP
