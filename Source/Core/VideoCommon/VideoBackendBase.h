// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/WindowSystemInfo.h"
#include "VideoCommon/PerfQueryBase.h"

namespace MMIO
{
class Mapping;
}
class PointerWrap;

enum class FieldType
{
  Odd,
  Even,
};

enum class EFBAccessType
{
  PeekZ,
  PokeZ,
  PeekColor,
  PokeColor
};

class VideoBackendBase
{
public:
  virtual ~VideoBackendBase() {}
  virtual bool Initialize(const WindowSystemInfo& wsi) = 0;
  virtual void Shutdown() = 0;

  virtual std::string GetName() const = 0;
  virtual std::string GetDisplayName() const { return GetName(); }
  virtual void InitBackendInfo() = 0;

  // Prepares a native window for rendering. This is called on the main thread, or the
  // thread which owns the window.
  virtual void PrepareWindow(const WindowSystemInfo& wsi) {}

  void Video_ExitLoop();

  void Video_BeginField(u32 xfb_addr, u32 fb_width, u32 fb_stride, u32 fb_height, u64 ticks);

  u32 Video_AccessEFB(EFBAccessType type, u32 x, u32 y, u32 data);
  u32 Video_GetQueryResult(PerfQueryType type);
  u16 Video_GetBoundingBox(int index);

  static void PopulateList();
  static void ClearList();
  static void ActivateBackend(const std::string& name);

  // Fills the backend_info fields with the capabilities of the selected backend/device.
  // Called by the UI thread when the graphics config is opened.
  static void PopulateBackendInfo();

  // the implementation needs not do synchronization logic, because calls to it are surrounded by
  // PauseAndLock now
  void DoState(PointerWrap& p);

  void CheckInvalidState();

protected:
  void InitializeShared();
  void ShutdownShared();

  bool m_initialized = false;
  bool m_invalid = false;
};

extern std::vector<std::unique_ptr<VideoBackendBase>> g_available_video_backends;
extern VideoBackendBase* g_video_backend;
