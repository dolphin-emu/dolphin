// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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

#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PowerPC.h"

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
        WARN_LOG(OSHLE, "MEGA database failed to parse code");
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
  *name = StripSpaces(buffer.substr(0, next));

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
    u64 offset = strtoul(ptr, &endptr, 16);

    if (ptr == endptr || offset > std::numeric_limits<u32>::max())
    {
      WARN_LOG(OSHLE, "MEGA database failed to parse reference %u offset", ref_count);
      return false;
    }

    if (!GetFunctionName(iss, &ref))
    {
      WARN_LOG(OSHLE, "MEGA database failed to parse reference %u name", ref_count);
      return false;
    }
    sig->refs.emplace_back(static_cast<u32>(offset), std::move(ref));

    ref_count += 1;
    num.clear();
    ref.clear();
  }
  return true;
}

bool Compare(u32 address, u32 size, const MEGASignature& sig)
{
  if (size != sig.code.size() * sizeof(u32))
    return false;

  for (size_t i = 0; i < sig.code.size(); ++i)
  {
    if (sig.code[i] != 0 &&
        PowerPC::HostRead_U32(static_cast<u32>(address + i * sizeof(u32))) != sig.code[i])
      return false;
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
      WARN_LOG(OSHLE, "MEGA database failed to parse line %zu", i);
    }
  }
  return true;
}

bool MEGASignatureDB::Save(const std::string& file_path) const
{
  ERROR_LOG(OSHLE, "MEGA database save unsupported yet.");
  return false;
}

void MEGASignatureDB::Apply(PPCSymbolDB* symbol_db) const
{
  for (auto& it : symbol_db->AccessSymbols())
  {
    auto& symbol = it.second;
    for (const auto& sig : m_signatures)
    {
      if (Compare(symbol.address, symbol.size, sig))
      {
        symbol.name = sig.name;
        INFO_LOG(OSHLE, "Found %s at %08x (size: %08x)!", sig.name.c_str(), symbol.address,
                 symbol.size);
        break;
      }
    }
  }
  symbol_db->Index();
}

void MEGASignatureDB::Populate(const PPCSymbolDB* func_db, const std::string& filter)
{
  ERROR_LOG(OSHLE, "MEGA database can't be populated yet.");
}

bool MEGASignatureDB::Add(u32 startAddr, u32 size, const std::string& name)
{
  ERROR_LOG(OSHLE, "Can't add symbol to MEGA database yet.");
  return false;
}

void MEGASignatureDB::List() const
{
  for (const auto& entry : m_signatures)
  {
    DEBUG_LOG(OSHLE, "%s : %zu bytes", entry.name.c_str(), entry.code.size() * sizeof(u32));
  }
  INFO_LOG(OSHLE, "%zu functions known in current MEGA database.", m_signatures.size());
}
