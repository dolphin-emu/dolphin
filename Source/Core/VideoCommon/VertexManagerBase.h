// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <vector>

#include "Common/BitSet.h"
#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"
#include "VideoCommon/CPUCull.h"
#include "VideoCommon/FrameGeneration.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/RenderState.h"
#include "VideoCommon/ShaderCache.h"
#include "VideoCommon/VideoEvents.h"

struct CustomPixelShaderContents;
class CustomShaderCache;
class DataReader;
class GeometryShaderManager;
class NativeVertexFormat;
class PixelShaderManager;
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
}

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

      MathUtil::RunningMean<float> average_ratio;
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
  void NotifyCustomShaderCacheOfHostChange(const ShaderHostConfig& host_config);

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

  // The draw calls captured for the frame generator to replay. Empty unless frame generation is
  // switched on.
  VideoCommon::FrameRecorder& GetFrameRecorder() { return m_frame_recorder; }

  // Whether frame generation is switched on and this configuration can carry it. Decides whether
  // frames are recorded as well as whether they are replayed, so that the two never disagree and
  // leave the recorder doing work for a replay that cannot happen.
  bool IsFrameGenerationUsable() const;

  // Whether a recorded frame may be replayed at all right now.
  bool CanReplayRecordedFrame() const;

  // Draws the most recent recording again into `target`, with every transform interpolated back
  // towards the previous recording by `phase`. A phase of one reproduces the recorded frame, and
  // a phase of zero reproduces the one before it.
  //
  // This lives on VertexManagerBase because submitting a draw needs UploadUniforms(),
  // UploadUtilityVertices() and InvalidateConstants(), which are not public. It must only be
  // called once the emulated frame is finished, and `target` must be a framebuffer whose state
  // matches the EFB's, since that is what the recorded pipelines were compiled against.
  void ReplayRecordedFrame(AbstractFramebuffer* target, float phase);

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
  void CalculateNormals(NativeVertexFormat* format);

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

  // How far either side of the running alignment a replay looks for the previous frame's copy of
  // a draw. Games submit their scene in a very stable order, so the lists only ever slip by a few
  // draws at a time.
  //
  // Searching wider used to mean pairing up unrelated objects, because whatever the search found
  // was accepted. Now that a candidate has to be within CORRESPONDENCE_MAX_RELATIVE_DISTANCE to be
  // accepted at all, a wider window only ever finds better counterparts or none, so this is set by
  // what it costs rather than by what it risks -- and what it costs is a few hundred thousand float
  // comparisons on a heavy frame, which is nothing.
  static constexpr int CORRESPONDENCE_SEARCH_DISTANCE = 16;

  // How far a candidate's transform may be from a draw's own before the two are taken to be
  // different objects, as a fraction of the squared magnitude of the transform itself.
  //
  // Both quantities are sums of squares, so this is the square of the relative change allowed: a
  // tenth of the transform's own size.
  //
  // The useful way to read it is as a rate of camera rotation. Turn the camera by an angle, and an
  // object at distance d sweeps about d*theta across view space while the rotation block of its
  // matrix changes by about theta times its own size -- so both parts of the matrix change in
  // proportion to themselves, the distance and the object's scale cancel, and the ratio this
  // compares is simply theta squared. A tenth is a ceiling of 5.7 degrees per frame, about 170
  // degrees a second on a thirty frame per second game.
  //
  // That is not generous, and it is deliberate. The two errors are not symmetric. A draw that finds
  // no counterpart is submitted where the game put it and stays there: wrong by one frame, but the
  // same wrong in every sub-frame, which reads as the object running at the game's own rate. A draw
  // that pairs with the wrong one has its projection and texture matrices blended with an unrelated
  // draw's, so it streaks, distorts and flickers, and it does so differently every frame because
  // which wrong candidate wins changes. Missing a match costs steadiness; making a bad one costs
  // far more, and raising this to let fast pans through bought a great deal of the second to avoid
  // a little of the first.
  //
  // So the ceiling stays where a real camera stays. Widening the window it searches, or giving it
  // something better than a single distance to rank by, is how to catch more of the fast pans --
  // not letting worse candidates through.
  static constexpr float CORRESPONDENCE_MAX_RELATIVE_DISTANCE = 0.01f;

  // Puts back the background a recorded clear left, over the region it covered.
  void ReplayClear(AbstractFramebuffer* target, const VideoCommon::RecordedClear& clear);

  // Submits one recorded draw, interpolated against whichever draw in the previous recording
  // turns out to be the same one.
  // `alignment` is how far this frame's draw list is offset from the previous one's, carried
  // across the whole replay and nudged whenever a match is found off it.
  void ReplayDraw(const VideoCommon::RecordedFrame& current,
                  const VideoCommon::RecordedFrame& previous, u32 draw_index, float phase);

  // Works out which draw in `previous` each of `current`'s draws is, filling m_replay_matches.
  void ResolveCorrespondence(const VideoCommon::RecordedFrame& current,
                             const VideoCommon::RecordedFrame& previous);

  // Submits one recorded draw exactly as recorded, with `vertex_constants` in place of the ones it
  // was recorded with. Shared by every path that puts a recorded draw back on the GPU.
  void SubmitRecordedDraw(const VideoCommon::RecordedFrame& frame,
                          const VideoCommon::RecordedDraw& draw,
                          const VertexShaderConstants& vertex_constants);

  // Rebuilds m_replay_matches if it does not already describe this pair.
  void EnsureReplayCorrespondence(const VideoCommon::RecordedFrame& current,
                                  const VideoCommon::RecordedFrame& previous, u64 frame_counter);

  // Makes the replay's own version of one of the game's render-to-texture results, reading the
  // replay's EFB rather than the emulated one.
  void ReplayCopy(AbstractFramebuffer* target, const VideoCommon::RecordedFrame& current,
                  u32 copy_index);

  // Binds the textures a recorded draw sampled, going through the replay's own copies for anything
  // the frame rendered for itself.
  void BindReplayTextures(const VideoCommon::RecordedDraw& draw);

  void RenderDrawCall(PixelShaderManager& pixel_shader_manager,
                      GeometryShaderManager& geometry_shader_manager,
                      const CustomPixelShaderContents& custom_pixel_shader_contents,
                      std::span<u8> custom_pixel_shader_uniforms, PrimitiveType primitive_type,
                      const AbstractPipeline* current_pipeline, BitSet32 used_textures,
                      const std::array<SamplerState, 8>& samplers);
  void UpdatePipelineConfig();
  void UpdatePipelineObject();

  const AbstractPipeline*
  GetCustomPipeline(const CustomPixelShaderContents& custom_pixel_shader_contents,
                    const VideoCommon::GXPipelineUid& current_pipeline_config,
                    const VideoCommon::GXUberPipelineUid& current_uber_pipeline_confi,
                    const AbstractPipeline* current_pipeline) const;

  bool m_is_flushed = true;
  FlushStatistics m_flush_statistics = {};

  // CPU access tracking
  u32 m_draw_counter = 0;
  u32 m_last_efb_copy_draw_counter = 0;
  bool m_unflushed_efb_copy = false;
  std::vector<u32> m_cpu_accesses_this_frame;
  std::vector<u32> m_scheduled_command_buffer_kicks;
  bool m_allow_background_execution = true;

  std::unique_ptr<CustomShaderCache> m_custom_shader_cache;
  u64 m_ticks_elapsed = 0;

  VideoCommon::FrameRecorder m_frame_recorder;

  // Which draw in the previous recording each of the current recording's draws is, or null where
  // nothing corresponded. Resolved once per pair of recordings rather than per generated frame,
  // since the same pair is replayed several times over.
  std::vector<const VideoCommon::RecordedDraw*> m_replay_matches;

  // Which of the previous recording's draws have already been claimed, so that no two draws in
  // this one interpolate against the same counterpart.
  std::vector<bool> m_replay_claimed;

  u64 m_replay_counter = 0;

  // The render targets a replay makes its own copies of the game's render-to-texture results into.
  // Kept between replays, since the same frame is replayed several times over and the targets it
  // needs do not change from one sub-frame to the next.
  VideoCommon::ReplayTargets m_replay_targets;

  Common::EventHook m_frame_end_event;
  Common::EventHook m_after_present_event;
};

extern std::unique_ptr<VertexManagerBase> g_vertex_manager;
