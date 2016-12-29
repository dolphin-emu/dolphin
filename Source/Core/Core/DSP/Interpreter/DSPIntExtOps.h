// Copyright 2008 Dolphin Emulator Project
// Copyright 2005 Duddie
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/DSP/DSPCommon.h"

// Extended opcode support.
// Many opcode have the lower 0xFF (some only 0x7f) free - there, an opcode extension
// can be stored.

namespace DSP
{
namespace Interpreter
{
namespace Ext
{
void l(const UDSPInstruction opc);
void ln(const UDSPInstruction opc);
void ls(const UDSPInstruction opc);
void lsn(const UDSPInstruction opc);
void lsm(const UDSPInstruction opc);
void lsnm(const UDSPInstruction opc);
void sl(const UDSPInstruction opc);
void sln(const UDSPInstruction opc);
void slm(const UDSPInstruction opc);
void slnm(const UDSPInstruction opc);
void s(const UDSPInstruction opc);
void sn(const UDSPInstruction opc);
void ld(const UDSPInstruction opc);
void ldax(const UDSPInstruction opc);
void ldn(const UDSPInstruction opc);
void ldaxn(const UDSPInstruction opc);
void ldm(const UDSPInstruction opc);
void ldaxm(const UDSPInstruction opc);
void ldnm(const UDSPInstruction opc);
void ldaxnm(const UDSPInstruction opc);
void mv(const UDSPInstruction opc);
void dr(const UDSPInstruction opc);
void ir(const UDSPInstruction opc);
void nr(const UDSPInstruction opc);
void nop(const UDSPInstruction opc);

}  // namespace Ext
}  // namespace Interpeter
}  // namespace DSP
