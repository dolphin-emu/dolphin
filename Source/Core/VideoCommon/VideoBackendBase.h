// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "Common/CommonTypes.h"
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
  virtual unsigned int PeekMessages() = 0;

  virtual bool Initialize(void* window_handle) = 0;
  virtual bool InitializeOtherThread(void* window_handle, std::thread* video_thread) = 0;
  virtual void Shutdown() = 0;
  virtual void ShutdownOtherThread() = 0;

  virtual std::string GetName() const = 0;
  virtual std::string GetDisplayName() const { return GetName(); }
  void ShowConfig(void*);
  virtual void InitBackendInfo() = 0;

  virtual void Video_Prepare() = 0;             // called from CPU-GPU thread or Video thread
  virtual void Video_PrepareOtherThread() = 0;  // called from VR thread
  void Video_ExitLoop();
  virtual void Video_AsyncTimewarpDraw();
  virtual bool Video_CanDoAsync() { return false; };
  virtual void Video_Cleanup() = 0;             // called from gl/d3d thread
  virtual void Video_CleanupOtherThread() = 0;  // called from VR thread

  void Video_BeginField(u32, u32, u32, u32, u64);

  u32 Video_AccessEFB(EFBAccessType, u32, u32, u32);
  u32 Video_GetQueryResult(PerfQueryType type);
  u16 Video_GetBoundingBox(int index);

  static void PopulateList();
  static void ClearList();
  static void ActivateBackend(const std::string& name);

  // the implementation needs not do synchronization logic, because calls to it are surrounded by
  // PauseAndLock now
  void DoState(PointerWrap& p);

  void CheckInvalidState();

public:
  std::thread* m_video_thread;

protected:
  void InitializeShared();
  void ShutdownShared();
  void CleanupShared();

  bool m_initialized = false;
  bool m_invalid = false;
};

extern std::vector<std::unique_ptr<VideoBackendBase>> g_available_video_backends;
extern VideoBackendBase* g_video_backend;
