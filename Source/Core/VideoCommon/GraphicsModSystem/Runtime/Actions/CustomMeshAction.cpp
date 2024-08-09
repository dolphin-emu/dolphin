// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/Actions/CustomMeshAction.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "Common/JsonUtil.h"
#include "Core/System.h"

#include "VideoCommon/Assets/CustomAssetLoader.h"
#include "VideoCommon/GraphicsModEditor/Controls/AssetDisplay.h"
#include "VideoCommon/GraphicsModEditor/EditorEvents.h"
#include "VideoCommon/GraphicsModEditor/EditorMain.h"
#include "VideoCommon/GraphicsModSystem/Runtime/CustomTextureCache.h"

std::unique_ptr<CustomMeshAction>
CustomMeshAction::Create(const picojson::value& json_data,
                         std::shared_ptr<VideoCommon::CustomAssetLibrary> library,
                         std::shared_ptr<VideoCommon::CustomTextureCache> texture_cache)
{
  VideoCommon::CustomAssetLibrary::AssetID mesh_asset;

  if (!json_data.is<picojson::object>())
    return nullptr;

  const auto& obj = json_data.get<picojson::object>();

  if (const auto it = obj.find("mesh_asset"); it != obj.end())
  {
    if (it->second.is<std::string>())
    {
      mesh_asset = it->second.to_str();
    }
  }

  Common::Vec3 scale{1, 1, 1};
  if (const auto it = obj.find("scale"); it != obj.end())
  {
    if (it->second.is<picojson::object>())
    {
      FromJson(it->second.get<picojson::object>(), scale);
    }
  }

  Common::Vec3 translation{};
  if (const auto it = obj.find("translation"); it != obj.end())
  {
    if (it->second.is<picojson::object>())
    {
      FromJson(it->second.get<picojson::object>(), translation);
    }
  }

  Common::Vec3 rotation{};
  if (const auto it = obj.find("rotation"); it != obj.end())
  {
    if (it->second.is<picojson::object>())
    {
      FromJson(it->second.get<picojson::object>(), rotation);
    }
  }

  return std::make_unique<CustomMeshAction>(std::move(library), std::move(texture_cache),
                                            std::move(rotation), std::move(scale),
                                            std::move(translation), std::move(mesh_asset));
}

CustomMeshAction::CustomMeshAction(std::shared_ptr<VideoCommon::CustomAssetLibrary> library,
                                   std::shared_ptr<VideoCommon::CustomTextureCache> texture_cache)
    : m_library(std::move(library)), m_texture_cache(std::move(texture_cache))
{
}

CustomMeshAction::CustomMeshAction(std::shared_ptr<VideoCommon::CustomAssetLibrary> library,
                                   std::shared_ptr<VideoCommon::CustomTextureCache> texture_cache,
                                   Common::Vec3 rotation, Common::Vec3 scale,
                                   Common::Vec3 translation,
                                   VideoCommon::CustomAssetLibrary::AssetID mesh_asset_id)
    : m_library(std::move(library)), m_texture_cache(std::move(texture_cache)),
      m_mesh_asset_id(std::move(mesh_asset_id)), m_scale(std::move(scale)),
      m_rotation(std::move(rotation)), m_translation(std::move(translation))
{
}

CustomMeshAction::~CustomMeshAction() = default;

void CustomMeshAction::OnDrawStarted(GraphicsModActionData::DrawStarted* draw_started)
{
  if (!draw_started) [[unlikely]]
    return;

  if (!draw_started->mesh_chunk) [[unlikely]]
    return;

  if (!draw_started->current_mesh_index) [[unlikely]]
    return;

  if (!draw_started->more_data) [[unlikely]]
    return;

  if (!draw_started->custom_pixel_shader) [[unlikely]]
    return;

  if (!draw_started->material_uniform_buffer) [[unlikely]]
    return;

  if (m_mesh_asset_id == "") [[unlikely]]
    return;

  if (m_recalculate_original_mesh_center)
  {
    auto vert_ptr = draw_started->draw_data_view.vertex_data.data();
    Common::Vec3 center_point{};
    for (std::size_t vert_index = 0; vert_index < draw_started->draw_data_view.vertex_data.size();
         vert_index++)
    {
      float vx = 0;
      std::memcpy(&vx, vert_ptr, sizeof(float));
      center_point.x += vx;

      float vy = 0;
      std::memcpy(&vy, vert_ptr + sizeof(float), sizeof(float));
      center_point.y += vy;

      float vz = 0;
      std::memcpy(&vz, vert_ptr + sizeof(float) * 2, sizeof(float));
      center_point.z += vz;

      vert_ptr += draw_started->draw_data_view.vertex_format->GetVertexStride();
    }
    center_point.x /= draw_started->draw_data_view.vertex_data.size();
    center_point.y /= draw_started->draw_data_view.vertex_data.size();
    center_point.z /= draw_started->draw_data_view.vertex_data.size();
    m_original_mesh_center = center_point;
  }

  auto& loader = Core::System::GetInstance().GetCustomAssetLoader();

  if (!m_cached_mesh_asset.m_asset || m_mesh_asset_id != m_cached_mesh_asset.m_asset->GetAssetId())
  {
    m_cached_mesh_asset.m_asset = loader.LoadMesh(m_mesh_asset_id, m_library);
  }

  const auto mesh_data = m_cached_mesh_asset.m_asset->GetData();
  if (!mesh_data)
  {
    return;
  }

  if (m_cached_mesh_asset.m_asset->GetLastLoadedTime() > m_cached_mesh_asset.m_cached_write_time ||
      m_transform_changed || m_mesh_asset_changed)
  {
    // Hold onto a reference since our stored render chunks
    // point to this data
    m_mesh_data = mesh_data;
    m_cached_mesh_asset.m_cached_write_time = m_cached_mesh_asset.m_asset->GetLastLoadedTime();
    m_render_chunks.clear();
    for (const auto& mesh_chunk : mesh_data->m_mesh_chunks)
    {
      RenderChunk render_chunk;
      PortableVertexDeclaration vertex_declaration = mesh_chunk.vertex_declaration;
      vertex_declaration.posmtx =
          draw_started->draw_data_view.vertex_format->GetVertexDeclaration().posmtx;
      render_chunk.m_native_vertex_format = g_gfx->CreateNativeVertexFormat(vertex_declaration);
      render_chunk.m_mesh_chunk.index_data =
          std::span<u16>(mesh_chunk.indices.get(), mesh_chunk.num_indices);
      render_chunk.m_mesh_chunk.vertex_data =
          std::span<u8>(mesh_chunk.vertex_data.get(), mesh_chunk.num_vertices);
      render_chunk.m_mesh_chunk.vertex_format = render_chunk.m_native_vertex_format.get();
      render_chunk.m_mesh_chunk.vertex_stride =
          render_chunk.m_native_vertex_format->GetVertexStride();
      render_chunk.m_mesh_chunk.primitive_type = mesh_chunk.primitive_type;
      render_chunk.m_mesh_chunk.components_available = mesh_chunk.components_available;

      // TODO: this should be passed from mesh and _technically_
      // is a state of the render pipeline
      render_chunk.m_mesh_chunk.cull_mode = CullMode::None;

      const auto scale = Common::Matrix33::Scale(m_scale);
      const auto rotation = Common::Quaternion::RotateXYZ(m_rotation);

      if (m_ignore_mesh_transform)
      {
        render_chunk.m_mesh_chunk.transform = Common::Matrix44::FromMatrix33(scale) *
                                              Common::Matrix44::FromQuaternion(rotation) *
                                              Common::Matrix44::Translate(m_translation);
      }
      else
      {
        const auto mesh_chunk_center =
            (mesh_chunk.minimum_position + mesh_chunk.maximum_position) / 2.0f;
        render_chunk.m_mesh_chunk.transform =
            Common::Matrix44::Translate(m_original_mesh_center - mesh_chunk_center) *
            Common::Matrix44::FromMatrix33(scale) * Common::Matrix44::FromQuaternion(rotation) *
            Common::Matrix44::Translate(m_translation) * mesh_chunk.transform;
      }

      for (std::size_t i = 0; i < vertex_declaration.texcoords.size(); i++)
      {
        auto& texcoord = vertex_declaration.texcoords[i];
        if (texcoord.enable)
        {
          GraphicsModSystem::TextureView texture_view;
          texture_view.unit = static_cast<u8>(i);
          render_chunk.m_textures.push_back(texture_view);
        }
      }

      m_render_chunks.push_back(std::move(render_chunk));
    }
    m_transform_changed = false;
    m_mesh_asset_changed = false;
  }

  if (m_render_chunks.empty() || mesh_data->m_mesh_chunks.empty())
    return;

  auto& curr_render_chunk = m_render_chunks[*draw_started->current_mesh_index];
  const auto& curr_mesh_chunk = mesh_data->m_mesh_chunks[*draw_started->current_mesh_index];

  curr_render_chunk.m_custom_pipeline.UpdatePixelData(
      loader, m_library, m_texture_cache, curr_render_chunk.m_textures, {},
      mesh_data->m_mesh_material_to_material_asset_id[curr_mesh_chunk.material_name]);

  *draw_started->mesh_chunk = curr_render_chunk.m_mesh_chunk;
  if (!m_use_game_material)
  {
    CustomPixelShader custom_pixel_shader;
    custom_pixel_shader.custom_shader =
        curr_render_chunk.m_custom_pipeline.m_last_generated_shader_code.GetBuffer();
    custom_pixel_shader.material_uniform_block =
        curr_render_chunk.m_custom_pipeline.m_last_generated_material_code.GetBuffer();
    *draw_started->custom_pixel_shader = custom_pixel_shader;
    *draw_started->material_uniform_buffer = curr_render_chunk.m_custom_pipeline.m_material_data;
  }

  (*draw_started->current_mesh_index)++;
  if (*draw_started->current_mesh_index < mesh_data->m_mesh_chunks.size())
  {
    *draw_started->more_data = true;
  }
}

void CustomMeshAction::DrawImGui()
{
  auto& editor = Core::System::GetInstance().GetGraphicsModEditor();
  if (ImGui::CollapsingHeader("Custom mesh", ImGuiTreeNodeFlags_DefaultOpen))
  {
    if (ImGui::BeginTable("CustomMeshForm", 2))
    {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Mesh");
      ImGui::TableNextColumn();
      if (GraphicsModEditor::Controls::AssetDisplay("MeshValue", editor.GetEditorState(),
                                                    &m_mesh_asset_id, GraphicsModEditor::Mesh))
      {
        GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(m_mesh_asset_id);
        m_mesh_asset_changed = true;
      }
      ImGui::EndTable();
    }
  }
  if (ImGui::CollapsingHeader("Custom mesh transform", ImGuiTreeNodeFlags_DefaultOpen))
  {
    if (ImGui::BeginTable("CustomMeshTransform", 2))
    {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Scale");
      ImGui::TableNextColumn();
      if (ImGui::InputFloat3("##Scale", m_scale.data.data()))
      {
        GraphicsModEditor::EditorEvents::ChangeOccurredEvent::Trigger();
        m_transform_changed = true;
      }
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Rotation");
      ImGui::TableNextColumn();
      if (ImGui::InputFloat3("##Rotation", m_rotation.data.data()))
      {
        GraphicsModEditor::EditorEvents::ChangeOccurredEvent::Trigger();
        m_transform_changed = true;
      }
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Translate");
      ImGui::TableNextColumn();
      if (ImGui::InputFloat3("##Translate", m_translation.data.data()))
      {
        GraphicsModEditor::EditorEvents::ChangeOccurredEvent::Trigger();
        m_transform_changed = true;
      }
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Ignore Mesh Transform");
      ImGui::TableNextColumn();
      ImGui::SetTooltip(
          "Ignore any set mesh transform and use only apply the game's transform, this "
          "can be useful when making simple model edits with mesh dumped from Dolphin");
      if (ImGui::Checkbox("##IgnoreMeshTransform", &m_ignore_mesh_transform))
      {
        GraphicsModEditor::EditorEvents::ChangeOccurredEvent::Trigger();
        m_transform_changed = true;
      }
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Use Game Material");
      ImGui::TableNextColumn();
      ImGui::SetTooltip(
          "Ignore any set mesh material and use the game's material instead, this "
          "can be useful when making simple model or uv (leveraging a custom texture) edits");
      if (ImGui::Checkbox("##UseGameMaterial", &m_use_game_material))
      {
        GraphicsModEditor::EditorEvents::ChangeOccurredEvent::Trigger();
      }
      ImGui::EndTable();
    }
  }
}

void CustomMeshAction::SerializeToConfig(picojson::object* obj)
{
  if (!obj) [[unlikely]]
    return;

  auto& json_obj = *obj;
  json_obj.emplace("translation", ToJsonObject(m_translation));
  json_obj.emplace("scale", ToJsonObject(m_scale));
  json_obj.emplace("rotation", ToJsonObject(m_rotation));
  json_obj.emplace("mesh_asset", m_mesh_asset_id);
  json_obj.emplace("ignore_mesh_transform", m_ignore_mesh_transform);
}

std::string CustomMeshAction::GetFactoryName() const
{
  return std::string{factory_name};
}
