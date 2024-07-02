// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <set>

#include "Common/HookableEvent.h"
#include "VideoCommon/GraphicsModEditor/EditorState.h"
#include "VideoCommon/GraphicsModEditor/EditorTypes.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModBackend.h"

namespace GraphicsModEditor
{
class EditorBackend final : public GraphicsModSystem::Runtime::GraphicsModBackend
{
public:
  explicit EditorBackend(EditorState&);
  void OnDraw(const GraphicsModSystem::DrawDataView& draw_started,
              VertexManagerBase* vertex_manager) override;
  void OnTextureLoad(const GraphicsModSystem::TextureView& texture) override;
  void OnTextureUnload(GraphicsModSystem::TextureType texture_type,
                       std::string_view texture_hash) override;
  void OnTextureCreate(const GraphicsModSystem::TextureView& texture) override;
  void OnLight() override;
  void OnFramePresented(const PresentInfo& present_info) override;

  void AddIndices(OpcodeDecoder::Primitive primitive, u32 num_vertices) override;
  void ResetIndices() override;

private:
  void SelectionOccurred(const std::set<SelectableType>& selection);

  EditorState& m_state;
  Common::EventHook m_selection_event;
  std::set<SelectableType> m_selected_objects;

  u64 m_xfb_counter = 0;
};
}  // namespace GraphicsModEditor
