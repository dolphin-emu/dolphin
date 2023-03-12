// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/SignatureDB/MEGASignatureDB.h"

#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>

#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PPCSymbolDB.h"

namespace
{
constexpr size_t INSTRUCTION_HEXSTRING_LENGTH = 8;

bool GetCode(MEGASignature* sig, std::istringstream* iss)
{
  std::string code;
  if ((*iss >> code) && (code.length() % INSTRUCTION_HEXSTRING_LENGTH) == 0)
  {
    for (size_t i = 0; i < code.length(); i += INSTRUCTION_HEXSTRING_LENGTH)
    {
      std::string instruction = code.substr(i, INSTRUCTION_HEXSTRING_LENGTH);
      u32 num = static_cast<u32>(strtoul(instruction.c_str(), nullptr, 16));
      if (num != 0 || instruction == "........")
      {
        sig->code.emplace_back(num);
      }
      else
      {
        WARN_LOG_FMT(SYMBOLS, "MEGA database failed to parse code");
        return false;
      }
    }
    return true;
  }
  return false;
}

bool GetFunctionName(std::istringstream* iss, std::string* name)
{
  std::string buffer;

  std::getline(*iss, buffer);
  size_t next = buffer.find(" ^");
  *name = StripWhitespace(buffer.substr(0, next));

  if (name->empty())
    return false;

  if (next == std::string::npos)
    next = buffer.length();
  iss->str(buffer.substr(next));
  iss->clear();
  return true;
}

bool GetName(MEGASignature* sig, std::istringstream* iss)
{
  std::string unknown;
  return (*iss >> unknown) && GetFunctionName(iss, &sig->name);
}

bool GetRefs(MEGASignature* sig, std::istringstream* iss)
{
  std::string num, ref;
  u32 ref_count = 1;
  while (*iss && (*iss >> num) && !num.empty())
  {
    num = num.substr(1);
    const char* ptr = num.c_str();
    char* endptr;
    const u64 offset = std::strtoul(ptr, &endptr, 16);

    if (ptr == endptr || offset > std::numeric_limits<u32>::max())
    {
      WARN_LOG_FMT(SYMBOLS, "MEGA database failed to parse reference {} offset", ref_count);
      return false;
    }

    if (!GetFunctionName(iss, &ref))
    {
      WARN_LOG_FMT(SYMBOLS, "MEGA database failed to parse reference {} name", ref_count);
      return false;
    }
    sig->refs.emplace_back(static_cast<u32>(offset), std::move(ref));

    ref_count += 1;
    num.clear();
    ref.clear();
  }
  return true;
}

bool Compare(const Core::CPUThreadGuard& guard, u32 address, u32 size, const MEGASignature& sig)
{
  if (size != sig.code.size() * sizeof(u32))
    return false;

  for (size_t i = 0; i < sig.code.size(); ++i)
  {
    if (sig.code[i] != 0 && PowerPC::MMU::HostRead_U32(
                                guard, static_cast<u32>(address + i * sizeof(u32))) != sig.code[i])
    {
      return false;
    }
  }
  return true;
}
}  // Anonymous namespace

MEGASignatureDB::MEGASignatureDB() = default;
MEGASignatureDB::~MEGASignatureDB() = default;

void MEGASignatureDB::Clear()
{
  m_signatures.clear();
}

bool MEGASignatureDB::Load(const std::string& file_path)
{
  std::ifstream ifs;
  File::OpenFStream(ifs, file_path, std::ios_base::in);

  if (!ifs)
    return false;

  std::string line;
  for (size_t i = 1; std::getline(ifs, line); ++i)
  {
    std::istringstream iss(line);
    MEGASignature sig;

    if (GetCode(&sig, &iss) && GetName(&sig, &iss) && GetRefs(&sig, &iss))
    {
      m_signatures.push_back(std::move(sig));
    }
    else
    {
      WARN_LOG_FMT(SYMBOLS, "MEGA database failed to parse line {}", i);
    }
  }
  return true;
}

bool MEGASignatureDB::Save(const std::string& file_path) const
{
  ERROR_LOG_FMT(SYMBOLS, "MEGA database save unsupported yet.");
  return false;
}

void MEGASignatureDB::Apply(const Core::CPUThreadGuard& guard, PPCSymbolDB* symbol_db) const
{
  for (auto& it : symbol_db->AccessSymbols())
  {
    auto& symbol = it.second;
    for (const auto& sig : m_signatures)
    {
      if (Compare(guard, symbol.address, symbol.size, sig))
      {
        symbol.name = sig.name;
        INFO_LOG_FMT(SYMBOLS, "Found {} at {:08x} (size: {:08x})!", sig.name, symbol.address,
                     symbol.size);
        break;
      }
    }
  }
  symbol_db->Index();
}

void MEGASignatureDB::Populate(const PPCSymbolDB* func_db, const std::string& filter)
{
  ERROR_LOG_FMT(SYMBOLS, "MEGA database can't be populated yet.");
}

bool MEGASignatureDB::Add(const Core::CPUThreadGuard& guard, u32 startAddr, u32 size,
                          const std::string& name)
{
  ERROR_LOG_FMT(SYMBOLS, "Can't add symbol to MEGA database yet.");
  return false;
}

void MEGASignatureDB::List() const
{
  for (const auto& entry : m_signatures)
  {
    DEBUG_LOG_FMT(SYMBOLS, "{} : {} bytes", entry.name, entry.code.size() * sizeof(u32));
  }
  INFO_LOG_FMT(SYMBOLS, "{} functions known in current MEGA database.", m_signatures.size());
}
