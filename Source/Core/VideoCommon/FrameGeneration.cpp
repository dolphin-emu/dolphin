// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/FrameGeneration.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>

#include "Common/Hash.h"
#include "Common/Logging/Log.h"
#include "Common/MathUtil.h"

#include "Core/System.h"

#include "VideoCommon/PerformanceMetrics.h"

#include "VideoCommon/AbstractFramebuffer.h"
#include "VideoCommon/AbstractPipeline.h"
#include "VideoCommon/AbstractShader.h"
#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/FramebufferShaderGen.h"
#include "VideoCommon/ShaderCache.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VideoConfig.h"

namespace VideoCommon
{
namespace
{
// Linearly interpolates a run of matrix rows held as an array of float4.
template <size_t N>
void LerpRows(const std::array<float4, N>& from, const std::array<float4, N>& to, float phase,
              std::array<float4, N>* out)
{
  for (size_t row = 0; row < N; ++row)
  {
    for (size_t component = 0; component < 4; ++component)
    {
      const float a = from[row][component];
      const float b = to[row][component];
      (*out)[row][component] = a + (b - a) * phase;
    }
  }
}

// Linearly interpolates one float4 and puts the result back on the unit sphere.
//
// A direction is stored as a unit vector, and the straight line between two of them cuts the
// corner: at half way it is shorter than either, which the shaders read as the light being weaker
// there than at both ends. Only the first three components are a direction; the fourth carries
// something else and is left as it is found.
void LerpDirection(const float4& from, const float4& to, float phase, float4* out)
{
  for (size_t component = 0; component < 3; ++component)
    (*out)[component] = from[component] + (to[component] - from[component]) * phase;

  const float length =
      std::sqrt((*out)[0] * (*out)[0] + (*out)[1] * (*out)[1] + (*out)[2] * (*out)[2]);
  if (length <= 0.0f)
    return;

  for (size_t component = 0; component < 3; ++component)
    (*out)[component] /= length;
}
}  // Anonymous namespace

void RecordedFrame::Reset()
{
  // The capacity is deliberately kept, as a recording is refilled with a very similar amount of
  // data every frame and reallocating all of it each time would show up in the frame time.
  xfb_copy = {};
  has_xfb_copy = false;
  commands.clear();
  clears.clear();
  draws.clear();
  copies.clear();
  vertex_data.clear();
  index_data.clear();
  copy_uniform_data.clear();
  vertex_constants.clear();
  pixel_constants.clear();
  geometry_constants.clear();
}

TextureIdentity IdentifyTexture(const TCacheEntry& entry)
{
  return TextureIdentity{.addr = entry.addr,
                         .size_in_bytes = entry.size_in_bytes,
                         .memory_stride = entry.memory_stride,
                         .format = entry.format};
}

DrawCorrespondence CompareDraws(const RecordedDraw& a, const RecordedDraw& b)
{
  // The pipeline covers the shaders and all of the raster, depth and blend state, so two draws
  // sharing one are being drawn the same way. Everything else here establishes that they are also
  // drawing the same amount of the same kind of thing.
  if (a.pipeline != b.pipeline || a.vertex_format != b.vertex_format ||
      a.primitive_type != b.primitive_type || a.vertex_count != b.vertex_count ||
      a.index_count != b.index_count)
  {
    return DrawCorrespondence::DifferentShape;
  }

  // Two draws that use different textures are different objects, even if everything else lines up.
  for (u32 unit = 0; unit < NUM_RECORDED_TEXTURES; ++unit)
  {
    // A texture the game rendered this frame is a fresh cache entry every time, so comparing the
    // entries would say every draw that samples one is a different draw from the one before it,
    // and nothing that reads a reflection or a shadow map would ever interpolate. What identifies
    // it across frames is which copy of the frame produced it, not where that copy landed.
    if (a.texture_copy_slots[unit] != 0 || b.texture_copy_slots[unit] != 0)
    {
      if (a.texture_copy_slots[unit] != b.texture_copy_slots[unit])
        return DrawCorrespondence::DifferentTexture;
      continue;
    }

    const RcTcacheEntry& a_texture = a.textures[unit];
    const RcTcacheEntry& b_texture = b.textures[unit];
    if (!a_texture || !b_texture)
    {
      if (a_texture != b_texture)
        return DrawCorrespondence::DifferentTexture;
      continue;
    }

    // By the slot the texture occupies rather than by which cache entry it happens to be. See
    // TextureIdentity for why the entry and its hash are both the wrong thing to compare.
    if (!(IdentifyTexture(*a_texture) == IdentifyTexture(*b_texture)))
      return DrawCorrespondence::DifferentTexture;
  }

  return DrawCorrespondence::Yes;
}

bool DrawsCorrespond(const RecordedDraw& a, const RecordedDraw& b)
{
  return CompareDraws(a, b) == DrawCorrespondence::Yes;
}

float TransformDistance(const VertexShaderConstants& a, const VertexShaderConstants& b)
{
  // The position matrix alone. The normal matrix follows it for anything rigid, and the texture
  // and post transform matrices say nothing about where an object is.
  float total = 0.0f;
  for (size_t row = 0; row < 3; ++row)
  {
    for (size_t component = 0; component < 4; ++component)
    {
      const float difference =
          a.posnormalmatrix[row][component] - b.posnormalmatrix[row][component];
      total += difference * difference;
    }
  }

  return total;
}

void InterpolateTransforms(const VertexShaderConstants& from, const VertexShaderConstants& to,
                           float phase, VertexShaderConstants* out)
{
  // Start from the current frame, so that everything not interpolated below (lighting, material
  // colours, texture dimensions, the vertex loader offsets) is whatever this frame asked for.
  *out = to;

  LerpRows(from.posnormalmatrix, to.posnormalmatrix, phase, &out->posnormalmatrix);
  LerpRows(from.projection, to.projection, phase, &out->projection);
  LerpRows(from.transformmatrices, to.transformmatrices, phase, &out->transformmatrices);
  LerpRows(from.normalmatrices, to.normalmatrices, phase, &out->normalmatrices);
  LerpRows(from.texmatrices, to.texmatrices, phase, &out->texmatrices);
  LerpRows(from.posttransformmatrices, to.posttransformmatrices, phase,
           &out->posttransformmatrices);

  // Lights are held in view space, so a static lamp in a room still moves every frame as the
  // camera does. Leaving them on the newer frame's values puts the lighting one whole frame ahead
  // of the geometry it is lighting, which shows up as a highlight that jitters against the surface
  // it sits on rather than sliding across it. It is the same story for the emboss texture
  // coordinate, which is generated from the direction to the light and so is a texture coordinate
  // that moves with the lighting rather than with any matrix.
  //
  // Only the geometry of a light is interpolated. Its colour and its attenuation curves are what
  // the game asked for this frame, and a game that changes those is changing the look of the
  // scene rather than moving anything through it.
  for (size_t light = 0; light < from.lights.size(); ++light)
  {
    const auto& light_from = from.lights[light];
    const auto& light_to = to.lights[light];
    auto& light_out = out->lights[light];

    for (size_t component = 0; component < 4; ++component)
    {
      light_out.pos[component] =
          light_from.pos[component] + (light_to.pos[component] - light_from.pos[component]) * phase;
    }

    LerpDirection(light_from.dir, light_to.dir, phase, &light_out.dir);
  }

  // Deliberately left alone: cached_normal, cached_tangent and cached_binormal. They look like
  // directions, but they are vertex attributes rather than animated state, and the length of the
  // tangent and binormal is what sets the size of the emboss effect. Putting them back on the unit
  // sphere would change how strong that effect is, and interpolating them without doing so would
  // only ever mix one mesh's attributes with another's.
}

float TransformMagnitude(const VertexShaderConstants& constants)
{
  float total = 0.0f;
  for (size_t row = 0; row < 3; ++row)
  {
    for (size_t component = 0; component < 4; ++component)
    {
      const float value = constants.posnormalmatrix[row][component];
      total += value * value;
    }
  }

  return total;
}

FrameRecorder::FrameRecorder() = default;
FrameRecorder::~FrameRecorder() = default;

template <typename T>
u32 FrameRecorder::PoolConstants(std::vector<T>* pool, const T& value)
{
  if (!pool->empty() && std::memcmp(&pool->back(), &value, sizeof(T)) == 0)
    return static_cast<u32>(pool->size() - 1);

  pool->push_back(value);
  return static_cast<u32>(pool->size() - 1);
}

void FrameRecorder::BeginFrame()
{
  // A frame that draws nothing leaves the draw counter at zero, so this can be reached more than
  // once for the same frame. Opening a recording that is already open would step over one of the
  // two completed ones.
  if (m_recording)
    return;

  // The slot that is neither of the two completed ones, so that a generated frame being drawn from
  // those is unaffected by the emulated GPU starting the next frame underneath it.
  m_current = Newer(m_completed);
  m_frames[m_current].Reset();
  m_copy_slots.clear();
  m_recording = true;

  // The clear that ended the last frame is the one that sets this frame's background, so it opens
  // the new recording rather than being lost with the old one.
  if (m_has_pending_clear)
  {
    m_has_pending_clear = false;
    RecordClear(m_pending_clear);
  }
}

void FrameRecorder::RecordClear(const RecordedClear& clear)
{
  // Games issue the frame's main clear alongside the copy that ends the frame, which is to say
  // after the recording has already been closed. Hold it for the next one rather than dropping it.
  if (!m_recording)
  {
    m_pending_clear = clear;
    m_has_pending_clear = true;
    return;
  }

  RecordedFrame& frame = m_frames[m_current];
  frame.clears.push_back(clear);
  frame.commands.push_back({RecordedCommandType::Clear, static_cast<u32>(frame.clears.size() - 1)});
}

void FrameRecorder::RecordXFBCopy(const AbstractPipeline* pipeline, bool linear_filter,
                                  const void* uniforms, u32 uniform_size,
                                  const TextureConfig& destination_config)
{
  if (!m_recording || pipeline == nullptr)
    return;

  RecordedFrame& frame = m_frames[m_current];

  // Its own arena slot at the end of the copy uniforms, so it does not disturb the offsets the
  // ordinary copies were recorded with.
  RecordedCopy& copy = frame.xfb_copy;
  copy = {};
  copy.pipeline = pipeline;
  copy.from_depth = false;
  copy.linear_filter = linear_filter;
  copy.uniform_offset = static_cast<u32>(frame.copy_uniform_data.size());
  copy.uniform_size = uniform_size;
  copy.destination_config = destination_config;

  const u8* const bytes = static_cast<const u8*>(uniforms);
  frame.copy_uniform_data.insert(frame.copy_uniform_data.end(), bytes, bytes + uniform_size);

  frame.has_xfb_copy = true;
}

void FrameRecorder::RecordCopy(const AbstractPipeline* pipeline, bool from_depth,
                               bool linear_filter, const void* uniforms, u32 uniform_size,
                               const RcTcacheEntry& destination)
{
  if (!m_recording || pipeline == nullptr || !destination || !destination->texture)
    return;

  RecordedFrame& frame = m_frames[m_current];
  if (frame.copies.size() >= MAX_RECORDED_COPIES)
    return;

  RecordedCopy copy;
  copy.pipeline = pipeline;
  copy.from_depth = from_depth;
  copy.linear_filter = linear_filter;
  copy.uniform_offset = static_cast<u32>(frame.copy_uniform_data.size());
  copy.uniform_size = uniform_size;
  copy.destination_config = destination->texture->GetConfig();
  copy.destination = destination;

  const u8* const bytes = static_cast<const u8*>(uniforms);
  frame.copy_uniform_data.insert(frame.copy_uniform_data.end(), bytes, bytes + uniform_size);

  frame.copies.push_back(std::move(copy));
  const u32 index = static_cast<u32>(frame.copies.size() - 1);
  frame.commands.push_back({RecordedCommandType::Copy, index});

  // A later draw that samples this entry has to be pointed at the replay's own copy rather than
  // the one the emulated GPU just made. Slots are numbered from one so that zero can mean the
  // texture is an ordinary one, which is the overwhelmingly common case.
  m_copy_slots[destination.get()] = index + 1;
}

void FrameRecorder::RecordDraw(const AbstractPipeline* pipeline, NativeVertexFormat* vertex_format,
                               PrimitiveType primitive_type, const u8* vertex_data,
                               u32 vertex_count, u32 vertex_stride, const u16* index_data,
                               u32 index_count, const VertexShaderConstants& vertex_constants,
                               const PixelShaderConstants& pixel_constants,
                               const GeometryShaderConstants& geometry_constants,
                               const std::array<RcTcacheEntry, NUM_RECORDED_TEXTURES>& textures,
                               const std::array<SamplerState, NUM_RECORDED_TEXTURES>& samplers,
                               BitSet32 used_textures)
{
  if (!m_recording || pipeline == nullptr || index_count == 0)
    return;

  RecordedFrame& frame = m_frames[m_current];

  const size_t geometry_bytes = frame.vertex_data.size() + frame.index_data.size() * sizeof(u16) +
                                static_cast<size_t>(vertex_count) * vertex_stride +
                                index_count * sizeof(u16);
  if (geometry_bytes > MAX_RECORDED_GEOMETRY_BYTES)
  {
    // Give up on this frame instead of letting the recording grow without bound.
    //
    // The slot is emptied but still completed at the end of the frame, which is what m_abandoned is
    // for. Simply stopping here would leave EndFrame() with nothing to do, so neither the completed
    // slot nor the frame counter would move, and the replay would go on showing the pair from
    // before this frame for as long as the game kept drawing this much: frozen picture, live
    // emulation, then a jump. An empty recording is unusable, so instead nothing is generated until
    // a frame comes in under the budget.
    frame.Reset();
    m_copy_slots.clear();
    m_abandoned = true;
    m_recording = false;
    return;
  }

  RecordedDraw draw;
  draw.pipeline = pipeline;
  draw.vertex_format = vertex_format;
  draw.primitive_type = primitive_type;
  draw.vertex_count = vertex_count;
  draw.vertex_stride = vertex_stride;
  draw.index_count = index_count;
  draw.viewport_and_scissor = g_gfx->GetViewportAndScissor();

  // 'vertex_data' points into the mapped streaming buffer, which the backends allocate as
  // write-combined memory: OpenGL maps it without GL_MAP_READ_BIT and Vulkan asks VMA for
  // sequential host writes only. Reading it back is uncached, so it is read exactly once, in one
  // sequential pass, and everything that wants to look at the vertices again works from the copy.
  const u32 vertex_data_size = vertex_count * vertex_stride;
  draw.vertex_data_offset = static_cast<u32>(frame.vertex_data.size());
  frame.vertex_data.insert(frame.vertex_data.end(), vertex_data, vertex_data + vertex_data_size);

  // Hashed from the copy rather than from the streaming buffer. The hash reads in a pattern that
  // an uncached mapping serves badly, and on a busy frame that alone costs more than every other
  // part of recording put together.
  draw.vertex_hash =
      Common::GetHash64(frame.vertex_data.data() + draw.vertex_data_offset, vertex_data_size, 0);

  draw.index_data_offset = static_cast<u32>(frame.index_data.size());
  frame.index_data.insert(frame.index_data.end(), index_data, index_data + index_count);

  draw.vertex_constants = PoolConstants(&frame.vertex_constants, vertex_constants);
  draw.pixel_constants = PoolConstants(&frame.pixel_constants, pixel_constants);
  draw.geometry_constants = PoolConstants(&frame.geometry_constants, geometry_constants);

  // Only the units this draw actually samples. The cache keeps entries bound on the other units
  // from earlier draws, and recording those would make two otherwise identical draws look
  // different to DrawsCorrespond purely because of what came before them.
  for (u32 unit = 0; unit < NUM_RECORDED_TEXTURES; ++unit)
  {
    if (!used_textures[unit] || !textures[unit])
      continue;

    draw.textures[unit] = textures[unit];
    draw.samplers[unit] = samplers[unit];

    const auto slot = m_copy_slots.find(textures[unit].get());
    if (slot != m_copy_slots.end())
      draw.texture_copy_slots[unit] = slot->second;
  }

  frame.draws.push_back(std::move(draw));
  frame.commands.push_back({RecordedCommandType::Draw, static_cast<u32>(frame.draws.size() - 1)});
}

void FrameRecorder::EndFrame()
{
  // An abandoned recording is completed too, empty. See the budget check in RecordDraw().
  if (!m_recording && !m_abandoned)
    return;

  m_recording = false;
  m_abandoned = false;
  m_completed = m_current;
  ++m_frame_counter;

  // How far apart real frames are arriving, measured where the pair being interpolated between
  // changes rather than at present time, so the two describe the same thing.
  const TimePoint now = Clock::now();
  m_frame_interval = (m_last_frame_time == TimePoint{}) ? DT::zero() : now - m_last_frame_time;
  m_last_frame_time = now;
}

void FrameRecorder::Invalidate()
{
  for (RecordedFrame& frame : m_frames)
    frame.Reset();
  m_copy_slots.clear();
  m_recording = false;
  m_abandoned = false;
  m_has_pending_clear = false;
  m_last_frame_time = {};
  m_frame_interval = DT::zero();
}

bool FrameRecorder::CanInterpolate() const
{
  return GetCurrentFrame().IsUsable() && GetPreviousFrame().IsUsable();
}

bool FrameRecorder::HasSteadyFrameInterval() const
{
  // Outside this range the emulation is not running at a steady rate: it has just started, been
  // paused, hit a loading screen, or is being fast forwarded. Interpolating across that would
  // produce a long smear, so the real frame is shown on its own instead.
  constexpr DT MIN_INTERVAL = std::chrono::milliseconds(1);
  constexpr DT MAX_INTERVAL = std::chrono::milliseconds(200);
  return m_frame_interval >= MIN_INTERVAL && m_frame_interval <= MAX_INTERVAL;
}

ReplayTargets::ReplayTargets() = default;

ReplayTargets::~ReplayTargets()
{
  Release();
}

AbstractFramebuffer* ReplayTargets::GetFramebuffer(u32 slot, const TextureConfig& config)
{
  if (slot >= m_targets.size())
    m_targets.resize(slot + 1);

  Target& target = m_targets[slot];
  if (target.framebuffer && target.texture->GetConfig() == config)
    return target.framebuffer.get();

  // What is here is the wrong shape for the copy that wants it. Slots are reused frame after frame
  // and games are consistent about what they render where, so this happens when the internal
  // resolution changes or when a game switches to a different set of passes, not every frame.
  if (target.texture && g_gfx)
    g_gfx->UnbindTexture(target.texture.get());
  target.framebuffer.reset();
  target.texture.reset();

  target.texture = g_gfx->CreateTexture(config, "Frame generation replay copy");
  if (!target.texture)
    return nullptr;

  target.framebuffer = g_gfx->CreateFramebuffer(target.texture.get(), nullptr);
  if (!target.framebuffer)
  {
    target.texture.reset();
    return nullptr;
  }

  return target.framebuffer.get();
}

AbstractTexture* ReplayTargets::GetTexture(u32 slot) const
{
  if (slot >= m_targets.size())
    return nullptr;

  return m_targets[slot].texture.get();
}

void ReplayTargets::Release()
{
  // A texture that is still bound must not be left dangling in the backend's descriptor state.
  // This also runs from the destructor during shutdown, by which point there may be no backend
  // left to tell.
  if (g_gfx)
  {
    for (Target& target : m_targets)
    {
      if (target.texture)
        g_gfx->UnbindTexture(target.texture.get());
    }
  }

  m_targets.clear();
}

FrameGenerator::FrameGenerator() = default;

FrameGenerator::~FrameGenerator()
{
  DestroyResources();
}

void FrameGenerator::DestroyResources()
{
  // A texture that is still bound must not be left dangling in the backend's descriptor state.
  // The backends dereference what they are handed, so nothing here may be null: this also runs
  // before the first allocation, when none of it exists yet.
  if (g_gfx)
  {
    // The pipelines below are destroyed the moment they are released, and a pipeline must outlive
    // every command buffer that has been submitted with it still bound. Textures and buffers are
    // safe without this -- the backends hold those back until the frame they belong to has retired
    // -- but nothing defers a pipeline, so this has to wait for the work already in flight.
    //
    // The case that reaches this is a change of internal resolution: EnsureResources() finds the
    // EFB a different size, calls here, and on a backend that encodes command buffers on its own
    // thread the pipeline is freed underneath one still being encoded. This is the same wait
    // CheckForConfigChanges() does before reloading the shader cache, for the same reason.
    if (m_accumulate_pipeline || m_resolve_pipeline)
      g_gfx->WaitForGPUIdle();

    if (m_color_texture)
      g_gfx->UnbindTexture(m_color_texture.get());
    if (m_bucket_texture)
      g_gfx->UnbindTexture(m_bucket_texture.get());
    if (m_output_texture)
      g_gfx->UnbindTexture(m_output_texture.get());
    if (m_xfb_texture)
      g_gfx->UnbindTexture(m_xfb_texture.get());
  }

  m_xfb_framebuffer.reset();
  m_xfb_texture.reset();
  m_xfb_config = {};
  m_resolve_pipeline.reset();
  m_resolve_shader.reset();
  m_output_framebuffer.reset();
  m_output_texture.reset();
  m_accumulate_pipeline.reset();
  m_accumulate_shader.reset();
  m_bucket_framebuffer.reset();
  m_bucket_texture.reset();
  m_framebuffer.reset();
  m_depth_texture.reset();
  m_color_texture.reset();

  m_samples = 0;
  m_last_frame_counter = 0;
  m_fields_this_frame = 0;
  m_fields_per_frame = 1;
  m_field_advanced_by = 0;
  m_resources_failed = false;
}

bool FrameGenerator::EnsureResources()
{
  if (m_resources_failed)
    return false;

  const TextureConfig color_config(
      g_framebuffer_manager->GetEFBWidth(), g_framebuffer_manager->GetEFBHeight(), 1,
      g_framebuffer_manager->GetEFBLayers(), 1, FramebufferManager::GetEFBColorFormat(),
      AbstractTextureFlag_RenderTarget, AbstractTextureType::Texture_2DArray);

  if (m_resolve_pipeline && m_color_texture->GetConfig() == color_config)
    return true;

  // Something about the EFB has changed, so everything built to match it has to be built again.
  DestroyResources();

  TextureConfig depth_config = color_config;
  depth_config.format = FramebufferManager::GetEFBDepthFormat();

  // Averaging happens in light, where the values run further apart than the stored ones do and the
  // darks need far more precision than eight bits would leave them after repeated blending.
  TextureConfig bucket_config = color_config;
  bucket_config.format = AbstractTextureFormat::RGBA16F;

  m_color_texture = g_gfx->CreateTexture(color_config, "Frame generation color");
  m_depth_texture = g_gfx->CreateTexture(depth_config, "Frame generation depth");
  m_bucket_texture = g_gfx->CreateTexture(bucket_config, "Frame generation bucket");
  m_output_texture = g_gfx->CreateTexture(color_config, "Frame generation output");

  if (m_color_texture && m_depth_texture)
    m_framebuffer = g_gfx->CreateFramebuffer(m_color_texture.get(), m_depth_texture.get());
  if (m_bucket_texture)
    m_bucket_framebuffer = g_gfx->CreateFramebuffer(m_bucket_texture.get(), nullptr);
  if (m_output_texture)
    m_output_framebuffer = g_gfx->CreateFramebuffer(m_output_texture.get(), nullptr);

  m_accumulate_shader = g_gfx->CreateShaderFromSource(
      ShaderStage::Pixel, FramebufferShaderGen::GenerateFrameAccumulatePixelShader(), nullptr,
      "Frame generation accumulate pixel shader");

  m_resolve_shader = g_gfx->CreateShaderFromSource(
      ShaderStage::Pixel, FramebufferShaderGen::GenerateFrameResolvePixelShader(), nullptr,
      "Frame generation resolve pixel shader");

  if (m_framebuffer && m_bucket_framebuffer && m_output_framebuffer && m_accumulate_shader &&
      m_resolve_shader)
  {
    AbstractPipelineConfig config = {};
    config.vertex_shader = g_shader_cache->GetScreenQuadVertexShader();
    config.geometry_shader = g_framebuffer_manager->IsEFBStereo() ?
                                 g_shader_cache->GetTexcoordGeometryShader() :
                                 nullptr;
    config.pixel_shader = m_accumulate_shader.get();
    config.rasterization_state = RenderState::GetNoCullRasterizationState(PrimitiveType::Triangles);
    config.depth_state = RenderState::GetNoDepthTestingDepthState();
    config.blending_state = RenderState::GetNoBlendingBlendState();
    config.blending_state.blend_enable = true;
    config.blending_state.src_factor = SrcBlendFactor::SrcAlpha;
    config.blending_state.dst_factor = DstBlendFactor::InvSrcAlpha;
    // The weight of the running average travels in the alpha channel, so it must not be written
    // out. Leaving the bucket's own alpha alone keeps the opaque value it is cleared to below.
    config.blending_state.alpha_update = false;
    config.framebuffer_state = RenderState::GetColorFramebufferState(bucket_config.format);
    config.usage = AbstractPipelineUsage::Utility;
    m_accumulate_pipeline = g_gfx->CreatePipeline(config);

    // The resolve just writes, so it wants none of the blending the accumulation is built around.
    config.pixel_shader = m_resolve_shader.get();
    config.blending_state = RenderState::GetNoBlendingBlendState();
    config.framebuffer_state = RenderState::GetColorFramebufferState(color_config.format);
    m_resolve_pipeline = g_gfx->CreatePipeline(config);
  }

  if (!m_accumulate_pipeline || !m_resolve_pipeline)
  {
    ERROR_LOG_FMT(VIDEO, "Failed to create the frame generation render targets, disabling it.");
    DestroyResources();
    m_resources_failed = true;
    return false;
  }

  // The first frame of every display frame is drawn with a weight of one and so replaces the
  // colour entirely, but alpha is never written, so it is set once here and then left alone.
  g_gfx->SetAndClearFramebuffer(m_bucket_framebuffer.get(), {0.0f, 0.0f, 0.0f, 1.0f});
  return true;
}

void FrameGenerator::RenderIntoBucket(u32 part, u32 parts)
{
  m_samples = 0;

  if (!g_ActiveConfig.frame_generation.bEnabled)
  {
    // Give the memory back rather than holding onto targets the size of the EFB indefinitely.
    // Unconditional, so that a setup which failed once is tried again after switching off and on.
    DestroyResources();
    return;
  }

  if (g_vertex_manager == nullptr)
    return;

  const FrameRecorder& recorder = g_vertex_manager->GetFrameRecorder();

  // Counted whatever happens to this field, ahead of everything that can return early, so that a
  // field which generates nothing still moves the pattern along rather than leaving the next one
  // on the same slice.
  if (const u64 counter = recorder.GetFrameCounter(); counter != m_last_frame_counter)
  {
    m_last_frame_counter = counter;
    if (m_fields_this_frame > 0)
      m_fields_per_frame = std::clamp(m_fields_this_frame, 1u, MAX_FIELDS_PER_FRAME);
    m_fields_this_frame = 0;
    m_field_advanced_by = 0;
  }

  // Which slice of the interpolation window this picture is the exposure for, and how wide that
  // slice is. Both come from counting fields rather than from the clock.
  //
  // A real frame is shown for a whole number of fields, so the fields covering it divide its
  // interval into exactly that many equal slices. Which one this field is, is just how many have
  // gone by since the recording last changed. Consecutive exposures then butt up against each
  // other exactly, every time, and a fixed sub-frame count puts the samples on the same phases
  // frame after frame instead of wherever the last measurement happened to land.
  //
  // How many fields a frame gets is taken from how many the one before it got, which is right as
  // long as the game holds a steady rate and self-corrects within one frame when it does not.
  const u32 slot = std::min(m_fields_this_frame, m_fields_per_frame - 1);

  // A field presented several times over is several exposures rather than one, each covering its
  // own share of the field's slice, so the field advances once the last of them has been drawn.
  // Advancing on the first instead would leave every image after it reading the next field's slot
  // and the exposures jumping about inside the frame rather than tiling it.
  if (++m_field_advanced_by >= parts)
  {
    m_field_advanced_by = 0;
    ++m_fields_this_frame;
  }

  const u32 slices = m_fields_per_frame * parts;
  const float width = 1.0f / static_cast<float>(slices);
  const float phase_start = static_cast<float>(slot * parts + part) * width;
  const float phase_end = phase_start + width;

  if (!g_vertex_manager->CanReplayRecordedFrame())
    return;

  const RecordedFrame& frame = recorder.GetCurrentFrame();

  // Without the copy that made the visible image there is nothing to put a generated frame through,
  // and so no way to hand one over that is toned like the real one. The timing also has to have
  // settled into a steady rate before interpolating across it means anything.
  if (!frame.has_xfb_copy || !recorder.HasSteadyFrameInterval() || !EnsureResources())
    return;

  // How many sub-frames this one exposure is made of.
  //
  // The configured multiplier is how many frames are generated for each frame the game produces,
  // so it is shared out across every image that frame is shown as rather than spent again on each
  // of them. A display taking more images therefore gets more distinct pictures out of the same
  // budget, not more work: the extra refreshes are what the frames are spent on, and each exposure
  // gets correspondingly shorter, which is exactly the trade a faster display should buy.
  //
  // One is the floor. That is a single unblurred picture per image, which is what a display fast
  // enough to show every generated frame on its own should be given.
  const u32 images = slices;
  const u32 count = std::clamp(g_ActiveConfig.frame_generation.iMultiplier / images, 1u,
                               MAX_GENERATED_FRAMES_PER_DISPLAY_FRAME);

  for (u32 sample = 0; sample < count; ++sample)
  {
    // Bin centres rather than edges, so the samples sit evenly inside the window and the exposure
    // adds up to a flat one, without either end being weighted twice.
    const float phase =
        phase_start + (phase_end - phase_start) * ((sample + 0.5f) / static_cast<float>(count));

    g_vertex_manager->ReplayRecordedFrame(m_framebuffer.get(), phase);

    Accumulate();
    ++m_samples;

    // Each of these is a frame the GPU really drew, which is what the frame rate counter measures.
    Core::System::GetInstance().GetPerfMetrics().CountFrame();
  }

  Resolve();

  // Nothing fit to show without this, so drop the exposure rather than hand over a frame that is
  // toned differently from the real ones it sits between.
  if (!ConvertToXFB(frame))
    m_samples = 0;
}

void FrameGenerator::Resolve()
{
  g_gfx->BeginUtilityDrawing();

  g_gfx->SetFramebuffer(m_output_framebuffer.get());
  g_gfx->SetViewportAndScissor(m_output_framebuffer->GetRect());
  g_gfx->SetPipeline(m_resolve_pipeline.get());
  g_gfx->SetTexture(0, m_bucket_texture.get());
  g_gfx->SetSamplerState(0, RenderState::GetPointSamplerState());
  g_gfx->Draw(0, 3);

  g_gfx->EndUtilityDrawing();
}

bool FrameGenerator::ConvertToXFB(const RecordedFrame& frame)
{
  if (!frame.has_xfb_copy)
    return false;

  const RecordedCopy& copy = frame.xfb_copy;

  // Made to match whatever the game copied into, and remade when that changes. Games are steady
  // about this, so it happens when the internal resolution or the video mode does, not per frame.
  if (!m_xfb_framebuffer || m_xfb_config != copy.destination_config)
  {
    if (m_xfb_texture && g_gfx)
      g_gfx->UnbindTexture(m_xfb_texture.get());
    m_xfb_framebuffer.reset();
    m_xfb_texture.reset();

    m_xfb_texture = g_gfx->CreateTexture(copy.destination_config, "Frame generation XFB");
    if (!m_xfb_texture)
      return false;

    m_xfb_framebuffer = g_gfx->CreateFramebuffer(m_xfb_texture.get(), nullptr);
    if (!m_xfb_framebuffer)
    {
      m_xfb_texture.reset();
      return false;
    }

    m_xfb_config = copy.destination_config;
  }

  g_gfx->BeginUtilityDrawing();
  m_output_texture->FinishedRendering();

  // The recorded uniforms address the EFB in normalised coordinates and the resolved frame is the
  // same size as the EFB, so they land on it exactly as they landed on the real one. Running the
  // game's own copy is what gets the pixel engine's gamma, the deflicker filter and the clamping
  // applied identically to a generated frame and a real one.
  g_vertex_manager->UploadUtilityUniforms(frame.copy_uniform_data.data() + copy.uniform_offset,
                                          copy.uniform_size);
  TextureCacheBase::DrawEFBCopy(m_xfb_framebuffer.get(), m_output_texture.get(), copy.pipeline,
                                copy.linear_filter);

  return true;
}

void FrameGenerator::Accumulate()
{
  // The accumulation is an ordinary utility draw, unlike the replay that precedes it. On OpenGL
  // this is also what switches off the clip distances that the emulated pipelines write, which
  // the screen quad vertex shader does not.
  g_gfx->BeginUtilityDrawing();

  g_gfx->SetFramebuffer(m_bucket_framebuffer.get());
  g_gfx->SetViewportAndScissor(m_bucket_framebuffer->GetRect());
  g_gfx->SetPipeline(m_accumulate_pipeline.get());
  g_gfx->SetTexture(0, m_color_texture.get());
  g_gfx->SetSamplerState(0, RenderState::GetPointSamplerState());

  // A linear ramp, not a flat average: the nth frame of the exposure counts n times as much as the
  // first. The shutter opens gently and closes on the instant the picture is shown, which keeps the
  // smoothing while leaving the result weighted towards where things actually are now rather than
  // the middle of the window. That buys back much of the latency a flat exposure of the same length
  // would cost, for very little of its steadiness.
  //
  // Feeding a running mean, the weights 1, 2 ... n sum to n(n+1)/2, so the nth frame has to be
  // blended in with n over that, which reduces to 2/(n+1), for the bucket to hold the ramp at every
  // point along the way.
  const float nth = static_cast<float>(m_samples + 1);
  const std::array<float, 4> weight = {2.0f / (nth + 1.0f), 0.0f, 0.0f, 0.0f};
  g_vertex_manager->UploadUtilityUniforms(weight.data(),
                                          static_cast<u32>(sizeof(float) * weight.size()));

  g_gfx->Draw(0, 3);

  // Puts the EFB and the emulated viewport back, and on OpenGL turns the clip distances that the
  // next replayed frame depends on for depth clamping back on.
  g_gfx->EndUtilityDrawing();
}

const AbstractTexture* FrameGenerator::GetGeneratedXFB() const
{
  if (m_samples == 0)
    return nullptr;

  return m_xfb_texture.get();
}
}  // namespace VideoCommon
