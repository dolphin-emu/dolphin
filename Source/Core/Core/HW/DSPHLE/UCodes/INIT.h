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

class INITUCode : public UCodeInterface
{
public:
  INITUCode(DSPHLE* dsphle, u32 crc);
  virtual ~INITUCode();

  void Initialize() override;
  void HandleMail(u32 mail) override;
  void Update() override;
  void Init();
};
}  // namespace HLE
}  // namespace DSP
