// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <vector>

#include "Common/BitSet.h"
#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"
#include "VideoCommon/CPUCull.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/RenderState.h"
#include "VideoCommon/ShaderCache.h"
#include "VideoCommon/VideoEvents.h"

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

// View format of the input data to the texture decoding shader.
enum TexelBufferFormat : u32
{
  TEXEL_BUFFER_FORMAT_R8_UINT,
  TEXEL_BUFFER_FORMAT_R16_UINT,
  TEXEL_BUFFER_FORMAT_RGBA8_UINT,
  TEXEL_BUFFER_FORMAT_R32G32_UINT,
  NUM_TEXEL_BUFFER_FORMATS
};

namespace OpcodeDecoder
{
enum class Primitive : u8;
};

class VertexManagerBase
{
private:
  // 3 pos
  static constexpr u32 SMALLEST_POSSIBLE_VERTEX = sizeof(float) * 3;
  // 3 pos, 3*3 normal, 2*u32 color, 8*4 tex, 1 posMat
  static constexpr u32 LARGEST_POSSIBLE_VERTEX = sizeof(float) * 45 + sizeof(u32) * 2;

  static constexpr u32 MAX_PRIMITIVES_PER_COMMAND = 65535;

  // Used for 16:9 anamorphic widescreen heuristic.
  struct FlushStatistics
  {
    struct ProjectionCounts
    {
      size_t normal_flush_count;
      size_t anamorphic_flush_count;
      size_t other_flush_count;

      size_t normal_vertex_count;
      size_t anamorphic_vertex_count;
      size_t other_vertex_count;

      size_t GetTotalFlushCount() const
      {
        return normal_flush_count + anamorphic_flush_count + other_flush_count;
      }

      size_t GetTotalVertexCount() const
      {
        return normal_vertex_count + anamorphic_vertex_count + other_vertex_count;
      }
    };

    ProjectionCounts perspective;
    ProjectionCounts orthographic;
  };

public:
  static constexpr u32 MAXVBUFFERSIZE =
      MathUtil::NextPowerOf2(MAX_PRIMITIVES_PER_COMMAND * LARGEST_POSSIBLE_VERTEX);

  // We may convert triangle-fans to triangle-lists, almost 3x as many indices.
  static constexpr u32 MAXIBUFFERSIZE = MathUtil::NextPowerOf2(MAX_PRIMITIVES_PER_COMMAND * 3);

  // Streaming buffer sizes.
  // Texel buffer will fit the maximum size of an encoded GX texture. 1024x1024, RGBA8 = 4MB.
  static constexpr u32 VERTEX_STREAM_BUFFER_SIZE = 48 * 1024 * 1024;
  static constexpr u32 INDEX_STREAM_BUFFER_SIZE = 8 * 1024 * 1024;
  static constexpr u32 UNIFORM_STREAM_BUFFER_SIZE = 64 * 1024 * 1024;
  static constexpr u32 TEXEL_STREAM_BUFFER_SIZE = 16 * 1024 * 1024;

  VertexManagerBase();
  virtual ~VertexManagerBase();

  virtual bool Initialize();

  PrimitiveType GetCurrentPrimitiveType() const { return m_current_primitive_type; }
  void AddIndices(OpcodeDecoder::Primitive primitive, u32 num_vertices);
  bool AreAllVerticesCulled(VertexLoaderBase* loader, OpcodeDecoder::Primitive primitive,
                            const u8* src, u32 count);
  virtual DataReader PrepareForAdditionalData(OpcodeDecoder::Primitive primitive, u32 count,
                                              u32 stride, bool cullall);
  /// Switch cullall off after a call to PrepareForAdditionalData with cullall true
  /// Expects that you will add a nonzero number of primitives before the next flush
  /// Returns whether cullall was changed (false if cullall was already off)
  DataReader DisableCullAll(u32 stride);
  void FlushData(u32 count, u32 stride);

  void Flush();
  bool HasSendableVertices() const { return !m_is_flushed && !m_cull_all; }

  void DoState(PointerWrap& p);

  FlushStatistics ResetFlushAspectRatioCount();

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
  virtual void UploadUtilityUniforms(const void* uniforms, u32 uniforms_size);
  void UploadUtilityVertices(const void* vertices, u32 vertex_stride, u32 num_vertices,
                             const u16* indices, u32 num_indices, u32* out_base_vertex,
                             u32* out_base_index);

  // Determine how many bytes there are in each element of the texel buffer.
  // Needed for alignment and stride calculations.
  static u32 GetTexelBufferElementSize(TexelBufferFormat buffer_format);

  // Texel buffer, used for palette conversion.
  virtual bool UploadTexelBuffer(const void* data, u32 data_size, TexelBufferFormat format,
                                 u32* out_offset);

  // The second set of parameters uploads a second blob in the same buffer, used for GPU texture
  // decoding for palette textures, as both the texture data and palette must be uploaded.
  virtual bool UploadTexelBuffer(const void* data, u32 data_size, TexelBufferFormat format,
                                 u32* out_offset, const void* palette_data, u32 palette_size,
                                 TexelBufferFormat palette_format, u32* out_palette_offset);

  // Call if active config changes
  void OnConfigChange();

  // CPU access tracking - call after a draw call is made.
  void OnDraw();

  // Call after CPU access is requested.
  void OnCPUEFBAccess();

  // Call after an EFB copy to RAM. If true, the current command buffer should be executed.
  void OnEFBCopyToRAM();

  // Call at the end of a frame.
  void OnEndFrame();

protected:
  // When utility uniforms are used, the GX uniforms need to be re-written afterwards.
  static void InvalidateConstants();

  // Prepares the buffer for the next batch of vertices.
  virtual void ResetBuffer(u32 vertex_stride);

  // Commits/uploads the current batch of vertices.
  virtual void CommitBuffer(u32 num_vertices, u32 vertex_stride, u32 num_indices,
                            u32* out_base_vertex, u32* out_base_index);

  // Uploads uniform buffers for GX draws.
  virtual void UploadUniforms();

  // Issues the draw call for the current batch in the backend.
  virtual void DrawCurrentBatch(u32 base_index, u32 num_indices, u32 base_vertex);

  u32 GetRemainingSize() const;
  u32 GetRemainingIndices(OpcodeDecoder::Primitive primitive) const;

  void CalculateZSlope(NativeVertexFormat* format);
  void CalculateBinormals(NativeVertexFormat* format);

  BitSet32 UsedTextures() const;

  u8* m_cur_buffer_pointer = nullptr;
  u8* m_base_buffer_pointer = nullptr;
  u8* m_end_buffer_pointer = nullptr;

  // Alternative buffers in CPU memory for primitives we are going to discard.
  std::vector<u8> m_cpu_vertex_buffer;
  std::vector<u16> m_cpu_index_buffer;

  Slope m_zslope = {};

  VideoCommon::GXPipelineUid m_current_pipeline_config;
  VideoCommon::GXUberPipelineUid m_current_uber_pipeline_config;
  const AbstractPipeline* m_current_pipeline_object = nullptr;
  PrimitiveType m_current_primitive_type = PrimitiveType::Points;
  bool m_pipeline_config_changed = true;
  bool m_rasterization_state_changed = true;
  bool m_depth_state_changed = true;
  bool m_blending_state_changed = true;
  bool m_cull_all = false;

  IndexGenerator m_index_generator;
  CPUCull m_cpu_cull;

private:
  // Minimum number of draws per command buffer when attempting to preempt a readback operation.
  static constexpr u32 MINIMUM_DRAW_CALLS_PER_COMMAND_BUFFER_FOR_READBACK = 10;

  void UpdatePipelineConfig();
  void UpdatePipelineObject();

  bool m_is_flushed = true;
  FlushStatistics m_flush_statistics = {};

  // CPU access tracking
  u32 m_draw_counter = 0;
  u32 m_last_efb_copy_draw_counter = 0;
  bool m_unflushed_efb_copy = false;
  std::vector<u32> m_cpu_accesses_this_frame;
  std::vector<u32> m_scheduled_command_buffer_kicks;
  bool m_allow_background_execution = true;

  Common::EventHook m_frame_end_event;
};

extern std::unique_ptr<VertexManagerBase> g_vertex_manager;
