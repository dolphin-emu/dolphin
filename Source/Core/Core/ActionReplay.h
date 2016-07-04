// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>
#include "Common/CommonTypes.h"

class IniFile;

namespace ActionReplay
{
struct AREntry
{
  AREntry() {}
  AREntry(u32 _addr, u32 _value) : cmd_addr(_addr), value(_value) {}
  u32 cmd_addr;
  u32 value;
};

struct ARCode
{
  std::string name;
  std::vector<AREntry> ops;
  bool active;
  bool user_defined;
};

struct ARAddr
{
  union {
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

void RunAllActive();

void ApplyCodes(const std::vector<ARCode>& codes);
void AddCode(ARCode new_code);
void LoadAndApplyCodes(const IniFile& global_ini, const IniFile& local_ini);

std::vector<ARCode> LoadCodes(const IniFile& global_ini, const IniFile& local_ini);
void SaveCodes(IniFile* local_ini, const std::vector<ARCode>& codes);

void EnableSelfLogging(bool enable);
std::vector<std::string> GetSelfLog();
void ClearSelfLog();
bool IsSelfLogging();
}  // namespace
