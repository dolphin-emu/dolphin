// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _OPCODE_DECODING_H
#define _OPCODE_DECODING_H

#define GX_NOP                      0x00

#define GX_LOAD_BP_REG              0x61
#define GX_LOAD_CP_REG              0x08
#define GX_LOAD_XF_REG              0x10
#define GX_LOAD_INDX_A              0x20
#define GX_LOAD_INDX_B              0x28
#define GX_LOAD_INDX_C              0x30
#define GX_LOAD_INDX_D              0x38

#define GX_CMD_CALL_DL              0x40
#define GX_CMD_UNKNOWN_METRICS      0x44
#define GX_CMD_INVL_VC              0x48

#define GX_PRIMITIVE_MASK           0x78
#define GX_PRIMITIVE_SHIFT          3
#define GX_VAT_MASK                 0x07

//these are defined 1/8th of their real values and without their top bit
#define GX_DRAW_QUADS               0x0   // 0x80
#define GX_DRAW_TRIANGLES           0x2   // 0x90
#define GX_DRAW_TRIANGLE_STRIP      0x3   // 0x98
#define GX_DRAW_TRIANGLE_FAN        0x4   // 0xA0
#define GX_DRAW_LINES               0x5   // 0xA8
#define GX_DRAW_LINE_STRIP          0x6   // 0xB0
#define GX_DRAW_POINTS              0x7   // 0xB8
#define GX_DRAW_NONE                0x1;  // This is a fake value to used in the backends

extern bool g_bRecordFifoData;

void OpcodeDecoder_Init();
void OpcodeDecoder_Shutdown();
u32 OpcodeDecoder_Run(bool skipped_frame);
void ExecuteDisplayList(u32 address, u32 size);
#endif // _OPCODE_DECODING_H
