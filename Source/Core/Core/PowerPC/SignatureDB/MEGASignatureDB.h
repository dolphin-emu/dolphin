// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <sstream>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

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
class MEGASignatureDB
{
public:
  MEGASignatureDB();
  ~MEGASignatureDB();

  bool Load(const std::string& file_path);
  void Apply(PPCSymbolDB* symbol_db) const;
  void List() const;

private:
  bool GetCode(MEGASignature* sig, std::istringstream* iss) const;
  bool GetFunctionName(std::istringstream* iss, std::string* name) const;
  bool GetName(MEGASignature* sig, std::istringstream* iss) const;
  bool GetRefs(MEGASignature* sig, std::istringstream* iss) const;
  bool Compare(u32 address, u32 size, const MEGASignature& sig) const;

  std::vector<MEGASignature> m_signatures;
};
