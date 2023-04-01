// Copyright 2008 Dolphin Emulator Project
// Copyright 2004 Duddie & Tratax
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

#include "Core/DSP/DSPCore.h"
#include "Core/DSP/DSPTables.h"

u16  dsp_imem_read(u16 addr);
void dsp_dmem_write(u16 addr, u16 val);
u16  dsp_dmem_read(u16 addr);

inline u16 dsp_fetch_code()
{
	u16 opc = dsp_imem_read(g_dsp.pc);

	g_dsp.pc++;
	return opc;
}

inline u16 dsp_peek_code()
{
	return dsp_imem_read(g_dsp.pc);
}

inline void dsp_skip_inst()
{
	g_dsp.pc += opTable[dsp_peek_code()]->size;
}
