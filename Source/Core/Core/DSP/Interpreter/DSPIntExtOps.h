// Copyright 2008 Dolphin Emulator Project
// Copyright 2005 Duddie
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/DSP/DSPCommon.h"

// Extended opcode support.
// Many opcode have the lower 0xFF (some only 0x7f) free - there, an opcode extension
// can be stored.

namespace DSP::Interpreter::Ext
{
void l(UDSPInstruction opc);
void ln(UDSPInstruction opc);
void ls(UDSPInstruction opc);
void lsn(UDSPInstruction opc);
void lsm(UDSPInstruction opc);
void lsnm(UDSPInstruction opc);
void sl(UDSPInstruction opc);
void sln(UDSPInstruction opc);
void slm(UDSPInstruction opc);
void slnm(UDSPInstruction opc);
void s(UDSPInstruction opc);
void sn(UDSPInstruction opc);
void ld(UDSPInstruction opc);
void ldax(UDSPInstruction opc);
void ldn(UDSPInstruction opc);
void ldaxn(UDSPInstruction opc);
void ldm(UDSPInstruction opc);
void ldaxm(UDSPInstruction opc);
void ldnm(UDSPInstruction opc);
void ldaxnm(UDSPInstruction opc);
void mv(UDSPInstruction opc);
void dr(UDSPInstruction opc);
void ir(UDSPInstruction opc);
void nr(UDSPInstruction opc);
void nop(UDSPInstruction opc);
}  // namespace DSP::Interpreter::Ext
