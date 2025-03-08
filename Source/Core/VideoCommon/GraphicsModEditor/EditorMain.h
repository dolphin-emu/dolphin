// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "Common/HookableEvent.h"
#include "VideoCommon/GraphicsModEditor/SceneDumper.h"
#include "VideoCommon/GraphicsModSystem/Types.h"
#include "VideoCommon/XFMemory.h"

struct FBInfo;
class GraphicsModAction;

namespace GraphicsModEditor
{
struct DrawCallData;
struct FBCallData;
struct LightData;
struct EditorState;

namespace Panels
{
class ActiveTargetsPanel;
class AssetBrowserPanel;
class PropertiesPanel;
}  // namespace Panels
class EditorMain
{
public:
  EditorMain();
  ~EditorMain();

  bool Initialize();
  void Shutdown();

  bool IsEnabled() const { return m_enabled; }

  // Creates a new graphics mod for editing
  bool NewMod(std::string_view name, std::string_view author, std::string_view description);

  // Loads an existing graphics mod for editing
  bool LoadMod(std::string_view name);

  // Renders ImGui windows to the currently-bound framebuffer.
  // Should be called by main UI manager
  void DrawImGui();

  EditorState* GetEditorState() const;

private:
  bool RebuildState();

  void DrawMenu();

  // Saves the current editor session
  void Save();

  // Closes this editor session
  void Close();

  void OnChangeOccured();
  Common::EventHook m_change_occurred_event;
  Common::EventHook m_asset_reload_event;
  bool m_has_changes = false;
  bool m_editor_session_in_progress = false;
  bool m_enabled = false;

  // Inspect mode allows the user to look and add some basic
  // graphics mods but they can't create any new files or save
  bool m_inspect_only = false;

  std::unique_ptr<EditorState> m_state;

  std::unique_ptr<Panels::ActiveTargetsPanel> m_active_targets_panel;
  std::unique_ptr<Panels::AssetBrowserPanel> m_asset_browser_panel;
  std::unique_ptr<Panels::PropertiesPanel> m_properties_panel;

  std::string m_editor_new_mod_name;
  std::string m_editor_new_mod_author;
  std::string m_editor_new_mod_description;

  SceneDumper::RecordingRequest m_last_mesh_dump_request;
  bool m_open_mesh_dump_export_window = false;
};
}  // namespace GraphicsModEditor
