// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/PowerPC/SignatureDB/SignatureDB.h"

class PPCSymbolDB;

struct MEGASignatureReference
{
  MEGASignatureReference(u32 ref_offset, std::string ref_name)
      : offset(ref_offset), name(std::move(ref_name))
  {
  }
  u32 offset;
  std::string name;
};

struct MEGASignature
{
  std::vector<u32> code;
  std::string name;
  std::vector<MEGASignatureReference> refs;
};

// MEGA files from Megazig's WiiTools IDA plugin
//  -> https://github.com/Megazig/WiiTools
//
// Each line contains a function signature composed of:
//  - Hexstring representation with "." acting as a wildcard
//  - Name, represented as follow: ":0000 function_name"
//  - References located in the hexstring at offset: "^offset reference_name"
class MEGASignatureDB : public SignatureDBFormatHandler
{
public:
  MEGASignatureDB();
  ~MEGASignatureDB();

  void Clear();
  bool Load(const std::string& file_path);
  bool Save(const std::string& file_path) const;
  void List() const;

  void Apply(PPCSymbolDB* symbol_db) const;
  void Populate(const PPCSymbolDB* func_db, const std::string& filter = "");

  bool Add(u32 startAddr, u32 size, const std::string& name);

private:
  std::vector<MEGASignature> m_signatures;
};
