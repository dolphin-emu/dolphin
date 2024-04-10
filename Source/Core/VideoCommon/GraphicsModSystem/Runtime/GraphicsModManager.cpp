// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModManager.h"

#include "Core/ConfigManager.h"

#include "VideoCommon/GraphicsModSystem/Config/GraphicsModGroup.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModRuntimeBackend.h"
#include "VideoCommon/VideoConfig.h"

namespace GraphicsModSystem::Runtime
{
GraphicsModManager::GraphicsModManager() = default;
GraphicsModManager::~GraphicsModManager() = default;

bool GraphicsModManager::Initialize()
{
  if (g_ActiveConfig.bGraphicMods)
  {
    // If a config change occurred in a previous session,
    // remember the old change count value.  By setting
    // our current change count to the old value, we
    // avoid loading the stale data when we
    // check for config changes.
    const u32 old_game_mod_changes = g_ActiveConfig.graphics_mod_config ?
                                         g_ActiveConfig.graphics_mod_config->GetChangeCount() :
                                         0;
    g_ActiveConfig.graphics_mod_config =
        Config::GraphicsModGroup(SConfig::GetInstance().GetGameID());
    g_ActiveConfig.graphics_mod_config->Load();
    g_ActiveConfig.graphics_mod_config->SetChangeCount(old_game_mod_changes);
    Load(*g_ActiveConfig.graphics_mod_config);

    m_frame_present_event = AfterPresentEvent::Register(
        [this](const PresentInfo& present_info) { OnFramePresented(present_info); }, "ModManager");
  }

  return true;
}

void GraphicsModManager::Load(const Config::GraphicsModGroup& config)
{
  m_backend = std::make_unique<GraphicsModRuntimeBackend>(config);
}

const GraphicsModBackend& GraphicsModManager::GetBackend() const
{
  if (m_editor_backend)
    return *m_editor_backend;

  return *m_backend;
}

GraphicsModBackend& GraphicsModManager::GetBackend()
{
  if (m_editor_backend)
    return *m_editor_backend;

  return *m_backend;
}

void GraphicsModManager::SetEditorBackend(std::unique_ptr<GraphicsModBackend> editor_backend)
{
  m_editor_backend = std::move(editor_backend);
}

bool GraphicsModManager::IsEditorEnabled() const
{
  return true;
  // return m_editor_backend.get() != nullptr;
}

void GraphicsModManager::OnFramePresented(const PresentInfo& present_info)
{
  // Ignore duplicates
  if (present_info.reason == PresentInfo::PresentReason::VideoInterfaceDuplicate)
    return;

  if (m_editor_backend)
  {
    m_editor_backend->OnFramePresented(present_info);
  }
  else
  {
    m_backend->OnFramePresented(present_info);
  }
}
}  // namespace GraphicsModSystem::Runtime
