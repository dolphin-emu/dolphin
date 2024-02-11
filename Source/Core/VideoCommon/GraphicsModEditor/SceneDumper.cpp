// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/SceneDumper.h"

#include <array>
#include <string>

#include <fmt/format.h>
#include <tinygltf/tiny_gltf.h>

#include "Common/EnumUtils.h"

#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/TextureUtils.h"
#include "VideoCommon/VideoEvents.h"

namespace
{
std::size_t ComponentFormatToSize(ComponentFormat format)
{
  switch (format)
  {
  case ComponentFormat::Byte:
    return 1;
  case ComponentFormat::Float:
    return 4;
  case ComponentFormat::Short:
    return 2;
  case ComponentFormat::UByte:
    return 1;
  case ComponentFormat::UShort:
    return 2;
  };

  return 0;
}

int ComponentFormatAndIntegerToComponentType(ComponentFormat format, bool integer)
{
  switch (format)
  {
  case ComponentFormat::Byte:
    return TINYGLTF_COMPONENT_TYPE_BYTE;
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

  tinygltf::Scene m_scene;
  tinygltf::Model m_model;
  void SaveSceneToFile(const std::string& path)
  {
    const bool embed_images = false;
    const bool embed_buffers = false;
    const bool pretty_print = true;
    const bool write_binary = false;

    m_model.scenes.push_back(m_scene);

    tinygltf::TinyGLTF gltf;
    gltf.WriteGltfSceneToFile(&m_model, path, embed_images, embed_buffers, pretty_print,
                              write_binary);
  }
};

SceneDumper::SceneDumper()
{
  m_impl = std::make_unique<SceneDumperImpl>();

  // Note: ideally we'd use begin/end frame for this logic, however, determining
  // real end of frame is difficult for some games that use multiple XFBs
  // The current approach may dump duplicated objects based on when the present is triggered
  // but we at least know everything requested is accounted for
  m_frame_end = AfterPresentEvent::Register([this](auto& present) { OnFrameEnd(); }, "SceneDumper");
}

SceneDumper::~SceneDumper() = default;

bool SceneDumper::IsDrawCallInRecording(GraphicsMods::DrawCallID draw_call_id) const
{
  return m_record_request.m_draw_call_ids.contains(draw_call_id);
}

void SceneDumper::AddDataToRecording(GraphicsMods::DrawCallID draw_call_id, DrawData draw_data)
{
  const auto& declaration = draw_data.vertex_format->GetVertexDeclaration();

  // Skip 2 point position elements for now
  if (declaration.position.enable && declaration.position.components == 2)
    return;

  // Initial primitive data
  tinygltf::Primitive primitive;
  primitive.mode = PrimitiveTypeToMode(draw_data.primitive_type);
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
      texture_material_name += texture.name;
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
      if (draw_data.enable_blending && m_record_request.m_enable_blending)
      {
        material.alphaMode = "BLEND";
      }
      for (const auto& texture : draw_data.textures)
      {
        if (material.pbrMetallicRoughness.baseColorTexture.index != -1)
        {
          break;
        }

        if (const auto texture_iter = m_texturehash_to_texture_id.find(texture.name);
            texture_iter != m_texturehash_to_texture_id.end())
        {
          material.pbrMetallicRoughness.baseColorTexture.index = texture_iter->second;
        }
        else
        {
          VideoCommon::TextureUtils::DumpTexture(*texture.texture, std::string{texture.name}, 0,
                                                 false);

          tinygltf::Texture tinygltf_texture;
          tinygltf_texture.source = static_cast<int>(m_impl->m_model.images.size());

          tinygltf::Image image;
          image.uri = fmt::format("{}.png", texture.name);
          m_impl->m_model.images.push_back(image);

          material.pbrMetallicRoughness.baseColorTexture.index =
              static_cast<int>(m_impl->m_model.textures.size());
          m_impl->m_model.textures.push_back(tinygltf_texture);

          m_texturehash_to_texture_id[std::string{texture.name}] =
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
  index_accessor.count = draw_data.num_indices;

  auto indice_ptr = reinterpret_cast<const u8*>(draw_data.indices);
  for (u64 indice_index = 0; indice_index < draw_data.num_indices; indice_index++)
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
  index_buffer_view.byteLength = draw_data.num_indices * sizeof(u16);
  index_buffer_view.byteOffset = 0;
  index_buffer_view.target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;
  m_impl->m_model.bufferViews.push_back(std::move(index_buffer_view));

  tinygltf::Buffer index_buffer;

  const auto index_data = reinterpret_cast<const unsigned char*>(draw_data.indices);
  index_buffer.data = {index_data, index_data + index_buffer_view.byteLength};
  m_impl->m_model.buffers.push_back(std::move(index_buffer));

  // Fill out all vertex data
  const std::size_t stride = declaration.stride;

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
    accessor.count = draw_data.num_vertices;

    auto vert_ptr = reinterpret_cast<const u8*>(draw_data.vertices);
    for (u64 vert_index = 0; vert_index < draw_data.num_vertices; vert_index++)
    {
      float vx = 0;
      std::memcpy(&vx, vert_ptr + accessor.byteOffset, sizeof(float));

      float vy = 0;
      std::memcpy(&vy, vert_ptr + sizeof(float) + accessor.byteOffset, sizeof(float));

      float vz = 0;
      std::memcpy(&vz, vert_ptr + sizeof(float) * 2 + accessor.byteOffset, sizeof(float));

      if (accessor.minValues.empty())
      {
        accessor.minValues.push_back(static_cast<double>(vx));
        accessor.minValues.push_back(static_cast<double>(vy));
        accessor.minValues.push_back(static_cast<double>(vz));
      }
      else
      {
        float last_min_x = static_cast<float>(accessor.minValues[0]);
        float last_min_y = static_cast<float>(accessor.minValues[1]);
        float last_min_z = static_cast<float>(accessor.minValues[2]);

        if (vx < last_min_x)
          accessor.minValues[0] = vx;
        if (vy < last_min_y)
          accessor.minValues[1] = vy;
        if (vz < last_min_z)
          accessor.minValues[2] = vz;
      }

      if (accessor.maxValues.empty())
      {
        accessor.maxValues.push_back(static_cast<double>(vx));
        accessor.maxValues.push_back(static_cast<double>(vy));
        accessor.maxValues.push_back(static_cast<double>(vz));
      }
      else
      {
        float last_max_x = static_cast<float>(accessor.maxValues[0]);
        float last_max_y = static_cast<float>(accessor.maxValues[1]);
        float last_max_z = static_cast<float>(accessor.maxValues[2]);

        if (vx > last_max_x)
          accessor.maxValues[0] = vx;
        if (vy > last_max_y)
          accessor.maxValues[1] = vy;
        if (vz > last_max_z)
          accessor.maxValues[2] = vz;
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
      accessor.count = draw_data.num_vertices;
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
      accessor.count = draw_data.num_vertices;

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
      accessor.count = draw_data.num_vertices;
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
  vertex_buffer_view.byteLength = draw_data.num_vertices * stride;
  vertex_buffer_view.byteOffset = 0;
  vertex_buffer_view.byteStride = stride;
  vertex_buffer_view.target = TINYGLTF_TARGET_ARRAY_BUFFER;
  m_impl->m_model.bufferViews.push_back(std::move(vertex_buffer_view));

  tinygltf::Buffer buffer;
  const auto vert_data = static_cast<const unsigned char*>(draw_data.vertices);
  buffer.data = {vert_data, vert_data + vertex_buffer_view.byteLength};

  m_impl->m_model.buffers.push_back(std::move(buffer));

  // Node data
  tinygltf::Node node;
  node.name = fmt::format("Node {}", Common::ToUnderlying(draw_call_id));
  node.mesh = static_cast<int>(m_impl->m_model.meshes.size());

  // We expect to get data as a 3x3 if there's a global transform
  if (m_record_request.m_include_transform && draw_data.transform.size() == 12)
  {
    // GLTF expects to be passed a 4x4
    node.matrix.reserve(16);

    for (int w = 0; w < 4; w++)
    {
      for (int h = 0; h < 3; h++)
      {
        node.matrix.push_back(draw_data.transform[w + h * 4]);
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
  m_impl->m_scene.nodes.push_back(static_cast<int>(m_impl->m_model.nodes.size()));
  m_impl->m_model.nodes.push_back(std::move(node));

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

void SceneDumper::OnFrameBegin()
{
  if (m_recording_state == RecordingState::WANTS_RECORDING)
  {
    m_recording_state = RecordingState::IS_RECORDING;
  }
}

void SceneDumper::OnFrameEnd()
{
  if (!m_saw_frame_end)
  {
    m_saw_frame_end = true;
    OnFrameBegin();
    return;
  }

  if (m_recording_state == RecordingState::IS_RECORDING)
  {
    m_impl->SaveSceneToFile(m_scene_save_path);
    m_recording_state = RecordingState::NOT_RECORDING;

    // Release state
    m_record_request = {};
    m_materialhash_to_material_id = {};
    m_texturehash_to_texture_id = {};
    m_impl = std::make_unique<SceneDumperImpl>();
  }

  m_saw_frame_end = false;
}
}  // namespace GraphicsModEditor
