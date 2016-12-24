// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <GLFW/glfw3.h>
#include <string>
#include "Common/GL/GLInterfaceBase.h"

class cInterfaceGLFW : public cInterfaceBase
{
public:
  void SwapInterval(int interval) override;
  void Swap() override;
  void* GetFuncAddress(const std::string& name) override;
  bool Create(void* window_handle, bool core) override;
  bool Create(cInterfaceBase* main_context) override;
  bool MakeCurrent() override;
  bool ClearCurrent() override;
  void Shutdown() override;

  void Update() override;
  bool PeekMessages() override;

  std::unique_ptr<cInterfaceBase> CreateSharedContext() override;

private:
  GLFWwindow* window = nullptr;
};
