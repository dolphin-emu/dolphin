// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <memory>
#include <string>

#include "Common/CommonTypes.h"

// You're not meant to keep around SignatureDB objects persistently. Use 'em, throw them away.

class PPCSymbolDB;
class SignatureDBFormatHandler;

class SignatureDB
{
public:
  struct DBFunc
  {
    u32 size;
    std::string name;
    std::string object_name;
    std::string object_location;
    DBFunc() : size(0) {}
  };
  using FuncDB = std::map<u32, DBFunc>;

  // Returns the hash.
  u32 Add(u32 startAddr, u32 size, const std::string& name);

  // Does not clear. Remember to clear first if that's what you want.
  bool Load(const std::string& file_path);
  bool Save(const std::string& file_path);
  void Clear();
  void List();

  void Initialize(PPCSymbolDB* func_db, const std::string& prefix = "");
  void Apply(PPCSymbolDB* func_db);

  static u32 ComputeCodeChecksum(u32 offsetStart, u32 offsetEnd);

private:
  std::unique_ptr<SignatureDBFormatHandler> CreateFormatHandler(const std::string& file_path);
  // Map from signature to function. We store the DB in this map because it optimizes the
  // most common operation - lookup. We don't care about ordering anyway.
  FuncDB m_database;
};

class SignatureDBFormatHandler
{
public:
  virtual ~SignatureDBFormatHandler();
  virtual bool Load(const std::string& file_path, SignatureDB::FuncDB& database) const = 0;
  virtual bool Save(const std::string& file_path, const SignatureDB::FuncDB& database) const = 0;
};
