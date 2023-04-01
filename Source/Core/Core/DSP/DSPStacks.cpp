// Copyright 2008 Dolphin Emulator Project
// Copyright 2004 Duddie & Tratax
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"

#include "Core/DSP/DSPCore.h"
#include "Core/DSP/DSPStacks.h"

// Stacks. The stacks are outside the DSP RAM, in dedicated hardware.

static void dsp_reg_stack_push(int stack_reg)
{
	g_dsp.reg_stack_ptr[stack_reg]++;
	g_dsp.reg_stack_ptr[stack_reg] &= DSP_STACK_MASK;
	g_dsp.reg_stack[stack_reg][g_dsp.reg_stack_ptr[stack_reg]] = g_dsp.r.st[stack_reg];
}

static void dsp_reg_stack_pop(int stack_reg)
{
	g_dsp.r.st[stack_reg] = g_dsp.reg_stack[stack_reg][g_dsp.reg_stack_ptr[stack_reg]];
	g_dsp.reg_stack_ptr[stack_reg]--;
	g_dsp.reg_stack_ptr[stack_reg] &= DSP_STACK_MASK;
}

void dsp_reg_store_stack(int stack_reg, u16 val)
{
	dsp_reg_stack_push(stack_reg);
	g_dsp.r.st[stack_reg] = val;
}

u16 dsp_reg_load_stack(int stack_reg)
{
	u16 val = g_dsp.r.st[stack_reg];
	dsp_reg_stack_pop(stack_reg);
	return val;
}
