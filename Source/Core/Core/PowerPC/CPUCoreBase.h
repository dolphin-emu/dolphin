// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

class CPUCoreBase
{
public:
  virtual ~CPUCoreBase() = default;
  virtual void Init() = 0;
  virtual void Shutdown() = 0;
  virtual void ClearCache() = 0;
  virtual void Run() = 0;
  virtual void SingleStep() = 0;
  virtual const char* GetName() const = 0;
};
