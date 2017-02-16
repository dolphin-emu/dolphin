// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "Core/HW/DSPHLE/UCodes/UCodes.h"

namespace DSP
{
namespace HLE
{
class DSPHLE;

class CARDUCode : public UCodeInterface
{
public:
  CARDUCode(DSPHLE* dsphle, u32 crc);
  virtual ~CARDUCode();

  void Initialize() override;
  void HandleMail(u32 mail) override;
  void Update() override;
};
}  // namespace HLE
}  // namespace DSP
