// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "VideoCommon/PerfQueryBase.h"

class VideoBackendBase;

enum class EFBAccessType
{
  PeekZ,
  PokeZ,
  PeekColor,
  PokeColor
};

class PointerWrap;
struct BackendInfo;

// TODO: Think of a better name for this
// Essentially this is the root class for the shared "flipper/hollywood to modern graphics"
// translation layer that makes up most of VideoCommon
class VideoBase final
{
public:
  bool Initialize();

  void Shutdown();

  void OutputXFB(u32 xfb_addr, u32 fb_width, u32 fb_stride, u32 fb_height, u64 ticks);

  u32 AccessEFB(EFBAccessType type, u32 x, u32 y, u32 data);
  u32 GetQueryResult(PerfQueryType type);
  u16 GetBoundingBox(int index);

  // Wrapper function which pushes the event to the GPU thread.
  void DoState(PointerWrap& p);

  VideoBackendBase* GetBackend() const { return m_backend; }
  const BackendInfo& GetBackendInfo() const;

private:
  bool InitializeShared(VideoBackendBase* backend);

  VideoBackendBase* m_backend = nullptr;
  bool m_initialized = false;
};
