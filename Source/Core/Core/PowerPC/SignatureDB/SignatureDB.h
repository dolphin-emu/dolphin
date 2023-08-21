// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <memory>
#include <string>

#include "Common/CommonTypes.h"

// You're not meant to keep around SignatureDB objects persistently. Use 'em, throw them away.

namespace Core
{
class CPUThreadGuard;
}

class PPCSymbolDB;
class SignatureDBFormatHandler;

class SignatureDB
{
public:
  enum class HandlerType
  {
    DSY,
    CSV,
    MEGA
  };
  explicit SignatureDB(HandlerType handler);
  explicit SignatureDB(const std::string& file_path);

  void Clear();
  // Does not clear. Remember to clear first if that's what you want.
  bool Load(const std::string& file_path);
  bool Save(const std::string& file_path) const;
  void List() const;

  void Populate(const PPCSymbolDB* func_db, const std::string& filter = "");
  void Apply(const Core::CPUThreadGuard& guard, PPCSymbolDB* func_db) const;

  bool Add(const Core::CPUThreadGuard& guard, u32 start_addr, u32 size, const std::string& name);

private:
  std::unique_ptr<SignatureDBFormatHandler> m_handler;
};

class SignatureDBFormatHandler
{
public:
  virtual ~SignatureDBFormatHandler();

  virtual void Clear() = 0;
  virtual bool Load(const std::string& file_path) = 0;
  virtual bool Save(const std::string& file_path) const = 0;
  virtual void List() const = 0;

  virtual void Populate(const PPCSymbolDB* func_db, const std::string& filter = "") = 0;
  virtual void Apply(const Core::CPUThreadGuard& guard, PPCSymbolDB* func_db) const = 0;

  virtual bool Add(const Core::CPUThreadGuard& guard, u32 startAddr, u32 size,
                   const std::string& name) = 0;
};

class HashSignatureDB : public SignatureDBFormatHandler
{
public:
  struct DBFunc
  {
    u32 size = 0;
    std::string name;
    std::string object_name;
    std::string object_location;
  };
  using FuncDB = std::map<u32, DBFunc>;

  static u32 ComputeCodeChecksum(const Core::CPUThreadGuard& guard, u32 offsetStart, u32 offsetEnd);

  void Clear() override;
  void List() const override;

  void Populate(const PPCSymbolDB* func_db, const std::string& filter = "") override;
  void Apply(const Core::CPUThreadGuard& guard, PPCSymbolDB* func_db) const override;

  bool Add(const Core::CPUThreadGuard& guard, u32 startAddr, u32 size,
           const std::string& name) override;

protected:
  // Map from signature to function. We store the DB in this map because it optimizes the
  // most common operation - lookup. We don't care about ordering anyway.
  FuncDB m_database;
};
