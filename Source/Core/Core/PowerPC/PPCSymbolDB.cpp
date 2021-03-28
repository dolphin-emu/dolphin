// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/PPCSymbolDB.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <queue>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include "Common/CommonTypes.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Core/Core.h"
#include "Core/HLE/HLE.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/SignatureDB/SignatureDB.h"

PPCSymbolDB g_symbolDB;

PPCSymbolDB::PPCSymbolDB() : debugger{&PowerPC::debug_interface}
{
}

PPCSymbolDB::~PPCSymbolDB() = default;

// Adds the function to the list, unless it's already there
Common::Symbol* PPCSymbolDB::AddFunction(u32 start_addr)
{
  // It's already in the list
  if (m_functions.find(start_addr) != m_functions.end())
    return nullptr;

  Common::Symbol symbol;
  if (!PPCAnalyst::AnalyzeFunction(start_addr, symbol))
    return nullptr;

  m_functions[start_addr] = std::move(symbol);
  Common::Symbol* ptr = &m_functions[start_addr];
  ptr->type = Common::Symbol::Type::Function;
  m_checksum_to_function[ptr->hash].insert(ptr);
  return ptr;
}

// Returns true if an existing symbol was updated, false if a new symbol was added.
bool PPCSymbolDB::AddKnownSymbol(u32 start_addr, u32 size, const std::string& name,
                                 Common::Symbol::Type type)
{
  Common::Symbol* sym;
  Common::Symbol temp;
  bool already_have;

  auto iter = m_functions.find(start_addr);
  if (iter != m_functions.end())
  {
    sym = &iter->second;
    already_have = true;
  }
  else
  {
    sym = &temp;
    already_have = false;
  }

  sym->address = start_addr;
  sym->size = size;
  sym->Rename(name);
  sym->type = type;

  if (type == Common::Symbol::Type::Function)
  {
    PPCAnalyst::AnalyzeFunction(start_addr, *sym, size);
    // Undo possible truncating of the symbol.  We want the original size, even if it seems wrong.
    if (size != 0 && sym->size != size)
    {
      WARN_LOG_FMT(SYMBOLS, "Analysed symbol ({}) size mismatch, {} expected but {} computed", name,
                   size, sym->size);
      sym->size = size;
    }
  }

  if (already_have == false)
  {
    if (type == Common::Symbol::Type::Function)
      m_checksum_to_function[sym->hash].insert(&m_functions[start_addr]);
    m_functions[start_addr] = *sym;
  }

  return already_have;
}

Common::Symbol* PPCSymbolDB::GetSymbolFromAddr(u32 addr)
{
  auto it = m_functions.lower_bound(addr);
  if (it == m_functions.end())
    return nullptr;

  // If the address is exactly the start address of a symbol, we're done.
  if (it->second.address == addr)
    return &it->second;

  // Otherwise, check whether the address is within the bounds of a symbol.
  if (it != m_functions.begin())
    --it;
  if (addr >= it->second.address && addr < it->second.address + it->second.size)
    return &it->second;

  return nullptr;
}

std::string PPCSymbolDB::GetDescription(u32 addr)
{
  Common::Symbol* symbol = GetSymbolFromAddr(addr);
  if (symbol)
    return symbol->name;
  else
    return " --- ";
}

void PPCSymbolDB::FillInCallers()
{
  for (auto& p : m_functions)
  {
    p.second.callers.clear();
  }

  for (auto& entry : m_functions)
  {
    Common::Symbol& f = entry.second;
    for (const Common::SCall& call : f.calls)
    {
      const Common::SCall new_call(entry.first, call.call_address);
      const u32 function_address = call.function;

      auto func_iter = m_functions.find(function_address);
      if (func_iter != m_functions.end())
      {
        Common::Symbol& called_function = func_iter->second;
        called_function.callers.push_back(new_call);
      }
      else
      {
        // LOG(SYMBOLS, "FillInCallers tries to fill data in an unknown function 0x%08x.",
        // FunctionAddress);
        // TODO - analyze the function instead.
      }
    }
  }
}

void PPCSymbolDB::PrintCalls(u32 funcAddr) const
{
  const auto iter = m_functions.find(funcAddr);
  if (iter == m_functions.end())
  {
    WARN_LOG_FMT(SYMBOLS, "Symbol does not exist");
    return;
  }

  const Common::Symbol& f = iter->second;
  DEBUG_LOG_FMT(SYMBOLS, "The function {} at {:08x} calls:", f.name, f.address);
  for (const Common::SCall& call : f.calls)
  {
    const auto n = m_functions.find(call.function);
    if (n != m_functions.end())
    {
      DEBUG_LOG_FMT(SYMBOLS, "* {:08x} : {}", call.call_address, n->second.name);
    }
  }
}

void PPCSymbolDB::PrintCallers(u32 funcAddr) const
{
  const auto iter = m_functions.find(funcAddr);
  if (iter == m_functions.end())
    return;

  const Common::Symbol& f = iter->second;
  DEBUG_LOG_FMT(SYMBOLS, "The function {} at {:08x} is called by:", f.name, f.address);
  for (const Common::SCall& caller : f.callers)
  {
    const auto n = m_functions.find(caller.function);
    if (n != m_functions.end())
    {
      DEBUG_LOG_FMT(SYMBOLS, "* {:08x} : {}", caller.call_address, n->second.name);
    }
  }
}

void PPCSymbolDB::LogFunctionCall(u32 addr)
{
  auto iter = m_functions.find(addr);
  if (iter == m_functions.end())
    return;

  Common::Symbol& f = iter->second;
  f.num_calls++;
}

// TODO: This really belongs somewhere else
bool VerifyVTable(u32 vaddress_start, u32 size)
{
  // TODO: Pointer 1 extra checks.
  //  - Always points to __RTTI__ for the class associated with the __vt__.
  //    If __RTTI__ was stripped, this is a nullptr.
  // TODO: Pointer 2 extra checks.
  //  - This was stripped in Pikmin.  What does this normally point to?

  // PPC Gekko's pointer size is 4 bytes.
  // The minimum __vt__ size is 8 bytes.
  if (size % 4 || size < 8)
    return false;

  u32 vaddress_end = vaddress_start + size;

  for (u32 vaddress = vaddress_start; vaddress <= vaddress_end; vaddress += 4)
  {
    u32 ptr = PowerPC::HostRead_U32(vaddress);
    // The pointer must either be null or in valid memory.
    if (ptr != 0x00000000 || !PowerPC::HostIsRAMAddress(ptr))
      return false;
  }

  return true;
}

// TODO: This really belongs somewhere else
bool VerifyRTTI(u32 vaddress_start, u32 size)
{
  // TODO: Pointer 1 extra checks.
  //  - char* pointing to a cstring of the class's name.
  //    Demangle the __RTTI__ symbol name to compare against this.
  // TODO: Pointer 2 extra checks.
  //  - This is always a pointer to an unnamed pointer table in the .data section.
  //    PPCSymbolDB::GetSymbolFromAddr could be useful here if only all symbols were already
  //    loaded. Pointers such as the base class __RTTI__ can be found.

  // __RTTI__ is always(?) 8 bytes large.
  if (size != 8)
    return false;

  u32 ptr;

  // Class name pointer
  ptr = PowerPC::HostRead_U32(vaddress_start);
  if (ptr != 0x00000000 || !PowerPC::HostIsRAMAddress(ptr))
    return false;
  // ??? table pointer
  ptr = PowerPC::HostRead_U32(vaddress_start);
  if (ptr != 0x00000000 || !PowerPC::HostIsRAMAddress(ptr))
    return false;

  return true;
}

// Returns false if the Machine State Register is in a bad state.
// Virtual address translation is required for loading symbol maps.
// File::IOFile must be in "r" mode.
bool PPCSymbolDB::HostLoadMap(File::IOFile& f, bool bad)
{
  bool retval = true;
  Core::RunAsCPUThread([this, &retval, &f, &bad] {
    if (!MSR.DR || !MSR.IR)
    {
      retval = false;
      return;
    }
    LoadMap(f, bad);
  });
  return retval;
}

// Load CodeWarrior Symbol Maps or mapfiles produced by SaveSymbolMap below.
// bad_map=true means check for symbols that don't make sense and discard them.
// This is useful if you have a symbol map that only partially matches the binary you have.
// File::IOFile must be in "r" mode.
void PPCSymbolDB::LoadMap(File::IOFile& f, bool bad_map)
{
  // Dolphin's PPCSymbolDB is bad. Closure Symbols and Entry Symbols are not accounted for at all.
  // Additionally, this function is bloated and ineffective. It would be better if all symbols were
  // be loaded before bad map checks. That way, more sophisticated checks could be done. I've done
  // my best to document what is being done in this function and fix what was fundamentally broken,
  // but this is like a band-aid over a bullet hole. A proper reworking is way outside the scope of
  // my PR, however, so this is all I can do for now.

  // TODO: Symbols need to keep track of child Entry Symbols. I think a SymbolDB or something
  // similar within the symbol is the solution. Maybe Symbol and SymbolDB ought to be merged into
  // one class? That might make recording Closure Symbols feasible. Each Closure Symbol would be
  // its own miniature symbol database?

  // Two columns are used by Super Smash Bros. Brawl Korean map file
  // Three columns are commonly used
  // Four columns are used in American Mensa Academy map files and perhaps other games
  int column_count = 0;

  int new_symbol_count = 0;
  int updated_symbol_count = 0;
  int fill_symbol_count = 0;

  int invalid_address_count = 0;
  int empty_name_count = 0;
  int unused_symbol_count = 0;
  int closure_symbol_count = 0;
  int entry_symbol_count = 0;
  int suspicious_symbol_count = 0;
  std::queue<std::string> suspicious_lines;

  char line[512];
  std::string section_name;
  while (fgets(line, 512, f.GetHandle()))
  {
    char temp[256]{};

    // Obtain first token and check if the line is blank.
    if (sscanf(line, "%255s", temp) == EOF)
      continue;

    // Link map example:
    //
    // Link map of __start
    //  1] __start(func, weak) found in os.a __start.c
    //   2] __init_registers(func, local) found in os.a __start.c
    //    3] _stack_addr found as linker generated symbol
    // ...
    //           10] EXILock(func, global) found in exi.a EXIBios.c

    // Skip link map.
    if (StringEndsWith(temp, "]") || strstr(line, "Link map of "))
      continue;

    // Discard unused symbols
    if (!strcmp(temp, "UNUSED"))
    {
      ++unused_symbol_count;
      continue;
    }

    // Update current section name
    // The section headers of three and four column symbol maps are detected by the first
    // condition. Two column symbol maps have very barebones section headers. They is detected by
    // the second and third conditions below.
    if (strstr(line, " section layout\n") || !strcmp(temp, ".text") || !strcmp(temp, ".init"))
    {
      section_name = temp;
      continue;
    }

    // Detect four columns
    if (column_count == 0)
    {
      if (strstr(line, "Starting        Virtual  File"))
      {
        column_count = 4;
        continue;
      }
    }

    // Three columns header example:
    //
    //   Starting        Virtual
    //   address  Size   address
    //   -----------------------

    // Four columns header example:
    //
    //   Starting        Virtual  File
    //   address  Size   address  offset
    //   ---------------------------------

    // Skip Column Headers
    if (strstr(line, "Starting        Virtual"))
      continue;
    if (strstr(line, "address  Size   address"))
      continue;
    if (strstr(line, "-----------------------"))
      continue;

    // TODO - Handle/Write a parser for:
    //  - Link map
    //  - Memory map
    //  - Linker generated symbols
    // The latter two are usually at the end of a symbol map, so aborting here is fine.
    if (strstr(line, "Memory map:") || strstr(line, "Linker generated symbols:"))
      break;

    // If no section has been decided yet, default to text.
    if (section_name.empty())
    {
      section_name = ".text";
    }

    // Two column symbols example:
    // 8000c860 srGetAppGamename/(sr_getappname.o)
    // 8000c864 srGetAppInitialCode/(sr_getappname.o)
    // ...
    // 80406d9c MWInitializeCriticalSection/(MWCriticalSection_gc.o)

    // Detect two columns with three columns fallback
    if (column_count == 0)
    {
      const std::string_view stripped_line = StripSpaces(line);
      if (std::count(stripped_line.begin(), stripped_line.end(), ' ') == 1)
        column_count = 2;
      else
        column_count = 3;
    }

    u32 address = 0, vaddress = 0, size = 0, offset = 0, alignment = 0;
    bool is_entry = false;
    char name[512]{}, parent_name[512]{};
    Common::Symbol::Type type;

    if (column_count == 4)
    {
      // We can assume CodeWarrior Linker Map text formatting went something like this...
      // For typical symbols:
      // "  %08x %06x %08x %08x %2i %s \t%s %s"
      // For unused symbols (including unused entry symbols):
      // "  UNUSED   %06x ........ ........    %s %s %s"
      // For entry symbols:
      // "  %08x %06x %08x %08x    %s (entry of %s) \t%s %s"

      // Note: All unused symbols are discarded, so we only need to detect and handle entry
      // symbols
      char* strptr;
      strptr = strstr(line, "(entry of ");
      // Entry Symbols
      if (strptr)
      {
        sscanf(line, "%08x %08x %08x %08x %511s", &address, &size, &vaddress, &offset, name);
        alignment = 0;
        is_entry = true;

        // When the day comes that Entry Symbols can be properly recorded, the following code gets
        // the parent symbol's name from the "(entry of _______)" part of the current line.
        sscanf(strptr + 10, "%511s", parent_name);
        strptr = (strrchr(parent_name, ')'));
        // Overwrite closing ')'
        if (strptr)
          *strptr = '\0';
      }
      // All other symbols
      else
      {
        sscanf(line, "%08x %08x %08x %08x %i %511s", &address, &size, &vaddress, &offset,
               &alignment, name);
        is_entry = false;
      }
    }
    else if (column_count == 3)
    {
      // We can assume CodeWarrior Linker Map text formatting went something like this...
      // For typical symbols:
      // "  %08x %06x %08x %2i %s \t%s %s"
      // For unused symbols (including unused entry symbols):
      // "  UNUSED   %06x ........ %s %s %s"
      // For entry symbols:
      // "  %08x %06x %08x %s (entry of %s) \t%s %s"

      // Note: All unused symbols are discarded, so we only need to detect and handle entry
      // symbols
      char* strptr;
      strptr = strstr(line, "(entry of ");
      // Entry Symbols
      if (strptr)
      {
        sscanf(line, "%08x %08x %08x %511s", &address, &size, &vaddress, name);
        alignment = 0;
        is_entry = true;

        // When the day comes that Entry Symbols can be properly recorded, the following code gets
        // the parent symbol's name from the "(entry of _______)" part of the current line.
        sscanf(strptr + 10, "%511s", parent_name);
        strptr = (strrchr(parent_name, ')'));
        // Overwrite closing ')'
        if (strptr)
          *strptr = '\0';
      }
      // All other symbols
      else
      {
        sscanf(line, "%08x %08x %08x %i %511s", &address, &size, &vaddress, &alignment, name);
        is_entry = false;
      }
    }
    else if (column_count == 2)
    {
      // Two Column Linker Maps have no unused nor entry symbols.
      // We can assume CodeWarrior Linker Map text formatting went something like this...
      // "%08x %s/[%s]/(%s)"
      sscanf(line, "%08x %511s", &address, name);
      vaddress = address;
      is_entry = false;
    }
    else  // Unsupported column count
    {
      break;
    }

    // Symbols with empty names are not valid entries.
    if (strlen(name) == 0)
    {
      ++empty_name_count;
      continue;
    }

    // Just earlier ago, sscanf read the line to get the symbol name, but there's a problem...
    // It only got this part:       "~~~~~~~~~~~"
    //   000000a0 000004 80005600 32 Probe_Start    jaudio.o dummyprobe.c
    // When we really wanted this:  "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
    //   000000a0 000004 80005600 32 Probe_Start    jaudio.o dummyprobe.c

    // The following hacky workaround does that for us.  Note: this fails if name is empty.
    const char* namepos = strstr(line, name);
    if (namepos != nullptr)  // would be odd if not :P
      strcpy(name, namepos);
    name[strlen(name) - 1] = 0;
    if (name[strlen(name) - 1] == '\r')
      name[strlen(name) - 1] = 0;

    // Later CodeWarrior Symbol Maps record the padding between compilation units with symbols named
    // "*fill*".  If something special needs to be done with these symbols in the future, this would
    // be the place to do so.
    if (!strcmp(name, "*fill*"))
    {
      fill_symbol_count++;
    }

    // Q&A: What is a Closure Symbol?
    // Closure Symbols are symbols created by the compiler(?) that envelope all symbols in a
    // section from the same compilation unit. They come in two variants, which I've dubbed
    // "Start" and "End" Closure Symbols. Static Libraries (*.a), when linked, only generate Start
    // Closure Symbols.  Objects (*.o), when linked, generate both Start and End Closure Symbols
    // for text sections, and only Start Closure Symbols for everything else. Start Closure
    // Symbols typically have names such as ".text" or ".sdata". End Closure Symbols typically
    // have names such as
    // ".text.44391". Closure Symbols almost always have a reported alignment of 1 byte.

    // Closure Symbols are not useful with how Dolphin's Symbol Database works.  Start Closure
    // Symbols overlap the entire compilation unit, which interferes with disassembly
    // highlighting. Or, it would, if it weren't for the fact that the function "AddKnownSymbol"
    // called later in this function is guaranteed to overwrite any Closure Symbols, as they have
    // the same virtual address as the first symbol of the compilation unit they represent.
    // Furthermore, the reported size of Start Closure Symbols includes those of Unused Symbols
    // stripped from the binary, making them inaccurate for usage at runtime.  Thus, the following
    // check formalizes the removal of Closure Symbols until they are more useful / less harmful
    // to Dolphin.

    // The ctors and dtors sections don't have traditional Closure Symbols. Also, some symbol maps
    // have extra linker generated closure symbols in these sections.  These symbols have oddities
    // such as "$00" to "$99" tacked on the end of the name, '_' instead of '.' as the first
    // character, a reported alignment of 4 bytes, and maybe more?  Basically, ctors and dtors are
    // left as is, because they are too weird.

    if (alignment == 1)
    {
      if (section_name != ".ctors" && section_name != ".dtors")
      {
        if (!strncmp(name, section_name.c_str(), section_name.size()))
        {
          ++closure_symbol_count;
          continue;
        }
      }
    }

    // Q&A: What is an Entry Symbol?
    // In the following C Function featuring inline ASM, "__OSSystemCallVectorStart" and
    // "__OSSystemCallVectorEnd" are Entry Symbols of the parent symbol, "SystemCallVector".

    // void SystemCallVector()
    // {
    //   __asm {
    //     __OSSystemCallVectorStart:
    //     mfspr   r9, HID0
    //     ori     r10, r9, 0x0008
    //     mtspr   HID0, r10
    //     isync
    //     sync
    //     mtspr   HID0, r9
    //     rfi
    //     __OSSystemCallVectorEnd:
    //   }
    // }

    // The following is an abridged example of how Entry Symbols are recorded in a symbol map:
    // 001f63c8 000020 801fb928  4 SystemCallVector
    // 001f63c8 000000 801fb928 __OSSystemCallVectorStart (entry of SystemCallVector)
    // 001f63e4 000000 801fb944 __OSSystemCallVectorEnd (entry of SystemCallVector)

    // Notice that Entry Symbols have a reported size of 0 bytes, and show up in the middle of or
    // at the start of other symbols.  Until Dolphin's Symbols / Symbol Database is improved to
    // account for Entry Symbols properly, they are useless and even harmful to code highlighting.

    // Also, compiler generated(?) Entry Symbols named things like "...data.0 (entry of .data)"
    // are commonly children to Closure Symbols, which makes them doubly useless since Closure
    // Symbols are being discarded atm.
    if (is_entry)
    {
      ++entry_symbol_count;
      continue;
    }

    // Function symbols final checks
    if (section_name == ".init" || section_name == ".text")
    {
      type = Common::Symbol::Type::Function;

      // Symbols outside of RAM are not valid entries.
      // This also doubles as a check that the function's size is a multiple of 4
      if (!PowerPC::HostIsInstructionRAMAddress(vaddress) ||
          !(PowerPC::HostIsInstructionRAMAddress(vaddress + size - 4) && size != 0))
      {
        ++invalid_address_count;
        continue;
      }

      // Additional checks if the symbol map is suspected to be non-matching.
      if (bad_map)
      {
        UGeckoInstruction prelude = PowerPC::HostRead_Instruction(vaddress - 4);
        UGeckoInstruction ending = PowerPC::HostRead_Instruction(vaddress + size - 4);

        auto IsBLR = [](UGeckoInstruction inst) { return inst.hex == 0x4e800020; };
        auto IsRFI = [](UGeckoInstruction inst) { return inst.hex == 0x4c000064; };
        auto IsB = [](UGeckoInstruction inst) { return (inst.hex & 0xfc000003) == 0x48000000; };
        auto IsNOP = [](UGeckoInstruction inst) { return inst.hex == 0x60000000; };
        auto IsBCTR = [](UGeckoInstruction inst) { return inst.hex == 0x4e800420; };
        auto IsBLA = [](UGeckoInstruction inst) { return (inst.hex & 0xfc000003) == 0x48000003; };
        auto IsPadding = [](UGeckoInstruction inst) { return inst.hex == 0x00000000; };

        // Things that can prelude a function symbol:
        //  - BLR or RFI at end of previous function
        //  - Branch at end of previous function (TODO: to outside of previous function)
        //     - Default function parameter signatures or handwritten ASM.
        //  - NOP at end of previous function (inline ASM with naked end label)
        //  - BCTR (__ptmf_scall)
        //  - BLA (__OSDBJump)
        //  - Padding null bytes (alignment)
        if (!IsBLR(prelude) && !IsRFI(prelude) && !IsB(prelude) && !IsNOP(prelude) &&
            !IsBCTR(prelude) && !IsBLA(prelude) && !IsPadding(prelude))
        {
          ++suspicious_symbol_count;
          suspicious_lines.push(line);
          continue;
        }

        // Things that can end a function symbol
        //  - BLR or RFI at end of function
        //  - Branch at end of previous function (TODO: to outside of function)
        //     - Default function parameter signatures or handwritten ASM.
        //  - NOP at end of function (inline ASM with naked end label)
        //  - BCTR (__ptmf_scall)
        //  - BLA (__OSDBJump)
        if (!IsBLR(ending) && !IsRFI(ending) && !IsB(ending) && !IsNOP(ending) && !IsBCTR(ending) &&
            !IsBLA(ending))
        {
          ++suspicious_symbol_count;
          suspicious_lines.push(line);
          continue;
        }
      }
    }
    // Data symbols final checks
    else
    {
      type = Common::Symbol::Type::Data;

      // Symbols outside of RAM are not valid entries.
      if (!PowerPC::HostIsRAMAddress(vaddress) ||
          !(PowerPC::HostIsRAMAddress(vaddress + size - 1) && size != 0))
      {
        ++invalid_address_count;
        continue;
      }

      // Additional checks if the symbol map is suspected to be non-matching.
      if (bad_map)
      {
        if (!strcmp(name, "__vt__") && !VerifyVTable(vaddress, size))
        {
          ++suspicious_symbol_count;
          suspicious_lines.push(line);
          continue;
        }
        if (!strcmp(name, "__RTTI__") && !VerifyRTTI(vaddress, size))
        {
          ++suspicious_symbol_count;
          suspicious_lines.push(line);
          continue;
        }
      }
    }

    if (AddKnownSymbol(vaddress, size, name, type))
      ++updated_symbol_count;
    else
      ++new_symbol_count;
  }

  NOTICE_LOG_FMT(SYMBOLS, "{} symbols were added", new_symbol_count);
  NOTICE_LOG_FMT(SYMBOLS, "{} symbols were updated", updated_symbol_count);
  NOTICE_LOG_FMT(SYMBOLS, "{} symbols were discarded",
                 unused_symbol_count + closure_symbol_count + entry_symbol_count +
                     invalid_address_count + empty_name_count + suspicious_symbol_count);
  NOTICE_LOG_FMT(SYMBOLS, " - {:>5} Unused Symbol(s)", unused_symbol_count);
  NOTICE_LOG_FMT(SYMBOLS, " - {:>5} Closure Symbol(s)", closure_symbol_count);
  NOTICE_LOG_FMT(SYMBOLS, " - {:>5} Entry Symbol(s)", entry_symbol_count);
  NOTICE_LOG_FMT(SYMBOLS, " - {:>5} symbol(s) had an invalid address", invalid_address_count);
  NOTICE_LOG_FMT(SYMBOLS, " - {:>5} symbol(s) had no name", empty_name_count);
  if (bad_map)
  {
    NOTICE_LOG_FMT(SYMBOLS, " - {:>5} symbol(s) seemed suspicious", suspicious_symbol_count);

    if (suspicious_symbol_count != 0)
    {
      if (AskYesNoFmtT("{0} suspicious symbols found.  Would you like to print them to the log?",
                       suspicious_symbol_count))
      {
        while (!suspicious_lines.empty())
        {
          NOTICE_LOG_FMT(SYMBOLS, "{}", suspicious_lines.front().c_str());
          suspicious_lines.pop();
        }
      }
    }
  }

  Index();
  HLE::Clear();
  HLE::PatchFunctions();
}

// Save symbol map similar to CodeWarrior's map file
// File::IOFile must be in "w" mode.
void PPCSymbolDB::SaveSymbolMap(File::IOFile& f) const
{
  std::vector<const Common::Symbol*> function_symbols;
  std::vector<const Common::Symbol*> data_symbols;

  for (const auto& function : m_functions)
  {
    const Common::Symbol& symbol = function.second;
    if (symbol.type == Common::Symbol::Type::Function)
      function_symbols.push_back(&symbol);
    else
      data_symbols.push_back(&symbol);
  }

  // Write .text section
  f.WriteString(".text section layout\n");
  for (const auto& symbol : function_symbols)
  {
    // Write symbol address, size, virtual address, alignment, name
    f.WriteString(fmt::format("  {0:08x} {1:06x} {2:08x} {3:2} {4}\n", symbol->address,
                              symbol->size, symbol->address, 0, symbol->name));
  }

  // Write .data section
  f.WriteString("\n.data section layout\n");
  for (const auto& symbol : data_symbols)
  {
    // Write symbol starting address, size, virtual address, alignment, name
    f.WriteString(fmt::format("  {0:08x} {1:06x} {2:08x} {3:2} {4}\n", symbol->address,
                              symbol->size, symbol->address, 0, symbol->name));
  }
}

// Returns false if the Machine State Register is in a bad state.
// Virtual address translation is required for saving code maps.
// File::IOFile must be in "w" mode.
bool PPCSymbolDB::HostSaveCodeMap(File::IOFile& f) const
{
  bool retval = true;
  Core::RunAsCPUThread([this, &retval, &f] {
    if (!MSR.DR || !MSR.IR)
    {
      retval = false;
      return;
    }
    SaveCodeMap(f);
  });
  return retval;
}

// Save code map
// Notes:
//  - Dolphin doesn't load back code maps
//  - It's a custom code map format
// File::IOFile must be in "w" mode.
void PPCSymbolDB::SaveCodeMap(File::IOFile& f) const
{
  constexpr int SYMBOL_NAME_LIMIT = 30;

  // Write ".text" at the top
  f.WriteString(".text\n");

  u32 next_address = 0;
  for (const auto& function : m_functions)
  {
    const Common::Symbol& symbol = function.second;

    // Skip functions which are inside bigger functions
    if (symbol.address + symbol.size <= next_address)
    {
      // At least write the symbol name and address
      f.WriteString(fmt::format("// {0:08x} beginning of {1}\n", symbol.address, symbol.name));
      continue;
    }

    // Write the symbol full name
    f.WriteString(fmt::format("\n{0}:\n", symbol.name));
    next_address = symbol.address + symbol.size;

    // Write the code
    for (u32 address = symbol.address; address < next_address; address += 4)
    {
      const std::string disasm = debugger->Disassemble(address);
      f.WriteString(fmt::format("{0:08x} {1:<{2}.{3}} {4}\n", address, symbol.name,
                                SYMBOL_NAME_LIMIT, SYMBOL_NAME_LIMIT, disasm));
    }
  }
}
