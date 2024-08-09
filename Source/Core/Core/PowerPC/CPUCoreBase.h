// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

class CPUCoreBase
{
public:
  virtual ~CPUCoreBase() = default;
  virtual void Init() = 0;
  virtual void Shutdown() = 0;
  virtual void ClearCache() = 0;
  virtual void RegisterCPUFunction(std::function<void()> function){};
  virtual void Run() = 0;
  virtual void SingleStep() = 0;
  virtual const char* GetName() const = 0;
};
