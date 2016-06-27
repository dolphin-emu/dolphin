// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "dsp_interface.h"

class RealDSP : public IDSP
{
public:
  virtual void Init();
  virtual void Reset();
  virtual u32 CheckMailTo();
  virtual void SendMailTo(u32 mail);
};
