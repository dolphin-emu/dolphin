// Copyright 2011 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <optional>
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

  virtual std::string GetName() const = 0;
  virtual std::string GetDisplayName() const { return GetName(); }
  virtual void InitBackendInfo() = 0;
  virtual std::optional<std::string> GetWarningMessage() const { return {}; }

  // Prepares a native window for rendering. This is called on the main thread, or the
  // thread which owns the window.
  virtual void PrepareWindow(WindowSystemInfo& wsi) {}
  // Undoes PrepareWindow
  virtual void UnPrepareWindow(WindowSystemInfo& wsi) {}

  bool Initialize(const WindowSystemInfo& wsi);
  void Shutdown();

  static std::string BadShaderFilename(const char* shader_stage, int counter);

  void Video_ExitLoop();

  void Video_OutputXFB(u32 xfb_addr, u32 fb_width, u32 fb_stride, u32 fb_height, u64 ticks);

  u32 Video_AccessEFB(EFBAccessType type, u32 x, u32 y, u32 data);
  u32 Video_GetQueryResult(PerfQueryType type);
  u16 Video_GetBoundingBox(int index);

  static std::string GetDefaultBackendName();
  static const std::vector<std::unique_ptr<VideoBackendBase>>& GetAvailableBackends();
  static void ActivateBackend(const std::string& name);

  // Fills the backend_info fields with the capabilities of the selected backend/device.
  static void PopulateBackendInfo();
  // Called by the UI thread when the graphics config is opened.
  static void PopulateBackendInfoFromUI();
  // Request a backend reload at the next call to BackendReloadIfRequested
  static void RequestBackendReload();
  // Does a full backend reload if previously requested
  static void BackendReloadIfRequested();
  // Fully reloads the backend, allowing for any graphics config to be changed
  // If run is non-null, it will be run while the backend is shut down
  static void FullBackendReload(void (*run)(void* ctx) = nullptr, void* run_ctx = nullptr);
  // FullBackendReload for being called by the CPU thread
  static void FullBackendReloadFromCPU(void (*run)(void* ctx) = nullptr, void* run_ctx = nullptr);

  // Wrapper function which pushes the event to the GPU thread.
  void DoState(PointerWrap& p);

protected:
  virtual bool InitializeBackend(const WindowSystemInfo& wsi) = 0;
  virtual void ShutdownBackend() = 0;

  void InitializeShared();
  void InitializeConfig();
  void ShutdownShared();

  bool m_initialized = false;
};

extern WindowSystemInfo g_video_wsi;
extern VideoBackendBase* g_video_backend;
