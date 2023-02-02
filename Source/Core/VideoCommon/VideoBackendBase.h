// Copyright 2011 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/WindowSystemInfo.h"

class AbstractGfx;
class BoundingBox;
class Renderer;
class TextureCacheBase;
class VertexManagerBase;
class PerfQueryBase;

class VideoBackendBase
{
public:
  virtual ~VideoBackendBase() {}
  virtual bool Initialize(const WindowSystemInfo& wsi) { return true; }
  virtual void Shutdown() {}

  virtual std::string GetName() const = 0;
  virtual std::string GetDisplayName() const { return GetName(); }
  virtual void InitBackendInfo() = 0;
  virtual std::optional<std::string> GetWarningMessage() const { return {}; }

  // The various subclassed components that backends must supply
  virtual std::unique_ptr<AbstractGfx> CreateGfx() = 0;
  virtual std::unique_ptr<VertexManagerBase> CreateVertexManager() = 0;
  virtual std::unique_ptr<PerfQueryBase> CreatePerfQuery() = 0;
  virtual std::unique_ptr<BoundingBox> CreateBoundingBox() = 0;

  // Optional components that backends only VideoSoftware and VideoNull override
  virtual std::unique_ptr<Renderer> CreateRenderer();
  virtual std::unique_ptr<TextureCacheBase> CreateTextureCache();

  // Prepares a native window for rendering. This is called on the main thread, or the
  // thread which owns the window.
  virtual void PrepareWindow(WindowSystemInfo& wsi) { m_wsi = wsi; }

  // filename to save shader errors to
  std::string BadShaderFilename(const char* shader_stage, int counter) const;

  static std::string GetDefaultBackendName();
  static const std::vector<std::unique_ptr<VideoBackendBase>>& GetAvailableBackends();

  // Maps config strings to backends.
  static VideoBackendBase* GetConfiguredBackend();

  // Fills the backend_info fields with the capabilities of the selected backend/device.
  void PopulateBackendInfo();
  // Called by the UI thread when the graphics config is opened.
  static void PopulateBackendInfoFromUI();

protected:
  bool m_initialized = false;
  WindowSystemInfo m_wsi;
};
