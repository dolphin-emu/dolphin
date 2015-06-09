// Copyright 2008 Dolphin Emulator Project
// Copyright 2004 Duddie & Tratax
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

void dsp_reg_store_stack(int stack_reg, u16 val);
u16 dsp_reg_load_stack(int stack_reg);
