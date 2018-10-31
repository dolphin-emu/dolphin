// Copyright 2008 Dolphin Emulator Project
// Copyright 2004 Duddie & Tratax
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

namespace DSP
{
u16 dsp_imem_read(u16 addr);
void dsp_dmem_write(u16 addr, u16 val);
u16 dsp_dmem_read(u16 addr);

u16 dsp_fetch_code();
u16 dsp_peek_code();
void dsp_skip_inst();
}  // namespace DSP
