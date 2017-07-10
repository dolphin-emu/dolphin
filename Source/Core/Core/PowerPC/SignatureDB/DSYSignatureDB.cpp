// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/SignatureDB/DSYSignatureDB.h"

#include <cstddef>
#include <cstring>
#include <memory>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/Logging/Log.h"

namespace
{
// On-disk format for SignatureDB entries.
struct FuncDesc
{
  u32 checksum;
  u32 size;
  char name[128];
};
}  // namespace

bool DSYSignatureDB::Load(const File::Path& file_path)
{
  std::unique_ptr<File::ReadOnlyFile> file = file_path.OpenFile(true);

  if (!*file)
    return false;
  u32 fcount = 0;
  file->ReadArray(&fcount, 1);
  for (size_t i = 0; i < fcount; i++)
  {
    FuncDesc temp;
    memset(&temp, 0, sizeof(temp));

    file->ReadArray(&temp, 1);
    temp.name[sizeof(temp.name) - 1] = 0;

    HashSignatureDB::DBFunc func;
    func.name = temp.name;
    func.size = temp.size;
    m_database[temp.checksum] = func;
  }

  return true;
}

bool DSYSignatureDB::Save(const std::string& file_path) const
{
  File::IOFile f(file_path, "wb");

  if (!f)
  {
    ERROR_LOG(OSHLE, "Database save failed");
    return false;
  }
  u32 fcount = static_cast<u32>(m_database.size());
  f.WriteArray(&fcount, 1);
  for (const auto& entry : m_database)
  {
    FuncDesc temp;
    memset(&temp, 0, sizeof(temp));
    temp.checksum = entry.first;
    temp.size = entry.second.size;
    strncpy(temp.name, entry.second.name.c_str(), 127);
    f.WriteArray(&temp, 1);
  }

  INFO_LOG(OSHLE, "Database save successful");
  return true;
}
