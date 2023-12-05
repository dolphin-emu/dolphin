// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/PPCAnalyst.h"

#include <algorithm>
#include <map>
#include <queue>
#include <string>
#include <vector>

#include <fmt/format.h>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HLE/HLE.h"
#include "Core/PowerPC/JitCommon/JitBase.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PPCTables.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/SignatureDB/SignatureDB.h"
#include "Core/System.h"

// Analyzes PowerPC code in memory to find functions
// After running, for each function we will know what functions it calls
// and what functions calls it. That is, we will have an incomplete call graph,
// but only missing indirect branches.

// The results of this analysis is displayed in the code browsing sections at the bottom left
// of the disassembly window (debugger).

// It is also useful for finding function boundaries so that we can find, fingerprint and detect
// library functions.
// We don't do this much currently. Only for the special case Super Monkey Ball.

namespace PPCAnalyst
{
// 0 does not perform block merging
constexpr u32 BRANCH_FOLLOWING_THRESHOLD = 2;

constexpr u32 INVALID_BRANCH_TARGET = 0xFFFFFFFF;

static u32 EvaluateBranchTarget(UGeckoInstruction instr, u32 pc)
{
  switch (instr.OPCD)
  {
  case 16:  // bcx - Branch Conditional instructions
  {
    u32 target = SignExt16(instr.BD << 2);
    if (!instr.AA)
      target += pc;

    return target;
  }
  case 18:  // bx - Branch instructions
  {
    u32 target = SignExt26(instr.LI << 2);
    if (!instr.AA)
      target += pc;

    return target;
  }
  default:
    return INVALID_BRANCH_TARGET;
  }
}

// To find the size of each found function, scan
// forward until we hit blr or rfi. In the meantime, collect information
// about which functions this function calls.
// Also collect which internal branch goes the farthest.
// If any one goes farther than the blr or rfi, assume that there is more than
// one blr or rfi, and keep scanning.
bool AnalyzeFunction(const Core::CPUThreadGuard& guard, u32 startAddr, Common::Symbol& func,
                     u32 max_size)
{
  if (func.name.empty())
    func.Rename(fmt::format("zz_{:08x}_", startAddr));
  if (func.analyzed)
    return true;  // No error, just already did it.

  auto& mmu = guard.GetSystem().GetMMU();

  func.calls.clear();
  func.callers.clear();
  func.size = 0;
  func.flags = Common::FFLAG_LEAF;

  u32 farthestInternalBranchTarget = startAddr;
  int numInternalBranches = 0;
  for (u32 addr = startAddr; true; addr += 4)
  {
    func.size += 4;
    if (func.size >= JitBase::code_buffer_size * 4 ||
        !PowerPC::MMU::HostIsInstructionRAMAddress(guard, addr))
    {
      return false;
    }

    if (max_size && func.size > max_size)
    {
      func.address = startAddr;
      func.analyzed = true;
      func.size -= 4;
      func.hash = HashSignatureDB::ComputeCodeChecksum(guard, startAddr, addr - 4);
      if (numInternalBranches == 0)
        func.flags |= Common::FFLAG_STRAIGHT;
      return true;
    }
    const PowerPC::TryReadInstResult read_result = mmu.TryReadInstruction(addr);
    const UGeckoInstruction instr = read_result.hex;
    if (read_result.valid && PPCTables::IsValidInstruction(instr, addr))
    {
      // BLR or RFI
      // 4e800021 is blrl, not the end of a function
      if (instr.hex == 0x4e800020 || instr.hex == 0x4C000064)
      {
        // Not this one, continue..
        if (farthestInternalBranchTarget > addr)
          continue;

        // A final blr!
        // We're done! Looks like we have a neat valid function. Perfect.
        // Let's calc the checksum and get outta here
        func.address = startAddr;
        func.analyzed = true;
        func.hash = HashSignatureDB::ComputeCodeChecksum(guard, startAddr, addr);
        if (numInternalBranches == 0)
          func.flags |= Common::FFLAG_STRAIGHT;
        return true;
      }
      else if (instr.hex == 0x4e800021 || instr.hex == 0x4e800420 || instr.hex == 0x4e800421)
      {
        func.flags &= ~Common::FFLAG_LEAF;
        func.flags |= Common::FFLAG_EVIL;
      }
      else if (instr.hex == 0x4c000064)
      {
        func.flags &= ~Common::FFLAG_LEAF;
        func.flags |= Common::FFLAG_RFI;
      }
      else
      {
        u32 target = EvaluateBranchTarget(instr, addr);
        if (target == INVALID_BRANCH_TARGET)
          continue;

        const bool is_external = target < startAddr || (max_size && target >= startAddr + max_size);
        if (instr.LK || is_external)
        {
          // Found a function call
          func.calls.emplace_back(target, addr);
          func.flags &= ~Common::FFLAG_LEAF;
        }
        else if (instr.OPCD == 16)
        {
          // Found a conditional branch
          if (target > farthestInternalBranchTarget)
          {
            farthestInternalBranchTarget = target;
          }
          numInternalBranches++;
        }
      }
    }
    else
    {
      return false;
    }
  }
}

bool ReanalyzeFunction(const Core::CPUThreadGuard& guard, u32 start_addr, Common::Symbol& func,
                       u32 max_size)
{
  ASSERT_MSG(SYMBOLS, func.analyzed, "The function wasn't previously analyzed!");

  func.analyzed = false;
  return AnalyzeFunction(guard, start_addr, func, max_size);
}

// Second pass analysis, done after the first pass is done for all functions
// so we have more information to work with
static void AnalyzeFunction2(Common::Symbol* func)
{
  u32 flags = func->flags;

  bool nonleafcall = std::any_of(func->calls.begin(), func->calls.end(), [](const auto& call) {
    const Common::Symbol* called_func = g_symbolDB.GetSymbolFromAddr(call.function);
    return called_func && (called_func->flags & Common::FFLAG_LEAF) == 0;
  });

  if (nonleafcall && !(flags & Common::FFLAG_EVIL) && !(flags & Common::FFLAG_RFI))
    flags |= Common::FFLAG_ONLYCALLSNICELEAFS;

  func->flags = flags;
}

static bool IsMtspr(UGeckoInstruction inst)
{
  return inst.OPCD == 31 && inst.SUBOP10 == 467;
}

static bool IsSprInstructionUsingMmcr(UGeckoInstruction inst)
{
  const u32 index = (inst.SPRU << 5) | (inst.SPRL & 0x1F);
  return index == SPR_MMCR0 || index == SPR_MMCR1;
}

static bool InstructionCanEndBlock(const CodeOp& op)
{
  return (op.opinfo->flags & FL_ENDBLOCK) &&
         (!IsMtspr(op.inst) || IsSprInstructionUsingMmcr(op.inst));
}

bool PPCAnalyzer::CanSwapAdjacentOps(const CodeOp& a, const CodeOp& b) const
{
  const GekkoOPInfo* a_info = a.opinfo;
  const GekkoOPInfo* b_info = b.opinfo;
  u64 a_flags = a_info->flags;
  u64 b_flags = b_info->flags;

  // can't reorder around breakpoints
  if (m_is_debugging_enabled)
  {
    auto& breakpoints = Core::System::GetInstance().GetPowerPC().GetBreakPoints();
    if (breakpoints.IsAddressBreakPoint(a.address) || breakpoints.IsAddressBreakPoint(b.address))
      return false;
  }
  // Any instruction which can raise an interrupt is *not* a possible swap candidate:
  // see [1] for an example of a crash caused by this error.
  //
  // [1] https://bugs.dolphin-emu.org/issues/5864#note-7
  if (a.canCauseException || b.canCauseException)
    return false;
  if (a.canEndBlock || b.canEndBlock)
    return false;
  if (a_flags & (FL_TIMER | FL_NO_REORDER | FL_SET_OE))
    return false;
  if (b_flags & (FL_TIMER | FL_NO_REORDER | FL_SET_OE))
    return false;
  if ((a_flags & (FL_SET_CA | FL_READ_CA)) && (b_flags & (FL_SET_CA | FL_READ_CA)))
    return false;

  // For now, only integer ops are acceptable.
  if (b_info->type != OpType::Integer)
    return false;

  // Check that we have no register collisions.
  // That is, check that none of b's outputs matches any of a's inputs,
  // and that none of a's outputs matches any of b's inputs.

  // register collision: b outputs to one of a's inputs
  if (b.regsOut & a.regsIn)
    return false;
  if (b.crOut & a.crIn)
    return false;
  // register collision: a outputs to one of b's inputs
  if (a.regsOut & b.regsIn)
    return false;
  if (a.crOut & b.crIn)
    return false;
  // register collision: b outputs to one of a's outputs (overwriting it)
  if (b.regsOut & a.regsOut)
    return false;
  if (b.crOut & a.crOut)
    return false;

  return true;
}

// Most functions that are relevant to analyze should be
// called by another function. Therefore, let's scan the
// entire space for bl operations and find what functions
// get called.
static void FindFunctionsFromBranches(const Core::CPUThreadGuard& guard, u32 startAddr, u32 endAddr,
                                      Common::SymbolDB* func_db)
{
  auto& mmu = guard.GetSystem().GetMMU();
  for (u32 addr = startAddr; addr < endAddr; addr += 4)
  {
    const PowerPC::TryReadInstResult read_result = mmu.TryReadInstruction(addr);
    const UGeckoInstruction instr = read_result.hex;

    if (read_result.valid && PPCTables::IsValidInstruction(instr, addr))
    {
      switch (instr.OPCD)
      {
      case 18:  // branch instruction
      {
        if (instr.LK)  // bl
        {
          u32 target = SignExt26(instr.LI << 2);
          if (!instr.AA)
            target += addr;
          if (PowerPC::MMU::HostIsRAMAddress(guard, target))
          {
            func_db->AddFunction(guard, target);
          }
        }
      }
      break;
      default:
        break;
      }
    }
  }
}

static void FindFunctionsFromHandlers(const Core::CPUThreadGuard& guard, PPCSymbolDB* func_db)
{
  static const std::map<u32, const char* const> handlers = {
      {0x80000100, "system_reset_exception_handler"},
      {0x80000200, "machine_check_exception_handler"},
      {0x80000300, "dsi_exception_handler"},
      {0x80000400, "isi_exception_handler"},
      {0x80000500, "external_interrupt_exception_handler"},
      {0x80000600, "alignment_exception_handler"},
      {0x80000700, "program_exception_handler"},
      {0x80000800, "floating_point_unavailable_exception_handler"},
      {0x80000900, "decrementer_exception_handler"},
      {0x80000C00, "system_call_exception_handler"},
      {0x80000D00, "trace_exception_handler"},
      {0x80000E00, "floating_point_assist_exception_handler"},
      {0x80000F00, "performance_monitor_interrupt_handler"},
      {0x80001300, "instruction_address_breakpoint_exception_handler"},
      {0x80001400, "system_management_interrupt_handler"},
      {0x80001700, "thermal_management_interrupt_exception_handler"}};

  auto& mmu = guard.GetSystem().GetMMU();
  for (const auto& entry : handlers)
  {
    const PowerPC::TryReadInstResult read_result = mmu.TryReadInstruction(entry.first);
    if (read_result.valid && PPCTables::IsValidInstruction(read_result.hex, entry.first))
    {
      // Check if this function is already mapped
      Common::Symbol* f = func_db->AddFunction(guard, entry.first);
      if (!f)
        continue;
      f->Rename(entry.second);
    }
  }
}

static void FindFunctionsAfterReturnInstruction(const Core::CPUThreadGuard& guard,
                                                PPCSymbolDB* func_db)
{
  std::vector<u32> funcAddrs;

  for (const auto& func : func_db->Symbols())
    funcAddrs.push_back(func.second.address + func.second.size);

  auto& mmu = guard.GetSystem().GetMMU();
  for (u32& location : funcAddrs)
  {
    while (true)
    {
      // Skip zeroes (e.g. Donkey Kong Country Returns) and nop (e.g. libogc)
      // that sometimes pad function to 16 byte boundary.
      PowerPC::TryReadInstResult read_result = mmu.TryReadInstruction(location);
      while (read_result.valid && (location & 0xf) != 0)
      {
        if (read_result.hex != 0 && read_result.hex != 0x60000000)
          break;
        location += 4;
        read_result = mmu.TryReadInstruction(location);
      }
      if (read_result.valid && PPCTables::IsValidInstruction(read_result.hex, location))
      {
        // check if this function is already mapped
        Common::Symbol* f = func_db->AddFunction(guard, location);
        if (!f)
          break;
        else
          location += f->size;
      }
      else
        break;
    }
  }
}

void FindFunctions(const Core::CPUThreadGuard& guard, u32 startAddr, u32 endAddr,
                   PPCSymbolDB* func_db)
{
  // Step 1: Find all functions
  FindFunctionsFromBranches(guard, startAddr, endAddr, func_db);
  FindFunctionsFromHandlers(guard, func_db);
  FindFunctionsAfterReturnInstruction(guard, func_db);

  // Step 2:
  func_db->FillInCallers();
  func_db->Index();

  int numLeafs = 0, numNice = 0, numUnNice = 0;
  int numTimer = 0, numRFI = 0, numStraightLeaf = 0;
  int leafSize = 0, niceSize = 0, unniceSize = 0;
  for (auto& func : func_db->AccessSymbols())
  {
    if (func.second.address == 4)
    {
      WARN_LOG_FMT(SYMBOLS, "Weird function");
      continue;
    }
    AnalyzeFunction2(&(func.second));
    Common::Symbol& f = func.second;
    if (f.name.substr(0, 3) == "zzz")
    {
      if (f.flags & Common::FFLAG_LEAF)
        f.Rename(f.name + "_leaf");
      if (f.flags & Common::FFLAG_STRAIGHT)
        f.Rename(f.name + "_straight");
    }
    if (f.flags & Common::FFLAG_LEAF)
    {
      numLeafs++;
      leafSize += f.size;
    }
    else if (f.flags & Common::FFLAG_ONLYCALLSNICELEAFS)
    {
      numNice++;
      niceSize += f.size;
    }
    else
    {
      numUnNice++;
      unniceSize += f.size;
    }

    if (f.flags & Common::FFLAG_TIMERINSTRUCTIONS)
      numTimer++;
    if (f.flags & Common::FFLAG_RFI)
      numRFI++;
    if ((f.flags & Common::FFLAG_STRAIGHT) && (f.flags & Common::FFLAG_LEAF))
      numStraightLeaf++;
  }
  if (numLeafs == 0)
    leafSize = 0;
  else
    leafSize /= numLeafs;

  if (numNice == 0)
    niceSize = 0;
  else
    niceSize /= numNice;

  if (numUnNice == 0)
    unniceSize = 0;
  else
    unniceSize /= numUnNice;

  INFO_LOG_FMT(SYMBOLS,
               "Functions analyzed. {} leafs, {} nice, {} unnice."
               "{} timer, {} rfi. {} are branchless leafs.",
               numLeafs, numNice, numUnNice, numTimer, numRFI, numStraightLeaf);
  INFO_LOG_FMT(SYMBOLS, "Average size: {} (leaf), {} (nice), {}(unnice)", leafSize, niceSize,
               unniceSize);
}

static bool isCarryOp(const CodeOp& a)
{
  return (a.opinfo->flags & FL_SET_CA) && !(a.opinfo->flags & FL_SET_OE) &&
         a.opinfo->type == OpType::Integer;
}

static bool isCror(const CodeOp& a)
{
  return a.inst.OPCD == 19 && a.inst.SUBOP10 == 449;
}

void PPCAnalyzer::ReorderInstructionsCore(u32 instructions, CodeOp* code, bool reverse,
                                          ReorderType type) const
{
  // Instruction Reordering Pass
  // Carry pass: bubble carry-using instructions as close to each other as possible, so we can avoid
  // storing the carry flag.
  // Compare pass: bubble compare instructions next to branches, so they can be merged.

  const int start = reverse ? instructions - 1 : 0;
  const int end = reverse ? 0 : instructions - 1;
  const int increment = reverse ? -1 : 1;

  int i = start;
  int next = start;
  bool go_backwards = false;

  while (true)
  {
    if (go_backwards)
    {
      i -= increment;
      go_backwards = false;
    }
    else
    {
      i = next;
      next += increment;
    }

    if (i == end)
      break;

    CodeOp& a = code[i];
    CodeOp& b = code[i + increment];

    // Reorder integer compares, rlwinm., and carry-affecting ops
    // (if we add more merged branch instructions, add them here!)
    if ((type == ReorderType::CROR && isCror(a)) || (type == ReorderType::Carry && isCarryOp(a)) ||
        (type == ReorderType::CMP && (type == ReorderType::CMP && a.crOut)))
    {
      // once we're next to a carry instruction, don't move away!
      if (type == ReorderType::Carry && i != start)
      {
        // if we read the CA flag, and the previous instruction sets it, don't move away.
        if (!reverse && (a.opinfo->flags & FL_READ_CA) &&
            (code[i - increment].opinfo->flags & FL_SET_CA))
        {
          continue;
        }

        // if we set the CA flag, and the next instruction reads it, don't move away.
        if (reverse && (a.opinfo->flags & FL_SET_CA) &&
            (code[i - increment].opinfo->flags & FL_READ_CA))
        {
          continue;
        }
      }

      if (CanSwapAdjacentOps(a, b))
      {
        // Alright, let's bubble it!
        std::swap(a, b);

        if (i != start)
        {
          // Bubbling an instruction sometimes reveals another opportunity to bubble an instruction,
          // so go one step backwards and check if we have such an opportunity.
          go_backwards = true;
        }
      }
    }
  }
}

void PPCAnalyzer::ReorderInstructions(u32 instructions, CodeOp* code) const
{
  // For carry, bubble instructions *towards* each other; one direction often isn't enough
  // to get pairs like addc/adde next to each other.
  if (HasOption(OPTION_CARRY_MERGE))
  {
    ReorderInstructionsCore(instructions, code, false, ReorderType::Carry);
    ReorderInstructionsCore(instructions, code, true, ReorderType::Carry);
  }

  // Reorder instructions which write to CR (typically compare instructions) towards branches.
  if (HasOption(OPTION_BRANCH_MERGE))
    ReorderInstructionsCore(instructions, code, false, ReorderType::CMP);

  // Reorder cror instructions upwards (e.g. towards an fcmp). Technically we should be more
  // picky about this, but cror seems to almost solely be used for this purpose in real code.
  // Additionally, the other boolean ops seem to almost never be used.
  if (HasOption(OPTION_CROR_MERGE))
    ReorderInstructionsCore(instructions, code, true, ReorderType::CROR);
}

void PPCAnalyzer::SetInstructionStats(CodeBlock* block, CodeOp* code,
                                      const GekkoOPInfo* opinfo) const
{
  bool first_fpu_instruction = false;
  if (opinfo->flags & FL_USE_FPU)
  {
    first_fpu_instruction = !block->m_fpa->any;
    block->m_fpa->any = true;
  }

  code->crIn = BitSet8(0);
  if (opinfo->flags & FL_READ_ALL_CR)
  {
    code->crIn = BitSet8(0xFF);
  }
  else if (opinfo->flags & FL_READ_CRn)
  {
    code->crIn[code->inst.CRFS] = true;
  }
  else if (opinfo->flags & FL_READ_CR_BI)
  {
    code->crIn[code->inst.BI] = true;
  }
  else if (opinfo->type == OpType::CR)
  {
    code->crIn[code->inst.CRBA >> 2] = true;
    code->crIn[code->inst.CRBB >> 2] = true;

    // CR instructions only write to one bit of the destination CR,
    // so treat the other three bits of the destination as inputs
    code->crIn[code->inst.CRBD >> 2] = true;
  }

  code->crOut = BitSet8(0);
  if (opinfo->flags & FL_SET_ALL_CR)
    code->crOut = BitSet8(0xFF);
  else if (opinfo->flags & FL_SET_CRn)
    code->crOut[code->inst.CRFD] = true;
  else if ((opinfo->flags & FL_SET_CR0) || ((opinfo->flags & FL_RC_BIT) && code->inst.Rc))
    code->crOut[0] = true;
  else if ((opinfo->flags & FL_SET_CR1) || ((opinfo->flags & FL_RC_BIT_F) && code->inst.Rc))
    code->crOut[1] = true;
  else if (opinfo->type == OpType::CR)
    code->crOut[code->inst.CRBD >> 2] = true;

  code->wantsFPRF = (opinfo->flags & FL_READ_FPRF) != 0;
  code->outputFPRF = (opinfo->flags & FL_SET_FPRF) != 0;
  code->canEndBlock = InstructionCanEndBlock(*code);

  code->canCauseException = first_fpu_instruction ||
                            (opinfo->flags & (FL_LOADSTORE | FL_PROGRAMEXCEPTION)) != 0 ||
                            (m_enable_float_exceptions && (opinfo->flags & FL_FLOAT_EXCEPTION)) ||
                            (m_enable_div_by_zero_exceptions && (opinfo->flags & FL_FLOAT_DIV));

  code->wantsCA = (opinfo->flags & FL_READ_CA) != 0;
  code->outputCA = (opinfo->flags & FL_SET_CA) != 0;

  // We're going to try to avoid storing carry in XER if we can avoid it -- keep it in the x86 carry
  // flag!
  // If the instruction reads CA but doesn't write it, we still need to store CA in XER; we can't
  // leave it in flags.
  if (HasOption(OPTION_CARRY_MERGE))
    code->wantsCAInFlags = code->wantsCA && code->outputCA && opinfo->type == OpType::Integer;
  else
    code->wantsCAInFlags = false;

  // mfspr/mtspr can affect/use XER, so be super careful here
  // we need to note specifically that mfspr needs CA in XER, not in the x86 carry flag
  if (code->inst.OPCD == 31 && code->inst.SUBOP10 == 339)  // mfspr
    code->wantsCA = ((code->inst.SPRU << 5) | (code->inst.SPRL & 0x1F)) == SPR_XER;
  if (code->inst.OPCD == 31 && code->inst.SUBOP10 == 467)  // mtspr
    code->outputCA = ((code->inst.SPRU << 5) | (code->inst.SPRL & 0x1F)) == SPR_XER;

  code->regsIn = BitSet32(0);
  code->regsOut = BitSet32(0);
  if (opinfo->flags & FL_OUT_A)
  {
    code->regsOut[code->inst.RA] = true;
  }
  if (opinfo->flags & FL_OUT_D)
  {
    code->regsOut[code->inst.RD] = true;
  }
  if ((opinfo->flags & FL_IN_A) || ((opinfo->flags & FL_IN_A0) && code->inst.RA != 0))
  {
    code->regsIn[code->inst.RA] = true;
  }
  if (opinfo->flags & FL_IN_B)
  {
    code->regsIn[code->inst.RB] = true;
  }
  if (opinfo->flags & FL_IN_C)
  {
    code->regsIn[code->inst.RC] = true;
  }
  if (opinfo->flags & FL_IN_S)
  {
    code->regsIn[code->inst.RS] = true;
  }
  if (code->inst.OPCD == 46)  // lmw
  {
    for (int iReg = code->inst.RD; iReg < 32; ++iReg)
    {
      code->regsOut[iReg] = true;
    }
  }
  else if (code->inst.OPCD == 47)  // stmw
  {
    for (int iReg = code->inst.RS; iReg < 32; ++iReg)
    {
      code->regsIn[iReg] = true;
    }
  }

  code->fregOut = -1;
  if (opinfo->flags & FL_OUT_FLOAT_D)
    code->fregOut = code->inst.FD;

  code->fregsIn = BitSet32(0);
  if (opinfo->flags & FL_IN_FLOAT_A)
    code->fregsIn[code->inst.FA] = true;
  if (opinfo->flags & FL_IN_FLOAT_B)
    code->fregsIn[code->inst.FB] = true;
  if (opinfo->flags & FL_IN_FLOAT_C)
    code->fregsIn[code->inst.FC] = true;
  if (opinfo->flags & FL_IN_FLOAT_D)
    code->fregsIn[code->inst.FD] = true;
  if (opinfo->flags & FL_IN_FLOAT_S)
    code->fregsIn[code->inst.FS] = true;

  code->branchUsesCtr = false;
  code->branchTo = UINT32_MAX;

  // For branch with immediate addresses (bx/bcx), compute the destination.
  if (code->inst.OPCD == 18)  // bx
  {
    if (code->inst.AA)  // absolute
      code->branchTo = SignExt26(code->inst.LI << 2);
    else
      code->branchTo = code->address + SignExt26(code->inst.LI << 2);
  }
  else if (code->inst.OPCD == 16)  // bcx
  {
    if (code->inst.AA)  // absolute
      code->branchTo = SignExt16(code->inst.BD << 2);
    else
      code->branchTo = code->address + SignExt16(code->inst.BD << 2);
    if (!(code->inst.BO & BO_DONT_DECREMENT_FLAG))
      code->branchUsesCtr = true;
  }
  else if (code->inst.OPCD == 19 && code->inst.SUBOP10 == 16)  // bclrx
  {
    if (!(code->inst.BO & BO_DONT_DECREMENT_FLAG))
      code->branchUsesCtr = true;
  }
  else if (code->inst.OPCD == 19 && code->inst.SUBOP10 == 528)  // bcctrx
  {
    if (!(code->inst.BO & BO_DONT_DECREMENT_FLAG))
      code->branchUsesCtr = true;
  }
}

bool PPCAnalyzer::IsBusyWaitLoop(CodeBlock* block, CodeOp* code, size_t instructions) const
{
  // Very basic algorithm to detect busy wait loops:
  //   * It loops to itself and does not contain any other branches.
  //   * It does not write to memory.
  //   * It only reads from registers it wrote to earlier in the loop, or it
  //     does not write to these registers.
  //
  // Would benefit a lot from basic inlining support - a lot of the most
  // used busy loops are DSP register interactions, which are bl/cmp/bne
  // (with the bl target a pure function that follows the above rules). We
  // don't detect these at the moment.
  std::bitset<32> write_disallowed_regs;
  std::bitset<32> written_regs;
  for (size_t i = 0; i <= instructions; ++i)
  {
    if (code[i].opinfo->type == OpType::Branch)
    {
      if (code[i].branchUsesCtr)
        return false;
      if (code[i].branchTo == block->m_address && i == instructions)
        return true;
    }
    else if (code[i].opinfo->type != OpType::Integer && code[i].opinfo->type != OpType::Load)
    {
      // In the future, some subsets of other instruction types might get
      // supported. Right now, only try loops that have this very
      // restricted instruction set.
      return false;
    }
    else
    {
      for (int reg : code[i].regsIn)
      {
        if (reg == -1)
          continue;
        if (written_regs[reg])
          continue;
        write_disallowed_regs[reg] = true;
      }
      for (int reg : code[i].regsOut)
      {
        if (reg == -1)
          continue;
        if (write_disallowed_regs[reg])
          return false;
        written_regs[reg] = true;
      }
    }
  }
  return false;
}

static bool CanCauseGatherPipeInterruptCheck(const CodeOp& op)
{
  // eieio
  if (op.inst.OPCD == 31 && op.inst.SUBOP10 == 854)
    return true;

  return op.opinfo->type == OpType::Store || op.opinfo->type == OpType::StoreFP ||
         op.opinfo->type == OpType::StorePS;
}

u32 PPCAnalyzer::Analyze(u32 address, CodeBlock* block, CodeBuffer* buffer,
                         std::size_t block_size) const
{
  // Clear block stats
  *block->m_stats = {};

  // Clear register stats
  block->m_gpa->any = true;
  block->m_fpa->any = false;

  // Set the blocks start address
  block->m_address = address;

  // Reset our block state
  block->m_broken = false;
  block->m_memory_exception = false;
  block->m_num_instructions = 0;
  block->m_gqr_used = BitSet8(0);
  block->m_physical_addresses.clear();

  CodeOp* const code = buffer->data();

  bool found_exit = false;
  bool found_call = false;
  size_t caller = 0;
  u32 numFollows = 0;
  u32 num_inst = 0;

  const bool enable_follow = m_enable_branch_following;

  auto& mmu = Core::System::GetInstance().GetMMU();
  for (std::size_t i = 0; i < block_size; ++i)
  {
    auto result = mmu.TryReadInstruction(address);
    if (!result.valid)
    {
      if (i == 0)
        block->m_memory_exception = true;
      break;
    }

    num_inst++;

    const UGeckoInstruction inst = result.hex;
    const GekkoOPInfo* opinfo = PPCTables::GetOpInfo(inst, address);
    code[i] = {};
    code[i].opinfo = opinfo;
    code[i].address = address;
    code[i].inst = inst;
    code[i].skip = false;
    block->m_stats->numCycles += opinfo->num_cycles;
    block->m_physical_addresses.insert(result.physical_address);

    SetInstructionStats(block, &code[i], opinfo);

    bool follow = false;

    bool conditional_continue = false;

    // TODO: Find the optimal value for BRANCH_FOLLOWING_THRESHOLD.
    //       If it is small, the performance will be down.
    //       If it is big, the size of generated code will be big and
    //       cache clearning will happen many times.
    if (enable_follow && HasOption(OPTION_BRANCH_FOLLOW))
    {
      if (inst.OPCD == 18 && block_size > 1)
      {
        // Always follow BX instructions.
        follow = true;
        if (inst.LK)
        {
          found_call = true;
          caller = i;
        }
      }
      else if (inst.OPCD == 16 && (inst.BO & BO_DONT_DECREMENT_FLAG) &&
               (inst.BO & BO_DONT_CHECK_CONDITION) && block_size > 1)
      {
        // Always follow unconditional BCX instructions, but they are very rare.
        follow = true;
        if (inst.LK)
        {
          found_call = true;
          caller = i;
        }
      }
      else if (inst.OPCD == 19 && inst.SUBOP10 == 16 && !inst.LK && found_call)
      {
        code[i].branchTo = code[caller].address + 4;
        if ((inst.BO & BO_DONT_DECREMENT_FLAG) && (inst.BO & BO_DONT_CHECK_CONDITION) &&
            numFollows < BRANCH_FOLLOWING_THRESHOLD)
        {
          // bclrx with unconditional branch = return
          // Follow it if we can propagate the LR value of the last CALL instruction.
          // Through it would be easy to track the upper level of call/return,
          // we can't guarantee the LR value. The PPC ABI forces all functions to push
          // the LR value on the stack as there are no spare registers. So we'd need
          // to check all store instruction to not alias with the stack.
          follow = true;
          found_call = false;
          code[i].skip = true;

          // Skip the RET, so also don't generate the stack entry for the BLR optimization.
          code[caller].skipLRStack = true;
        }
      }
      else if (inst.OPCD == 31 && inst.SUBOP10 == 467)
      {
        // mtspr, skip CALL/RET merging as LR is overwritten.
        const u32 index = (inst.SPRU << 5) | (inst.SPRL & 0x1F);
        if (index == SPR_LR)
        {
          // We give up to follow the return address
          // because we have to check the register usage.
          found_call = false;
        }
      }
    }

    if (HasOption(OPTION_CONDITIONAL_CONTINUE))
    {
      if (inst.OPCD == 16 &&
          ((inst.BO & BO_DONT_DECREMENT_FLAG) == 0 || (inst.BO & BO_DONT_CHECK_CONDITION) == 0))
      {
        // bcx with conditional branch
        conditional_continue = true;
      }
      else if (inst.OPCD == 19 && inst.SUBOP10 == 16 &&
               ((inst.BO & BO_DONT_DECREMENT_FLAG) == 0 ||
                (inst.BO & BO_DONT_CHECK_CONDITION) == 0))
      {
        // bclrx with conditional branch
        conditional_continue = true;
      }
      else if (inst.OPCD == 3 || (inst.OPCD == 31 && inst.SUBOP10 == 4))
      {
        // tw/twi tests and raises an exception
        conditional_continue = true;
      }
      else if (inst.OPCD == 19 && inst.SUBOP10 == 528 && (inst.BO_2 & BO_DONT_CHECK_CONDITION) == 0)
      {
        // Rare bcctrx with conditional branch
        // Seen in NES games
        conditional_continue = true;
      }
    }

    code[i].branchIsIdleLoop =
        code[i].branchTo == block->m_address && IsBusyWaitLoop(block, code, i);

    if (follow && numFollows < BRANCH_FOLLOWING_THRESHOLD)
    {
      // Follow the unconditional branch.
      numFollows++;
      address = code[i].branchTo;
    }
    else
    {
      // Just pick the next instruction
      address += 4;
      if (!conditional_continue && InstructionCanEndBlock(code[i]))  // right now we stop early
      {
        found_exit = true;
        break;
      }
      if (conditional_continue)
      {
        // If we skip any conditional branch, we can't garantee to get the matching CALL/RET pair.
        // So we stop inling the RET here and let the BLR optitmization handle this case.
        found_call = false;
      }
    }
  }

  block->m_num_instructions = num_inst;

  if (block->m_num_instructions > 1)
    ReorderInstructions(block->m_num_instructions, code);

  if ((!found_exit && num_inst > 0) || block_size == 1)
  {
    // We couldn't find an exit
    block->m_broken = true;
  }

  // Scan for flag dependencies; assume the next block (or any branch that can leave the block)
  // wants flags, to be safe.
  bool wantsFPRF = true;
  bool wantsCA = true;
  BitSet8 crInUse, crDiscardable;
  BitSet32 gprBlockInputs, gprInUse, fprInUse, gprDiscardable, fprDiscardable, fprInXmm;
  for (int i = block->m_num_instructions - 1; i >= 0; i--)
  {
    CodeOp& op = code[i];

    if (CanCauseGatherPipeInterruptCheck(op))
    {
      // If we're doing a gather pipe interrupt check after this instruction, we need to
      // be able to flush all registers, so we can't have any discarded registers.
      gprDiscardable = BitSet32{};
      fprDiscardable = BitSet32{};
      crDiscardable = BitSet8{};
    }

    const bool hle = !!HLE::TryReplaceFunction(op.address);
    const bool may_exit_block = hle || op.canEndBlock || op.canCauseException;

    const bool opWantsFPRF = op.wantsFPRF;
    const bool opWantsCA = op.wantsCA;
    op.wantsFPRF = wantsFPRF || may_exit_block;
    op.wantsCA = wantsCA || may_exit_block;
    wantsFPRF |= opWantsFPRF || may_exit_block;
    wantsCA |= opWantsCA || may_exit_block;
    wantsFPRF &= !op.outputFPRF || opWantsFPRF;
    wantsCA &= !op.outputCA || opWantsCA;
    op.gprInUse = gprInUse;
    op.fprInUse = fprInUse;
    op.crInUse = crInUse;
    op.gprDiscardable = gprDiscardable;
    op.fprDiscardable = fprDiscardable;
    op.crDiscardable = crDiscardable;
    op.fprInXmm = fprInXmm;
    gprBlockInputs &= ~op.regsOut;
    gprBlockInputs |= op.regsIn;
    gprInUse |= op.regsIn | op.regsOut;
    fprInUse |= op.fregsIn | op.GetFregsOut();
    crInUse |= op.crIn | op.crOut;

    if (strncmp(op.opinfo->opname, "stfd", 4))
      fprInXmm |= op.fregsIn;

    if (hle)
    {
      gprInUse = BitSet32{};
      fprInUse = BitSet32{};
      fprInXmm = BitSet32{};
      crInUse = BitSet8{};
      gprDiscardable = BitSet32{};
      fprDiscardable = BitSet32{};
      crDiscardable = BitSet8{};
    }
    else if (op.canEndBlock || op.canCauseException)
    {
      gprDiscardable = BitSet32{};
      fprDiscardable = BitSet32{};
      crDiscardable = BitSet8{};
    }
    else
    {
      gprDiscardable |= op.regsOut;
      gprDiscardable &= ~op.regsIn;
      fprDiscardable |= op.GetFregsOut();
      fprDiscardable &= ~op.fregsIn;
      crDiscardable |= op.crOut;
      crDiscardable &= ~op.crIn;
    }
  }

  // Forward scan, for flags that need the other direction for calculation.
  BitSet32 fprIsSingle, fprIsDuplicated, fprIsStoreSafe;
  BitSet8 gqrUsed, gqrModified;
  for (u32 i = 0; i < block->m_num_instructions; i++)
  {
    CodeOp& op = code[i];

    op.fprIsSingle = fprIsSingle;
    op.fprIsDuplicated = fprIsDuplicated;
    op.fprIsStoreSafeBeforeInst = fprIsStoreSafe;
    if (op.fregOut >= 0)
    {
      BitSet32 bitexact_inputs;
      if (op.opinfo->flags &
          (FL_IN_FLOAT_A_BITEXACT | FL_IN_FLOAT_B_BITEXACT | FL_IN_FLOAT_C_BITEXACT))
      {
        if (op.opinfo->flags & FL_IN_FLOAT_A_BITEXACT)
          bitexact_inputs[op.inst.FA] = true;
        if (op.opinfo->flags & FL_IN_FLOAT_B_BITEXACT)
          bitexact_inputs[op.inst.FB] = true;
        if (op.opinfo->flags & FL_IN_FLOAT_C_BITEXACT)
          bitexact_inputs[op.inst.FC] = true;
      }

      if (op.opinfo->type == OpType::SingleFP || !strncmp(op.opinfo->opname, "frsp", 4))
      {
        fprIsSingle[op.fregOut] = true;
        fprIsDuplicated[op.fregOut] = true;
      }
      else if (!strncmp(op.opinfo->opname, "lfs", 3))
      {
        fprIsSingle[op.fregOut] = true;
        fprIsDuplicated[op.fregOut] = true;
      }
      else if (bitexact_inputs)
      {
        fprIsSingle[op.fregOut] = (fprIsSingle & bitexact_inputs) == bitexact_inputs;
        fprIsDuplicated[op.fregOut] = false;
      }
      else if (op.opinfo->type == OpType::PS || op.opinfo->type == OpType::LoadPS)
      {
        fprIsSingle[op.fregOut] = true;
        fprIsDuplicated[op.fregOut] = false;
      }
      else
      {
        fprIsSingle[op.fregOut] = false;
        fprIsDuplicated[op.fregOut] = false;
      }

      if (!strncmp(op.opinfo->opname, "mtfs", 4))
      {
        // Careful: changing the float mode in a block breaks the store-safe optimization,
        // since a previous float op might have had FTZ off while the later store has FTZ on.
        // So, discard all information we have.
        fprIsStoreSafe = BitSet32(0);
      }
      else if (bitexact_inputs)
      {
        // If the instruction copies bits between registers (without flushing denormals to zero
        // or turning SNaN into QNaN), the output is store-safe if the inputs are.
        fprIsStoreSafe[op.fregOut] = (fprIsStoreSafe & bitexact_inputs) == bitexact_inputs;
      }
      else
      {
        // Other FPU instructions are store-safe if they perform a single-precision
        // arithmetic operation.

        // TODO: if we go directly from a load to store, skip conversion entirely?
        // TODO: if we go directly from a load to a float instruction, and the value isn't used
        // for anything else, we can use fast single -> double conversion after the load.

        fprIsStoreSafe[op.fregOut] = op.opinfo->type == OpType::SingleFP ||
                                     op.opinfo->type == OpType::PS ||
                                     !strncmp(op.opinfo->opname, "frsp", 4);
      }
    }
    op.fprIsStoreSafeAfterInst = fprIsStoreSafe;

    if (op.opinfo->type == OpType::StorePS || op.opinfo->type == OpType::LoadPS)
    {
      const int gqr = op.inst.OPCD == 4 ? op.inst.Ix : op.inst.I;
      gqrUsed[gqr] = true;
    }

    if (op.inst.OPCD == 31 && op.inst.SUBOP10 == 467)  // mtspr
    {
      const int gqr = ((op.inst.SPRU << 5) | op.inst.SPRL) - SPR_GQR0;
      if (gqr >= 0 && gqr <= 7)
        gqrModified[gqr] = true;
    }
  }
  block->m_gqr_used = gqrUsed;
  block->m_gqr_modified = gqrModified;
  block->m_gpr_inputs = gprBlockInputs;
  return address;
}

}  // namespace PPCAnalyst
