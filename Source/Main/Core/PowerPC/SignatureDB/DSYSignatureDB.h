// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/PowerPC/SignatureDB/SignatureDB.h"

class DSYSignatureDB final : public HashSignatureDB
{
public:
  ~DSYSignatureDB() = default;
  bool Load(const std::string& file_path) override;
  bool Save(const std::string& file_path) const override;
};
