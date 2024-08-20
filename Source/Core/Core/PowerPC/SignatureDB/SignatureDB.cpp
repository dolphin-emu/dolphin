// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/SignatureDB/SignatureDB.h"

#include <memory>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PPCSymbolDB.h"

// Format Handlers
#include "Core/PowerPC/SignatureDB/CSVSignatureDB.h"
#include "Core/PowerPC/SignatureDB/DSYSignatureDB.h"
#include "Core/PowerPC/SignatureDB/MEGASignatureDB.h"

namespace
{
SignatureDB::HandlerType GetHandlerType(const std::string& file_path)
{
  if (file_path.ends_with(".csv"))
    return SignatureDB::HandlerType::CSV;
  if (file_path.ends_with(".mega"))
    return SignatureDB::HandlerType::MEGA;
  return SignatureDB::HandlerType::DSY;
}

std::unique_ptr<SignatureDBFormatHandler> CreateFormatHandler(SignatureDB::HandlerType handler)
{
  switch (handler)
  {
  default:
  case SignatureDB::HandlerType::DSY:
    return std::make_unique<DSYSignatureDB>();
  case SignatureDB::HandlerType::CSV:
    return std::make_unique<CSVSignatureDB>();
  case SignatureDB::HandlerType::MEGA:
    return std::make_unique<MEGASignatureDB>();
  }
}
}  // Anonymous namespace

SignatureDB::SignatureDB(HandlerType handler) : m_handler(CreateFormatHandler(handler))
{
}

SignatureDB::SignatureDB(const std::string& file_path) : SignatureDB(GetHandlerType(file_path))
{
}

void SignatureDB::Clear()
{
  m_handler->Clear();
}

bool SignatureDB::Load(const std::string& file_path)
{
  return m_handler->Load(file_path);
}

bool SignatureDB::Save(const std::string& file_path) const
{
  return m_handler->Save(file_path);
}

void SignatureDB::List() const
{
  m_handler->List();
}

void SignatureDB::Populate(const PPCSymbolDB* func_db, const std::string& filter)
{
  m_handler->Populate(func_db, filter);
}

void SignatureDB::Apply(const Core::CPUThreadGuard& guard, PPCSymbolDB* func_db) const
{
  m_handler->Apply(guard, func_db);
}

bool SignatureDB::Add(const Core::CPUThreadGuard& guard, u32 start_addr, u32 size,
                      const std::string& name)
{
  return m_handler->Add(guard, start_addr, size, name);
}

// Adds a known function to the hash database
bool HashSignatureDB::Add(const Core::CPUThreadGuard& guard, u32 startAddr, u32 size,
                          const std::string& name)
{
  u32 hash = ComputeCodeChecksum(guard, startAddr, startAddr + size - 4);

  DBFunc temp_dbfunc;
  temp_dbfunc.size = size;
  temp_dbfunc.name = name;

  FuncDB::iterator iter = m_database.find(hash);
  if (iter == m_database.end())
  {
    m_database[hash] = temp_dbfunc;
    return true;
  }
  return false;
}

void HashSignatureDB::List() const
{
  for (const auto& entry : m_database)
  {
    DEBUG_LOG_FMT(SYMBOLS, "{} : {} bytes, hash = {:08x}", entry.second.name, entry.second.size,
                  entry.first);
  }
  INFO_LOG_FMT(SYMBOLS, "{} functions known in current database.", m_database.size());
}

void HashSignatureDB::Clear()
{
  m_database.clear();
}

void HashSignatureDB::Apply(const Core::CPUThreadGuard& guard, PPCSymbolDB* symbol_db) const
{
  for (const auto& entry : m_database)
  {
    for (const auto& function : symbol_db->GetSymbolsFromHash(entry.first))
    {
      // Found the function. Let's rename it according to the symbol file.
      function->Rename(entry.second.name);
      if (entry.second.size == static_cast<unsigned int>(function->size))
      {
        INFO_LOG_FMT(SYMBOLS, "Found {} at {:08x} (size: {:08x})!", entry.second.name,
                     function->address, function->size);
      }
      else
      {
        ERROR_LOG_FMT(SYMBOLS, "Wrong size! Found {} at {:08x} (size: {:08x} instead of {:08x})!",
                      entry.second.name, function->address, function->size, entry.second.size);
      }
    }
  }
  symbol_db->Index();
}

void HashSignatureDB::Populate(const PPCSymbolDB* symbol_db, const std::string& filter)
{
  for (const auto& symbol : symbol_db->Symbols())
  {
    if ((filter.empty() && (!symbol.second.name.empty()) &&
         symbol.second.name.substr(0, 3) != "zz_" && symbol.second.name.substr(0, 1) != ".") ||
        ((!filter.empty()) && symbol.second.name.substr(0, filter.size()) == filter))
    {
      DBFunc temp_dbfunc;
      temp_dbfunc.name = symbol.second.name;
      temp_dbfunc.size = symbol.second.size;
      m_database[symbol.second.hash] = temp_dbfunc;
    }
  }
}

u32 HashSignatureDB::ComputeCodeChecksum(const Core::CPUThreadGuard& guard, u32 offsetStart,
                                         u32 offsetEnd)
{
  u32 sum = 0;
  for (u32 offset = offsetStart; offset <= offsetEnd; offset += 4)
  {
    u32 opcode = PowerPC::MMU::HostRead_Instruction(guard, offset);
    u32 op = opcode & 0xFC000000;
    u32 op2 = 0;
    u32 op3 = 0;
    switch (u32 auxop = op >> 26)
    {
    case 4:  // PS instructions
      op2 = opcode & 0x0000003F;
      switch (op2)
      {
      case 0:
      case 8:
      case 16:
      case 21:
      case 22:
        op3 = opcode & 0x000007C0;
      }
      break;

    case 7:  // addi muli etc
    case 8:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
      op2 = opcode & 0x03FF0000;
      break;

    case 19:  // MCRF??
    case 31:  // integer
    case 63:  // fpu
      op2 = opcode & 0x000007FF;
      break;
    case 59:  // fpu
      op2 = opcode & 0x0000003F;
      if (op2 < 16)
        op3 = opcode & 0x000007C0;
      break;
    default:
      if (auxop >= 32 && auxop < 56)
        op2 = opcode & 0x03FF0000;
      break;
    }
    // Checksum only uses opcode, not opcode data, because opcode data changes
    // in all compilations, but opcodes don't!
    sum = (((sum << 17) & 0xFFFE0000) | ((sum >> 15) & 0x0001FFFF));
    sum = sum ^ (op | op2 | op3);
  }
  return sum;
}

SignatureDBFormatHandler::~SignatureDBFormatHandler() = default;
