// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/SceneDumper.h"

#include <array>
#include <map>
#include <string>

#include <fmt/format.h>
#include <tinygltf/tiny_gltf.h>

#include "Common/EnumUtils.h"
#include "Common/Logging/Log.h"

#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModBackend.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/TextureUtils.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VideoEvents.h"

namespace
{
int ComponentFormatAndIntegerToComponentType(ComponentFormat format, bool integer)
{
  switch (format)
  {
  case ComponentFormat::Byte:
    return TINYGLTF_COMPONENT_TYPE_BYTE;
  case ComponentFormat::InvalidFloat5:
  case ComponentFormat::InvalidFloat6:
  case ComponentFormat::InvalidFloat7:
  case ComponentFormat::Float:
  {
    if (integer)
    {
      return TINYGLTF_COMPONENT_TYPE_INT;
    }
    else
    {
      return TINYGLTF_COMPONENT_TYPE_FLOAT;
    }
  }
  case ComponentFormat::Short:
    return TINYGLTF_COMPONENT_TYPE_SHORT;
  case ComponentFormat::UByte:
    return TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
  case ComponentFormat::UShort:
    return TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
  };

  return -1;
}

int ComponentCountToType(std::size_t count)
{
  if (count == 1)
    return TINYGLTF_TYPE_SCALAR;
  else if (count == 2)
    return TINYGLTF_TYPE_VEC2;
  else if (count == 3)
    return TINYGLTF_TYPE_VEC3;
  else if (count == 4)
    return TINYGLTF_TYPE_VEC4;
  return -1;
}

int PrimitiveTypeToMode(PrimitiveType type)
{
  switch (type)
  {
  case PrimitiveType::Points:
    return TINYGLTF_MODE_POINTS;
  case PrimitiveType::Lines:
    return TINYGLTF_MODE_LINE;
  case PrimitiveType::Triangles:
    return TINYGLTF_MODE_TRIANGLES;
  case PrimitiveType::TriangleStrip:
    return TINYGLTF_MODE_TRIANGLE_STRIP;
  };

  return -1;
}
}  // namespace

namespace GraphicsModEditor
{
struct SceneDumper::SceneDumperImpl
{
  SceneDumperImpl()
  {
    m_model.asset.version = "2.0";
    m_model.asset.generator = "Dolphin emulator";
  }

  using NodeList = std::vector<tinygltf::Node>;
  std::map<std::string, NodeList, std::less<>> m_xfb_hash_to_scene;
  NodeList m_current_node_list;
  tinygltf::Model m_model;

  bool AreAllXFBsCapture(std::span<std::string> xfbs_presented)
  {
    if (m_xfb_hash_to_scene.empty())
      return false;

    return std::all_of(xfbs_presented.begin(), xfbs_presented.end(),
                       [this](const std::string& xfb_presented) {
                         return m_xfb_hash_to_scene.contains(xfb_presented);
                       });
  }

  void SaveScenesToFile(const std::string& path, std::span<std::string> xfbs_presented)
  {
    const bool embed_images = false;
    const bool embed_buffers = false;
    const bool pretty_print = true;
    const bool write_binary = false;

    for (const auto& xfb_presented : xfbs_presented)
    {
      if (const auto it = m_xfb_hash_to_scene.find(xfb_presented); it != m_xfb_hash_to_scene.end())
      {
        for (auto&& node : it->second)
        {
          m_model.nodes.push_back(std::move(node));
        }
      }
    }

    tinygltf::TinyGLTF gltf;
    if (!gltf.WriteGltfSceneToFile(&m_model, path, embed_images, embed_buffers, pretty_print,
                                   write_binary))
    {
      ERROR_LOG_FMT(VIDEO, "Failed to write GLTF file to '{}'", path);
    }
  }
};

SceneDumper::SceneDumper() : m_index_buffer(VertexManagerBase::MAXIBUFFERSIZE)
{
  m_impl = std::make_unique<SceneDumperImpl>();
  m_index_generator.Init(true);
  m_index_generator.Start(m_index_buffer.data());
}

SceneDumper::~SceneDumper() = default;

bool SceneDumper::IsDrawCallInRecording(GraphicsModSystem::DrawCallID draw_call_id) const
{
  return m_record_request.m_draw_call_ids.empty() ||
         m_record_request.m_draw_call_ids.contains(draw_call_id);
}

void SceneDumper::AddDataToRecording(GraphicsModSystem::DrawCallID draw_call_id,
                                     const GraphicsModSystem::DrawDataView& draw_data,
                                     AdditionalDrawData additional_draw_data)
{
  const auto& declaration = draw_data.vertex_format->GetVertexDeclaration();

  // Skip 2 point position elements for now
  if (declaration.position.enable && declaration.position.components == 2)
    return;

  // Initial primitive data
  tinygltf::Primitive primitive;
  if (draw_data.rasterization_state.primitive == PrimitiveType::TriangleStrip)
  {
    // Triangle strips are used for primitive restart but we don't support that in GLTF
    primitive.mode = PrimitiveTypeToMode(PrimitiveType::Triangles);
  }
  else
  {
    primitive.mode = PrimitiveTypeToMode(draw_data.rasterization_state.primitive);
  }
  primitive.indices = static_cast<int>(m_impl->m_model.accessors.size());

  u32 available_texcoords = 0;
  for (std::size_t i = 0; i < declaration.texcoords.size(); i++)
  {
    if (declaration.texcoords[i].enable && declaration.texcoords[i].components == 2)
    {
      available_texcoords++;
    }
  }

  // Material data
  if (draw_data.textures.empty() || available_texcoords == 0 ||
      !m_record_request.m_include_materials)
  {
    const std::string default_material_name = "Dolphin_Default_Material";
    if (const auto iter = m_materialhash_to_material_id.find(default_material_name);
        iter != m_materialhash_to_material_id.end())
    {
      primitive.material = iter->second;
    }
    else
    {
      tinygltf::Material material;
      material.pbrMetallicRoughness.baseColorFactor = {1.0f, 1.0f, 1.0f, 1.0f};
      material.doubleSided = true;
      material.name = default_material_name;

      primitive.material = static_cast<int>(m_impl->m_model.materials.size());
      m_impl->m_model.materials.push_back(material);
      m_materialhash_to_material_id[default_material_name] = primitive.material;
    }
  }
  else
  {
    std::string texture_material_name = "";
    for (const auto& texture : draw_data.textures)
    {
      texture_material_name += texture.hash_name;
    }
    if (const auto iter = m_materialhash_to_material_id.find(texture_material_name);
        iter != m_materialhash_to_material_id.end())
    {
      primitive.material = iter->second;
    }
    else
    {
      tinygltf::Material material;
      material.pbrMetallicRoughness.baseColorFactor = {1.0f, 1.0f, 1.0f, 1.0f};
      material.doubleSided = true;
      material.name = texture_material_name;
      if (draw_data.blending_state.blendenable && m_record_request.m_enable_blending)
      {
        material.alphaMode = "BLEND";
      }
      for (const auto& texture : draw_data.textures)
      {
        if (material.pbrMetallicRoughness.baseColorTexture.index != -1)
        {
          break;
        }

        if (const auto texture_iter = m_texturehash_to_texture_id.find(texture.hash_name);
            texture_iter != m_texturehash_to_texture_id.end())
        {
          material.pbrMetallicRoughness.baseColorTexture.index = texture_iter->second;
        }
        else
        {
          VideoCommon::TextureUtils::DumpTexture(*texture.texture_data,
                                                 std::string{texture.hash_name}, 0, false);

          tinygltf::Texture tinygltf_texture;
          tinygltf_texture.source = static_cast<int>(m_impl->m_model.images.size());

          tinygltf::Image image;
          image.uri = fmt::format("{}.png", texture.hash_name);
          m_impl->m_model.images.push_back(image);

          material.pbrMetallicRoughness.baseColorTexture.index =
              static_cast<int>(m_impl->m_model.textures.size());
          m_impl->m_model.textures.push_back(tinygltf_texture);

          m_texturehash_to_texture_id[std::string{texture.hash_name}] =
              material.pbrMetallicRoughness.baseColorTexture.index;
        }
      }
      primitive.material = static_cast<int>(m_impl->m_model.materials.size());
      m_materialhash_to_material_id[texture_material_name] = primitive.material;
      m_impl->m_model.materials.push_back(material);
    }
  }

  // Index data
  tinygltf::Accessor index_accessor;
  index_accessor.name = fmt::format("Accessor 'INDEX' for {}", Common::ToUnderlying(draw_call_id));
  index_accessor.bufferView = static_cast<int>(m_impl->m_model.bufferViews.size());
  index_accessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
  index_accessor.byteOffset = 0;
  index_accessor.type = TINYGLTF_TYPE_SCALAR;
  index_accessor.count = m_index_generator.GetIndexLen();

  auto indice_ptr = reinterpret_cast<const u8*>(m_index_generator.GetIndexDataStart());
  for (std::size_t indice_index = 0; indice_index < m_index_generator.GetIndexLen(); indice_index++)
  {
    u16 index = 0;
    std::memcpy(&index, indice_ptr + index_accessor.byteOffset, sizeof(u16));

    if (index_accessor.minValues.empty())
    {
      index_accessor.minValues.push_back(static_cast<double>(index));
    }
    else
    {
      u16 last_min = static_cast<u16>(index_accessor.minValues[0]);

      if (index < last_min)
        index_accessor.minValues[0] = index;
    }

    if (index_accessor.maxValues.empty())
    {
      index_accessor.maxValues.push_back(static_cast<double>(index));
    }
    else
    {
      u16 last_max = static_cast<u16>(index_accessor.maxValues[0]);

      if (index > last_max)
        index_accessor.maxValues[0] = index;
    }
    indice_ptr += sizeof(u16);
  }

  m_impl->m_model.accessors.push_back(std::move(index_accessor));

  tinygltf::BufferView index_buffer_view;
  index_buffer_view.buffer = static_cast<int>(m_impl->m_model.buffers.size());
  index_buffer_view.byteLength = m_index_generator.GetIndexLen() * sizeof(u16);
  index_buffer_view.byteOffset = 0;
  index_buffer_view.target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;
  m_impl->m_model.bufferViews.push_back(std::move(index_buffer_view));

  tinygltf::Buffer index_buffer;

  const auto index_data =
      reinterpret_cast<const unsigned char*>(m_index_generator.GetIndexDataStart());
  index_buffer.data = {index_data, index_data + index_buffer_view.byteLength};
  m_impl->m_model.buffers.push_back(std::move(index_buffer));

  // Fill out all vertex data
  const std::size_t stride = declaration.stride;

  Common::Vec3 origin_skinned;
  if (declaration.position.enable)
  {
    primitive.attributes["POSITION"] = static_cast<int>(m_impl->m_model.accessors.size());

    tinygltf::Accessor accessor;
    accessor.name = fmt::format("Accessor 'POSITION' for {}", Common::ToUnderlying(draw_call_id));
    accessor.bufferView = static_cast<int>(m_impl->m_model.bufferViews.size());
    accessor.componentType = ComponentFormatAndIntegerToComponentType(declaration.position.type,
                                                                      declaration.position.integer);
    accessor.byteOffset = declaration.position.offset;
    accessor.type = ComponentCountToType(declaration.position.components);
    accessor.count = draw_data.vertex_data.size();

    auto vert_ptr = reinterpret_cast<const u8*>(draw_data.vertex_data.data());

    // If gpu skinning is turned on but transforms is turned off, we need to calculate
    // what the positions are when centered at the origin
    std::optional<Common::Vec3> min_position_skinned;
    std::optional<Common::Vec3> max_position_skinned;
    if (m_record_request.m_apply_gpu_skinning && !m_record_request.m_include_transform &&
        !draw_data.gpu_skinning_position_transform.empty() && declaration.posmtx.enable)
    {
      for (std::size_t vert_index = 0; vert_index < draw_data.vertex_data.size(); vert_index++)
      {
        Common::Vec3 vertex_position;
        std::memcpy(&vertex_position.x, vert_ptr + accessor.byteOffset, sizeof(float));
        std::memcpy(&vertex_position.y, vert_ptr + sizeof(float) + accessor.byteOffset,
                    sizeof(float));
        std::memcpy(&vertex_position.z, vert_ptr + sizeof(float) * 2 + accessor.byteOffset,
                    sizeof(float));

        u32 gpu_skin_index;
        std::memcpy(&gpu_skin_index, vert_ptr + declaration.posmtx.offset, sizeof(u32));

        Common::Matrix44 position_transform;
        for (std::size_t i = 0; i < 3; i++)
        {
          position_transform.data[i * 4 + 0] =
              draw_data.gpu_skinning_position_transform[gpu_skin_index + i][0];
          position_transform.data[i * 4 + 1] =
              draw_data.gpu_skinning_position_transform[gpu_skin_index + i][1];
          position_transform.data[i * 4 + 2] =
              draw_data.gpu_skinning_position_transform[gpu_skin_index + i][2];
          position_transform.data[i * 4 + 3] =
              draw_data.gpu_skinning_position_transform[gpu_skin_index + i][3];
        }
        position_transform.data[12] = 0;
        position_transform.data[13] = 0;
        position_transform.data[14] = 0;
        position_transform.data[15] = 1;

        // Apply the transform to the position
        Common::Vec4 vp(vertex_position, 1);
        vp = position_transform * vp;
        vertex_position.x = vp.x;
        vertex_position.y = vp.y;
        vertex_position.z = vp.z;

        if (!min_position_skinned)
        {
          min_position_skinned = vertex_position;
        }
        else
        {
          if (vertex_position.x < min_position_skinned->x)
            min_position_skinned->x = vertex_position.x;
          if (vertex_position.y < min_position_skinned->y)
            min_position_skinned->y = vertex_position.y;
          if (vertex_position.z < min_position_skinned->z)
            min_position_skinned->z = vertex_position.z;
        }

        if (!max_position_skinned)
        {
          max_position_skinned = vertex_position;
        }
        else
        {
          if (vertex_position.x > max_position_skinned->x)
            max_position_skinned->x = vertex_position.x;
          if (vertex_position.y > max_position_skinned->y)
            max_position_skinned->y = vertex_position.y;
          if (vertex_position.z > max_position_skinned->z)
            max_position_skinned->z = vertex_position.z;
        }
        vert_ptr += stride;
      }
    }
    if (min_position_skinned && max_position_skinned)
    {
      origin_skinned = (*max_position_skinned - *min_position_skinned) / 2.0f;
    }
    else if (min_position_skinned)
    {
      origin_skinned = -*min_position_skinned / 2.0f;
    }
    else if (max_position_skinned)
    {
      origin_skinned = *max_position_skinned / 2.0f;
    }

    vert_ptr = reinterpret_cast<const u8*>(draw_data.vertex_data.data());
    for (std::size_t vert_index = 0; vert_index < draw_data.vertex_data.size(); vert_index++)
    {
      Common::Vec3 vertex_position;
      std::memcpy(&vertex_position.x, vert_ptr + accessor.byteOffset, sizeof(float));
      std::memcpy(&vertex_position.y, vert_ptr + sizeof(float) + accessor.byteOffset,
                  sizeof(float));
      std::memcpy(&vertex_position.z, vert_ptr + sizeof(float) * 2 + accessor.byteOffset,
                  sizeof(float));

      if (m_record_request.m_apply_gpu_skinning &&
          !draw_data.gpu_skinning_position_transform.empty() && declaration.posmtx.enable)
      {
        u32 gpu_skin_index;
        std::memcpy(&gpu_skin_index, vert_ptr + declaration.posmtx.offset, sizeof(u32));

        Common::Matrix44 position_transform;
        for (std::size_t i = 0; i < 3; i++)
        {
          position_transform.data[i * 4 + 0] =
              draw_data.gpu_skinning_position_transform[gpu_skin_index + i][0];
          position_transform.data[i * 4 + 1] =
              draw_data.gpu_skinning_position_transform[gpu_skin_index + i][1];
          position_transform.data[i * 4 + 2] =
              draw_data.gpu_skinning_position_transform[gpu_skin_index + i][2];
          position_transform.data[i * 4 + 3] =
              draw_data.gpu_skinning_position_transform[gpu_skin_index + i][3];
        }
        position_transform.data[12] = 0;
        position_transform.data[13] = 0;
        position_transform.data[14] = 0;
        position_transform.data[15] = 1;

        // Apply the transform to the position
        Common::Vec4 vp(vertex_position, 1);
        vp = position_transform * vp;
        vertex_position.x = vp.x;
        vertex_position.y = vp.y;
        vertex_position.z = vp.z;

        if (!m_record_request.m_include_transform)
        {
          vertex_position -= origin_skinned;
        }
      }

      if (accessor.minValues.empty())
      {
        accessor.minValues.push_back(static_cast<double>(vertex_position.x));
        accessor.minValues.push_back(static_cast<double>(vertex_position.y));
        accessor.minValues.push_back(static_cast<double>(vertex_position.z));
      }
      else
      {
        const float last_min_x = static_cast<float>(accessor.minValues[0]);
        const float last_min_y = static_cast<float>(accessor.minValues[1]);
        const float last_min_z = static_cast<float>(accessor.minValues[2]);

        if (vertex_position.x < last_min_x)
          accessor.minValues[0] = vertex_position.x;
        if (vertex_position.y < last_min_y)
          accessor.minValues[1] = vertex_position.y;
        if (vertex_position.z < last_min_z)
          accessor.minValues[2] = vertex_position.z;
      }

      if (accessor.maxValues.empty())
      {
        accessor.maxValues.push_back(static_cast<double>(vertex_position.x));
        accessor.maxValues.push_back(static_cast<double>(vertex_position.y));
        accessor.maxValues.push_back(static_cast<double>(vertex_position.z));
      }
      else
      {
        const float last_max_x = static_cast<float>(accessor.maxValues[0]);
        const float last_max_y = static_cast<float>(accessor.maxValues[1]);
        const float last_max_z = static_cast<float>(accessor.maxValues[2]);

        if (vertex_position.x > last_max_x)
          accessor.maxValues[0] = vertex_position.x;
        if (vertex_position.y > last_max_y)
          accessor.maxValues[1] = vertex_position.y;
        if (vertex_position.z > last_max_z)
          accessor.maxValues[2] = vertex_position.z;
      }
      vert_ptr += stride;
    }

    m_impl->m_model.accessors.push_back(std::move(accessor));
  }

  static std::array<std::string, 2> color_names{"COLOR_0", "COLOR_1"};
  for (std::size_t i = 0; i < declaration.colors.size(); i++)
  {
    if (declaration.colors[i].enable)
    {
      primitive.attributes[color_names[i]] = static_cast<int>(m_impl->m_model.accessors.size());

      tinygltf::Accessor accessor;
      accessor.name =
          fmt::format("Accessor '{}' for {}", color_names[i], Common::ToUnderlying(draw_call_id));
      accessor.bufferView = static_cast<int>(m_impl->m_model.bufferViews.size());
      accessor.componentType = ComponentFormatAndIntegerToComponentType(
          declaration.colors[i].type, declaration.colors[i].integer);
      accessor.byteOffset = declaration.colors[i].offset;
      accessor.type = ComponentCountToType(declaration.colors[i].components);
      accessor.count = draw_data.vertex_data.size();
      accessor.normalized = declaration.colors[i].type == ComponentFormat::UByte ||
                            declaration.colors[i].type == ComponentFormat::UShort ||
                            declaration.colors[i].type == ComponentFormat::Byte ||
                            declaration.colors[i].type == ComponentFormat::Short;

      m_impl->m_model.accessors.push_back(std::move(accessor));
    }
  }

  // TODO: tangent/binormal?
  static std::array<std::string, 1> norm_names{"NORMAL"};
  for (std::size_t i = 0; i < declaration.normals.size(); i++)
  {
    if (declaration.normals[i].enable)
    {
      primitive.attributes[norm_names[i]] = static_cast<int>(m_impl->m_model.accessors.size());

      tinygltf::Accessor accessor;
      accessor.name =
          fmt::format("Accessor '{}' for {}", norm_names[i], Common::ToUnderlying(draw_call_id));
      accessor.bufferView = static_cast<int>(m_impl->m_model.bufferViews.size());
      accessor.componentType = ComponentFormatAndIntegerToComponentType(
          declaration.normals[i].type, declaration.normals[i].integer);
      accessor.byteOffset = declaration.normals[i].offset;
      accessor.type = ComponentCountToType(declaration.normals[i].components);
      accessor.count = draw_data.vertex_data.size();

      m_impl->m_model.accessors.push_back(std::move(accessor));
    }
  }

  static std::array<std::string, 8> texcoord_names = {
      "TEXCOORD_0", "TEXCOORD_1", "TEXCOORD_2", "TEXCOORD_3",
      "TEXCOORD_4", "TEXCOORD_5", "TEXCOORD_6", "TEXCOORD_7",
  };
  for (std::size_t i = 0; i < declaration.texcoords.size(); i++)
  {
    // Ignore 3 component texcoords for now..they are tex matrixes?
    if (declaration.texcoords[i].enable && declaration.texcoords[i].components == 2)
    {
      primitive.attributes[texcoord_names[i]] = static_cast<int>(m_impl->m_model.accessors.size());

      tinygltf::Accessor accessor;
      accessor.name = fmt::format("Accessor '{}' for {}", texcoord_names[i],
                                  Common::ToUnderlying(draw_call_id));
      accessor.bufferView = static_cast<int>(m_impl->m_model.bufferViews.size());
      accessor.componentType = ComponentFormatAndIntegerToComponentType(
          declaration.texcoords[i].type, declaration.texcoords[i].integer);
      accessor.byteOffset = declaration.texcoords[i].offset;
      accessor.type = ComponentCountToType(declaration.texcoords[i].components);
      accessor.count = draw_data.vertex_data.size();
      accessor.normalized = declaration.texcoords[i].type == ComponentFormat::UByte ||
                            declaration.texcoords[i].type == ComponentFormat::UShort ||
                            declaration.texcoords[i].type == ComponentFormat::Byte ||
                            declaration.texcoords[i].type == ComponentFormat::Short;

      m_impl->m_model.accessors.push_back(std::move(accessor));
    }
  }

  // Vertex buffer data
  tinygltf::BufferView vertex_buffer_view;
  vertex_buffer_view.buffer = static_cast<int>(m_impl->m_model.buffers.size());
  vertex_buffer_view.byteLength = draw_data.vertex_data.size() * stride;
  vertex_buffer_view.byteOffset = 0;
  vertex_buffer_view.byteStride = stride;
  vertex_buffer_view.target = TINYGLTF_TARGET_ARRAY_BUFFER;
  m_impl->m_model.bufferViews.push_back(std::move(vertex_buffer_view));

  tinygltf::Buffer buffer;
  const auto vert_data = static_cast<const unsigned char*>(draw_data.vertex_data.data());
  buffer.data = {vert_data, vert_data + vertex_buffer_view.byteLength};

  // If gpu skinning is turned on but transforms is turned off, we need to calculate
  // what the positions are when centered at the origin
  if (m_record_request.m_apply_gpu_skinning && !draw_data.gpu_skinning_position_transform.empty() &&
      declaration.posmtx.enable && declaration.position.enable)
  {
    auto vert_ptr = buffer.data.data();
    for (std::size_t vert_index = 0; vert_index < draw_data.vertex_data.size(); vert_index++)
    {
      Common::Vec3 vertex_position;
      std::memcpy(&vertex_position.x, vert_ptr + declaration.position.offset, sizeof(float));
      std::memcpy(&vertex_position.y, vert_ptr + sizeof(float) + declaration.position.offset,
                  sizeof(float));
      std::memcpy(&vertex_position.z, vert_ptr + sizeof(float) * 2 + declaration.position.offset,
                  sizeof(float));

      u32 gpu_skin_index;
      std::memcpy(&gpu_skin_index, vert_ptr + declaration.posmtx.offset, sizeof(u32));

      Common::Matrix44 position_transform;
      for (std::size_t i = 0; i < 3; i++)
      {
        position_transform.data[i * 4 + 0] =
            draw_data.gpu_skinning_position_transform[gpu_skin_index + i][0];
        position_transform.data[i * 4 + 1] =
            draw_data.gpu_skinning_position_transform[gpu_skin_index + i][1];
        position_transform.data[i * 4 + 2] =
            draw_data.gpu_skinning_position_transform[gpu_skin_index + i][2];
        position_transform.data[i * 4 + 3] =
            draw_data.gpu_skinning_position_transform[gpu_skin_index + i][3];
      }
      position_transform.data[12] = 0;
      position_transform.data[13] = 0;
      position_transform.data[14] = 0;
      position_transform.data[15] = 1;

      // Apply the transform to the position
      Common::Vec4 vp(vertex_position, 1);
      vp = position_transform * vp;
      vertex_position.x = vp.x;
      vertex_position.y = vp.y;
      vertex_position.z = vp.z;

      if (!m_record_request.m_include_transform)
      {
        vertex_position -= origin_skinned;
      }

      std::memcpy(vert_ptr + declaration.position.offset, &vertex_position.x, sizeof(float));
      std::memcpy(vert_ptr + sizeof(float) + declaration.position.offset, &vertex_position.y,
                  sizeof(float));
      std::memcpy(vert_ptr + sizeof(float) * 2 + declaration.position.offset, &vertex_position.z,
                  sizeof(float));

      if (declaration.normals[0].enable)
      {
        Common::Vec3 vertex_normal;
        std::memcpy(&vertex_normal.x, vert_ptr + declaration.normals[0].offset, sizeof(float));
        std::memcpy(&vertex_normal.y, vert_ptr + sizeof(float) + declaration.normals[0].offset,
                    sizeof(float));
        std::memcpy(&vertex_normal.z, vert_ptr + sizeof(float) * 2 + declaration.normals[0].offset,
                    sizeof(float));

        Common::Matrix33 normal_transform;
        for (std::size_t i = 0; i < 3; i++)
        {
          normal_transform.data[i * 3 + 0] =
              draw_data.gpu_skinning_normal_transform[gpu_skin_index + i][0];
          normal_transform.data[i * 3 + 1] =
              draw_data.gpu_skinning_normal_transform[gpu_skin_index + i][1];
          normal_transform.data[i * 3 + 2] =
              draw_data.gpu_skinning_normal_transform[gpu_skin_index + i][2];
        }

        // Apply the transform to the normal
        vertex_normal = normal_transform * vertex_normal;

        // Save into output
        std::memcpy(vert_ptr + declaration.normals[0].offset, &vertex_normal.x, sizeof(float));
        std::memcpy(vert_ptr + sizeof(float) + declaration.normals[0].offset, &vertex_normal.y,
                    sizeof(float));
        std::memcpy(vert_ptr + sizeof(float) * 2 + declaration.normals[0].offset, &vertex_normal.z,
                    sizeof(float));
      }

      vert_ptr += stride;
    }
  }

  m_impl->m_model.buffers.push_back(std::move(buffer));

  // Node data
  tinygltf::Node node;
  node.name = fmt::format("Node {} for xfb {}", Common::ToUnderlying(draw_call_id),
                          m_xfbs_since_recording_present);
  node.mesh = static_cast<int>(m_impl->m_model.meshes.size());

  // We expect to get data as a 3x3 if there's a global transform
  if (m_record_request.m_include_transform && additional_draw_data.transform.size() == 12)
  {
    // GLTF expects to be passed a 4x4
    node.matrix.reserve(16);

    for (int w = 0; w < 4; w++)
    {
      for (int h = 0; h < 3; h++)
      {
        node.matrix.push_back(additional_draw_data.transform[w + h * 4]);
      }
      if (w == 3)
      {
        node.matrix.push_back(1);
      }
      else
      {
        node.matrix.push_back(0);
      }
    }
  }
  m_impl->m_current_node_list.push_back(std::move(node));

  // Mesh data
  tinygltf::Mesh mesh;
  mesh.name = fmt::format("Mesh {}", Common::ToUnderlying(draw_call_id));
  mesh.primitives.push_back(std::move(primitive));
  m_impl->m_model.meshes.push_back(std::move(mesh));
}

void SceneDumper::Record(const std::string& path, const RecordingRequest& request)
{
  m_scene_save_path = path;
  m_record_request = request;
  m_recording_state = RecordingState::WANTS_RECORDING;
}

bool SceneDumper::IsRecording() const
{
  return m_recording_state == RecordingState::IS_RECORDING;
}

void SceneDumper::OnXFBCreated(const std::string& hash)
{
  if (m_recording_state != RecordingState::IS_RECORDING)
  {
    return;
  }

  m_xfbs_since_recording_present++;

  // We saw a XFB create without any data and we just started capturing
  // ignore it
  if (m_impl->m_current_node_list.empty() && m_xfbs_since_recording_present == 1)
  {
    return;
  }

  // When our frame present happens, we might be in the middle of an xfb
  // which means this initial create won't have all the data that we expect
  // it to have.  Skip the first xfb and start collecting data after that
  // This ensures our capture has all data it needs
  if (m_xfbs_since_recording_present > 1)
  {
    m_impl->m_xfb_hash_to_scene.try_emplace(hash, std::move(m_impl->m_current_node_list));
  }
  m_impl->m_current_node_list = {};
}

void SceneDumper::OnFramePresented(std::span<std::string> xfbs_presented)
{
  if (m_recording_state == RecordingState::IS_RECORDING)
  {
    // If all our xfbs aren't captured in this frame, wait for next frame
    if (!m_impl->AreAllXFBsCapture(xfbs_presented))
      return;

    m_impl->SaveScenesToFile(m_scene_save_path, xfbs_presented);
    m_recording_state = RecordingState::NOT_RECORDING;

    // Release state
    m_record_request = {};
    m_materialhash_to_material_id = {};
    m_texturehash_to_texture_id = {};
    m_impl = std::make_unique<SceneDumperImpl>();
    m_xfbs_since_recording_present = 0;
  }

  if (m_recording_state == RecordingState::WANTS_RECORDING)
  {
    m_recording_state = RecordingState::IS_RECORDING;
  }
}

void SceneDumper::AddIndices(OpcodeDecoder::Primitive primitive, u32 num_vertices)
{
  m_index_generator.AddIndices(primitive, num_vertices);
}

void SceneDumper::ResetIndices()
{
  m_index_generator.Start(m_index_buffer.data());
}
}  // namespace GraphicsModEditor
