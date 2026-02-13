// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/EditorBackend.h"

#include <chrono>
#include <memory>
#include <numbers>
#include <utility>
#include <variant>

#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"
#include "Core/System.h"

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/FreeLookCamera.h"
#include "VideoCommon/GraphicsModEditor/EditorEvents.h"
#include "VideoCommon/GraphicsModEditor/MaterialGeneration.h"
#include "VideoCommon/GraphicsModEditor/ShaderGeneration.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModHash.h"
#include "VideoCommon/GraphicsModSystem/Types.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/Resources/CustomResourceManager.h"
#include "VideoCommon/ShaderGenCommon.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexShaderGen.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"

namespace GraphicsModEditor
{
EditorBackend::EditorBackend(EditorState& state) : m_state(state)
{
  m_selection_event = GetEditorEvents().items_selected_event.Register(
      [this](const auto& selected_targets) { SelectionOccurred(selected_targets); });
}

void EditorBackend::OnDraw(const GraphicsModSystem::DrawDataView& draw_data,
                           VertexManagerBase* vertex_manager)
{
  // Determine our texture tags per texture
  Common::SmallVector<GraphicsModSystem::TextureTags, 8> tags_per_texture_hash;
  for (std::size_t i = 0; i < draw_data.textures.size(); i++)
  {
    const auto& texture = draw_data.textures[i];
    if (const auto iter =
            m_state.m_user_data.m_texture_cache_id_to_user_data.find(texture.hash_name);
        iter != m_state.m_user_data.m_texture_cache_id_to_user_data.end())
    {
      tags_per_texture_hash.push_back(iter->second.tag_flags);
    }
    else
    {
      tags_per_texture_hash.push_back({});
    }
  }

  const auto hash_output = GraphicsModSystem::Runtime::GetDrawDataHash(
      m_state.m_user_data.m_hash_policy, draw_data, tags_per_texture_hash);

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

  if (m_state.m_vertex_group_dumper.IsRecording())
  {
    if (m_state.m_vertex_group_dumper.IsDrawCallInRecording(draw_call_id))
    {
      m_state.m_vertex_group_dumper.AddDataToRecording(hash_output.draw_call_id, draw_data);
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
        iter->second.create_time = now;
      }
      iter->second.draw_data.blending_state = draw_data.uid->blending_state;
      iter->second.draw_data.depth_state = draw_data.uid->depth_state;
      iter->second.draw_data.projection_type = draw_data.projection_type;
      iter->second.draw_data.projection_mat = xfmem.projection.rawProjection;
      iter->second.draw_data.rasterization_state = draw_data.uid->rasterization_state;
      iter->second.draw_data.xfb_counter = m_xfb_counter;
      iter->second.draw_data.vertex_count = draw_data.vertex_count;
      iter->second.draw_data.index_count = draw_data.index_data.size();
      iter->second.draw_data.is_gpu_skinning =
          IsDrawGPUSkinned(draw_data.vertex_format, draw_data.uid->rasterization_state.primitive);

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
                                                                  iter->second.create_time};
      m_state.m_runtime_data.m_current_xfb.m_draw_call_ids.insert(draw_call_with_time);
      iter->second.last_update_time = now;
    }
    else
    {
      GraphicsModEditor::DrawCallData data;
      data.id = hash_output.draw_call_id;
      data.material_id = hash_output.material_id;
      data.mesh_id = hash_output.mesh_id;
      data.draw_data.blending_state = draw_data.uid->blending_state;
      data.draw_data.depth_state = draw_data.uid->depth_state;
      data.draw_data.projection_type = draw_data.projection_type;
      data.draw_data.projection_mat = xfmem.projection.rawProjection;
      data.draw_data.rasterization_state = draw_data.uid->rasterization_state;
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

      data.create_time = now;
      data.last_update_time = now;
      m_state.m_runtime_data.m_draw_call_id_to_data.try_emplace(hash_output.draw_call_id,
                                                                std::move(data));
      RuntimeState::XFBData::DrawCallWithTime draw_call_with_time{hash_output.draw_call_id, now};
      m_state.m_runtime_data.m_current_xfb.m_draw_call_ids.insert(draw_call_with_time);
    }
  }

  if (m_state.m_editor_data.m_export_dolphin_material_draw_call == draw_call_id)
  {
    const std::string path = File::GetUserPath(D_DUMP_IDX) + SConfig::GetInstance().GetGameID();
    const auto path_drawcall = fmt::format("{}-{}", path, std::to_underlying(draw_call_id));
    const auto vertex_shader_code =
        GenerateVertexShaderCode(g_backend_info.api_type, m_shader_host_config,
                                 draw_data.uid->vs_uid.GetUidData(), CustomVertexContents{});
    File::WriteStringToFile(path_drawcall + ".vs.glsl", vertex_shader_code.GetBuffer());
    const auto pixel_shader_code =
        GeneratePixelShaderCode(g_backend_info.api_type, m_shader_host_config,
                                draw_data.uid->ps_uid.GetUidData(), CustomPixelContents{});
    File::WriteStringToFile(path_drawcall + ".ps.glsl", pixel_shader_code.GetBuffer());
    const auto geometry_shader_code = GenerateGeometryShaderCode(
        g_backend_info.api_type, m_shader_host_config, draw_data.uid->gs_uid.GetUidData());
    File::WriteStringToFile(path_drawcall + ".gs.glsl", geometry_shader_code.GetBuffer());

    m_state.m_editor_data.m_export_dolphin_material_draw_call = {};
  }

  if (m_state.m_editor_data.m_create_material_draw_call == draw_call_id)
  {
    ShaderCode ps_out;
    // ps_out.Write("{}\n", PixelShader::fragment_definition);
    ps_out.Write("{{\n");

    WriteFragmentBody(g_backend_info.api_type, m_shader_host_config,
                      draw_data.uid->ps_uid.GetUidData(), ps_out);

    ps_out.Write("}}\n");

    ShaderCode vs_out;
    // vs_out.Write("{}\n", VertexShader::vertex_definition);
    vs_out.Write("{{\n");

    WriteVertexBody(g_backend_info.api_type, m_shader_host_config,
                    draw_data.uid->vs_uid.GetUidData(), vs_out);

    vs_out.Write("}}\n");

    const auto material_metadata_name =
        fmt::format("{}.rastermaterial", std::to_underlying(draw_call_id));
    const auto shader_metadata_name =
        fmt::format("{}.rastershader", std::to_underlying(draw_call_id));
    const auto pixel_name = fmt::format("{}.ps.glsl", std::to_underlying(draw_call_id));
    const auto vertex_name = fmt::format("{}.vs.glsl", std::to_underlying(draw_call_id));

    GraphicsModEditor::ShaderGenerationContext shader_gen_context;
    shader_gen_context.metadata_name = shader_metadata_name;

    VideoCommon::RasterSurfaceShaderData shader_template;
    shader_template.pixel_source = ps_out.GetBuffer();
    shader_template.vertex_source = vs_out.GetBuffer();

    shader_gen_context.shader_template = std::move(shader_template);
    shader_gen_context.pixel_name = pixel_name;
    shader_gen_context.vertex_name = vertex_name;

    VideoCommon::MaterialData material_template;

    GraphicsModEditor::GenerateMaterial(
        material_metadata_name, m_state.m_user_data.m_current_mod_path, material_template,
        shader_gen_context, m_state.m_user_data.m_asset_library.get());

    m_state.m_editor_data.m_create_material_draw_call = {};
  }

  static Common::Matrix44 identity = Common::Matrix44::Identity();

  if (m_selected_objects.contains(draw_call_id))
  {
    auto& resource_manager = Core::System::GetInstance().GetCustomResourceManager();
    auto* material_resource = resource_manager.GetMaterialFromAsset(
        "editor-highlight-material", *draw_data.uid, m_state.m_editor_data.m_asset_library);
    const auto material_data = material_resource->GetData();
    if (material_data)
    {
      vertex_manager->DrawEmulatedMesh(*material_data, draw_data, identity, m_camera_manager);
    }
  }
  else if (m_state.m_editor_data.m_view_lighting &&
           draw_data.projection_type != ProjectionType::Orthographic)
  {
    auto& resource_manager = Core::System::GetInstance().GetCustomResourceManager();
    auto* material_resource = resource_manager.GetMaterialFromAsset(
        "editor-lighting-vis-material", *draw_data.uid, m_state.m_editor_data.m_asset_library);
    const auto material_data = material_resource->GetData();
    if (material_data)
    {
      vertex_manager->DrawEmulatedMesh(*material_data, draw_data, identity, m_camera_manager);
    }
  }
  else if (m_state.m_editor_data.m_view_normals &&
           draw_data.projection_type != ProjectionType::Orthographic)
  {
    auto& resource_manager = Core::System::GetInstance().GetCustomResourceManager();
    auto* material_resource = resource_manager.GetMaterialFromAsset(
        "editor-normal-vis-material", *draw_data.uid, m_state.m_editor_data.m_asset_library);
    const auto material_data = material_resource->GetData();
    if (material_data)
    {
      vertex_manager->DrawEmulatedMesh(*material_data, draw_data, identity, m_camera_manager);
    }
  }
  else
  {
    if (m_state.m_editor_data.m_disable_all_actions)
    {
      vertex_manager->DrawEmulatedMesh(m_camera_manager);
    }
    else
    {
      if (const auto iter = m_state.m_user_data.m_draw_call_id_to_actions.find(draw_call_id);
          iter != m_state.m_user_data.m_draw_call_id_to_actions.end())
      {
        CustomDraw(draw_data, vertex_manager, iter->second, hash_output.draw_call_id);
      }
      else
      {
        vertex_manager->DrawEmulatedMesh(m_camera_manager);
      }
    }
  }

  // Klunky, shouldn't be in backend...
  if (m_selected_transforms.size() == 1 && m_selected_transforms[0].first == draw_call_id)
  {
    auto vert_ptr = draw_data.vertex_data;

    // TODO: move
    // Calculate centroid...
    Common::Vec3 centroid;
    std::size_t vert_count = 0;

    const auto declaration = draw_data.vertex_format->GetVertexDeclaration();
    for (u32 vert_index = 0; vert_index < draw_data.vertex_count; vert_index++)
    {
      Common::Vec3 vertex_position;
      std::memcpy(&vertex_position.x, vert_ptr + declaration.position.offset, sizeof(float));
      std::memcpy(&vertex_position.y, vert_ptr + sizeof(float) + declaration.position.offset,
                  sizeof(float));
      std::memcpy(&vertex_position.z, vert_ptr + sizeof(float) * 2 + declaration.position.offset,
                  sizeof(float));

      centroid += vertex_position;
      vert_count++;

      vert_ptr += draw_data.vertex_format->GetVertexStride();
    }

    if (vert_count != 0)
    {
      centroid.x /= vert_count;
      centroid.y /= vert_count;
      centroid.z /= vert_count;
    }

    // TODO: orthographic is forced because the position is behind the camera for non ortho views
    // ImGuizmo::SetOrthographic(draw_data.projection_type == ProjectionType::Orthographic);
    ImGuizmo::SetOrthographic(true);
    Common::Matrix44 projection;
    std::memcpy(projection.data.data(), draw_data.projection_transform.data(), sizeof(float) * 16);
    /*if (draw_data.projection_type == ProjectionType::Orthographic)
    {
      projection.data =
          VideoCommon::ProjectionUtils::GetOrthographicMatrix(draw_data.projection_details);
    }
    else
    {
      projection.data = VideoCommon::ProjectionUtils::GetPerspectiveMatrix(
          draw_data.projection_details,
          Common::Vec2{g_ActiveConfig.fAspectRatioHackW, g_ActiveConfig.fAspectRatioHackH});
      projection *= g_freelook_camera.GetView();
      projection = Common::Matrix44::FromQuaternion(
                       Common::Quaternion::RotateY(static_cast<float>(std::numbers::pi))) *
                   projection;
      projection.data[14] *= -1;
    }*/
    Common::Matrix44 object_transform;
    std::memcpy(object_transform.data.data(), draw_data.object_transform.data(),
                sizeof(float) * 12);
    // Initialize last four values...
    object_transform.data[12] = 0;
    object_transform.data[13] = 0;
    object_transform.data[14] = 0;
    object_transform.data[15] = 1;

    const Common::Matrix44 delta = *m_selected_transforms[0].second;
    const Common::Matrix44 view = object_transform * Common::Matrix44::Translate(centroid);
    const Common::Matrix44 view_t = view.Transposed();
    Common::Matrix44 delta_t = delta.Transposed();
    Common::Matrix44 object_t = delta_t;
    if (ImGuizmo::Manipulate(view_t.data.data(), projection.Transposed().data.data(),
                             m_transform_operation, ImGuizmo::LOCAL, object_t.data.data(),
                             delta_t.data.data()))
    {
      *m_selected_transforms[0].second *= delta_t.Transposed();
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
    data.id = std::move(texture_cache_id);
    data.create_time = std::chrono::steady_clock::now();
    iter = m_state.m_runtime_data.m_texture_cache_id_to_data.emplace_hint(iter, texture.hash_name,
                                                                          std::move(data));
  }
  else
  {
    m_state.m_runtime_data.m_current_xfb.m_texture_cache_ids.insert(std::move(texture_cache_id));
  }
  iter->second.texture = texture;
  iter->second.active = true;
  iter->second.last_load_time = std::chrono::steady_clock::now();
}

void EditorBackend::OnTextureUnload(GraphicsModSystem::TextureType, std::string_view texture_hash)
{
  if (auto iter = m_state.m_runtime_data.m_texture_cache_id_to_data.find(texture_hash);
      iter != m_state.m_runtime_data.m_texture_cache_id_to_data.end())
  {
    iter->second.active = false;
  }
}

void EditorBackend::OnTextureCreate(const GraphicsModSystem::TextureView& texture)
{
  GraphicsModSystem::Runtime::GraphicsModBackend::OnTextureCreate(texture);

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
  data.id = texture_cache_id;
  data.texture = texture;
  data.create_time = std::chrono::steady_clock::now();
  m_state.m_runtime_data.m_texture_cache_id_to_data.insert_or_assign(std::move(texture_cache_id),
                                                                     std::move(data));
}

void EditorBackend::OnLight()
{
}

void EditorBackend::OnFramePresented(const PresentInfo& present_info)
{
  GraphicsModSystem::Runtime::GraphicsModBackend::OnFramePresented(present_info);

  for (const auto& action : m_state.m_user_data.m_actions)
  {
    action->OnFrameEnd();
  }

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
  m_selected_transforms.clear();

  for (const auto& selected_object : m_selected_objects)
  {
    if (const auto action = std::get_if<GraphicsModAction*>(&selected_object))
    {
      if (const auto transform = (*action)->GetTransform())
        m_selected_transforms.emplace_back((*action)->GetDrawCall(), transform);
    }
  }
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
