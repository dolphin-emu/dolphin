// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "Common/HookableEvent.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModBackend.h"

namespace GraphicsModSystem::Config
{
class GraphicsModGroup;
}

namespace GraphicsModSystem::Runtime
{
class GraphicsModRuntimeBackend;
class GraphicsModManager
{
public:
  GraphicsModManager();
  ~GraphicsModManager();

  bool Initialize();
  void Load(const GraphicsModSystem::Config::GraphicsModGroup& config);

  const GraphicsModBackend& GetBackend() const;
  GraphicsModBackend& GetBackend();

  void SetEditorBackend(std::unique_ptr<GraphicsModBackend> editor_backend);
  bool IsEditorEnabled() const;

private:
  void OnFramePresented(const PresentInfo& present_info);

  std::unique_ptr<GraphicsModRuntimeBackend> m_backend;
  std::unique_ptr<GraphicsModBackend> m_editor_backend;
  Common::EventHook m_frame_present_event;
};
}  // namespace GraphicsModSystem::Runtime
