// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Assets/MeshAsset.h"

#include <algorithm>
#include <array>
#include <utility>

#include <tinygltf/tiny_gltf.h>

#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "VideoCommon/Assets/CustomAssetLibrary.h"

namespace VideoCommon
{
namespace
{
Common::Matrix44 BuildMatrixFromNode(const tinygltf::Node& node)
{
  if (!node.matrix.empty())
  {
    Common::Matrix44 matrix;
    for (std::size_t i = 0; i < node.matrix.size(); i++)
    {
      matrix.data[i] = static_cast<float>(node.matrix[i]);
    }
    return matrix;
  }

  Common::Matrix44 matrix = Common::Matrix44::Identity();

  // Check individual components

  if (!node.scale.empty())
  {
    matrix *= Common::Matrix44::FromMatrix33(Common::Matrix33::Scale(
        Common::Vec3{static_cast<float>(node.scale[0]), static_cast<float>(node.scale[1]),
                     static_cast<float>(node.scale[2])}));
  }

  if (!node.rotation.empty())
  {
    matrix *= Common::Matrix44::FromQuaternion(Common::Quaternion(
        static_cast<float>(node.rotation[3]), static_cast<float>(node.rotation[0]),
        static_cast<float>(node.rotation[1]), static_cast<float>(node.rotation[2])));
  }

  if (!node.translation.empty())
  {
    matrix *= Common::Matrix44::Translate(Common::Vec3{static_cast<float>(node.translation[0]),
                                                       static_cast<float>(node.translation[1]),
                                                       static_cast<float>(node.translation[2])});
  }

  return matrix;
}

bool GLTFComponentTypeToAttributeFormat(int component_type, AttributeFormat* format)
{
  switch (component_type)
  {
  case TINYGLTF_COMPONENT_TYPE_BYTE:
  {
    format->type = ComponentFormat::Byte;
    format->integer = false;
  }
  break;
  case TINYGLTF_COMPONENT_TYPE_DOUBLE:
  {
    return false;
  }
  case TINYGLTF_COMPONENT_TYPE_FLOAT:
  {
    format->type = ComponentFormat::Float;
    format->integer = false;
  }
  break;
  case TINYGLTF_COMPONENT_TYPE_INT:
  {
    format->type = ComponentFormat::Float;
    format->integer = true;
  }
  break;
  case TINYGLTF_COMPONENT_TYPE_SHORT:
  {
    format->type = ComponentFormat::Short;
    format->integer = false;
  }
  break;
  case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
  {
    format->type = ComponentFormat::UByte;
    format->integer = false;
  }
  break;
  case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
  {
    return false;
  }
  case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
  {
    format->type = ComponentFormat::UShort;
    format->integer = false;
  }
  break;
  };

  return true;
}

bool UpdateVertexStrideFromPrimitive(const tinygltf::Model& model, u32 accessor_index,
                                     MeshDataChunk* chunk)
{
  const tinygltf::Accessor& accessor = model.accessors[accessor_index];

  const int component_count = tinygltf::GetNumComponentsInType(accessor.type);
  if (component_count == -1)
  {
    ERROR_LOG_FMT(VIDEO, "Failed to update vertex stride, component count was invalid");
    return false;
  }

  const int component_size =
      tinygltf::GetComponentSizeInBytes(static_cast<uint32_t>(accessor.componentType));
  if (component_size == -1)
  {
    ERROR_LOG_FMT(VIDEO, "Failed to update vertex stride, component size was invalid");
    return false;
  }

  chunk->vertex_stride += component_size * component_count;
  return true;
}

bool CopyBufferDataFromPrimitive(const tinygltf::Model& model, u32 accessor_index,
                                 std::size_t* outbound_offset, MeshDataChunk* chunk)
{
  const tinygltf::Accessor& accessor = model.accessors[accessor_index];

  const int component_count = tinygltf::GetNumComponentsInType(accessor.type);
  if (component_count == -1)
  {
    ERROR_LOG_FMT(VIDEO, "Failed to copy buffer data from primitive, component count was invalid");
    return false;
  }

  const int component_size =
      tinygltf::GetComponentSizeInBytes(static_cast<uint32_t>(accessor.componentType));
  if (component_size == -1)
  {
    ERROR_LOG_FMT(VIDEO, "Failed to copy buffer data from primitive, component size was invalid");
    return false;
  }

  const tinygltf::BufferView& buffer_view = model.bufferViews[accessor.bufferView];
  const tinygltf::Buffer& buffer = model.buffers[buffer_view.buffer];

  if (buffer_view.byteStride == 0)
  {
    // Data is tightly packed
    const auto data = &buffer.data[accessor.byteOffset + buffer_view.byteOffset];
    for (std::size_t i = 0; i < accessor.count; i++)
    {
      const std::size_t vertex_data_offset = i * chunk->vertex_stride + *outbound_offset;
      memcpy(&chunk->vertex_data[vertex_data_offset], &data[i * component_size * component_count],
             component_size * component_count);
    }
  }
  else
  {
    // Data is interleaved
    const auto data = &buffer.data[accessor.byteOffset + buffer_view.byteOffset];
    for (std::size_t i = 0; i < accessor.count; i++)
    {
      const std::size_t vertex_data_offset = i * chunk->vertex_stride + *outbound_offset;
      const std::size_t gltf_data_offset = i * buffer_view.byteStride;

      memcpy(&chunk->vertex_data[vertex_data_offset], &data[gltf_data_offset],
             component_size * component_count);
    }
  }

  *outbound_offset += component_size * component_count;

  return true;
}

bool ReadGLTFMesh(std::string_view mesh_file, const tinygltf::Model& model,
                  const tinygltf::Mesh& mesh, const Common::Matrix44& mat, MeshData* data)
{
  for (std::size_t primitive_index = 0; primitive_index < mesh.primitives.size(); ++primitive_index)
  {
    MeshDataChunk chunk;
    chunk.transform = mat;
    const tinygltf::Primitive& primitive = mesh.primitives[primitive_index];
    if (primitive.indices == -1)
    {
      ERROR_LOG_FMT(VIDEO, "Mesh '{}' is expected to have indices but doesn't have any", mesh_file);
      return false;
    }
    chunk.material_name = model.materials[primitive.material].name;
    const tinygltf::Accessor& index_accessor = model.accessors[primitive.indices];
    const tinygltf::BufferView& index_buffer_view = model.bufferViews[index_accessor.bufferView];
    const tinygltf::Buffer& index_buffer = model.buffers[index_buffer_view.buffer];
    const int index_stride = index_accessor.ByteStride(index_buffer_view);
    if (index_stride == -1)
    {
      ERROR_LOG_FMT(VIDEO, "Mesh '{}' has invalid stride", mesh_file);
      return false;
    }
    // TODO C++23: use make_unique_overwrite
    chunk.indices = std::unique_ptr<u16[]>(new u16[index_accessor.count]);
    auto index_src = &index_buffer.data[index_accessor.byteOffset + index_buffer_view.byteOffset];
    for (std::size_t i = 0; i < index_accessor.count; i++)
    {
      if (index_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
      {
        std::memcpy(&chunk.indices[i], &index_src[i * index_stride], sizeof(u16));
      }
      else if (index_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
      {
        u8 unsigned_byte;
        std::memcpy(&unsigned_byte, &index_src[i * index_stride], sizeof(u8));
        chunk.indices[i] = unsigned_byte;
      }
      else if (index_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
      {
        // TODO: update Dolphin to support u32 indices
        ERROR_LOG_FMT(
            VIDEO,
            "Mesh '{}' uses an indice format of unsigned int which is not currently supported",
            mesh_file);
        return false;
      }
    }

    chunk.num_indices = static_cast<u32>(index_accessor.count);

    if (primitive.mode == TINYGLTF_MODE_TRIANGLES)
    {
      chunk.primitive_type = PrimitiveType::Triangles;
    }
    else if (primitive.mode == TINYGLTF_MODE_TRIANGLE_STRIP)
    {
      chunk.primitive_type = PrimitiveType::TriangleStrip;
    }
    else if (primitive.mode == TINYGLTF_MODE_TRIANGLE_FAN)
    {
      ERROR_LOG_FMT(VIDEO, "Mesh '{}' requires triangle fan but that is not supported", mesh_file);
      return false;
    }
    else if (primitive.mode == TINYGLTF_MODE_LINE)
    {
      chunk.primitive_type = PrimitiveType::Lines;
    }
    else if (primitive.mode == TINYGLTF_MODE_POINTS)
    {
      chunk.primitive_type = PrimitiveType::Points;
    }

    chunk.vertex_stride = 0;
    static constexpr std::array<std::string_view, 12> all_names = {
        "POSITION",   "NORMAL",     "COLOR_0",    "COLOR_1",    "TEXCOORD_0", "TEXCOORD_1",
        "TEXCOORD_2", "TEXCOORD_3", "TEXCOORD_4", "TEXCOORD_5", "TEXCOORD_6", "TEXCOORD_7",
    };
    for (std::size_t i = 0; i < all_names.size(); i++)
    {
      const auto it = primitive.attributes.find(std::string{all_names[i]});
      if (it != primitive.attributes.end())
      {
        if (!UpdateVertexStrideFromPrimitive(model, it->second, &chunk))
          return false;
      }
    }
    chunk.vertex_declaration.stride = chunk.vertex_stride;

    const auto position_it = primitive.attributes.find("POSITION");
    if (position_it == primitive.attributes.end())
    {
      ERROR_LOG_FMT(VIDEO, "Mesh '{}' does not provide a POSITION attribute, that is required",
                    mesh_file);
      return false;
    }
    std::size_t outbound_offset = 0;
    const tinygltf::Accessor& pos_accessor = model.accessors[position_it->second];
    chunk.num_vertices = static_cast<u32>(pos_accessor.count);
    // TODO C++23: use make_unique_overwrite
    chunk.vertex_data = std::unique_ptr<u8[]>(new u8[chunk.num_vertices * chunk.vertex_stride]);
    if (!CopyBufferDataFromPrimitive(model, position_it->second, &outbound_offset, &chunk))
      return false;
    chunk.components_available = 0;
    chunk.vertex_declaration.position.enable = true;
    chunk.vertex_declaration.position.components = 3;
    chunk.vertex_declaration.position.offset = 0;
    if (!GLTFComponentTypeToAttributeFormat(pos_accessor.componentType,
                                            &chunk.vertex_declaration.position))
    {
      ERROR_LOG_FMT(VIDEO, "Mesh '{}' has invalid attribute format for position", mesh_file);
      return false;
    }

    // Save off min and max position in case we want to compute bounds
    // GLTF spec expects these values to exist but error if they don't
    if (pos_accessor.minValues.size() != 3)
    {
      ERROR_LOG_FMT(VIDEO, "Mesh '{}' is expected to have a minimum value but it is missing",
                    mesh_file);
      return false;
    }
    chunk.minimum_position.x = static_cast<float>(pos_accessor.minValues[0]);
    chunk.minimum_position.y = static_cast<float>(pos_accessor.minValues[1]);
    chunk.minimum_position.z = static_cast<float>(pos_accessor.minValues[2]);

    if (pos_accessor.maxValues.size() != 3)
    {
      ERROR_LOG_FMT(VIDEO, "Mesh '{}' is expected to have a maximum value but it is missing",
                    mesh_file);
      return false;
    }
    chunk.maximum_position.x = static_cast<float>(pos_accessor.maxValues[0]);
    chunk.maximum_position.y = static_cast<float>(pos_accessor.maxValues[1]);
    chunk.maximum_position.z = static_cast<float>(pos_accessor.maxValues[2]);

    static constexpr std::array<std::string_view, 2> color_names = {
        "COLOR_0",
        "COLOR_1",
    };
    for (std::size_t i = 0; i < color_names.size(); i++)
    {
      const auto color_it = primitive.attributes.find(std::string{color_names[i]});
      if (color_it != primitive.attributes.end())
      {
        chunk.vertex_declaration.colors[i].offset = static_cast<int>(outbound_offset);
        if (!CopyBufferDataFromPrimitive(model, color_it->second, &outbound_offset, &chunk))
          return false;
        chunk.components_available |= VB_HAS_COL0 << i;

        chunk.vertex_declaration.colors[i].enable = true;
        chunk.vertex_declaration.colors[i].components = 3;
        const tinygltf::Accessor& accessor = model.accessors[color_it->second];
        if (!GLTFComponentTypeToAttributeFormat(accessor.componentType,
                                                &chunk.vertex_declaration.colors[i]))
        {
          ERROR_LOG_FMT(VIDEO, "Mesh '{}' has invalid attribute format for {}", mesh_file,
                        color_names[i]);
          return false;
        }
      }
      else
      {
        chunk.vertex_declaration.colors[i].enable = false;
      }
    }

    const auto normal_it = primitive.attributes.find("NORMAL");
    if (normal_it != primitive.attributes.end())
    {
      chunk.vertex_declaration.normals[0].offset = static_cast<int>(outbound_offset);
      if (!CopyBufferDataFromPrimitive(model, normal_it->second, &outbound_offset, &chunk))
        return false;
      chunk.components_available |= VB_HAS_NORMAL;
      chunk.vertex_declaration.normals[0].enable = true;
      chunk.vertex_declaration.normals[0].components = 3;
      const tinygltf::Accessor& accessor = model.accessors[normal_it->second];
      if (!GLTFComponentTypeToAttributeFormat(accessor.componentType,
                                              &chunk.vertex_declaration.normals[0]))
      {
        ERROR_LOG_FMT(VIDEO, "Mesh '{}' has invalid attribute format for NORMAL", mesh_file);
        return false;
      }
    }
    else
    {
      chunk.vertex_declaration.normals[0].enable = false;
    }

    static constexpr std::array<std::string_view, 8> texcoord_names = {
        "TEXCOORD_0", "TEXCOORD_1", "TEXCOORD_2", "TEXCOORD_3",
        "TEXCOORD_4", "TEXCOORD_5", "TEXCOORD_6", "TEXCOORD_7",
    };
    for (std::size_t i = 0; i < texcoord_names.size(); i++)
    {
      const auto texture_it = primitive.attributes.find(std::string{texcoord_names[i]});
      if (texture_it != primitive.attributes.end())
      {
        chunk.vertex_declaration.texcoords[i].offset = static_cast<int>(outbound_offset);
        if (!CopyBufferDataFromPrimitive(model, texture_it->second, &outbound_offset, &chunk))
          return false;
        chunk.components_available |= VB_HAS_UV0 << i;
        chunk.vertex_declaration.texcoords[i].enable = true;
        chunk.vertex_declaration.texcoords[i].components = 2;
        const tinygltf::Accessor& accessor = model.accessors[texture_it->second];
        if (!GLTFComponentTypeToAttributeFormat(accessor.componentType,
                                                &chunk.vertex_declaration.texcoords[i]))
        {
          ERROR_LOG_FMT(VIDEO, "Mesh '{}' has invalid attribute format for {}", mesh_file,
                        texcoord_names[i]);
          return false;
        }
      }
      else
      {
        chunk.vertex_declaration.texcoords[i].enable = false;
      }
    }

    // Position matrix can be enabled if the draw that is using
    // this mesh needs it
    chunk.vertex_declaration.posmtx.enable = false;

    data->m_mesh_chunks.push_back(std::move(chunk));
  }

  return true;
}

bool ReadGLTFNodes(std::string_view mesh_file, const tinygltf::Model& model,
                   const tinygltf::Node& node, const Common::Matrix44& mat, MeshData* data)
{
  if (node.mesh != -1)
  {
    if (!ReadGLTFMesh(mesh_file, model, model.meshes[node.mesh], mat, data))
      return false;
  }

  for (std::size_t i = 0; i < node.children.size(); i++)
  {
    const tinygltf::Node& child = model.nodes[node.children[i]];
    const auto child_mat = mat * BuildMatrixFromNode(child);
    if (!ReadGLTFNodes(mesh_file, model, child, child_mat, data))
      return false;
  }

  return true;
}

bool ReadGLTFMaterials(std::string_view mesh_file, const tinygltf::Model& model, MeshData* data)
{
  for (std::size_t i = 0; i < model.materials.size(); i++)
  {
    const tinygltf::Material& material = model.materials[i];

    // TODO: support converting material data into Dolphin material assets
    data->m_mesh_material_to_material_asset_id.insert_or_assign(material.name, "");
  }

  return true;
}

bool ReadGLTF(std::string_view mesh_file, const tinygltf::Model& model, MeshData* data)
{
  int scene_index = model.defaultScene;
  if (scene_index == -1)
    scene_index = 0;

  const auto& scene = model.scenes[scene_index];
  const auto scene_node_indices = scene.nodes;
  for (std::size_t i = 0; i < scene_node_indices.size(); i++)
  {
    const tinygltf::Node& node = model.nodes[scene_node_indices[i]];
    const auto mat = BuildMatrixFromNode(node);
    if (!ReadGLTFNodes(mesh_file, model, node, mat, data))
      return false;
  }

  return ReadGLTFMaterials(mesh_file, model, data);
}
}  // namespace
bool MeshData::FromJson(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                        const picojson::object& json, MeshData* data)
{
  if (const auto iter = json.find("material_mapping"); iter != json.end())
  {
    if (!iter->second.is<picojson::object>())
    {
      ERROR_LOG_FMT(
          VIDEO,
          "Asset '{}' failed to parse json, expected 'material_mapping' to be of type object",
          asset_id);
      return false;
    }

    for (const auto& [material_name, asset_id_json] : iter->second.get<picojson::object>())
    {
      if (!asset_id_json.is<std::string>())
      {
        ERROR_LOG_FMT(
            VIDEO,
            "Asset '{}' failed to parse json, material name '{}' linked to a non-string value",
            asset_id, material_name);
        return false;
      }

      data->m_mesh_material_to_material_asset_id[material_name] = asset_id_json.to_str();
    }
  }
  return true;
}

void MeshData::ToJson(picojson::object& obj, const MeshData& data)
{
  picojson::object material_mapping;
  for (const auto& [material_name, asset_id] : data.m_mesh_material_to_material_asset_id)
  {
    material_mapping.emplace(material_name, asset_id);
  }
  obj.emplace("material_mapping", std::move(material_mapping));
}

bool MeshData::FromDolphinMesh(std::span<const u8> raw_data, MeshData* data)
{
  std::size_t offset = 0;

  std::size_t chunk_size = 0;
  std::memcpy(&chunk_size, raw_data.data(), sizeof(std::size_t));
  offset += sizeof(std::size_t);

  data->m_mesh_chunks.reserve(chunk_size);
  for (std::size_t i = 0; i < chunk_size; i++)
  {
    MeshDataChunk chunk;

    std::memcpy(&chunk.num_vertices, raw_data.data() + offset, sizeof(u32));
    offset += sizeof(u32);

    std::memcpy(&chunk.vertex_stride, raw_data.data() + offset, sizeof(u32));
    offset += sizeof(u32);

    // TODO C++23: use make_unique_overwrite
    chunk.vertex_data = std::unique_ptr<u8[]>(new u8[chunk.num_vertices * chunk.vertex_stride]);
    std::memcpy(chunk.vertex_data.get(), raw_data.data() + offset,
                chunk.num_vertices * chunk.vertex_stride);
    offset += chunk.num_vertices * chunk.vertex_stride;

    std::memcpy(&chunk.num_indices, raw_data.data() + offset, sizeof(u32));
    offset += sizeof(u32);

    // TODO C++23: use make_unique_overwrite
    chunk.indices = std::unique_ptr<u16[]>(new u16[chunk.num_indices]);
    std::memcpy(chunk.indices.get(), raw_data.data() + offset, chunk.num_indices * sizeof(u16));
    offset += chunk.num_indices * sizeof(u16);

    std::memcpy(&chunk.vertex_declaration, raw_data.data() + offset,
                sizeof(PortableVertexDeclaration));
    offset += sizeof(PortableVertexDeclaration);

    std::memcpy(&chunk.primitive_type, raw_data.data() + offset, sizeof(PrimitiveType));
    offset += sizeof(PrimitiveType);

    std::memcpy(&chunk.components_available, raw_data.data() + offset, sizeof(u32));
    offset += sizeof(u32);

    std::memcpy(&chunk.minimum_position, raw_data.data() + offset, sizeof(Common::Vec3));
    offset += sizeof(Common::Vec3);

    std::memcpy(&chunk.maximum_position, raw_data.data() + offset, sizeof(Common::Vec3));
    offset += sizeof(Common::Vec3);

    std::memcpy(&chunk.transform.data[0], raw_data.data() + offset,
                chunk.transform.data.size() * sizeof(float));
    offset += chunk.transform.data.size() * sizeof(float);

    std::size_t material_name_size = 0;
    std::memcpy(&material_name_size, raw_data.data() + offset, sizeof(std::size_t));
    offset += sizeof(std::size_t);

    chunk.material_name.assign(raw_data.data() + offset,
                               raw_data.data() + offset + material_name_size);
    offset += material_name_size * sizeof(char);

    data->m_mesh_chunks.push_back(std::move(chunk));
  }

  return true;
}

bool MeshData::ToDolphinMesh(File::IOFile* file_data, const MeshData& data)
{
  const std::size_t chunk_size = data.m_mesh_chunks.size();
  file_data->WriteBytes(&chunk_size, sizeof(std::size_t));
  for (const auto& chunk : data.m_mesh_chunks)
  {
    if (!file_data->WriteBytes(&chunk.num_vertices, sizeof(u32)))
      return false;
    if (!file_data->WriteBytes(&chunk.vertex_stride, sizeof(u32)))
      return false;
    if (!file_data->WriteBytes(chunk.vertex_data.get(), chunk.num_vertices * chunk.vertex_stride))
      return false;
    if (!file_data->WriteBytes(&chunk.num_indices, sizeof(u32)))
      return false;
    if (!file_data->WriteBytes(chunk.indices.get(), chunk.num_indices * sizeof(u16)))
      return false;
    if (!file_data->WriteBytes(&chunk.vertex_declaration, sizeof(PortableVertexDeclaration)))
      return false;
    if (!file_data->WriteBytes(&chunk.primitive_type, sizeof(PrimitiveType)))
      return false;
    if (!file_data->WriteBytes(&chunk.components_available, sizeof(u32)))
      return false;
    if (!file_data->WriteBytes(&chunk.minimum_position, sizeof(Common::Vec3)))
      return false;
    if (!file_data->WriteBytes(&chunk.maximum_position, sizeof(Common::Vec3)))
      return false;
    if (!file_data->WriteBytes(&chunk.transform.data[0],
                               chunk.transform.data.size() * sizeof(float)))
    {
      return false;
    }

    const std::size_t material_name_size = chunk.material_name.size();
    if (!file_data->WriteBytes(&material_name_size, sizeof(std::size_t)))
      return false;
    if (!file_data->WriteBytes(&chunk.material_name[0], chunk.material_name.size() * sizeof(char)))
      return false;
  }
  return true;
}

bool MeshData::FromGLTF(std::string_view gltf_file, MeshData* data)
{
  if (gltf_file.ends_with(".glb"))
  {
    ERROR_LOG_FMT(VIDEO, "File '{}' with glb extension is not supported at this time", gltf_file);
    return false;
  }

  if (gltf_file.ends_with(".gltf"))
  {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string model_errors;
    std::string model_warnings;
    if (!loader.LoadASCIIFromFile(&model, &model_errors, &model_warnings, std::string{gltf_file}))
    {
      ERROR_LOG_FMT(VIDEO, "File '{}' was invalid GLTF, error: {}, warning: {}", gltf_file,
                    model_errors, model_warnings);
      return false;
    }
    return ReadGLTF(gltf_file, model, data);
  }

  ERROR_LOG_FMT(VIDEO, "GLTF '{}' has invalid extension", gltf_file);
  return false;
}

CustomAssetLibrary::LoadInfo MeshAsset::LoadImpl(const CustomAssetLibrary::AssetID& asset_id)
{
  auto potential_data = std::make_shared<MeshData>();
  const auto loaded_info = m_owning_library->LoadMesh(asset_id, potential_data.get());
  if (loaded_info.m_bytes_loaded == 0)
    return {};
  {
    std::lock_guard lk(m_data_lock);
    m_loaded = true;
    m_data = std::move(potential_data);
  }
  return loaded_info;
}
}  // namespace VideoCommon
