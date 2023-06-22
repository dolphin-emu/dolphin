// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/DSP/Jit/x64/DSPJitTables.h"

#include <array>

#include "Common/CommonTypes.h"
#include "Core/DSP/DSPTables.h"
#include "Core/DSP/Jit/x64/DSPEmitter.h"

namespace DSP::JIT::x64
{
struct JITOpInfo
{
  u16 opcode;
  u16 opcode_mask;
  JITFunction function;
};

// clang-format off
const std::array<JITOpInfo, 125> s_opcodes =
{{
  {0x0000, 0xfffc, &DSPEmitter::nop},

  {0x0004, 0xfffc, &DSPEmitter::dar},
  {0x0008, 0xfffc, &DSPEmitter::iar},
  {0x000c, 0xfffc, &DSPEmitter::subarn},
  {0x0010, 0xfff0, &DSPEmitter::addarn},

  {0x0021, 0xffff, &DSPEmitter::halt},

  {0x02d0, 0xfff0, &DSPEmitter::ret},

  {0x02f0, 0xfff0, &DSPEmitter::rti},

  {0x02b0, 0xfff0, &DSPEmitter::call},

  {0x0270, 0xfff0, &DSPEmitter::ifcc},

  {0x0290, 0xfff0, &DSPEmitter::jcc},

  {0x1700, 0xff10, &DSPEmitter::jmprcc},

  {0x1710, 0xff10, &DSPEmitter::callr},

  {0x1200, 0xff00, &DSPEmitter::sbclr},
  {0x1300, 0xff00, &DSPEmitter::sbset},

  {0x1400, 0xfec0, &DSPEmitter::lsl},
  {0x1440, 0xfec0, &DSPEmitter::lsr},
  {0x1480, 0xfec0, &DSPEmitter::asl},
  {0x14c0, 0xfec0, &DSPEmitter::asr},

  // These two were discovered by ector
  {0x02ca, 0xffff, &DSPEmitter::lsrn},
  {0x02cb, 0xffff, &DSPEmitter::asrn},

  {0x0080, 0xffe0, &DSPEmitter::lri},
  {0x00c0, 0xffe0, &DSPEmitter::lr},
  {0x00e0, 0xffe0, &DSPEmitter::sr},

  {0x1c00, 0xfc00, &DSPEmitter::mrr},

  {0x1600, 0xff00, &DSPEmitter::si},

  {0x0400, 0xfe00, &DSPEmitter::addis},
  {0x0600, 0xfe00, &DSPEmitter::cmpis},
  {0x0800, 0xf800, &DSPEmitter::lris},

  {0x0200, 0xfeff, &DSPEmitter::addi},
  {0x0220, 0xfeff, &DSPEmitter::xori},
  {0x0240, 0xfeff, &DSPEmitter::andi},
  {0x0260, 0xfeff, &DSPEmitter::ori},
  {0x0280, 0xfeff, &DSPEmitter::cmpi},

  {0x02a0, 0xfeff, &DSPEmitter::andf},
  {0x02c0, 0xfeff, &DSPEmitter::andcf},

  {0x0210, 0xfefc, &DSPEmitter::ilrr},
  {0x0214, 0xfefc, &DSPEmitter::ilrrd},
  {0x0218, 0xfefc, &DSPEmitter::ilrri},
  {0x021c, 0xfefc, &DSPEmitter::ilrrn},

  // LOOPS
  {0x0040, 0xffe0, &DSPEmitter::loop},
  {0x0060, 0xffe0, &DSPEmitter::bloop},
  {0x1000, 0xff00, &DSPEmitter::loopi},
  {0x1100, 0xff00, &DSPEmitter::bloopi},

  // Load and store value pointed by indexing reg and increment; LRR/SRR variants
  {0x1800, 0xff80, &DSPEmitter::lrr},
  {0x1880, 0xff80, &DSPEmitter::lrrd},
  {0x1900, 0xff80, &DSPEmitter::lrri},
  {0x1980, 0xff80, &DSPEmitter::lrrn},

  {0x1a00, 0xff80, &DSPEmitter::srr},
  {0x1a80, 0xff80, &DSPEmitter::srrd},
  {0x1b00, 0xff80, &DSPEmitter::srri},
  {0x1b80, 0xff80, &DSPEmitter::srrn},

  // 2
  {0x2000, 0xf800, &DSPEmitter::lrs},
  {0x2800, 0xfe00, &DSPEmitter::srsh},
  {0x2c00, 0xfc00, &DSPEmitter::srs},

  // opcodes that can be extended

  // 3 - main opcode defined by 9 bits, extension defined by last 7 bits!!
  {0x3000, 0xfc80, &DSPEmitter::xorr},
  {0x3400, 0xfc80, &DSPEmitter::andr},
  {0x3800, 0xfc80, &DSPEmitter::orr},
  {0x3c00, 0xfe80, &DSPEmitter::andc},
  {0x3e00, 0xfe80, &DSPEmitter::orc},
  {0x3080, 0xfe80, &DSPEmitter::xorc},
  {0x3280, 0xfe80, &DSPEmitter::notc},
  {0x3480, 0xfc80, &DSPEmitter::lsrnrx},
  {0x3880, 0xfc80, &DSPEmitter::asrnrx},
  {0x3c80, 0xfe80, &DSPEmitter::lsrnr},
  {0x3e80, 0xfe80, &DSPEmitter::asrnr},

  // 4
  {0x4000, 0xf800, &DSPEmitter::addr},
  {0x4800, 0xfc00, &DSPEmitter::addax},
  {0x4c00, 0xfe00, &DSPEmitter::add},
  {0x4e00, 0xfe00, &DSPEmitter::addp},

  // 5
  {0x5000, 0xf800, &DSPEmitter::subr},
  {0x5800, 0xfc00, &DSPEmitter::subax},
  {0x5c00, 0xfe00, &DSPEmitter::sub},
  {0x5e00, 0xfe00, &DSPEmitter::subp},

  // 6
  {0x6000, 0xf800, &DSPEmitter::movr},
  {0x6800, 0xfc00, &DSPEmitter::movax},
  {0x6c00, 0xfe00, &DSPEmitter::mov},
  {0x6e00, 0xfe00, &DSPEmitter::movp},

  // 7
  {0x7000, 0xfc00, &DSPEmitter::addaxl},
  {0x7400, 0xfe00, &DSPEmitter::incm},
  {0x7600, 0xfe00, &DSPEmitter::inc},
  {0x7800, 0xfe00, &DSPEmitter::decm},
  {0x7a00, 0xfe00, &DSPEmitter::dec},
  {0x7c00, 0xfe00, &DSPEmitter::neg},
  {0x7e00, 0xfe00, &DSPEmitter::movnp},

  // 8
  {0x8000, 0xf700, &DSPEmitter::nx},
  {0x8100, 0xf700, &DSPEmitter::clr},
  {0x8200, 0xff00, &DSPEmitter::cmp},
  {0x8300, 0xff00, &DSPEmitter::mulaxh},
  {0x8400, 0xff00, &DSPEmitter::clrp},
  {0x8500, 0xff00, &DSPEmitter::tstprod},
  {0x8600, 0xfe00, &DSPEmitter::tstaxh},
  {0x8a00, 0xff00, &DSPEmitter::srbith},
  {0x8b00, 0xff00, &DSPEmitter::srbith},
  {0x8c00, 0xff00, &DSPEmitter::srbith},
  {0x8d00, 0xff00, &DSPEmitter::srbith},
  {0x8e00, 0xff00, &DSPEmitter::srbith},
  {0x8f00, 0xff00, &DSPEmitter::srbith},

  // 9
  {0x9000, 0xf700, &DSPEmitter::mul},
  {0x9100, 0xf700, &DSPEmitter::asr16},
  {0x9200, 0xf600, &DSPEmitter::mulmvz},
  {0x9400, 0xf600, &DSPEmitter::mulac},
  {0x9600, 0xf600, &DSPEmitter::mulmv},

  // A-B
  {0xa000, 0xe700, &DSPEmitter::mulx},
  {0xa100, 0xf700, &DSPEmitter::abs},
  {0xa200, 0xe600, &DSPEmitter::mulxmvz},
  {0xa400, 0xe600, &DSPEmitter::mulxac},
  {0xa600, 0xe600, &DSPEmitter::mulxmv},
  {0xb100, 0xf700, &DSPEmitter::tst},

  // C-D
  {0xc000, 0xe700, &DSPEmitter::mulc},
  {0xc100, 0xe700, &DSPEmitter::cmpaxh},
  {0xc200, 0xe600, &DSPEmitter::mulcmvz},
  {0xc400, 0xe600, &DSPEmitter::mulcac},
  {0xc600, 0xe600, &DSPEmitter::mulcmv},

  // E
  {0xe000, 0xfc00, &DSPEmitter::maddx},
  {0xe400, 0xfc00, &DSPEmitter::msubx},
  {0xe800, 0xfc00, &DSPEmitter::maddc},
  {0xec00, 0xfc00, &DSPEmitter::msubc},

  // F
  {0xf000, 0xfe00, &DSPEmitter::lsl16},
  {0xf200, 0xfe00, &DSPEmitter::madd},
  {0xf400, 0xfe00, &DSPEmitter::lsr16},
  {0xf600, 0xfe00, &DSPEmitter::msub},
  {0xf800, 0xfc00, &DSPEmitter::addpaxz},
  {0xfc00, 0xfe00, &DSPEmitter::clrl},
  {0xfe00, 0xfe00, &DSPEmitter::movpz},
}};

constexpr std::array<JITOpInfo, 25> s_opcodes_ext
{{
  {0x0000, 0x00fc, &DSPEmitter::nop},

  {0x0004, 0x00fc, &DSPEmitter::dr},
  {0x0008, 0x00fc, &DSPEmitter::ir},
  {0x000c, 0x00fc, &DSPEmitter::nr},
  {0x0010, 0x00f0, &DSPEmitter::mv},

  {0x0020, 0x00e4, &DSPEmitter::s},
  {0x0024, 0x00e4, &DSPEmitter::sn},

  {0x0040, 0x00c4, &DSPEmitter::l},
  {0x0044, 0x00c4, &DSPEmitter::ln},

  {0x0080, 0x00ce, &DSPEmitter::ls},
  {0x0082, 0x00ce, &DSPEmitter::sl},
  {0x0084, 0x00ce, &DSPEmitter::lsn},
  {0x0086, 0x00ce, &DSPEmitter::sln},
  {0x0088, 0x00ce, &DSPEmitter::lsm},
  {0x008a, 0x00ce, &DSPEmitter::slm},
  {0x008c, 0x00ce, &DSPEmitter::lsnm},
  {0x008e, 0x00ce, &DSPEmitter::slnm},

  {0x00c3, 0x00cf, &DSPEmitter::ldax},
  {0x00c7, 0x00cf, &DSPEmitter::ldaxn},
  {0x00cb, 0x00cf, &DSPEmitter::ldaxm},
  {0x00cf, 0x00cf, &DSPEmitter::ldaxnm},

  {0x00c0, 0x00cc, &DSPEmitter::ld},
  {0x00c4, 0x00cc, &DSPEmitter::ldn},
  {0x00c8, 0x00cc, &DSPEmitter::ldm},
  {0x00cc, 0x00cc, &DSPEmitter::ldnm},
}};
// clang-format on

namespace
{
std::array<JITFunction, 65536> s_op_table;
std::array<JITFunction, 256> s_ext_op_table;
bool s_tables_initialized = false;
}  // Anonymous namespace

JITFunction GetOp(UDSPInstruction inst)
{
  return s_op_table[inst];
}

JITFunction GetExtOp(UDSPInstruction inst)
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
    s_ext_op_table[i] = nullptr;

    const auto iter = FindByOpcode(static_cast<UDSPInstruction>(i), s_opcodes_ext);
    if (iter == s_opcodes_ext.cend())
      continue;

    s_ext_op_table[i] = iter->function;
  }

  // op table
  for (size_t i = 0; i < s_op_table.size(); i++)
  {
    s_op_table[i] = nullptr;

    const auto iter = FindByOpcode(static_cast<UDSPInstruction>(i), s_opcodes);
    if (iter == s_opcodes.cend())
      continue;

    s_op_table[i] = iter->function;
  }

  s_tables_initialized = true;
}
}  // namespace DSP::JIT::x64
