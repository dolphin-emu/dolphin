// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/GL/GLInterfaceBase.h"

class BWindow;
class BGLView;

class cInterfaceBGL final : public cInterfaceBase
{
public:
  void Swap() override;
  void* GetFuncAddress(const std::string& name) override;
  bool Create(void* window_handle, bool stereo, bool core) override;
  bool MakeCurrent() override;
  bool ClearCurrent() override;
  void Shutdown() override;
  void Update() override;
  void SwapInterval(int interval) override;

private:
  BWindow* m_window;
  BGLView* m_gl;
};
