// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// -----------------------------------------------------------------------------------------
// Partial Action Replay code system implementation.
// Will never be able to support some AR codes - specifically those that patch the running
// Action Replay engine itself - yes they do exist!!!
// Action Replay actually is a small virtual machine with a limited number of commands.
// It probably is Turing complete - but what does that matter when AR codes can write
// actual PowerPC code...
// -----------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------------------------------
// Code Types:
// (Unconditional) Normal Codes (0): this one has subtypes inside
// (Conditional) Normal Codes (1 - 7): these just compare values and set the line skip info
// Zero Codes: any code with no address.  These codes are used to do special operations like memory
// copy, etc
// -------------------------------------------------------------------------------------------------------------

#include "Core/ActionReplay.h"

#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <iterator>
#include <mutex>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/IniFile.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"

#include "Core/ARDecrypt.h"
#include "Core/ConfigManager.h"
#include "Core/PowerPC/PowerPC.h"

namespace ActionReplay
{
enum
{
  // Zero Code Types
  ZCODE_END = 0x00,
  ZCODE_NORM = 0x02,
  ZCODE_ROW = 0x03,
  ZCODE_04 = 0x04,

  // Conditional Codes
  CONDTIONAL_EQUAL = 0x01,
  CONDTIONAL_NOT_EQUAL = 0x02,
  CONDTIONAL_LESS_THAN_SIGNED = 0x03,
  CONDTIONAL_GREATER_THAN_SIGNED = 0x04,
  CONDTIONAL_LESS_THAN_UNSIGNED = 0x05,
  CONDTIONAL_GREATER_THAN_UNSIGNED = 0x06,
  CONDTIONAL_AND = 0x07,  // bitwise AND

  // Conditional Line Counts
  CONDTIONAL_ONE_LINE = 0x00,
  CONDTIONAL_TWO_LINES = 0x01,
  CONDTIONAL_ALL_LINES_UNTIL = 0x02,
  CONDTIONAL_ALL_LINES = 0x03,

  // Data Types
  DATATYPE_8BIT = 0x00,
  DATATYPE_16BIT = 0x01,
  DATATYPE_32BIT = 0x02,
  DATATYPE_32BIT_FLOAT = 0x03,

  // Normal Code 0 Subtypes
  SUB_RAM_WRITE = 0x00,
  SUB_WRITE_POINTER = 0x01,
  SUB_ADD_CODE = 0x02,
  SUB_MASTER_CODE = 0x03,
};

// General lock. Protects codes list and internal log.
static std::mutex s_lock;
static std::vector<ARCode> s_active_codes;
static std::vector<std::string> s_internal_log;
static std::atomic<bool> s_use_internal_log{false};
// pointer to the code currently being run, (used by log messages that include the code name)
static const ARCode* s_current_code = nullptr;
static bool s_disable_logging = false;

struct ARAddr
{
  union
  {
    u32 address;
    struct
    {
      u32 gcaddr : 25;
      u32 size : 2;
      u32 type : 3;
      u32 subtype : 2;
    };
  };

  ARAddr(const u32 addr) : address(addr) {}
  u32 GCAddress() const { return gcaddr | 0x80000000; }
  operator u32() const { return address; }
};

// ----------------------
// AR Remote Functions
void ApplyCodes(const std::vector<ARCode>& codes)
{
  if (!SConfig::GetInstance().bEnableCheats)
    return;

  std::lock_guard<std::mutex> guard(s_lock);
  s_disable_logging = false;
  s_active_codes.clear();
  std::copy_if(codes.begin(), codes.end(), std::back_inserter(s_active_codes),
               [](const ARCode& code) { return code.active; });
  s_active_codes.shrink_to_fit();
}

void AddCode(ARCode code)
{
  if (!SConfig::GetInstance().bEnableCheats)
    return;

  if (code.active)
  {
    std::lock_guard<std::mutex> guard(s_lock);
    s_disable_logging = false;
    s_active_codes.emplace_back(std::move(code));
  }
}

void LoadAndApplyCodes(const IniFile& global_ini, const IniFile& local_ini)
{
  ApplyCodes(LoadCodes(global_ini, local_ini));
}

// Parses the Action Replay section of a game ini file.
std::vector<ARCode> LoadCodes(const IniFile& global_ini, const IniFile& local_ini)
{
  std::vector<ARCode> codes;

  std::unordered_set<std::string> enabled_names;
  {
    std::vector<std::string> enabled_lines;
    local_ini.GetLines("ActionReplay_Enabled", &enabled_lines);
    for (const std::string& line : enabled_lines)
    {
      if (line.size() != 0 && line[0] == '$')
      {
        std::string name = line.substr(1, line.size() - 1);
        enabled_names.insert(name);
      }
    }
  }

  const IniFile* inis[2] = {&global_ini, &local_ini};
  for (const IniFile* ini : inis)
  {
    std::vector<std::string> lines;
    std::vector<std::string> encrypted_lines;
    ARCode current_code;

    ini->GetLines("ActionReplay", &lines);

    for (const std::string& line : lines)
    {
      if (line.empty())
      {
        continue;
      }

      // Check if the line is a name of the code
      if (line[0] == '$')
      {
        if (current_code.ops.size())
        {
          codes.push_back(current_code);
          current_code.ops.clear();
        }
        if (encrypted_lines.size())
        {
          DecryptARCode(encrypted_lines, &current_code.ops);
          codes.push_back(current_code);
          current_code.ops.clear();
          encrypted_lines.clear();
        }

        current_code.name = line.substr(1, line.size() - 1);
        current_code.active = enabled_names.find(current_code.name) != enabled_names.end();
        current_code.user_defined = (ini == &local_ini);
      }
      else
      {
        std::vector<std::string> pieces = SplitString(line, ' ');

        // Check if the AR code is decrypted
        if (pieces.size() == 2 && pieces[0].size() == 8 && pieces[1].size() == 8)
        {
          AREntry op;
          bool success_addr = TryParse(std::string("0x") + pieces[0], &op.cmd_addr);
          bool success_val = TryParse(std::string("0x") + pieces[1], &op.value);

          if (success_addr && success_val)
          {
            current_code.ops.push_back(op);
          }
          else
          {
            PanicAlertT("Action Replay Error: invalid AR code line: %s", line.c_str());

            if (!success_addr)
              PanicAlertT("The address is invalid");

            if (!success_val)
              PanicAlertT("The value is invalid");
          }
        }
        else
        {
          pieces = SplitString(line, '-');
          if (pieces.size() == 3 && pieces[0].size() == 4 && pieces[1].size() == 4 &&
              pieces[2].size() == 5)
          {
            // Encrypted AR code
            // Decryption is done in "blocks", so we must push blocks into a vector,
            // then send to decrypt when a new block is encountered, or if it's the last block.
            encrypted_lines.emplace_back(pieces[0] + pieces[1] + pieces[2]);
          }
        }
      }
    }

    // Handle the last code correctly.
    if (current_code.ops.size())
    {
      codes.push_back(current_code);
    }
    if (encrypted_lines.size())
    {
      DecryptARCode(encrypted_lines, &current_code.ops);
      codes.push_back(current_code);
    }
  }

  return codes;
}

void SaveCodes(IniFile* local_ini, const std::vector<ARCode>& codes)
{
  std::vector<std::string> lines;
  std::vector<std::string> enabled_lines;
  for (const ActionReplay::ARCode& code : codes)
  {
    if (code.active)
      enabled_lines.emplace_back("$" + code.name);

    if (code.user_defined)
    {
      lines.emplace_back("$" + code.name);
      for (const ActionReplay::AREntry& op : code.ops)
      {
        lines.emplace_back(StringFromFormat("%08X %08X", op.cmd_addr, op.value));
      }
    }
  }
  local_ini->SetLines("ActionReplay_Enabled", enabled_lines);
  local_ini->SetLines("ActionReplay", lines);
}

static void LogInfo(const char* format, ...)
{
  if (s_disable_logging)
    return;
  bool use_internal_log = s_use_internal_log.load(std::memory_order_relaxed);
  if (MAX_LOGLEVEL < LogTypes::LINFO && !use_internal_log)
    return;

  va_list args;
  va_start(args, format);
  std::string text = StringFromFormatV(format, args);
  va_end(args);
  INFO_LOG(ACTIONREPLAY, "%s", text.c_str());

  if (use_internal_log)
  {
    text += '\n';
    s_internal_log.emplace_back(std::move(text));
  }
}

void EnableSelfLogging(bool enable)
{
  s_use_internal_log.store(enable, std::memory_order_relaxed);
}

std::vector<std::string> GetSelfLog()
{
  std::lock_guard<std::mutex> guard(s_lock);
  return s_internal_log;
}

void ClearSelfLog()
{
  std::lock_guard<std::mutex> guard(s_lock);
  s_internal_log.clear();
}

bool IsSelfLogging()
{
  return s_use_internal_log.load(std::memory_order_relaxed);
}

// ----------------------
// Code Functions
static bool Subtype_RamWriteAndFill(const ARAddr& addr, const u32 data)
{
  const u32 new_addr = addr.GCAddress();

  LogInfo("Hardware Address: %08x", new_addr);
  LogInfo("Size: %08x", addr.size);

  switch (addr.size)
  {
  case DATATYPE_8BIT:
  {
    LogInfo("8-bit Write");
    LogInfo("--------");
    u32 repeat = data >> 8;
    for (u32 i = 0; i <= repeat; ++i)
    {
      PowerPC::HostWrite_U8(data & 0xFF, new_addr + i);
      LogInfo("Wrote %08x to address %08x", data & 0xFF, new_addr + i);
    }
    LogInfo("--------");
    break;
  }

  case DATATYPE_16BIT:
  {
    LogInfo("16-bit Write");
    LogInfo("--------");
    u32 repeat = data >> 16;
    for (u32 i = 0; i <= repeat; ++i)
    {
      PowerPC::HostWrite_U16(data & 0xFFFF, new_addr + i * 2);
      LogInfo("Wrote %08x to address %08x", data & 0xFFFF, new_addr + i * 2);
    }
    LogInfo("--------");
    break;
  }

  case DATATYPE_32BIT_FLOAT:
  case DATATYPE_32BIT:  // Dword write
    LogInfo("32-bit Write");
    LogInfo("--------");
    PowerPC::HostWrite_U32(data, new_addr);
    LogInfo("Wrote %08x to address %08x", data, new_addr);
    LogInfo("--------");
    break;

  default:
    LogInfo("Bad Size");
    PanicAlertT("Action Replay Error: Invalid size "
                "(%08x : address = %08x) in Ram Write And Fill (%s)",
                addr.size, addr.gcaddr, s_current_code->name.c_str());
    return false;
  }

  return true;
}

static bool Subtype_WriteToPointer(const ARAddr& addr, const u32 data)
{
  const u32 new_addr = addr.GCAddress();
  const u32 ptr = PowerPC::HostRead_U32(new_addr);

  LogInfo("Hardware Address: %08x", new_addr);
  LogInfo("Size: %08x", addr.size);

  switch (addr.size)
  {
  case DATATYPE_8BIT:
  {
    LogInfo("Write 8-bit to pointer");
    LogInfo("--------");
    const u8 thebyte = data & 0xFF;
    const u32 offset = data >> 8;
    LogInfo("Pointer: %08x", ptr);
    LogInfo("Byte: %08x", thebyte);
    LogInfo("Offset: %08x", offset);
    PowerPC::HostWrite_U8(thebyte, ptr + offset);
    LogInfo("Wrote %08x to address %08x", thebyte, ptr + offset);
    LogInfo("--------");
    break;
  }

  case DATATYPE_16BIT:
  {
    LogInfo("Write 16-bit to pointer");
    LogInfo("--------");
    const u16 theshort = data & 0xFFFF;
    const u32 offset = (data >> 16) << 1;
    LogInfo("Pointer: %08x", ptr);
    LogInfo("Byte: %08x", theshort);
    LogInfo("Offset: %08x", offset);
    PowerPC::HostWrite_U16(theshort, ptr + offset);
    LogInfo("Wrote %08x to address %08x", theshort, ptr + offset);
    LogInfo("--------");
    break;
  }

  case DATATYPE_32BIT_FLOAT:
  case DATATYPE_32BIT:
    LogInfo("Write 32-bit to pointer");
    LogInfo("--------");
    PowerPC::HostWrite_U32(data, ptr);
    LogInfo("Wrote %08x to address %08x", data, ptr);
    LogInfo("--------");
    break;

  default:
    LogInfo("Bad Size");
    PanicAlertT("Action Replay Error: Invalid size "
                "(%08x : address = %08x) in Write To Pointer (%s)",
                addr.size, addr.gcaddr, s_current_code->name.c_str());
    return false;
  }
  return true;
}

static bool Subtype_AddCode(const ARAddr& addr, const u32 data)
{
  // Used to increment/decrement a value in memory
  const u32 new_addr = addr.GCAddress();

  LogInfo("Hardware Address: %08x", new_addr);
  LogInfo("Size: %08x", addr.size);

  switch (addr.size)
  {
  case DATATYPE_8BIT:
    LogInfo("8-bit Add");
    LogInfo("--------");
    PowerPC::HostWrite_U8(PowerPC::HostRead_U8(new_addr) + data, new_addr);
    LogInfo("Wrote %08x to address %08x", PowerPC::HostRead_U8(new_addr) + (data & 0xFF), new_addr);
    LogInfo("--------");
    break;

  case DATATYPE_16BIT:
    LogInfo("16-bit Add");
    LogInfo("--------");
    PowerPC::HostWrite_U16(PowerPC::HostRead_U16(new_addr) + data, new_addr);
    LogInfo("Wrote %08x to address %08x", PowerPC::HostRead_U16(new_addr) + (data & 0xFFFF),
            new_addr);
    LogInfo("--------");
    break;

  case DATATYPE_32BIT:
    LogInfo("32-bit Add");
    LogInfo("--------");
    PowerPC::HostWrite_U32(PowerPC::HostRead_U32(new_addr) + data, new_addr);
    LogInfo("Wrote %08x to address %08x", PowerPC::HostRead_U32(new_addr) + data, new_addr);
    LogInfo("--------");
    break;

  case DATATYPE_32BIT_FLOAT:
  {
    LogInfo("32-bit floating Add");
    LogInfo("--------");

    const u32 read = PowerPC::HostRead_U32(new_addr);
    const float read_float = reinterpret_cast<const float&>(read);
    // data contains an (unsigned?) integer value
    const float fread = read_float + static_cast<float>(data);
    const u32 newval = reinterpret_cast<const u32&>(fread);
    PowerPC::HostWrite_U32(newval, new_addr);
    LogInfo("Old Value %08x", read);
    LogInfo("Increment %08x", data);
    LogInfo("New value %08x", newval);
    LogInfo("--------");
    break;
  }

  default:
    LogInfo("Bad Size");
    PanicAlertT("Action Replay Error: Invalid size "
                "(%08x : address = %08x) in Add Code (%s)",
                addr.size, addr.gcaddr, s_current_code->name.c_str());
    return false;
  }
  return true;
}

static bool Subtype_MasterCodeAndWriteToCCXXXXXX(const ARAddr& addr, const u32 data)
{
  // code not yet implemented - TODO
  // u32 new_addr = (addr & 0x01FFFFFF) | 0x80000000;
  // u8  mcode_type = (data & 0xFF0000) >> 16;
  // u8  mcode_count = (data & 0xFF00) >> 8;
  // u8  mcode_number = data & 0xFF;
  PanicAlertT("Action Replay Error: Master Code and Write To CCXXXXXX not implemented (%s)\n"
              "Master codes are not needed. Do not use master codes.",
              s_current_code->name.c_str());
  return false;
}

// This needs more testing
static bool ZeroCode_FillAndSlide(const u32 val_last, const ARAddr& addr, const u32 data)
{
  const u32 new_addr = ARAddr(val_last).GCAddress();
  const u8 size = ARAddr(val_last).size;

  const s16 addr_incr = static_cast<s16>(data & 0xFFFF);
  const s8 val_incr = static_cast<s8>(data >> 24);
  const u8 write_num = static_cast<u8>((data & 0xFF0000) >> 16);

  u32 val = addr;
  u32 curr_addr = new_addr;

  LogInfo("Current Hardware Address: %08x", new_addr);
  LogInfo("Size: %08x", addr.size);
  LogInfo("Write Num: %08x", write_num);
  LogInfo("Address Increment: %i", addr_incr);
  LogInfo("Value Increment: %i", val_incr);

  switch (size)
  {
  case DATATYPE_8BIT:
    LogInfo("8-bit Write");
    LogInfo("--------");
    for (int i = 0; i < write_num; ++i)
    {
      PowerPC::HostWrite_U8(val & 0xFF, curr_addr);
      curr_addr += addr_incr;
      val += val_incr;
      LogInfo("Write %08x to address %08x", val & 0xFF, curr_addr);

      LogInfo("Value Update: %08x", val);
      LogInfo("Current Hardware Address Update: %08x", curr_addr);
    }
    LogInfo("--------");
    break;

  case DATATYPE_16BIT:
    LogInfo("16-bit Write");
    LogInfo("--------");
    for (int i = 0; i < write_num; ++i)
    {
      PowerPC::HostWrite_U16(val & 0xFFFF, curr_addr);
      LogInfo("Write %08x to address %08x", val & 0xFFFF, curr_addr);
      curr_addr += addr_incr * 2;
      val += val_incr;
      LogInfo("Value Update: %08x", val);
      LogInfo("Current Hardware Address Update: %08x", curr_addr);
    }
    LogInfo("--------");
    break;

  case DATATYPE_32BIT:
    LogInfo("32-bit Write");
    LogInfo("--------");
    for (int i = 0; i < write_num; ++i)
    {
      PowerPC::HostWrite_U32(val, curr_addr);
      LogInfo("Write %08x to address %08x", val, curr_addr);
      curr_addr += addr_incr * 4;
      val += val_incr;
      LogInfo("Value Update: %08x", val);
      LogInfo("Current Hardware Address Update: %08x", curr_addr);
    }
    LogInfo("--------");
    break;

  default:
    LogInfo("Bad Size");
    PanicAlertT("Action Replay Error: Invalid size (%08x : address = %08x) in Fill and Slide (%s)",
                size, new_addr, s_current_code->name.c_str());
    return false;
  }
  return true;
}

// Looks like this is new?? - untested
static bool ZeroCode_MemoryCopy(const u32 val_last, const ARAddr& addr, const u32 data)
{
  const u32 addr_dest = val_last | 0x06000000;
  const u32 addr_src = addr.GCAddress();

  const u8 num_bytes = data & 0x7FFF;

  LogInfo("Dest Address: %08x", addr_dest);
  LogInfo("Src Address: %08x", addr_src);
  LogInfo("Size: %08x", num_bytes);

  if ((data & ~0x7FFF) == 0x0000)
  {
    if ((data >> 24) != 0x0)
    {  // Memory Copy With Pointers Support
      LogInfo("Memory Copy With Pointers Support");
      LogInfo("--------");
      for (int i = 0; i < 138; ++i)
      {
        PowerPC::HostWrite_U8(PowerPC::HostRead_U8(addr_src + i), addr_dest + i);
        LogInfo("Wrote %08x to address %08x", PowerPC::HostRead_U8(addr_src + i), addr_dest + i);
      }
      LogInfo("--------");
    }
    else
    {  // Memory Copy Without Pointer Support
      LogInfo("Memory Copy Without Pointers Support");
      LogInfo("--------");
      for (int i = 0; i < num_bytes; ++i)
      {
        PowerPC::HostWrite_U8(PowerPC::HostRead_U8(addr_src + i), addr_dest + i);
        LogInfo("Wrote %08x to address %08x", PowerPC::HostRead_U8(addr_src + i), addr_dest + i);
      }
      LogInfo("--------");
      return true;
    }
  }
  else
  {
    LogInfo("Bad Value");
    PanicAlertT("Action Replay Error: Invalid value (%08x) in Memory Copy (%s)", (data & ~0x7FFF),
                s_current_code->name.c_str());
    return false;
  }
  return true;
}

static bool NormalCode(const ARAddr& addr, const u32 data)
{
  switch (addr.subtype)
  {
  case SUB_RAM_WRITE:  // Ram write (and fill)
    LogInfo("Doing Ram Write And Fill");
    if (!Subtype_RamWriteAndFill(addr, data))
      return false;
    break;

  case SUB_WRITE_POINTER:  // Write to pointer
    LogInfo("Doing Write To Pointer");
    if (!Subtype_WriteToPointer(addr, data))
      return false;
    break;

  case SUB_ADD_CODE:  // Increment Value
    LogInfo("Doing Add Code");
    if (!Subtype_AddCode(addr, data))
      return false;
    break;

  case SUB_MASTER_CODE:  // Master Code & Write to CCXXXXXX
    LogInfo("Doing Master Code And Write to CCXXXXXX (ncode not supported)");
    if (!Subtype_MasterCodeAndWriteToCCXXXXXX(addr, data))
      return false;
    break;

  default:
    LogInfo("Bad Subtype");
    PanicAlertT("Action Replay: Normal Code 0: Invalid Subtype %08x (%s)", addr.subtype,
                s_current_code->name.c_str());
    return false;
  }

  return true;
}

static bool CompareValues(const u32 val1, const u32 val2, const int type)
{
  switch (type)
  {
  case CONDTIONAL_EQUAL:
    LogInfo("Type 1: If Equal");
    return val1 == val2;

  case CONDTIONAL_NOT_EQUAL:
    LogInfo("Type 2: If Not Equal");
    return val1 != val2;

  case CONDTIONAL_LESS_THAN_SIGNED:
    LogInfo("Type 3: If Less Than (Signed)");
    return static_cast<s32>(val1) < static_cast<s32>(val2);

  case CONDTIONAL_GREATER_THAN_SIGNED:
    LogInfo("Type 4: If Greater Than (Signed)");
    return static_cast<s32>(val1) > static_cast<s32>(val2);

  case CONDTIONAL_LESS_THAN_UNSIGNED:
    LogInfo("Type 5: If Less Than (Unsigned)");
    return val1 < val2;

  case CONDTIONAL_GREATER_THAN_UNSIGNED:
    LogInfo("Type 6: If Greater Than (Unsigned)");
    return val1 > val2;

  case CONDTIONAL_AND:
    LogInfo("Type 7: If And");
    return !!(val1 & val2);  // bitwise AND

  default:
    LogInfo("Unknown Compare type");
    PanicAlertT("Action Replay: Invalid Normal Code Type %08x (%s)", type,
                s_current_code->name.c_str());
    return false;
  }
}

static bool ConditionalCode(const ARAddr& addr, const u32 data, int* const pSkipCount)
{
  const u32 new_addr = addr.GCAddress();

  LogInfo("Size: %08x", addr.size);
  LogInfo("Hardware Address: %08x", new_addr);

  bool result = true;

  switch (addr.size)
  {
  case DATATYPE_8BIT:
    result = CompareValues(PowerPC::HostRead_U8(new_addr), (data & 0xFF), addr.type);
    break;

  case DATATYPE_16BIT:
    result = CompareValues(PowerPC::HostRead_U16(new_addr), (data & 0xFFFF), addr.type);
    break;

  case DATATYPE_32BIT_FLOAT:
  case DATATYPE_32BIT:
    result = CompareValues(PowerPC::HostRead_U32(new_addr), data, addr.type);
    break;

  default:
    LogInfo("Bad Size");
    PanicAlertT("Action Replay: Conditional Code: Invalid Size %08x (%s)", addr.size,
                s_current_code->name.c_str());
    return false;
  }

  // if the comparison failed we need to skip some lines
  if (false == result)
  {
    switch (addr.subtype)
    {
    case CONDTIONAL_ONE_LINE:
    case CONDTIONAL_TWO_LINES:
      *pSkipCount = addr.subtype + 1;  // Skip 1 or 2 lines
      break;

    // Skip all lines,
    // Skip lines until a "00000000 40000000" line is reached
    case CONDTIONAL_ALL_LINES:
    case CONDTIONAL_ALL_LINES_UNTIL:
      *pSkipCount = -static_cast<int>(addr.subtype);
      break;

    default:
      LogInfo("Bad Subtype");
      PanicAlertT("Action Replay: Normal Code %i: Invalid subtype %08x (%s)", 1, addr.subtype,
                  s_current_code->name.c_str());
      return false;
    }
  }

  return true;
}

// NOTE: Lock needed to give mutual exclusion to s_current_code and LogInfo
static bool RunCodeLocked(const ARCode& arcode)
{
  // The mechanism is different than what the real AR uses, so there may be compatibility problems.

  bool do_fill_and_slide = false;
  bool do_memory_copy = false;

  // used for conditional codes
  int skip_count = 0;

  u32 val_last = 0;

  s_current_code = &arcode;

  LogInfo("Code Name: %s", arcode.name.c_str());
  LogInfo("Number of codes: %zu", arcode.ops.size());

  for (const AREntry& entry : arcode.ops)
  {
    const ARAddr addr(entry.cmd_addr);
    const u32 data = entry.value;

    // after a conditional code, skip lines if needed
    if (skip_count)
    {
      if (skip_count > 0)  // skip x lines
      {
        LogInfo("Line skipped");
        --skip_count;
      }
      else if (-CONDTIONAL_ALL_LINES == skip_count)
      {
        // skip all lines
        LogInfo("All Lines skipped");
        return true;  // don't need to iterate through the rest of the ops
      }
      else if (-CONDTIONAL_ALL_LINES_UNTIL == skip_count)
      {
        // skip until a "00000000 40000000" line is reached
        LogInfo("Line skipped");
        if (addr == 0 && 0x40000000 == data)  // check for an endif line
          skip_count = 0;
      }

      continue;
    }

    LogInfo("--- Running Code: %08x %08x ---", addr.address, data);
    // LogInfo("Command: %08x", cmd);

    // Do Fill & Slide
    if (do_fill_and_slide)
    {
      do_fill_and_slide = false;
      LogInfo("Doing Fill And Slide");
      if (false == ZeroCode_FillAndSlide(val_last, addr, data))
        return false;
      continue;
    }

    // Memory Copy
    if (do_memory_copy)
    {
      do_memory_copy = false;
      LogInfo("Doing Memory Copy");
      if (false == ZeroCode_MemoryCopy(val_last, addr, data))
        return false;
      continue;
    }

    // ActionReplay program self modification codes
    if (addr >= 0x00002000 && addr < 0x00003000)
    {
      LogInfo(
          "This action replay simulator does not support codes that modify Action Replay itself.");
      PanicAlertT(
          "This action replay simulator does not support codes that modify Action Replay itself.");
      return false;
    }

    // skip these weird init lines
    // TODO: Where are the "weird init lines"?
    // if (iter == code.ops.begin() && cmd == 1)
    // continue;

    // Zero codes
    if (0x0 == addr)  // Check if the code is a zero code
    {
      const u8 zcode = data >> 29;

      LogInfo("Doing Zero Code %08x", zcode);

      switch (zcode)
      {
      case ZCODE_END:  // END OF CODES
        LogInfo("ZCode: End Of Codes");
        return true;

      // TODO: the "00000000 40000000"(end if) codes fall into this case, I don't think that is
      // correct
      case ZCODE_NORM:  // Normal execution of codes
        // Todo: Set register 1BB4 to 0
        LogInfo("ZCode: Normal execution of codes, set register 1BB4 to 0 (zcode not supported)");
        break;

      case ZCODE_ROW:  // Executes all codes in the same row
        // Todo: Set register 1BB4 to 1
        LogInfo("ZCode: Executes all codes in the same row, Set register 1BB4 to 1 (zcode not "
                "supported)");
        PanicAlertT("Zero 3 code not supported");
        return false;

      case ZCODE_04:  // Fill & Slide or Memory Copy
        if (0x3 == ((data >> 25) & 0x03))
        {
          LogInfo("ZCode: Memory Copy");
          do_memory_copy = true;
          val_last = data;
        }
        else
        {
          LogInfo("ZCode: Fill And Slide");
          do_fill_and_slide = true;
          val_last = data;
        }
        break;

      default:
        LogInfo("ZCode: Unknown");
        PanicAlertT("Zero code unknown to Dolphin: %08x", zcode);
        return false;
      }

      // done handling zero codes
      continue;
    }

    // Normal codes
    LogInfo("Doing Normal Code %08x", addr.type);
    LogInfo("Subtype: %08x", addr.subtype);

    switch (addr.type)
    {
    case 0x00:
      if (false == NormalCode(addr, data))
        return false;
      break;

    default:
      LogInfo("This Normal Code is a Conditional Code");
      if (false == ConditionalCode(addr, data, &skip_count))
        return false;
      break;
    }
  }

  return true;
}

void RunAllActive()
{
  if (!SConfig::GetInstance().bEnableCheats)
    return;

  // If the mutex is idle then acquiring it should be cheap, fast mutexes
  // are only atomic ops unless contested. It should be rare for this to
  // be contested.
  std::lock_guard<std::mutex> guard(s_lock);
  s_active_codes.erase(std::remove_if(s_active_codes.begin(), s_active_codes.end(),
                                      [](const ARCode& code) {
                                        bool success = RunCodeLocked(code);
                                        LogInfo("\n");
                                        return !success;
                                      }),
                       s_active_codes.end());
  s_disable_logging = true;
}

}  // namespace ActionReplay
