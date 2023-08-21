// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/DSP/Interpreter/DSPIntTables.h"

#include <array>

#include "Common/CommonTypes.h"
#include "Core/DSP/DSPTables.h"
#include "Core/DSP/Interpreter/DSPInterpreter.h"

namespace DSP::Interpreter
{
struct InterpreterOpInfo
{
  u16 opcode;
  u16 opcode_mask;
  InterpreterFunction function;
};

// clang-format off
constexpr std::array<InterpreterOpInfo, 125> s_opcodes
{{
  {0x0000, 0xfffc, &Interpreter::nop},

  {0x0004, 0xfffc, &Interpreter::dar},
  {0x0008, 0xfffc, &Interpreter::iar},
  {0x000c, 0xfffc, &Interpreter::subarn},
  {0x0010, 0xfff0, &Interpreter::addarn},

  {0x0021, 0xffff, &Interpreter::halt},

  {0x02d0, 0xfff0, &Interpreter::ret},

  {0x02f0, 0xfff0, &Interpreter::rti},

  {0x02b0, 0xfff0, &Interpreter::call},

  {0x0270, 0xfff0, &Interpreter::ifcc},

  {0x0290, 0xfff0, &Interpreter::jcc},

  {0x1700, 0xff10, &Interpreter::jmprcc},

  {0x1710, 0xff10, &Interpreter::callr},

  {0x1200, 0xff00, &Interpreter::sbclr},
  {0x1300, 0xff00, &Interpreter::sbset},

  {0x1400, 0xfec0, &Interpreter::lsl},
  {0x1440, 0xfec0, &Interpreter::lsr},
  {0x1480, 0xfec0, &Interpreter::asl},
  {0x14c0, 0xfec0, &Interpreter::asr},

  // these two were discovered by ector
  {0x02ca, 0xffff, &Interpreter::lsrn},
  {0x02cb, 0xffff, &Interpreter::asrn},

  {0x0080, 0xffe0, &Interpreter::lri},
  {0x00c0, 0xffe0, &Interpreter::lr},
  {0x00e0, 0xffe0, &Interpreter::sr},

  {0x1c00, 0xfc00, &Interpreter::mrr},

  {0x1600, 0xff00, &Interpreter::si},

  {0x0400, 0xfe00, &Interpreter::addis},
  {0x0600, 0xfe00, &Interpreter::cmpis},
  {0x0800, 0xf800, &Interpreter::lris},

  {0x0200, 0xfeff, &Interpreter::addi},
  {0x0220, 0xfeff, &Interpreter::xori},
  {0x0240, 0xfeff, &Interpreter::andi},
  {0x0260, 0xfeff, &Interpreter::ori},
  {0x0280, 0xfeff, &Interpreter::cmpi},

  {0x02a0, 0xfeff, &Interpreter::andf},
  {0x02c0, 0xfeff, &Interpreter::andcf},

  {0x0210, 0xfefc, &Interpreter::ilrr},
  {0x0214, 0xfefc, &Interpreter::ilrrd},
  {0x0218, 0xfefc, &Interpreter::ilrri},
  {0x021c, 0xfefc, &Interpreter::ilrrn},

  // LOOPS
  {0x0040, 0xffe0, &Interpreter::loop},
  {0x0060, 0xffe0, &Interpreter::bloop},
  {0x1000, 0xff00, &Interpreter::loopi},
  {0x1100, 0xff00, &Interpreter::bloopi},

  // load and store value pointed by indexing reg and increment; LRR/SRR variants
  {0x1800, 0xff80, &Interpreter::lrr},
  {0x1880, 0xff80, &Interpreter::lrrd},
  {0x1900, 0xff80, &Interpreter::lrri},
  {0x1980, 0xff80, &Interpreter::lrrn},

  {0x1a00, 0xff80, &Interpreter::srr},
  {0x1a80, 0xff80, &Interpreter::srrd},
  {0x1b00, 0xff80, &Interpreter::srri},
  {0x1b80, 0xff80, &Interpreter::srrn},

  // 2
  {0x2000, 0xf800, &Interpreter::lrs},
  {0x2800, 0xfe00, &Interpreter::srsh},
  {0x2c00, 0xfc00, &Interpreter::srs},

  // opcodes that can be extended

  // 3 - main opcode defined by 9 bits, extension defined by last 7 bits!!
  {0x3000, 0xfc80, &Interpreter::xorr},
  {0x3400, 0xfc80, &Interpreter::andr},
  {0x3800, 0xfc80, &Interpreter::orr},
  {0x3c00, 0xfe80, &Interpreter::andc},
  {0x3e00, 0xfe80, &Interpreter::orc},
  {0x3080, 0xfe80, &Interpreter::xorc},
  {0x3280, 0xfe80, &Interpreter::notc},
  {0x3480, 0xfc80, &Interpreter::lsrnrx},
  {0x3880, 0xfc80, &Interpreter::asrnrx},
  {0x3c80, 0xfe80, &Interpreter::lsrnr},
  {0x3e80, 0xfe80, &Interpreter::asrnr},

  // 4
  {0x4000, 0xf800, &Interpreter::addr},
  {0x4800, 0xfc00, &Interpreter::addax},
  {0x4c00, 0xfe00, &Interpreter::add},
  {0x4e00, 0xfe00, &Interpreter::addp},

  // 5
  {0x5000, 0xf800, &Interpreter::subr},
  {0x5800, 0xfc00, &Interpreter::subax},
  {0x5c00, 0xfe00, &Interpreter::sub},
  {0x5e00, 0xfe00, &Interpreter::subp},

  // 6
  {0x6000, 0xf800, &Interpreter::movr},
  {0x6800, 0xfc00, &Interpreter::movax},
  {0x6c00, 0xfe00, &Interpreter::mov},
  {0x6e00, 0xfe00, &Interpreter::movp},

  // 7
  {0x7000, 0xfc00, &Interpreter::addaxl},
  {0x7400, 0xfe00, &Interpreter::incm},
  {0x7600, 0xfe00, &Interpreter::inc},
  {0x7800, 0xfe00, &Interpreter::decm},
  {0x7a00, 0xfe00, &Interpreter::dec},
  {0x7c00, 0xfe00, &Interpreter::neg},
  {0x7e00, 0xfe00, &Interpreter::movnp},

  // 8
  {0x8000, 0xf700, &Interpreter::nx},
  {0x8100, 0xf700, &Interpreter::clr},
  {0x8200, 0xff00, &Interpreter::cmp},
  {0x8300, 0xff00, &Interpreter::mulaxh},
  {0x8400, 0xff00, &Interpreter::clrp},
  {0x8500, 0xff00, &Interpreter::tstprod},
  {0x8600, 0xfe00, &Interpreter::tstaxh},
  {0x8a00, 0xff00, &Interpreter::srbith},
  {0x8b00, 0xff00, &Interpreter::srbith},
  {0x8c00, 0xff00, &Interpreter::srbith},
  {0x8d00, 0xff00, &Interpreter::srbith},
  {0x8e00, 0xff00, &Interpreter::srbith},
  {0x8f00, 0xff00, &Interpreter::srbith},

  // 9
  {0x9000, 0xf700, &Interpreter::mul},
  {0x9100, 0xf700, &Interpreter::asr16},
  {0x9200, 0xf600, &Interpreter::mulmvz},
  {0x9400, 0xf600, &Interpreter::mulac},
  {0x9600, 0xf600, &Interpreter::mulmv},

  // A-B
  {0xa000, 0xe700, &Interpreter::mulx},
  {0xa100, 0xf700, &Interpreter::abs},
  {0xa200, 0xe600, &Interpreter::mulxmvz},
  {0xa400, 0xe600, &Interpreter::mulxac},
  {0xa600, 0xe600, &Interpreter::mulxmv},
  {0xb100, 0xf700, &Interpreter::tst},

  // C-D
  {0xc000, 0xe700, &Interpreter::mulc},
  {0xc100, 0xe700, &Interpreter::cmpaxh},
  {0xc200, 0xe600, &Interpreter::mulcmvz},
  {0xc400, 0xe600, &Interpreter::mulcac},
  {0xc600, 0xe600, &Interpreter::mulcmv},

  // E
  {0xe000, 0xfc00, &Interpreter::maddx},
  {0xe400, 0xfc00, &Interpreter::msubx},
  {0xe800, 0xfc00, &Interpreter::maddc},
  {0xec00, 0xfc00, &Interpreter::msubc},

  // F
  {0xf000, 0xfe00, &Interpreter::lsl16},
  {0xf200, 0xfe00, &Interpreter::madd},
  {0xf400, 0xfe00, &Interpreter::lsr16},
  {0xf600, 0xfe00, &Interpreter::msub},
  {0xf800, 0xfc00, &Interpreter::addpaxz},
  {0xfc00, 0xfe00, &Interpreter::clrl},
  {0xfe00, 0xfe00, &Interpreter::movpz},
}};

constexpr std::array<InterpreterOpInfo, 25> s_opcodes_ext
{{
  {0x0000, 0x00fc, &Interpreter::nop_ext},

  {0x0004, 0x00fc, &Interpreter::dr},
  {0x0008, 0x00fc, &Interpreter::ir},
  {0x000c, 0x00fc, &Interpreter::nr},
  {0x0010, 0x00f0, &Interpreter::mv},

  {0x0020, 0x00e4, &Interpreter::s},
  {0x0024, 0x00e4, &Interpreter::sn},

  {0x0040, 0x00c4, &Interpreter::l},
  {0x0044, 0x00c4, &Interpreter::ln},

  {0x0080, 0x00ce, &Interpreter::ls},
  {0x0082, 0x00ce, &Interpreter::sl},
  {0x0084, 0x00ce, &Interpreter::lsn},
  {0x0086, 0x00ce, &Interpreter::sln},
  {0x0088, 0x00ce, &Interpreter::lsm},
  {0x008a, 0x00ce, &Interpreter::slm},
  {0x008c, 0x00ce, &Interpreter::lsnm},
  {0x008e, 0x00ce, &Interpreter::slnm},

  {0x00c3, 0x00cf, &Interpreter::ldax},
  {0x00c7, 0x00cf, &Interpreter::ldaxn},
  {0x00cb, 0x00cf, &Interpreter::ldaxm},
  {0x00cf, 0x00cf, &Interpreter::ldaxnm},

  {0x00c0, 0x00cc, &Interpreter::ld},
  {0x00c4, 0x00cc, &Interpreter::ldn},
  {0x00c8, 0x00cc, &Interpreter::ldm},
  {0x00cc, 0x00cc, &Interpreter::ldnm},
}};
// clang-format on

namespace
{
std::array<InterpreterFunction, 65536> s_op_table;
std::array<InterpreterFunction, 256> s_ext_op_table;
bool s_tables_initialized = false;
}  // Anonymous namespace

InterpreterFunction GetOp(UDSPInstruction inst)
{
  return s_op_table[inst];
}

InterpreterFunction GetExtOp(UDSPInstruction inst)
{
  const bool has_seven_bit_extension = (inst >> 12) == 0x3;

  if (has_seven_bit_extension)
    return s_ext_op_table[inst & 0x7F];

  return s_ext_op_table[inst & 0xFF];
}

void InitInstructionTables()
{
  if (s_tables_initialized)
    return;

  // ext op table
  for (size_t i = 0; i < s_ext_op_table.size(); i++)
  {
    s_ext_op_table[i] = &Interpreter::nop;

    const auto iter = FindByOpcode(static_cast<UDSPInstruction>(i), s_opcodes_ext);
    if (iter == s_opcodes_ext.cend())
      continue;

    s_ext_op_table[i] = iter->function;
  }

  // op table
  for (size_t i = 0; i < s_op_table.size(); i++)
  {
    s_op_table[i] = &Interpreter::nop;

    const auto iter = FindByOpcode(static_cast<UDSPInstruction>(i), s_opcodes);
    if (iter == s_opcodes.cend())
      continue;

    s_op_table[i] = iter->function;
  }

  s_tables_initialized = true;
}
}  // namespace DSP::Interpreter
