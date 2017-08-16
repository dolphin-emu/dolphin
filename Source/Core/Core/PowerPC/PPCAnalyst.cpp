// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/PPCAnalyst.h"

#include <algorithm>
#include <map>
#include <queue>
#include <string>
#include <vector>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Core/ConfigManager.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PPCTables.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/SignatureDB/SignatureDB.h"

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
constexpr int CODEBUFFER_SIZE = 32000;

// 0 does not perform block merging
constexpr u32 BRANCH_FOLLOWING_THRESHOLD = 2;

constexpr u32 INVALID_BRANCH_TARGET = 0xFFFFFFFF;

CodeBuffer::CodeBuffer(int size)
{
  codebuffer = new PPCAnalyst::CodeOp[size];
  size_ = size;
}

CodeBuffer::~CodeBuffer()
{
  delete[] codebuffer;
}

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
bool AnalyzeFunction(u32 startAddr, Symbol& func, int max_size)
{
  if (func.name.empty())
    func.Rename(StringFromFormat("zz_%07x_", startAddr & 0x0FFFFFFF));
  if (func.analyzed)
    return true;  // No error, just already did it.

  func.calls.clear();
  func.callers.clear();
  func.size = 0;
  func.flags = FFLAG_LEAF;

  u32 farthestInternalBranchTarget = startAddr;
  int numInternalBranches = 0;
  for (u32 addr = startAddr; true; addr += 4)
  {
    func.size += 4;
    if (func.size >= CODEBUFFER_SIZE * 4 || !PowerPC::HostIsInstructionRAMAddress(addr))  // weird
      return false;

    if (max_size && func.size > max_size)
    {
      func.address = startAddr;
      func.analyzed = true;
      func.size -= 4;
      func.hash = HashSignatureDB::ComputeCodeChecksum(startAddr, addr - 4);
      if (numInternalBranches == 0)
        func.flags |= FFLAG_STRAIGHT;
      return true;
    }
    const PowerPC::TryReadInstResult read_result = PowerPC::TryReadInstruction(addr);
    const UGeckoInstruction instr = read_result.hex;
    if (read_result.valid && PPCTables::IsValidInstruction(instr))
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
        func.hash = HashSignatureDB::ComputeCodeChecksum(startAddr, addr);
        if (numInternalBranches == 0)
          func.flags |= FFLAG_STRAIGHT;
        return true;
      }
      /*
      else if ((instr.hex & 0xFC000000) == (0x4b000000 & 0xFC000000) && !instr.LK)
      {
        u32 target = addr + SignExt26(instr.LI << 2);
        if (target < startAddr || (max_size && target > max_size+startAddr))
        {
          //block ends by branching away. We're done!
          func.size *= 4; // into bytes
          func.address = startAddr;
          func.analyzed = 1;
          func.hash = HashSignatureDB::ComputeCodeChecksum(startAddr, addr);
          if (numInternalBranches == 0)
            func.flags |= FFLAG_STRAIGHT;
          return true;
        }
      }*/
      else if (instr.hex == 0x4e800021 || instr.hex == 0x4e800420 || instr.hex == 0x4e800421)
      {
        func.flags &= ~FFLAG_LEAF;
        func.flags |= FFLAG_EVIL;
      }
      else if (instr.hex == 0x4c000064)
      {
        func.flags &= ~FFLAG_LEAF;
        func.flags |= FFLAG_RFI;
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
          func.flags &= ~FFLAG_LEAF;
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

bool ReanalyzeFunction(u32 start_addr, Symbol& func, int max_size)
{
  _assert_msg_(OSHLE, func.analyzed, "The function wasn't previously analyzed!");

  func.analyzed = false;
  return AnalyzeFunction(start_addr, func, max_size);
}

// Second pass analysis, done after the first pass is done for all functions
// so we have more information to work with
static void AnalyzeFunction2(Symbol* func)
{
  u32 flags = func->flags;

  bool nonleafcall = std::any_of(func->calls.begin(), func->calls.end(), [](const auto& call) {
    const Symbol* called_func = g_symbolDB.GetSymbolFromAddr(call.function);
    return called_func && (called_func->flags & FFLAG_LEAF) == 0;
  });

  if (nonleafcall && !(flags & FFLAG_EVIL) && !(flags & FFLAG_RFI))
    flags |= FFLAG_ONLYCALLSNICELEAFS;

  func->flags = flags;
}

static bool CanSwapAdjacentOps(const CodeOp& a, const CodeOp& b)
{
  const GekkoOPInfo* a_info = a.opinfo;
  const GekkoOPInfo* b_info = b.opinfo;
  int a_flags = a_info->flags;
  int b_flags = b_info->flags;

  // can't reorder around breakpoints
  if (SConfig::GetInstance().bEnableDebugging &&
      (PowerPC::breakpoints.IsAddressBreakPoint(a.address) ||
       PowerPC::breakpoints.IsAddressBreakPoint(b.address)))
    return false;
  if (b_flags & (FL_SET_CRx | FL_ENDBLOCK | FL_TIMER | FL_EVIL | FL_SET_OE))
    return false;
  if ((b_flags & (FL_RC_BIT | FL_RC_BIT_F)) && (b.inst.Rc))
    return false;
  if ((a_flags & (FL_SET_CA | FL_READ_CA)) && (b_flags & (FL_SET_CA | FL_READ_CA)))
    return false;

  switch (b.inst.OPCD)
  {
  case 16:
  case 18:
  // branches. Do not swap.
  case 17:  // sc
  case 46:  // lmw
  case 19:  // table19 - lots of tricky stuff
    return false;
  }

  // For now, only integer ops acceptable. Any instruction which can raise an
  // interrupt is *not* a possible swap candidate: see [1] for an example of
  // a crash caused by this error.
  //
  // [1] https://bugs.dolphin-emu.org/issues/5864#note-7
  if (b_info->type != OPTYPE_INTEGER)
    return false;

  // And it's possible a might raise an interrupt too (fcmpo/fcmpu)
  if (a_info->type != OPTYPE_INTEGER)
    return false;

  // Check that we have no register collisions.
  // That is, check that none of b's outputs matches any of a's inputs,
  // and that none of a's outputs matches any of b's inputs.
  // The latter does not apply if a is a cmp, of course, but doesn't hurt to check.
  // register collision: b outputs to one of a's inputs
  if (b.regsOut & a.regsIn)
    return false;
  // register collision: a outputs to one of b's inputs
  if (a.regsOut & b.regsIn)
    return false;
  // register collision: b outputs to one of a's outputs (overwriting it)
  if (b.regsOut & a.regsOut)
    return false;

  return true;
}

// Most functions that are relevant to analyze should be
// called by another function. Therefore, let's scan the
// entire space for bl operations and find what functions
// get called.
static void FindFunctionsFromBranches(u32 startAddr, u32 endAddr, SymbolDB* func_db)
{
  for (u32 addr = startAddr; addr < endAddr; addr += 4)
  {
    const PowerPC::TryReadInstResult read_result = PowerPC::TryReadInstruction(addr);
    const UGeckoInstruction instr = read_result.hex;

    if (read_result.valid && PPCTables::IsValidInstruction(instr))
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
          if (PowerPC::HostIsRAMAddress(target))
          {
            func_db->AddFunction(target);
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

static void FindFunctionsFromHandlers(PPCSymbolDB* func_db)
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

  for (const auto& entry : handlers)
  {
    const PowerPC::TryReadInstResult read_result = PowerPC::TryReadInstruction(entry.first);
    if (read_result.valid && PPCTables::IsValidInstruction(read_result.hex))
    {
      // Check if this function is already mapped
      Symbol* f = func_db->AddFunction(entry.first);
      if (!f)
        continue;
      f->Rename(entry.second);
    }
  }
}

static void FindFunctionsAfterReturnInstruction(PPCSymbolDB* func_db)
{
  std::vector<u32> funcAddrs;

  for (const auto& func : func_db->Symbols())
    funcAddrs.push_back(func.second.address + func.second.size);

  for (u32& location : funcAddrs)
  {
    while (true)
    {
      // Skip zeroes (e.g. Donkey Kong Country Returns) and nop (e.g. libogc)
      // that sometimes pad function to 16 byte boundary.
      PowerPC::TryReadInstResult read_result = PowerPC::TryReadInstruction(location);
      while (read_result.valid && (location & 0xf) != 0)
      {
        if (read_result.hex != 0 && read_result.hex != 0x60000000)
          break;
        location += 4;
        read_result = PowerPC::TryReadInstruction(location);
      }
      if (read_result.valid && PPCTables::IsValidInstruction(read_result.hex))
      {
        // check if this function is already mapped
        Symbol* f = func_db->AddFunction(location);
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

void FindFunctions(u32 startAddr, u32 endAddr, PPCSymbolDB* func_db)
{
  // Step 1: Find all functions
  FindFunctionsFromBranches(startAddr, endAddr, func_db);
  FindFunctionsFromHandlers(func_db);
  FindFunctionsAfterReturnInstruction(func_db);

  // Step 2:
  func_db->FillInCallers();

  int numLeafs = 0, numNice = 0, numUnNice = 0;
  int numTimer = 0, numRFI = 0, numStraightLeaf = 0;
  int leafSize = 0, niceSize = 0, unniceSize = 0;
  for (auto& func : func_db->AccessSymbols())
  {
    if (func.second.address == 4)
    {
      WARN_LOG(OSHLE, "Weird function");
      continue;
    }
    AnalyzeFunction2(&(func.second));
    Symbol& f = func.second;
    if (f.name.substr(0, 3) == "zzz")
    {
      if (f.flags & FFLAG_LEAF)
        f.Rename(f.name + "_leaf");
      if (f.flags & FFLAG_STRAIGHT)
        f.Rename(f.name + "_straight");
    }
    if (f.flags & FFLAG_LEAF)
    {
      numLeafs++;
      leafSize += f.size;
    }
    else if (f.flags & FFLAG_ONLYCALLSNICELEAFS)
    {
      numNice++;
      niceSize += f.size;
    }
    else
    {
      numUnNice++;
      unniceSize += f.size;
    }

    if (f.flags & FFLAG_TIMERINSTRUCTIONS)
      numTimer++;
    if (f.flags & FFLAG_RFI)
      numRFI++;
    if ((f.flags & FFLAG_STRAIGHT) && (f.flags & FFLAG_LEAF))
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

  INFO_LOG(OSHLE, "Functions analyzed. %i leafs, %i nice, %i unnice."
                  "%i timer, %i rfi. %i are branchless leafs.",
           numLeafs, numNice, numUnNice, numTimer, numRFI, numStraightLeaf);
  INFO_LOG(OSHLE, "Average size: %i (leaf), %i (nice), %i(unnice)", leafSize, niceSize, unniceSize);
}

static bool isCmp(const CodeOp& a)
{
  return (a.inst.OPCD == 10 || a.inst.OPCD == 11) ||
         (a.inst.OPCD == 31 && (a.inst.SUBOP10 == 0 || a.inst.SUBOP10 == 32));
}

static bool isCarryOp(const CodeOp& a)
{
  return (a.opinfo->flags & FL_SET_CA) && !(a.opinfo->flags & FL_SET_OE) &&
         a.opinfo->type == OPTYPE_INTEGER;
}

static bool isCror(const CodeOp& a)
{
  return a.inst.OPCD == 19 && a.inst.SUBOP10 == 449;
}

void PPCAnalyzer::ReorderInstructionsCore(u32 instructions, CodeOp* code, bool reverse,
                                          ReorderType type)
{
  // Bubbling an instruction sometimes reveals another opportunity to bubble an instruction, so do
  // multiple passes.
  while (true)
  {
    // Instruction Reordering Pass
    // Carry pass: bubble carry-using instructions as close to each other as possible, so we can
    // avoid
    // storing the carry flag.
    // Compare pass: bubble compare instructions next to branches, so they can be merged.
    bool swapped = false;
    int increment = reverse ? -1 : 1;
    int start = reverse ? instructions - 1 : 0;
    int end = reverse ? 0 : instructions - 1;
    for (int i = start; i != end; i += increment)
    {
      CodeOp& a = code[i];
      CodeOp& b = code[i + increment];
      // Reorder integer compares, rlwinm., and carry-affecting ops
      // (if we add more merged branch instructions, add them here!)
      if ((type == REORDER_CROR && isCror(a)) || (type == REORDER_CARRY && isCarryOp(a)) ||
          (type == REORDER_CMP && (isCmp(a) || a.outputCR0)))
      {
        // once we're next to a carry instruction, don't move away!
        if (type == REORDER_CARRY && i != start)
        {
          // if we read the CA flag, and the previous instruction sets it, don't move away.
          if (!reverse && (a.opinfo->flags & FL_READ_CA) &&
              (code[i - increment].opinfo->flags & FL_SET_CA))
            continue;
          // if we set the CA flag, and the next instruction reads it, don't move away.
          if (reverse && (a.opinfo->flags & FL_SET_CA) &&
              (code[i - increment].opinfo->flags & FL_READ_CA))
            continue;
        }

        if (CanSwapAdjacentOps(a, b))
        {
          // Alright, let's bubble it!
          std::swap(a, b);
          swapped = true;
        }
      }
    }
    if (!swapped)
      return;
  }
}

void PPCAnalyzer::ReorderInstructions(u32 instructions, CodeOp* code)
{
  // Reorder cror instructions upwards (e.g. towards an fcmp). Technically we should be more
  // picky about this, but cror seems to almost solely be used for this purpose in real code.
  // Additionally, the other boolean ops seem to almost never be used.
  if (HasOption(OPTION_CROR_MERGE))
    ReorderInstructionsCore(instructions, code, true, REORDER_CROR);
  // For carry, bubble instructions *towards* each other; one direction often isn't enough
  // to get pairs like addc/adde next to each other.
  if (HasOption(OPTION_CARRY_MERGE))
  {
    ReorderInstructionsCore(instructions, code, false, REORDER_CARRY);
    ReorderInstructionsCore(instructions, code, true, REORDER_CARRY);
  }
  if (HasOption(OPTION_BRANCH_MERGE))
    ReorderInstructionsCore(instructions, code, false, REORDER_CMP);
}

void PPCAnalyzer::SetInstructionStats(CodeBlock* block, CodeOp* code, const GekkoOPInfo* opinfo,
                                      u32 index)
{
  code->wantsCR0 = false;
  code->wantsCR1 = false;

  if (opinfo->flags & FL_USE_FPU)
    block->m_fpa->any = true;

  if (opinfo->flags & FL_TIMER)
    block->m_gpa->anyTimer = true;

  // Does the instruction output CR0?
  if (opinfo->flags & FL_RC_BIT)
    code->outputCR0 = code->inst.hex & 1;  // todo fix
  else if ((opinfo->flags & FL_SET_CRn) && code->inst.CRFD == 0)
    code->outputCR0 = true;
  else
    code->outputCR0 = (opinfo->flags & FL_SET_CR0) ? true : false;

  // Does the instruction output CR1?
  if (opinfo->flags & FL_RC_BIT_F)
    code->outputCR1 = code->inst.hex & 1;  // todo fix
  else if ((opinfo->flags & FL_SET_CRn) && code->inst.CRFD == 1)
    code->outputCR1 = true;
  else
    code->outputCR1 = (opinfo->flags & FL_SET_CR1) ? true : false;

  code->wantsFPRF = (opinfo->flags & FL_READ_FPRF) ? true : false;
  code->outputFPRF = (opinfo->flags & FL_SET_FPRF) ? true : false;
  code->canEndBlock = (opinfo->flags & FL_ENDBLOCK) ? true : false;

  code->wantsCA = (opinfo->flags & FL_READ_CA) ? true : false;
  code->outputCA = (opinfo->flags & FL_SET_CA) ? true : false;

  // We're going to try to avoid storing carry in XER if we can avoid it -- keep it in the x86 carry
  // flag!
  // If the instruction reads CA but doesn't write it, we still need to store CA in XER; we can't
  // leave it in flags.
  if (HasOption(OPTION_CARRY_MERGE))
    code->wantsCAInFlags = code->wantsCA && code->outputCA && opinfo->type == OPTYPE_INTEGER;
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
    block->m_gpa->SetOutputRegister(code->inst.RA, index);
  }
  if (opinfo->flags & FL_OUT_D)
  {
    code->regsOut[code->inst.RD] = true;
    block->m_gpa->SetOutputRegister(code->inst.RD, index);
  }
  if ((opinfo->flags & FL_IN_A) || ((opinfo->flags & FL_IN_A0) && code->inst.RA != 0))
  {
    code->regsIn[code->inst.RA] = true;
    block->m_gpa->SetInputRegister(code->inst.RA, index);
  }
  if (opinfo->flags & FL_IN_B)
  {
    code->regsIn[code->inst.RB] = true;
    block->m_gpa->SetInputRegister(code->inst.RB, index);
  }
  if (opinfo->flags & FL_IN_C)
  {
    code->regsIn[code->inst.RC] = true;
    block->m_gpa->SetInputRegister(code->inst.RC, index);
  }
  if (opinfo->flags & FL_IN_S)
  {
    code->regsIn[code->inst.RS] = true;
    block->m_gpa->SetInputRegister(code->inst.RS, index);
  }
  if (code->inst.OPCD == 46)  // lmw
  {
    for (int iReg = code->inst.RD; iReg < 32; ++iReg)
    {
      code->regsOut[iReg] = true;
      block->m_gpa->SetOutputRegister(iReg, index);
    }
  }
  else if (code->inst.OPCD == 47)  // stmw
  {
    for (int iReg = code->inst.RS; iReg < 32; ++iReg)
    {
      code->regsIn[iReg] = true;
      block->m_gpa->SetInputRegister(iReg, index);
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

  switch (opinfo->type)
  {
  case OPTYPE_INTEGER:
  case OPTYPE_LOAD:
  case OPTYPE_STORE:
  case OPTYPE_LOADFP:
  case OPTYPE_STOREFP:
    break;
  case OPTYPE_SINGLEFP:
  case OPTYPE_DOUBLEFP:
    break;
  case OPTYPE_BRANCH:
    if (code->inst.hex == 0x4e800020)
    {
      // For analysis purposes, we can assume that blr eats opinfo->flags.
      code->outputCR0 = true;
      code->outputCR1 = true;
    }
    break;
  case OPTYPE_SYSTEM:
  case OPTYPE_SYSTEMFP:
    break;
  }
}

u32 PPCAnalyzer::Analyze(u32 address, CodeBlock* block, CodeBuffer* buffer, u32 blockSize)
{
  // Clear block stats
  memset(block->m_stats, 0, sizeof(BlockStats));

  // Clear register stats
  block->m_gpa->any = true;
  block->m_fpa->any = false;

  block->m_gpa->Clear();
  block->m_fpa->Clear();

  // Set the blocks start address
  block->m_address = address;

  // Reset our block state
  block->m_broken = false;
  block->m_memory_exception = false;
  block->m_num_instructions = 0;
  block->m_gqr_used = BitSet8(0);
  block->m_physical_addresses.clear();

  CodeOp* code = buffer->codebuffer;

  bool found_exit = false;
  bool found_call = false;
  size_t caller = 0;
  u32 numFollows = 0;
  u32 num_inst = 0;

  for (u32 i = 0; i < blockSize; ++i)
  {
    auto result = PowerPC::TryReadInstruction(address);
    if (!result.valid)
    {
      if (i == 0)
        block->m_memory_exception = true;
      break;
    }
    UGeckoInstruction inst = result.hex;

    num_inst++;
    memset(&code[i], 0, sizeof(CodeOp));
    GekkoOPInfo* opinfo = GetOpInfo(inst);

    code[i].opinfo = opinfo;
    code[i].address = address;
    code[i].inst = inst;
    code[i].branchTo = UINT32_MAX;
    code[i].branchToIndex = UINT32_MAX;
    code[i].skip = false;
    block->m_stats->numCycles += opinfo->numCycles;
    block->m_physical_addresses.insert(result.physical_address);

    SetInstructionStats(block, &code[i], opinfo, i);

    bool follow = false;
    u32 destination = 0;

    bool conditional_continue = false;

    // TODO: Find the optimal value for BRANCH_FOLLOWING_THRESHOLD.
    //       If it is small, the performance will be down.
    //       If it is big, the size of generated code will be big and
    //       cache clearning will happen many times.
    if (HasOption(OPTION_BRANCH_FOLLOW) && numFollows < BRANCH_FOLLOWING_THRESHOLD)
    {
      if (inst.OPCD == 18 && blockSize > 1)
      {
        // Always follow BX instructions.
        // TODO: Loop unrolling might bloat the code size too much.
        //       Enable it carefully.
        follow = destination != block->m_address;
        destination = SignExt26(inst.LI << 2) + (inst.AA ? 0 : address);
        if (inst.LK)
        {
          found_call = true;
          caller = i;
        }
      }
      else if (inst.OPCD == 16 && (inst.BO & BO_DONT_DECREMENT_FLAG) &&
               (inst.BO & BO_DONT_CHECK_CONDITION) && blockSize > 1)
      {
        // Always follow unconditional BCX instructions, but they are very rare.
        follow = true;
        destination = SignExt16(inst.BD << 2) + (inst.AA ? 0 : address);
        if (inst.LK)
        {
          found_call = true;
          caller = i;
        }
      }
      else if (inst.OPCD == 19 && inst.SUBOP10 == 16 && !inst.LK && found_call &&
               (inst.BO & BO_DONT_DECREMENT_FLAG) && (inst.BO & BO_DONT_CHECK_CONDITION))
      {
        // bclrx with unconditional branch = return
        // Follow it if we can propagate the LR value of the last CALL instruction.
        // Through it would be easy to track the upper level of call/return,
        // we can't guarantee the LR value. The PPC ABI forces all functions to push
        // the LR value on the stack as there are no spare registers. So we'd need
        // to check all store instruction to not alias with the stack.
        follow = true;
        destination = code[caller].address + 4;
        found_call = false;
        code[i].skip = true;

        // Skip the RET, so also don't generate the stack entry for the BLR optimization.
        code[caller].skipLRStack = true;
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
      else if (inst.OPCD == 19 && inst.SUBOP10 == 16 && ((inst.BO & BO_DONT_DECREMENT_FLAG) == 0 ||
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

    if (follow)
    {
      // Follow the unconditional branch.
      numFollows++;
      address = destination;
    }
    else
    {
      // Just pick the next instruction
      address += 4;
      if (!conditional_continue && opinfo->flags & FL_ENDBLOCK)  // right now we stop early
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

  if ((!found_exit && num_inst > 0) || blockSize == 1)
  {
    // We couldn't find an exit
    block->m_broken = true;
  }

  // Scan for flag dependencies; assume the next block (or any branch that can leave the block)
  // wants flags, to be safe.
  bool wantsCR0 = true, wantsCR1 = true, wantsFPRF = true, wantsCA = true;
  BitSet32 fprInUse, gprInUse, gprInReg, fprInXmm;
  for (int i = block->m_num_instructions - 1; i >= 0; i--)
  {
    bool opWantsCR0 = code[i].wantsCR0;
    bool opWantsCR1 = code[i].wantsCR1;
    bool opWantsFPRF = code[i].wantsFPRF;
    bool opWantsCA = code[i].wantsCA;
    code[i].wantsCR0 = wantsCR0 || code[i].canEndBlock;
    code[i].wantsCR1 = wantsCR1 || code[i].canEndBlock;
    code[i].wantsFPRF = wantsFPRF || code[i].canEndBlock;
    code[i].wantsCA = wantsCA || code[i].canEndBlock;
    wantsCR0 |= opWantsCR0 || code[i].canEndBlock;
    wantsCR1 |= opWantsCR1 || code[i].canEndBlock;
    wantsFPRF |= opWantsFPRF || code[i].canEndBlock;
    wantsCA |= opWantsCA || code[i].canEndBlock;
    wantsCR0 &= !code[i].outputCR0 || opWantsCR0;
    wantsCR1 &= !code[i].outputCR1 || opWantsCR1;
    wantsFPRF &= !code[i].outputFPRF || opWantsFPRF;
    wantsCA &= !code[i].outputCA || opWantsCA;
    code[i].gprInUse = gprInUse;
    code[i].fprInUse = fprInUse;
    code[i].gprInReg = gprInReg;
    code[i].fprInXmm = fprInXmm;
    // TODO: if there's no possible endblocks or exceptions in between, tell the regcache
    // we can throw away a register if it's going to be overwritten later.
    gprInUse |= code[i].regsIn;
    gprInReg |= code[i].regsIn;
    fprInUse |= code[i].fregsIn;
    if (strncmp(code[i].opinfo->opname, "stfd", 4))
      fprInXmm |= code[i].fregsIn;
    // For now, we need to count output registers as "used" though; otherwise the flush
    // will result in a redundant store (e.g. store to regcache, then store again to
    // the same location later).
    gprInUse |= code[i].regsOut;
    if (code[i].fregOut >= 0)
      fprInUse[code[i].fregOut] = true;
  }

  // Forward scan, for flags that need the other direction for calculation.
  BitSet32 fprIsSingle, fprIsDuplicated, fprIsStoreSafe, gprDefined, gprBlockInputs;
  BitSet8 gqrUsed, gqrModified;
  for (u32 i = 0; i < block->m_num_instructions; i++)
  {
    gprBlockInputs |= code[i].regsIn & ~gprDefined;
    gprDefined |= code[i].regsOut;

    code[i].fprIsSingle = fprIsSingle;
    code[i].fprIsDuplicated = fprIsDuplicated;
    code[i].fprIsStoreSafe = fprIsStoreSafe;
    if (code[i].fregOut >= 0)
    {
      fprIsSingle[code[i].fregOut] = false;
      fprIsDuplicated[code[i].fregOut] = false;
      fprIsStoreSafe[code[i].fregOut] = false;
      // Single, duplicated, and doesn't need PPC_FP.
      if (code[i].opinfo->type == OPTYPE_SINGLEFP)
      {
        fprIsSingle[code[i].fregOut] = true;
        fprIsDuplicated[code[i].fregOut] = true;
        fprIsStoreSafe[code[i].fregOut] = true;
      }
      // Single and duplicated, but might be a denormal (not safe to skip PPC_FP).
      // TODO: if we go directly from a load to store, skip conversion entirely?
      // TODO: if we go directly from a load to a float instruction, and the value isn't used
      // for anything else, we can skip PPC_FP on a load too.
      if (!strncmp(code[i].opinfo->opname, "lfs", 3))
      {
        fprIsSingle[code[i].fregOut] = true;
        fprIsDuplicated[code[i].fregOut] = true;
      }
      // Paired are still floats, but the top/bottom halves may differ.
      if (code[i].opinfo->type == OPTYPE_PS || code[i].opinfo->type == OPTYPE_LOADPS)
      {
        fprIsSingle[code[i].fregOut] = true;
        fprIsStoreSafe[code[i].fregOut] = true;
      }
      // Careful: changing the float mode in a block breaks this optimization, since
      // a previous float op might have had had FTZ off while the later store has FTZ
      // on. So, discard all information we have.
      if (!strncmp(code[i].opinfo->opname, "mtfs", 4))
        fprIsStoreSafe = BitSet32(0);
    }

    if (code[i].opinfo->type == OPTYPE_STOREPS || code[i].opinfo->type == OPTYPE_LOADPS)
    {
      int gqr = code[i].inst.OPCD == 4 ? code[i].inst.Ix : code[i].inst.I;
      gqrUsed[gqr] = true;
    }

    if (code[i].inst.OPCD == 31 && code[i].inst.SUBOP10 == 467)  // mtspr
    {
      int gqr = ((code[i].inst.SPRU << 5) | code[i].inst.SPRL) - SPR_GQR0;
      if (gqr >= 0 && gqr <= 7)
        gqrModified[gqr] = true;
    }
  }
  block->m_gqr_used = gqrUsed;
  block->m_gqr_modified = gqrModified;
  block->m_gpr_inputs = gprBlockInputs;
  return address;
}

}  // namespace
