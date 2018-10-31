// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/SignatureDB/CSVSignatureDB.h"

#include <cstdio>
#include <fstream>
#include <sstream>

#include "Common/File.h"
#include "Common/FileUtil.h"
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
      WARN_LOG(SYMBOLS, "CSV database failed to parse line %zu", i);
    }
  }

  return true;
}

bool CSVSignatureDB::Save(const std::string& file_path) const
{
  File::IOFile f(file_path, "w");

  if (!f)
  {
    ERROR_LOG(SYMBOLS, "CSV database save failed");
    return false;
  }
  for (const auto& func : m_database)
  {
    // The object name/location are unused for the time being.
    // To be implemented.
    fprintf(f.GetHandle(), "%08x\t%08x\t%s\t%s\t%s\n", func.first, func.second.size,
            func.second.name.c_str(), func.second.object_location.c_str(),
            func.second.object_name.c_str());
  }

  INFO_LOG(SYMBOLS, "CSV database save successful");
  return true;
}
