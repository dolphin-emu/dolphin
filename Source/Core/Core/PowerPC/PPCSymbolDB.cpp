// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/PPCSymbolDB.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/SignatureDB/SignatureDB.h"

PPCSymbolDB g_symbolDB;

PPCSymbolDB::PPCSymbolDB()
{
  // Get access to the disasm() fgnction
  debugger = &PowerPC::debug_interface;
}

PPCSymbolDB::~PPCSymbolDB()
{
}

// Adds the function to the list, unless it's already there
Symbol* PPCSymbolDB::AddFunction(u32 start_addr)
{
  // It's already in the list
  if (functions.find(start_addr) != functions.end())
    return nullptr;

  Symbol symbol;
  if (!PPCAnalyst::AnalyzeFunction(start_addr, symbol))
    return nullptr;

  functions[start_addr] = std::move(symbol);
  Symbol* ptr = &functions[start_addr];
  ptr->type = Symbol::Type::Function;
  checksumToFunction[ptr->hash].insert(ptr);
  return ptr;
}

void PPCSymbolDB::AddKnownSymbol(u32 startAddr, u32 size, const std::string& name,
                                 Symbol::Type type)
{
  XFuncMap::iterator iter = functions.find(startAddr);
  if (iter != functions.end())
  {
    // already got it, let's just update name, checksum & size to be sure.
    Symbol* tempfunc = &iter->second;
    tempfunc->Rename(name);
    tempfunc->hash = HashSignatureDB::ComputeCodeChecksum(startAddr, startAddr + size - 4);
    tempfunc->type = type;
    tempfunc->size = size;
  }
  else
  {
    // new symbol. run analyze.
    Symbol tf;
    tf.Rename(name);
    tf.type = type;
    tf.address = startAddr;
    if (tf.type == Symbol::Type::Function)
    {
      PPCAnalyst::AnalyzeFunction(startAddr, tf, size);
      checksumToFunction[tf.hash].insert(&functions[startAddr]);
    }
    tf.size = size;
    functions[startAddr] = tf;
  }
}

Symbol* PPCSymbolDB::GetSymbolFromAddr(u32 addr)
{
  XFuncMap::iterator it = functions.find(addr);
  if (it != functions.end())
  {
    return &it->second;
  }
  else
  {
    for (auto& p : functions)
    {
      if (addr >= p.second.address && addr < p.second.address + p.second.size)
        return &p.second;
    }
  }
  return nullptr;
}

std::string PPCSymbolDB::GetDescription(u32 addr)
{
  Symbol* symbol = GetSymbolFromAddr(addr);
  if (symbol)
    return symbol->name;
  else
    return " --- ";
}

void PPCSymbolDB::FillInCallers()
{
  for (auto& p : functions)
  {
    p.second.callers.clear();
  }

  for (auto& entry : functions)
  {
    Symbol& f = entry.second;
    for (const SCall& call : f.calls)
    {
      SCall NewCall(entry.first, call.callAddress);
      u32 FunctionAddress = call.function;

      XFuncMap::iterator FuncIterator = functions.find(FunctionAddress);
      if (FuncIterator != functions.end())
      {
        Symbol& rCalledFunction = FuncIterator->second;
        rCalledFunction.callers.push_back(NewCall);
      }
      else
      {
        // LOG(OSHLE, "FillInCallers tries to fill data in an unknown function 0x%08x.",
        // FunctionAddress);
        // TODO - analyze the function instead.
      }
    }
  }
}

void PPCSymbolDB::PrintCalls(u32 funcAddr) const
{
  XFuncMap::const_iterator iter = functions.find(funcAddr);
  if (iter != functions.end())
  {
    const Symbol& f = iter->second;
    DEBUG_LOG(OSHLE, "The function %s at %08x calls:", f.name.c_str(), f.address);
    for (const SCall& call : f.calls)
    {
      XFuncMap::const_iterator n = functions.find(call.function);
      if (n != functions.end())
      {
        DEBUG_LOG(CONSOLE, "* %08x : %s", call.callAddress, n->second.name.c_str());
      }
    }
  }
  else
  {
    WARN_LOG(CONSOLE, "Symbol does not exist");
  }
}

void PPCSymbolDB::PrintCallers(u32 funcAddr) const
{
  XFuncMap::const_iterator iter = functions.find(funcAddr);
  if (iter != functions.end())
  {
    const Symbol& f = iter->second;
    DEBUG_LOG(CONSOLE, "The function %s at %08x is called by:", f.name.c_str(), f.address);
    for (const SCall& caller : f.callers)
    {
      XFuncMap::const_iterator n = functions.find(caller.function);
      if (n != functions.end())
      {
        DEBUG_LOG(CONSOLE, "* %08x : %s", caller.callAddress, n->second.name.c_str());
      }
    }
  }
}

void PPCSymbolDB::LogFunctionCall(u32 addr)
{
  // u32 from = PC;
  XFuncMap::iterator iter = functions.find(addr);
  if (iter != functions.end())
  {
    Symbol& f = iter->second;
    f.numCalls++;
  }
}

// The use case for handling bad map files is when you have a game with a map file on the disc,
// but you can't tell whether that map file is for the particular release version used in that game,
// or when you know that the map file is not for that build, but perhaps half the functions in the
// map file are still at the correct locations. Which are both common situations. It will load any
// function names and addresses that have a BLR before the start and at the end, but ignore any that
// don't, and then tell you how many were good and how many it ignored. That way you either find out
// it is all good and use it, find out it is partly good and use the good part, or find out that
// only
// a handful of functions lined up by coincidence and then you can clear the symbols. In the future
// I
// want to make it smarter, so it checks that there are no BLRs in the middle of the function
// (by checking the code length), and also make it cope with added functions in the middle or work
// based on the order of the functions and their approximate length. Currently that process has to
// be
// done manually and is very tedious.
// The use case for separate handling of map files that aren't bad is that you usually want to also
// load names that aren't functions(if included in the map file) without them being rejected as
// invalid.
// You can see discussion about these kinds of issues here :
// https://forums.oculus.com/viewtopic.php?f=42&t=11241&start=580
// https://m2k2.taigaforum.com/post/metroid_prime_hacking_help_25.html#metroid_prime_hacking_help_25

// This one can load both leftover map files on game discs (like Zelda), and mapfiles
// produced by SaveSymbolMap below.
// bad=true means carefully load map files that might not be from exactly the right version
bool PPCSymbolDB::LoadMap(const std::string& filename, bool bad)
{
  File::IOFile f(filename, "r");
  if (!f)
    return false;

  // four columns are used in American Mensa Academy map files and perhaps other games
  bool started = false, four_columns = false;
  int good_count = 0, bad_count = 0;

  char line[512];
  while (fgets(line, 512, f.GetHandle()))
  {
    size_t length = strlen(line);
    if (length < 4)
      continue;

    if (length == 34 && strcmp(line, "  address  Size   address  offset\n") == 0)
    {
      four_columns = true;
      continue;
    }

    char temp[256];
    sscanf(line, "%255s", temp);

    if (strcmp(temp, "UNUSED") == 0)
      continue;
    if (strcmp(temp, ".text") == 0)
    {
      started = true;
      continue;
    };
    if (strcmp(temp, ".init") == 0)
    {
      started = true;
      continue;
    };
    if (strcmp(temp, "Starting") == 0)
      continue;
    if (strcmp(temp, "extab") == 0)
      continue;
    if (strcmp(temp, ".ctors") == 0)
      break;  // uh?
    if (strcmp(temp, ".dtors") == 0)
      break;
    if (strcmp(temp, ".rodata") == 0)
      continue;
    if (strcmp(temp, ".data") == 0)
      continue;
    if (strcmp(temp, ".sbss") == 0)
      continue;
    if (strcmp(temp, ".sdata") == 0)
      continue;
    if (strcmp(temp, ".sdata2") == 0)
      continue;
    if (strcmp(temp, "address") == 0)
      continue;
    if (strcmp(temp, "-----------------------") == 0)
      continue;
    if (strcmp(temp, ".sbss2") == 0)
      break;
    if (temp[1] == ']')
      continue;

    if (!started)
      continue;

    u32 address, vaddress, size, offset, alignment;
    char name[512], container[512];
    if (four_columns)
    {
      // sometimes there is no alignment value, and sometimes it is because it is an entry of
      // something else
      if (length > 37 && line[37] == ' ')
      {
        alignment = 0;
        sscanf(line, "%08x %08x %08x %08x %511s", &address, &size, &vaddress, &offset, name);
        char* s = strstr(line, "(entry of ");
        if (s)
        {
          sscanf(s + 10, "%511s", container);
          char* s2 = (strchr(container, ')'));
          if (s2 && container[0] != '.')
          {
            s2[0] = '\0';
            strcat(container, "::");
            strcat(container, name);
            strcpy(name, container);
          }
        }
      }
      else
      {
        sscanf(line, "%08x %08x %08x %08x %i %511s", &address, &size, &vaddress, &offset,
               &alignment, name);
      }
    }
    // some entries in the table have a function name followed by " (entry of " followed by a
    // container name, followed by ")"
    // instead of a space followed by a number followed by a space followed by a name
    else if (length > 27 && line[27] != ' ' && strstr(line, "(entry of "))
    {
      alignment = 0;
      sscanf(line, "%08x %08x %08x %511s", &address, &size, &vaddress, name);
      char* s = strstr(line, "(entry of ");
      if (s)
      {
        sscanf(s + 10, "%511s", container);
        char* s2 = (strchr(container, ')'));
        if (s2 && container[0] != '.')
        {
          s2[0] = '\0';
          strcat(container, "::");
          strcat(container, name);
          strcpy(name, container);
        }
      }
    }
    else
    {
      sscanf(line, "%08x %08x %08x %i %511s", &address, &size, &vaddress, &alignment, name);
    }

    const char* namepos = strstr(line, name);
    if (namepos != nullptr)  // would be odd if not :P
      strcpy(name, namepos);
    name[strlen(name) - 1] = 0;
    if (name[strlen(name) - 1] == '\r')
      name[strlen(name) - 1] = 0;

    // Check if this is a valid entry.
    if (strcmp(name, ".text") != 0 && strcmp(name, ".init") != 0 && strlen(name) > 0)
    {
      // Can't compute the checksum if not in RAM
      bool good = !bad && PowerPC::HostIsInstructionRAMAddress(vaddress) &&
                  PowerPC::HostIsInstructionRAMAddress(vaddress + size - 4);
      if (!good)
      {
        // check for BLR before function
        PowerPC::TryReadInstResult read_result = PowerPC::TryReadInstruction(vaddress - 4);
        if (read_result.valid && read_result.hex == 0x4e800020)
        {
          // check for BLR at end of function
          read_result = PowerPC::TryReadInstruction(vaddress + size - 4);
          good = read_result.valid && read_result.hex == 0x4e800020;
        }
      }
      if (good)
      {
        ++good_count;
        AddKnownSymbol(vaddress, size, name);  // ST_FUNCTION
      }
      else
      {
        ++bad_count;
      }
    }
  }

  Index();
  if (bad)
    SuccessAlertT("Loaded %d good functions, ignored %d bad functions.", good_count, bad_count);
  return true;
}

// Save symbol map similar to CodeWarrior's map file
bool PPCSymbolDB::SaveSymbolMap(const std::string& filename) const
{
  File::IOFile f(filename, "w");
  if (!f)
    return false;

  // Write ".text" at the top
  fprintf(f.GetHandle(), ".text\n");

  // Write symbol address, size, virtual address, alignment, name
  for (const auto& function : functions)
  {
    const Symbol& symbol = function.second;
    fprintf(f.GetHandle(), "%08x %08x %08x %i %s\n", symbol.address, symbol.size, symbol.address, 0,
            symbol.name.c_str());
  }
  return true;
}

// Save code map (won't work if Core is running)
//
// Notes:
//  - Dolphin doesn't load back code maps
//  - It's a custom code map format
bool PPCSymbolDB::SaveCodeMap(const std::string& filename) const
{
  constexpr int SYMBOL_NAME_LIMIT = 30;
  File::IOFile f(filename, "w");
  if (!f)
    return false;

  // Write ".text" at the top
  fprintf(f.GetHandle(), ".text\n");

  u32 next_address = 0;
  for (const auto& function : functions)
  {
    const Symbol& symbol = function.second;

    // Skip functions which are inside bigger functions
    if (symbol.address + symbol.size <= next_address)
    {
      // At least write the symbol name and address
      fprintf(f.GetHandle(), "// %08x beginning of %s\n", symbol.address, symbol.name.c_str());
      continue;
    }

    // Write the symbol full name
    fprintf(f.GetHandle(), "\n%s:\n", symbol.name.c_str());
    next_address = symbol.address + symbol.size;

    // Write the code
    for (u32 address = symbol.address; address < next_address; address += 4)
    {
      const std::string disasm = debugger->Disassemble(address);
      fprintf(f.GetHandle(), "%08x %-*.*s %s\n", address, SYMBOL_NAME_LIMIT, SYMBOL_NAME_LIMIT,
              symbol.name.c_str(), disasm.c_str());
    }
  }
  return true;
}
