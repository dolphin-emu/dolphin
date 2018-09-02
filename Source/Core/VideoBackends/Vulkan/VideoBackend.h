// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/Common.h"
#include "VideoCommon/VideoBackendBase.h"

namespace Vulkan
{
class VideoBackend : public VideoBackendBase
{
public:
  bool Initialize(void* window_handle) override;
  void Shutdown() override;

  std::string GetName() const override { return "Vulkan"; }
  std::string GetDisplayName() const override { return _trans("Vulkan"); }
  void InitBackendInfo() override;

  void* GetDisplayHandle() const { return m_display_handle; }

private:
  // Helpers to manage the connection to the X server.
  bool OpenDisplayConnection();
  void CloseDisplayConnection();

  // For X11 systems, contains a pointer to the Display connection.
  void* m_display_handle = nullptr;
};
}
