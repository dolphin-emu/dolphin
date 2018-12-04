// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"
#include "VideoCommon/RenderState.h"
#include "VideoCommon/ShaderCache.h"

class DataReader;
class NativeVertexFormat;
class PointerWrap;
struct PortableVertexDeclaration;

struct Slope
{
  float dfdx;
  float dfdy;
  float f0;
  bool dirty;
};

class VertexManagerBase
{
private:
  // 3 pos
  static constexpr u32 SMALLEST_POSSIBLE_VERTEX = sizeof(float) * 3;
  // 3 pos, 3*3 normal, 2*u32 color, 8*4 tex, 1 posMat
  static constexpr u32 LARGEST_POSSIBLE_VERTEX = sizeof(float) * 45 + sizeof(u32) * 2;

  static constexpr u32 MAX_PRIMITIVES_PER_COMMAND = 65535;

public:
  static constexpr u32 MAXVBUFFERSIZE =
      MathUtil::NextPowerOf2(MAX_PRIMITIVES_PER_COMMAND * LARGEST_POSSIBLE_VERTEX);

  // We may convert triangle-fans to triangle-lists, almost 3x as many indices.
  static constexpr u32 MAXIBUFFERSIZE = MathUtil::NextPowerOf2(MAX_PRIMITIVES_PER_COMMAND * 3);

  VertexManagerBase();
  // needs to be virtual for DX11's dtor
  virtual ~VertexManagerBase();

  PrimitiveType GetCurrentPrimitiveType() const { return m_current_primitive_type; }
  DataReader PrepareForAdditionalData(int primitive, u32 count, u32 stride, bool cullall);
  void FlushData(u32 count, u32 stride);

  void Flush();

  virtual std::unique_ptr<NativeVertexFormat>
  CreateNativeVertexFormat(const PortableVertexDeclaration& vtx_decl) = 0;

  void DoState(PointerWrap& p);

  std::pair<size_t, size_t> ResetFlushAspectRatioCount();

  // State setters, called from register update functions.
  void SetRasterizationStateChanged() { m_rasterization_state_changed = true; }
  void SetDepthStateChanged() { m_depth_state_changed = true; }
  void SetBlendingStateChanged() { m_blending_state_changed = true; }
  void InvalidatePipelineObject()
  {
    m_current_pipeline_object = nullptr;
    m_pipeline_config_changed = true;
  }

  // Utility pipeline drawing (e.g. EFB copies, post-processing, UI).
  virtual void UploadUtilityUniforms(const void* uniforms, u32 uniforms_size) = 0;
  void UploadUtilityVertices(const void* vertices, u32 vertex_stride, u32 num_vertices,
                             const u16* indices, u32 num_indices, u32* out_base_vertex,
                             u32* out_base_index);

protected:
  // Vertex buffers/index buffer creation.
  virtual void CreateDeviceObjects() {}
  virtual void DestroyDeviceObjects() {}

  // Prepares the buffer for the next batch of vertices.
  virtual void ResetBuffer(u32 vertex_stride, bool cull_all) = 0;

  // Commits/uploads the current batch of vertices.
  virtual void CommitBuffer(u32 num_vertices, u32 vertex_stride, u32 num_indices,
                            u32* out_base_vertex, u32* out_base_index) = 0;

  // Uploads uniform buffers for GX draws.
  virtual void UploadConstants() = 0;

  // Issues the draw call for the current batch in the backend.
  virtual void DrawCurrentBatch(u32 base_index, u32 num_indices, u32 base_vertex) = 0;

  u8* m_cur_buffer_pointer = nullptr;
  u8* m_base_buffer_pointer = nullptr;
  u8* m_end_buffer_pointer = nullptr;

  u32 GetRemainingSize() const;
  static u32 GetRemainingIndices(int primitive);

  Slope m_zslope = {};
  void CalculateZSlope(NativeVertexFormat* format);

  VideoCommon::GXPipelineUid m_current_pipeline_config;
  VideoCommon::GXUberPipelineUid m_current_uber_pipeline_config;
  const AbstractPipeline* m_current_pipeline_object = nullptr;
  PrimitiveType m_current_primitive_type = PrimitiveType::Points;
  bool m_pipeline_config_changed = true;
  bool m_rasterization_state_changed = true;
  bool m_depth_state_changed = true;
  bool m_blending_state_changed = true;
  bool m_cull_all = false;

private:
  bool m_is_flushed = true;
  size_t m_flush_count_4_3 = 0;
  size_t m_flush_count_anamorphic = 0;

  void UpdatePipelineConfig();
  void UpdatePipelineObject();
};

extern std::unique_ptr<VertexManagerBase> g_vertex_manager;
