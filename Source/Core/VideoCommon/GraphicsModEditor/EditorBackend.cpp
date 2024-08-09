// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/EditorBackend.h"

#include <chrono>

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/GraphicsModEditor/EditorEvents.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModHash.h"
#include "VideoCommon/GraphicsModSystem/Types.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/XFMemory.h"

namespace GraphicsModEditor
{
EditorBackend::EditorBackend(EditorState& state) : m_state(state)
{
  m_selection_event = EditorEvents::ItemsSelectedEvent::Register(
      [this](const auto& selected_targets) { SelectionOccurred(selected_targets); },
      "EditorBackendSelect");
}

void EditorBackend::OnDraw(const GraphicsModSystem::DrawDataView& draw_data,
                           VertexManagerBase* vertex_manager)
{
  const auto hash_output =
      GraphicsModSystem::Runtime::GetDrawDataHash(m_state.m_user_data.m_hash_policy, draw_data);

  const GraphicsModSystem::DrawCallID draw_call_id =
      GetSkinnedDrawCallID(hash_output.draw_call_id, hash_output.material_id, draw_data);

  if (m_state.m_scene_dumper.IsRecording())
  {
    if (m_state.m_scene_dumper.IsDrawCallInRecording(draw_call_id))
    {
      GraphicsModEditor::SceneDumper::AdditionalDrawData additional_draw_data;
      if (!draw_data.vertex_format->GetVertexDeclaration().posmtx.enable)
      {
        float* pos = &xfmem.posMatrices[g_main_cp_state.matrix_index_a.PosNormalMtxIdx * 4];
        additional_draw_data.transform = {pos, 12};
      }
      m_state.m_scene_dumper.AddDataToRecording(hash_output.draw_call_id, draw_data,
                                                std::move(additional_draw_data));
    }
  }

  if (draw_call_id == hash_output.draw_call_id)
  {
    const auto now = std::chrono::steady_clock::now();
    if (auto iter = m_state.m_runtime_data.m_draw_call_id_to_data.find(hash_output.draw_call_id);
        iter != m_state.m_runtime_data.m_draw_call_id_to_data.end())
    {
      if (m_xfb_counter > (iter->second.draw_data.xfb_counter + 1))
      {
        iter->second.m_create_time = now;
      }
      iter->second.draw_data.blending_state = draw_data.blending_state;
      iter->second.draw_data.depth_state = draw_data.depth_state;
      iter->second.draw_data.projection_type = draw_data.projection_type;
      iter->second.draw_data.rasterization_state = draw_data.rasterization_state;
      iter->second.draw_data.xfb_counter = m_xfb_counter;
      iter->second.draw_data.vertex_count = draw_data.vertex_data.size();
      iter->second.draw_data.index_count = draw_data.index_data.size();

      Common::SmallVector<GraphicsModSystem::Texture, 8> textures;
      for (const auto& texture_view : draw_data.textures)
      {
        GraphicsModSystem::Texture texture;
        texture.hash_name = std::string{texture_view.hash_name};
        texture.texture_type = texture_view.texture_type;
        texture.unit = texture_view.unit;
        textures.push_back(std::move(texture));
      }
      iter->second.draw_data.textures = textures;
      iter->second.draw_data.samplers = draw_data.samplers;
      RuntimeState::XFBData::DrawCallWithTime draw_call_with_time{hash_output.draw_call_id,
                                                                  iter->second.m_create_time};
      m_state.m_runtime_data.m_current_xfb.m_draw_call_ids.insert(draw_call_with_time);
      iter->second.m_last_update_time = now;
    }
    else
    {
      GraphicsModEditor::DrawCallData data;
      data.m_id = hash_output.draw_call_id;
      data.draw_data.blending_state = draw_data.blending_state;
      data.draw_data.depth_state = draw_data.depth_state;
      data.draw_data.projection_type = draw_data.projection_type;
      data.draw_data.rasterization_state = draw_data.rasterization_state;
      data.draw_data.xfb_counter = m_xfb_counter;

      Common::SmallVector<GraphicsModSystem::Texture, 8> textures;
      for (const auto& texture_view : draw_data.textures)
      {
        GraphicsModSystem::Texture texture;
        texture.hash_name = std::string{texture_view.hash_name};
        texture.texture_type = texture_view.texture_type;
        texture.unit = texture_view.unit;
        textures.push_back(std::move(texture));
      }
      data.draw_data.textures = textures;
      data.draw_data.samplers = draw_data.samplers;

      data.m_create_time = now;
      data.m_last_update_time = now;
      m_state.m_runtime_data.m_draw_call_id_to_data.try_emplace(hash_output.draw_call_id,
                                                                std::move(data));
      RuntimeState::XFBData::DrawCallWithTime draw_call_with_time{hash_output.draw_call_id, now};
      m_state.m_runtime_data.m_current_xfb.m_draw_call_ids.insert(draw_call_with_time);
    }
  }

  if (m_selected_objects.contains(draw_call_id))
  {
    std::array<GraphicsModAction*, 1> actions{m_state.m_editor_data.m_highlight_action.get()};
    CustomDraw(draw_data, vertex_manager, actions);
  }
  else if (m_state.m_editor_data.m_view_lighting)
  {
    std::array<GraphicsModAction*, 1> actions{
        m_state.m_editor_data.m_simple_light_visualization_action.get()};
    CustomDraw(draw_data, vertex_manager, actions);
  }
  else
  {
    if (m_state.m_editor_data.m_disable_all_actions)
    {
      vertex_manager->DrawEmulatedMesh();
    }
    else
    {
      if (const auto iter = m_state.m_user_data.m_draw_call_id_to_actions.find(draw_call_id);
          iter != m_state.m_user_data.m_draw_call_id_to_actions.end())
      {
        CustomDraw(draw_data, vertex_manager, iter->second);
      }
      else
      {
        vertex_manager->DrawEmulatedMesh();
      }
    }
  }
}

void EditorBackend::OnTextureLoad(const GraphicsModSystem::TextureView& texture)
{
  std::string texture_cache_id{texture.hash_name};
  auto iter = m_state.m_runtime_data.m_texture_cache_id_to_data.lower_bound(texture.hash_name);
  if (iter == m_state.m_runtime_data.m_texture_cache_id_to_data.end() ||
      iter->first != texture.hash_name)
  {
    m_state.m_runtime_data.m_current_xfb.m_texture_cache_ids.insert(texture_cache_id);

    GraphicsModEditor::TextureCacheData data;
    data.m_id = std::move(texture_cache_id);
    data.m_create_time = std::chrono::steady_clock::now();
    iter = m_state.m_runtime_data.m_texture_cache_id_to_data.emplace_hint(iter, texture.hash_name,
                                                                          std::move(data));
  }
  else
  {
    m_state.m_runtime_data.m_current_xfb.m_texture_cache_ids.insert(std::move(texture_cache_id));
  }
  iter->second.texture = texture;
  iter->second.m_active = true;
  iter->second.m_last_load_time = std::chrono::steady_clock::now();
}

void EditorBackend::OnTextureUnload(GraphicsModSystem::TextureType, std::string_view texture_hash)
{
  if (auto iter = m_state.m_runtime_data.m_texture_cache_id_to_data.find(texture_hash);
      iter != m_state.m_runtime_data.m_texture_cache_id_to_data.end())
  {
    iter->second.m_active = false;
  }
}

void EditorBackend::OnTextureCreate(const GraphicsModSystem::TextureView& texture)
{
  if (texture.texture_type == GraphicsModSystem::TextureType::XFB)
  {
    // Skip XFBs that have no draw calls
    if (m_state.m_runtime_data.m_current_xfb.m_draw_call_ids.empty())
      return;

    // TODO: use string_view directly in C++26 (or use a different container)
    const std::string xfb_hash_name{texture.hash_name};
    m_state.m_runtime_data.m_xfb_to_data[xfb_hash_name] =
        std::move(m_state.m_runtime_data.m_current_xfb);
    m_state.m_runtime_data.m_current_xfb = {};

    m_state.m_scene_dumper.OnXFBCreated(xfb_hash_name);
    m_xfb_counter++;
    return;
  }

  std::string texture_cache_id{texture.hash_name};
  GraphicsModEditor::TextureCacheData data;
  data.m_id = texture_cache_id;
  data.texture = texture;
  data.m_create_time = std::chrono::steady_clock::now();
  m_state.m_runtime_data.m_texture_cache_id_to_data.insert_or_assign(std::move(texture_cache_id),
                                                                     std::move(data));
}

void EditorBackend::OnLight()
{
}

void EditorBackend::OnFramePresented(const PresentInfo& present_info)
{
  for (const auto& action : m_state.m_user_data.m_actions)
  {
    action->OnFrameEnd();
  }

  m_state.m_editor_data.m_highlight_action->OnFrameEnd();

  // Clear out the old xfbs presented
  for (const auto& xfb_presented : m_state.m_runtime_data.m_xfbs_presented)
  {
    m_state.m_runtime_data.m_xfb_to_data.erase(xfb_presented);
  }

  // Set the new ones
  m_state.m_runtime_data.m_xfbs_presented.clear();
  for (const auto& xfb_presented : present_info.xfb_copy_hashes)
  {
    m_state.m_runtime_data.m_xfbs_presented.push_back(std::string{xfb_presented});
  }

  m_state.m_scene_dumper.OnFramePresented(m_state.m_runtime_data.m_xfbs_presented);
}

void EditorBackend::SelectionOccurred(const std::set<SelectableType>& selection)
{
  m_selected_objects = selection;
}

void EditorBackend::AddIndices(OpcodeDecoder::Primitive primitive, u32 num_vertices)
{
  m_state.m_scene_dumper.AddIndices(primitive, num_vertices);
}

void EditorBackend::ResetIndices()
{
  m_state.m_scene_dumper.ResetIndices();
}
}  // namespace GraphicsModEditor
