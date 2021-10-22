// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/SignatureDB/CSVSignatureDB.h"

#include <cstdio>
#include <fstream>
#include <sstream>

#include <fmt/format.h>

#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"

// CSV separated with tabs
// Checksum | Size | Symbol | [Object Location |] Object Name
bool CSVSignatureDB::Load(const std::string& file_path)
{
  std::string line;
  std::ifstream ifs;
  File::OpenFStream(ifs, file_path, std::ios_base::in);

  if (!ifs)
    return false;
  for (size_t i = 1; std::getline(ifs, line); i += 1)
  {
    std::istringstream iss(line);
    u32 checksum, size;
    std::string tab, symbol, object_location, object_name;

    iss >> std::hex >> checksum >> std::hex >> size;
    if (iss && std::getline(iss, tab, '\t'))
    {
      if (std::getline(iss, symbol, '\t') && std::getline(iss, object_location, '\t'))
        std::getline(iss, object_name);
      HashSignatureDB::DBFunc func;
      func.name = symbol;
      func.size = size;
      // Doesn't have an object location
      if (object_name.empty())
      {
        func.object_name = object_location;
      }
      else
      {
        func.object_location = object_location;
        func.object_name = object_name;
      }
      m_database[checksum] = func;
    }
    else
    {
      WARN_LOG_FMT(SYMBOLS, "CSV database failed to parse line {}", i);
    }
  }

  return true;
}

bool CSVSignatureDB::Save(const std::string& file_path) const
{
  File::IOFile f(file_path, "w");

  if (!f)
  {
    ERROR_LOG_FMT(SYMBOLS, "CSV database save failed");
    return false;
  }
  for (const auto& func : m_database)
  {
    // The object name/location are unused for the time being.
    // To be implemented.
    f.WriteString(fmt::format("{0:08x}\t{1:08x}\t{2}\t{3}\t{4}\n", func.first, func.second.size,
                              func.second.name, func.second.object_location,
                              func.second.object_name));
  }

  INFO_LOG_FMT(SYMBOLS, "CSV database save successful");
  return true;
}
