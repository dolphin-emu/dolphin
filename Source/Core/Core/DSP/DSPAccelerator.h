// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <functional>

#include "Common/CommonTypes.h"

namespace DSP
{
u16 ReadAccelerator(u32 start_address, u32 end_address, u32* current_address, u16 sample_format,
                    s16* yn1, s16* yn2, u16* pred_scale, s16* coefs,
                    std::function<void()> end_exception);

u16 dsp_read_accelerator();

u16 dsp_read_aram_d3();
void dsp_write_aram_d3(u16 value);
}  // namespace DSP
