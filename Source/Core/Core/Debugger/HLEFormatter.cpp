// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "HLEFormatter.h"

#include <fmt/args.h>
#include <fmt/format.h>

#include "Common/Logging/Log.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/PowerPC/PowerPC.h"

template <>
struct fmt::formatter<PowerPC::PairedSingle>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

  template <typename FormatContext>
  auto format(const PowerPC::PairedSingle& ps, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(), "{:016x} {:016x}", ps.ps0, ps.ps1);
  }
};

namespace Core::Debug
{
namespace
{
using HLENamedArgs = fmt::dynamic_format_arg_store<fmt::format_context>;

HLENamedArgs GetHLENamedArgs()
{
  auto args = HLENamedArgs();
  std::string name;

  for (int i = 0; i < 32; i++)
  {
    // General purpose registers (int)
    name = fmt::format("r{}", i);
    args.push_back(fmt::arg(name.c_str(), GPR(i)));

    // Floating point registers (double)
    name = fmt::format("f{}", i);
    args.push_back(fmt::arg(name.c_str(), rPS(i)));
    name = fmt::format("f{}_1", i);
    args.push_back(fmt::arg(name.c_str(), rPS(i).PS0AsDouble()));
    name = fmt::format("f{}_2", i);
    args.push_back(fmt::arg(name.c_str(), rPS(i).PS1AsDouble()));
  }

  // Special registers
  args.push_back(fmt::arg("TB", PowerPC::ReadFullTimeBaseValue()));
  args.push_back(fmt::arg("PC", PowerPC::ppcState.pc));
  args.push_back(fmt::arg("LR", PowerPC::ppcState.spr[SPR_LR]));
  args.push_back(fmt::arg("CTR", PowerPC::ppcState.spr[SPR_CTR]));
  args.push_back(fmt::arg("CR", PowerPC::ppcState.cr.Get()));
  args.push_back(fmt::arg("XER", PowerPC::GetXER().Hex));
  args.push_back(fmt::arg("FPSCR", PowerPC::ppcState.fpscr.Hex));
  args.push_back(fmt::arg("MSR", PowerPC::ppcState.msr.Hex));
  args.push_back(fmt::arg("SRR0", PowerPC::ppcState.spr[SPR_SRR0]));
  args.push_back(fmt::arg("SRR1", PowerPC::ppcState.spr[SPR_SRR1]));
  args.push_back(fmt::arg("Exceptions", PowerPC::ppcState.Exceptions));
  args.push_back(fmt::arg("IntMask", ProcessorInterface::GetMask()));
  args.push_back(fmt::arg("IntCause", ProcessorInterface::GetCause()));
  args.push_back(fmt::arg("DSISR", PowerPC::ppcState.spr[SPR_DSISR]));
  args.push_back(fmt::arg("DAR", PowerPC::ppcState.spr[SPR_DAR]));
  args.push_back(fmt::arg("Hash Mask", (PowerPC::ppcState.pagetable_hashmask << 6) |
                                           PowerPC::ppcState.pagetable_base));

  return args;
}
}  // namespace

std::string HLEFormatString(const std::string_view message)
{
  const HLENamedArgs args = GetHLENamedArgs();
  return fmt::vformat(message, args);
}
}  // namespace Core::Debug
