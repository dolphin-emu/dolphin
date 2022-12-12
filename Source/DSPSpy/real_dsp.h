// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "dsp_interface.h"

class RealDSP final : public IDSP
{
public:
  void Init() override;
  void Reset() override;
  u32 CheckMailTo() override;
  void SendMailTo(u32 mail) override;
  void SetInterrupt() override;
  bool CheckInterrupt() override;
};
