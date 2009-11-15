// Copyright (C) 2003-2009 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/


#ifndef _OPCODEDECODER_H_
#define _OPCODEDECODER_H_

#include "pluginspecs_video.h"

namespace OpcodeDecoder
{

    #define GX_NOP                      0x00

    #define GX_LOAD_BP_REG              0x61
    #define GX_LOAD_CP_REG              0x08
    #define GX_LOAD_XF_REG              0x10
    #define GX_LOAD_INDX_A              0x20
    #define GX_LOAD_INDX_B              0x28
    #define GX_LOAD_INDX_C              0x30
    #define GX_LOAD_INDX_D              0x38

    #define GX_CMD_CALL_DL              0x40
    #define GX_CMD_INVL_VC              0x48

    #define GX_PRIMITIVE_MASK           0x78
    #define GX_PRIMITIVE_SHIFT          3
    #define GX_VAT_MASK                 0x07

    //these are defined 1/8th of their real values and without their top bit
    #define GX_DRAW_QUADS               0x0   //0x80
    #define GX_DRAW_TRIANGLES           0x2   //0x90
    #define GX_DRAW_TRIANGLE_STRIP      0x3   //0x98
    #define GX_DRAW_TRIANGLE_FAN        0x4   //0xA0
    #define GX_DRAW_LINES               0x5   //0xA8
    #define GX_DRAW_LINE_STRIP          0x6   //0xB0
    #define GX_DRAW_POINTS              0x7   //0xB8

    void Init();

    void ResetDecoding();

    bool CommandRunnable(u32 iBufferSize);

    void Run(u32 iBufferSize);
}

#endif
