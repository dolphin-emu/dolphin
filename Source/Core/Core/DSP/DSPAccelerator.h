// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

u16 dsp_read_accelerator();

u16 dsp_read_aram_d3();
void dsp_write_aram_d3(u16 value);
