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

#include "Common/BitUtils.h"
#include "Common/CommonTypes.h"
#include "Common/IniFile.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"

#include "Core/ARDecrypt.h"
#include "Core/ConfigManager.h"
#include "Core/PowerPC/MMU.h"

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
static std::vector<ARCode> s_synced_codes;
// pointer to the code currently being run, (used by log messages that include the code name)
static const ARCode* s_current_code = nullptr;

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
  s_active_codes.clear();
  std::copy_if(codes.begin(), codes.end(), std::back_inserter(s_active_codes),
               [](const ARCode& code) { return code.active; });
  s_active_codes.shrink_to_fit();
}

void SetSyncedCodesAsActive()
{
  s_active_codes.clear();
  s_active_codes.reserve(s_synced_codes.size());
  s_active_codes = s_synced_codes;
}

void UpdateSyncedCodes(const std::vector<ARCode>& codes)
{
  s_synced_codes.clear();
  s_synced_codes.reserve(codes.size());
  std::copy_if(codes.begin(), codes.end(), std::back_inserter(s_synced_codes),
               [](const ARCode& code) { return code.active; });
  s_synced_codes.shrink_to_fit();
}

std::vector<ARCode> ApplyAndReturnCodes(const std::vector<ARCode>& codes)
{
  if (SConfig::GetInstance().bEnableCheats)
  {
    std::lock_guard<std::mutex> guard(s_lock);
    s_active_codes.clear();
    std::copy_if(codes.begin(), codes.end(), std::back_inserter(s_active_codes),
                 [](const ARCode& code) { return code.active; });
  }
  s_active_codes.shrink_to_fit();

  return s_active_codes;
}

void AddCode(ARCode code)
{
  if (!SConfig::GetInstance().bEnableCheats)
    return;

  if (code.active)
  {
    std::lock_guard<std::mutex> guard(s_lock);
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
      if (!line.empty() && line[0] == '$')
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
        if (!current_code.ops.empty())
        {
          codes.push_back(current_code);
          current_code.ops.clear();
        }
        if (!encrypted_lines.empty())
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
    if (!current_code.ops.empty())
    {
      codes.push_back(current_code);
    }
    if (!encrypted_lines.empty())
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

#if defined(ANDROID) && defined(DEBUG)
static void LOG_INFO(const char* format, ...)
{
  va_list args;
  va_start(args, format);
  std::string text = StringFromFormatV(format, args);
  va_end(args);
  __android_log_print(ANDROID_LOG_INFO, "ActionReplay", "%s", text.c_str());
}
#else
#define LOG_INFO(...)
#endif

// ----------------------
// Code Functions
static bool Subtype_RamWriteAndFill(const ARAddr& addr, const u32 data)
{
  const u32 new_addr = addr.GCAddress();

  LOG_INFO("Hardware Address: %08x", new_addr);
  LOG_INFO("Size: %08x", addr.size);

  switch (addr.size)
  {
  case DATATYPE_8BIT:
  {
    LOG_INFO("8-bit Write");
    LOG_INFO("--------");
    u32 repeat = data >> 8;
    for (u32 i = 0; i <= repeat; ++i)
    {
      PowerPC::HostWrite_U8(data & 0xFF, new_addr + i);
      LOG_INFO("Wrote %08x to address %08x", data & 0xFF, new_addr + i);
    }
    LOG_INFO("--------");
    break;
  }

  case DATATYPE_16BIT:
  {
    LOG_INFO("16-bit Write");
    LOG_INFO("--------");
    u32 repeat = data >> 16;
    for (u32 i = 0; i <= repeat; ++i)
    {
      PowerPC::HostWrite_U16(data & 0xFFFF, new_addr + i * 2);
      LOG_INFO("Wrote %08x to address %08x", data & 0xFFFF, new_addr + i * 2);
    }
    LOG_INFO("--------");
    break;
  }

  case DATATYPE_32BIT_FLOAT:
  case DATATYPE_32BIT:  // Dword write
    LOG_INFO("32-bit Write");
    LOG_INFO("--------");
    PowerPC::HostWrite_U32(data, new_addr);
    LOG_INFO("Wrote %08x to address %08x", data, new_addr);
    LOG_INFO("--------");
    break;

  default:
    LOG_INFO("Bad Size");
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

  LOG_INFO("Hardware Address: %08x", new_addr);
  LOG_INFO("Size: %08x", addr.size);

  switch (addr.size)
  {
  case DATATYPE_8BIT:
  {
    LOG_INFO("Write 8-bit to pointer");
    LOG_INFO("--------");
    const u8 thebyte = data & 0xFF;
    const u32 offset = data >> 8;
    LOG_INFO("Pointer: %08x", ptr);
    LOG_INFO("Byte: %08x", thebyte);
    LOG_INFO("Offset: %08x", offset);
    PowerPC::HostWrite_U8(thebyte, ptr + offset);
    LOG_INFO("Wrote %08x to address %08x", thebyte, ptr + offset);
    LOG_INFO("--------");
    break;
  }

  case DATATYPE_16BIT:
  {
    LOG_INFO("Write 16-bit to pointer");
    LOG_INFO("--------");
    const u16 theshort = data & 0xFFFF;
    const u32 offset = (data >> 16) << 1;
    LOG_INFO("Pointer: %08x", ptr);
    LOG_INFO("Byte: %08x", theshort);
    LOG_INFO("Offset: %08x", offset);
    PowerPC::HostWrite_U16(theshort, ptr + offset);
    LOG_INFO("Wrote %08x to address %08x", theshort, ptr + offset);
    LOG_INFO("--------");
    break;
  }

  case DATATYPE_32BIT_FLOAT:
  case DATATYPE_32BIT:
    LOG_INFO("Write 32-bit to pointer");
    LOG_INFO("--------");
    PowerPC::HostWrite_U32(data, ptr);
    LOG_INFO("Wrote %08x to address %08x", data, ptr);
    LOG_INFO("--------");
    break;

  default:
    LOG_INFO("Bad Size");
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

  LOG_INFO("Hardware Address: %08x", new_addr);
  LOG_INFO("Size: %08x", addr.size);

  switch (addr.size)
  {
  case DATATYPE_8BIT:
    LOG_INFO("8-bit Add");
    LOG_INFO("--------");
    PowerPC::HostWrite_U8(PowerPC::HostRead_U8(new_addr) + data, new_addr);
    LOG_INFO("Wrote %02x to address %08x", PowerPC::HostRead_U8(new_addr), new_addr);
    LOG_INFO("--------");
    break;

  case DATATYPE_16BIT:
    LOG_INFO("16-bit Add");
    LOG_INFO("--------");
    PowerPC::HostWrite_U16(PowerPC::HostRead_U16(new_addr) + data, new_addr);
    LOG_INFO("Wrote %04x to address %08x", PowerPC::HostRead_U16(new_addr), new_addr);
    LOG_INFO("--------");
    break;

  case DATATYPE_32BIT:
    LOG_INFO("32-bit Add");
    LOG_INFO("--------");
    PowerPC::HostWrite_U32(PowerPC::HostRead_U32(new_addr) + data, new_addr);
    LOG_INFO("Wrote %08x to address %08x", PowerPC::HostRead_U32(new_addr), new_addr);
    LOG_INFO("--------");
    break;

  case DATATYPE_32BIT_FLOAT:
  {
    LOG_INFO("32-bit floating Add");
    LOG_INFO("--------");

    const u32 read = PowerPC::HostRead_U32(new_addr);
    const float read_float = Common::BitCast<float>(read);
    // data contains an (unsigned?) integer value
    const float fread = read_float + static_cast<float>(data);
    const u32 newval = Common::BitCast<u32>(fread);
    PowerPC::HostWrite_U32(newval, new_addr);
    LOG_INFO("Old Value %08x", read);
    LOG_INFO("Increment %08x", data);
    LOG_INFO("New value %08x", newval);
    LOG_INFO("--------");
    break;
  }

  default:
    LOG_INFO("Bad Size");
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

  LOG_INFO("Current Hardware Address: %08x", new_addr);
  LOG_INFO("Size: %08x", addr.size);
  LOG_INFO("Write Num: %08x", write_num);
  LOG_INFO("Address Increment: %i", addr_incr);
  LOG_INFO("Value Increment: %i", val_incr);

  switch (size)
  {
  case DATATYPE_8BIT:
    LOG_INFO("8-bit Write");
    LOG_INFO("--------");
    for (int i = 0; i < write_num; ++i)
    {
      PowerPC::HostWrite_U8(val & 0xFF, curr_addr);
      curr_addr += addr_incr;
      val += val_incr;
      LOG_INFO("Write %08x to address %08x", val & 0xFF, curr_addr);

      LOG_INFO("Value Update: %08x", val);
      LOG_INFO("Current Hardware Address Update: %08x", curr_addr);
    }
    LOG_INFO("--------");
    break;

  case DATATYPE_16BIT:
    LOG_INFO("16-bit Write");
    LOG_INFO("--------");
    for (int i = 0; i < write_num; ++i)
    {
      PowerPC::HostWrite_U16(val & 0xFFFF, curr_addr);
      LOG_INFO("Write %08x to address %08x", val & 0xFFFF, curr_addr);
      curr_addr += addr_incr * 2;
      val += val_incr;
      LOG_INFO("Value Update: %08x", val);
      LOG_INFO("Current Hardware Address Update: %08x", curr_addr);
    }
    LOG_INFO("--------");
    break;

  case DATATYPE_32BIT:
    LOG_INFO("32-bit Write");
    LOG_INFO("--------");
    for (int i = 0; i < write_num; ++i)
    {
      PowerPC::HostWrite_U32(val, curr_addr);
      LOG_INFO("Write %08x to address %08x", val, curr_addr);
      curr_addr += addr_incr * 4;
      val += val_incr;
      LOG_INFO("Value Update: %08x", val);
      LOG_INFO("Current Hardware Address Update: %08x", curr_addr);
    }
    LOG_INFO("--------");
    break;

  default:
    LOG_INFO("Bad Size");
    PanicAlertT("Action Replay Error: Invalid size (%08x : address = %08x) in Fill and Slide (%s)",
                size, new_addr, s_current_code->name.c_str());
    return false;
  }
  return true;
}

// kenobi's "memory copy" Z-code. Requires an additional master code
// on a real AR device. Documented here:
// https://github.com/dolphin-emu/dolphin/wiki/GameCube-Action-Replay-Code-Types#type-z4-size-3--memory-copy
static bool ZeroCode_MemoryCopy(const u32 val_last, const ARAddr& addr, const u32 data)
{
  const u32 addr_dest = val_last & ~0x06000000;
  const u32 addr_src = addr.GCAddress();

  const u8 num_bytes = data & 0x7FFF;

  LOG_INFO("Dest Address: %08x", addr_dest);
  LOG_INFO("Src Address: %08x", addr_src);
  LOG_INFO("Size: %08x", num_bytes);

  if ((data & 0xFF0000) == 0)
  {
    if ((data >> 24) != 0x0)
    {  // Memory Copy With Pointers Support
      LOG_INFO("Memory Copy With Pointers Support");
      LOG_INFO("--------");
      const u32 ptr_dest = PowerPC::HostRead_U32(addr_dest);
      LOG_INFO("Resolved Dest Address to: %08x", ptr_dest);
      const u32 ptr_src = PowerPC::HostRead_U32(addr_src);
      LOG_INFO("Resolved Src Address to: %08x", ptr_src);
      for (int i = 0; i < num_bytes; ++i)
      {
        PowerPC::HostWrite_U8(PowerPC::HostRead_U8(ptr_src + i), ptr_dest + i);
        LOG_INFO("Wrote %08x to address %08x", PowerPC::HostRead_U8(ptr_src + i), ptr_dest + i);
      }
      LOG_INFO("--------");
    }
    else
    {  // Memory Copy Without Pointer Support
      LOG_INFO("Memory Copy Without Pointers Support");
      LOG_INFO("--------");
      for (int i = 0; i < num_bytes; ++i)
      {
        PowerPC::HostWrite_U8(PowerPC::HostRead_U8(addr_src + i), addr_dest + i);
        LOG_INFO("Wrote %08x to address %08x", PowerPC::HostRead_U8(addr_src + i), addr_dest + i);
      }
      LOG_INFO("--------");
      return true;
    }
  }
  else
  {
    LOG_INFO("Bad Value");
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
    LOG_INFO("Doing Ram Write And Fill");
    if (!Subtype_RamWriteAndFill(addr, data))
      return false;
    break;

  case SUB_WRITE_POINTER:  // Write to pointer
    LOG_INFO("Doing Write To Pointer");
    if (!Subtype_WriteToPointer(addr, data))
      return false;
    break;

  case SUB_ADD_CODE:  // Increment Value
    LOG_INFO("Doing Add Code");
    if (!Subtype_AddCode(addr, data))
      return false;
    break;

  case SUB_MASTER_CODE:  // Master Code & Write to CCXXXXXX
    LOG_INFO("Doing Master Code And Write to CCXXXXXX (ncode not supported)");
    if (!Subtype_MasterCodeAndWriteToCCXXXXXX(addr, data))
      return false;
    break;

  default:
    LOG_INFO("Bad Subtype");
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
    LOG_INFO("Type 1: If Equal");
    return val1 == val2;

  case CONDTIONAL_NOT_EQUAL:
    LOG_INFO("Type 2: If Not Equal");
    return val1 != val2;

  case CONDTIONAL_LESS_THAN_SIGNED:
    LOG_INFO("Type 3: If Less Than (Signed)");
    return static_cast<s32>(val1) < static_cast<s32>(val2);

  case CONDTIONAL_GREATER_THAN_SIGNED:
    LOG_INFO("Type 4: If Greater Than (Signed)");
    return static_cast<s32>(val1) > static_cast<s32>(val2);

  case CONDTIONAL_LESS_THAN_UNSIGNED:
    LOG_INFO("Type 5: If Less Than (Unsigned)");
    return val1 < val2;

  case CONDTIONAL_GREATER_THAN_UNSIGNED:
    LOG_INFO("Type 6: If Greater Than (Unsigned)");
    return val1 > val2;

  case CONDTIONAL_AND:
    LOG_INFO("Type 7: If And");
    return !!(val1 & val2);  // bitwise AND

  default:
    LOG_INFO("Unknown Compare type");
    PanicAlertT("Action Replay: Invalid Normal Code Type %08x (%s)", type,
                s_current_code->name.c_str());
    return false;
  }
}

static bool ConditionalCode(const ARAddr& addr, const u32 data, int* const pSkipCount)
{
  const u32 new_addr = addr.GCAddress();

  LOG_INFO("Size: %08x", addr.size);
  LOG_INFO("Hardware Address: %08x", new_addr);

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
    LOG_INFO("Bad Size");
    PanicAlertT("Action Replay: Conditional Code: Invalid Size %08x (%s)", addr.size,
                s_current_code->name.c_str());
    return false;
  }

  LOG_INFO("Conditional result: %d", result);
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
      LOG_INFO("Bad Subtype");
      PanicAlertT("Action Replay: Normal Code %i: Invalid subtype %08x (%s)", 1, addr.subtype,
                  s_current_code->name.c_str());
      return false;
    }
  }

  return true;
}

// NOTE: Lock needed to give mutual exclusion to s_current_code and LOG_INFO
static bool RunCodeLocked(const ARCode& arcode)
{
  // The mechanism is different than what the real AR uses, so there may be compatibility problems.

  bool do_fill_and_slide = false;
  bool do_memory_copy = false;

  // used for conditional codes
  int skip_count = 0;

  u32 val_last = 0;

  s_current_code = &arcode;

  LOG_INFO("Code Name: %s", arcode.name.c_str());
  LOG_INFO("Number of codes: %zu", arcode.ops.size());

  for (const AREntry& entry : arcode.ops)
  {
    const ARAddr addr(entry.cmd_addr);
    const u32 data = entry.value;

    // after a conditional code, skip lines if needed
    if (skip_count)
    {
      if (skip_count > 0)  // skip x lines
      {
        LOG_INFO("Line skipped");
        --skip_count;
      }
      else if (-CONDTIONAL_ALL_LINES == skip_count)
      {
        // skip all lines
        LOG_INFO("All Lines skipped");
        return true;  // don't need to iterate through the rest of the ops
      }
      else if (-CONDTIONAL_ALL_LINES_UNTIL == skip_count)
      {
        // skip until a "00000000 40000000" line is reached
        LOG_INFO("Line skipped");
        if (addr == 0 && 0x40000000 == data)  // check for an endif line
          skip_count = 0;
      }

      continue;
    }

    LOG_INFO("--- Running Code: %08x %08x ---", addr.address, data);
    // LOG_INFO("Command: %08x", cmd);

    // Do Fill & Slide
    if (do_fill_and_slide)
    {
      do_fill_and_slide = false;
      LOG_INFO("Doing Fill And Slide");
      if (false == ZeroCode_FillAndSlide(val_last, addr, data))
        return false;
      continue;
    }

    // Memory Copy
    if (do_memory_copy)
    {
      do_memory_copy = false;
      LOG_INFO("Doing Memory Copy");
      if (false == ZeroCode_MemoryCopy(val_last, addr, data))
        return false;
      continue;
    }

    // ActionReplay program self modification codes
    if (addr >= 0x00002000 && addr < 0x00003000)
    {
      LOG_INFO(
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

      LOG_INFO("Doing Zero Code %08x", zcode);

      switch (zcode)
      {
      case ZCODE_END:  // END OF CODES
        LOG_INFO("ZCode: End Of Codes");
        return true;

      // TODO: the "00000000 40000000"(end if) codes fall into this case, I don't think that is
      // correct
      case ZCODE_NORM:  // Normal execution of codes
        // Todo: Set register 1BB4 to 0
        LOG_INFO("ZCode: Normal execution of codes, set register 1BB4 to 0 (zcode not supported)");
        break;

      case ZCODE_ROW:  // Executes all codes in the same row
        // Todo: Set register 1BB4 to 1
        LOG_INFO("ZCode: Executes all codes in the same row, Set register 1BB4 to 1 (zcode not "
                 "supported)");
        PanicAlertT("Zero 3 code not supported");
        return false;

      case ZCODE_04:  // Fill & Slide or Memory Copy
        if (0x3 == ((data >> 25) & 0x03))
        {
          LOG_INFO("ZCode: Memory Copy");
          do_memory_copy = true;
          val_last = data;
        }
        else
        {
          LOG_INFO("ZCode: Fill And Slide");
          do_fill_and_slide = true;
          val_last = data;
        }
        break;

      default:
        LOG_INFO("ZCode: Unknown");
        PanicAlertT("Zero code unknown to Dolphin: %08x", zcode);
        return false;
      }

      // done handling zero codes
      continue;
    }

    // Normal codes
    LOG_INFO("Doing Normal Code %08x", addr.type);
    LOG_INFO("Subtype: %08x", addr.subtype);

    switch (addr.type)
    {
    case 0x00:
      if (false == NormalCode(addr, data))
        return false;
      break;

    default:
      LOG_INFO("This Normal Code is a Conditional Code");
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
                                        LOG_INFO("\n");
                                        return !success;
                                      }),
                       s_active_codes.end());
}

}  // namespace ActionReplay
