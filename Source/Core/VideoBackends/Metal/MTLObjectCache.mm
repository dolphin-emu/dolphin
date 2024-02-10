// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Metal/MTLObjectCache.h"

#include <map>
#include <mutex>
#include <optional>

#include "Common/Assert.h"
#include "Common/MsgHandler.h"

#include "VideoBackends/Metal/MTLPipeline.h"
#include "VideoBackends/Metal/MTLUtil.h"
#include "VideoBackends/Metal/MTLVertexFormat.h"

#include "VideoCommon/AbstractPipeline.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/VertexShaderGen.h"
#include "VideoCommon/VideoConfig.h"

MRCOwned<id<MTLDevice>> Metal::g_device;
MRCOwned<id<MTLCommandQueue>> Metal::g_queue;
std::unique_ptr<Metal::ObjectCache> Metal::g_object_cache;

static void SetupDepthStencil(
    MRCOwned<id<MTLDepthStencilState>> (&dss)[Metal::DepthStencilSelector::N_VALUES]);

Metal::ObjectCache::ObjectCache()
{
  m_internal = std::make_unique<Internal>();
  SetupDepthStencil(m_dss);
}

Metal::ObjectCache::~ObjectCache()
{
}

void Metal::ObjectCache::Initialize(MRCOwned<id<MTLDevice>> device)
{
  g_device = std::move(device);
  g_queue = MRCTransfer([g_device newCommandQueue]);
  g_object_cache = std::unique_ptr<ObjectCache>(new ObjectCache);
}

void Metal::ObjectCache::Shutdown()
{
  g_object_cache.reset();
  g_queue = nullptr;
  g_device = nullptr;
}

// MARK: Depth Stencil State

// clang-format off

static MTLCompareFunction Convert(CompareMode mode)
{
  const bool invert_depth = !g_Config.backend_info.bSupportsReversedDepthRange;
  switch (mode)
  {
  case CompareMode::Never:   return MTLCompareFunctionNever;
  case CompareMode::Less:    return invert_depth ? MTLCompareFunctionGreater
                                                 : MTLCompareFunctionLess;
  case CompareMode::Equal:   return MTLCompareFunctionEqual;
  case CompareMode::LEqual:  return invert_depth ? MTLCompareFunctionGreaterEqual
                                                 : MTLCompareFunctionLessEqual;
  case CompareMode::Greater: return invert_depth ? MTLCompareFunctionLess
                                                 : MTLCompareFunctionGreater;
  case CompareMode::NEqual:  return MTLCompareFunctionNotEqual;
  case CompareMode::GEqual:  return invert_depth ? MTLCompareFunctionLessEqual
                                                 : MTLCompareFunctionGreaterEqual;
  case CompareMode::Always:  return MTLCompareFunctionAlways;
  }
}

static const char* to_string(MTLCompareFunction compare)
{
  switch (compare)
  {
  case MTLCompareFunctionNever:        return "Never";
  case MTLCompareFunctionGreater:      return "Greater";
  case MTLCompareFunctionEqual:        return "Equal";
  case MTLCompareFunctionGreaterEqual: return "GEqual";
  case MTLCompareFunctionLess:         return "Less";
  case MTLCompareFunctionNotEqual:     return "NEqual";
  case MTLCompareFunctionLessEqual:    return "LEqual";
  case MTLCompareFunctionAlways:       return "Always";
  }
}

// clang-format on

static void
SetupDepthStencil(MRCOwned<id<MTLDepthStencilState>> (&dss)[Metal::DepthStencilSelector::N_VALUES])
{
  auto desc = MRCTransfer([MTLDepthStencilDescriptor new]);
  Metal::DepthStencilSelector sel;
  for (size_t i = 0; i < std::size(dss); ++i)
  {
    sel.value = i;
    MTLCompareFunction mcompare = Convert(sel.CompareMode());
    [desc setDepthWriteEnabled:sel.UpdateEnable()];
    [desc setDepthCompareFunction:mcompare];
    [desc setLabel:[NSString stringWithFormat:@"DSS %s%s", to_string(mcompare),
                                              sel.UpdateEnable() ? "+Write" : ""]];
    dss[i] = MRCTransfer([Metal::g_device newDepthStencilStateWithDescriptor:desc]);
  }
}

// MARK: Samplers

// clang-format off

static MTLSamplerMinMagFilter ConvertMinMag(FilterMode filter)
{
  switch (filter)
  {
  case FilterMode::Linear: return MTLSamplerMinMagFilterLinear;
  case FilterMode::Near:   return MTLSamplerMinMagFilterNearest;
  }
}

static MTLSamplerMipFilter ConvertMip(FilterMode filter)
{
  switch (filter)
  {
  case FilterMode::Linear: return MTLSamplerMipFilterLinear;
  case FilterMode::Near:   return MTLSamplerMipFilterNearest;
  }
}

static MTLSamplerAddressMode Convert(WrapMode wrap)
{
  switch (wrap)
  {
  case WrapMode::Clamp:  return MTLSamplerAddressModeClampToEdge;
  case WrapMode::Mirror: return MTLSamplerAddressModeMirrorRepeat;
  case WrapMode::Repeat: return MTLSamplerAddressModeRepeat;
  }
}

static const char* to_string(FilterMode filter)
{
  switch (filter)
  {
  case FilterMode::Linear: return "Ln";
  case FilterMode::Near:   return "Pt";
  }
}
static const char* to_string(WrapMode wrap)
{
  switch (wrap)
  {
  case WrapMode::Clamp:  return "C";
  case WrapMode::Mirror: return "M";
  case WrapMode::Repeat: return "R";
  }
}

// clang-format on

MRCOwned<id<MTLSamplerState>> Metal::ObjectCache::CreateSampler(SamplerSelector sel)
{
  @autoreleasepool
  {
    auto desc = MRCTransfer([MTLSamplerDescriptor new]);
    [desc setMinFilter:ConvertMinMag(sel.MinFilter())];
    [desc setMagFilter:ConvertMinMag(sel.MagFilter())];
    [desc setMipFilter:ConvertMip(sel.MipFilter())];
    [desc setSAddressMode:Convert(sel.WrapU())];
    [desc setTAddressMode:Convert(sel.WrapV())];
    [desc setMaxAnisotropy:1 << (sel.AnisotropicFiltering() ? g_ActiveConfig.iMaxAnisotropy : 0)];
    [desc setLabel:MRCTransfer([[NSString alloc]
                       initWithFormat:@"%s%s%s %s%s%s", to_string(sel.MinFilter()),
                                      to_string(sel.MagFilter()), to_string(sel.MipFilter()),
                                      to_string(sel.WrapU()), to_string(sel.WrapV()),
                                      sel.AnisotropicFiltering() ? "(AF)" : ""])];
    return MRCTransfer([Metal::g_device newSamplerStateWithDescriptor:desc]);
  }
}

void Metal::ObjectCache::ReloadSamplers()
{
  for (auto& sampler : m_samplers)
    sampler = nullptr;
}

// MARK: Pipelines

static MTLPrimitiveTopologyClass GetClass(PrimitiveType prim)
{
  switch (prim)
  {
  case PrimitiveType::Points:
    return MTLPrimitiveTopologyClassPoint;
  case PrimitiveType::Lines:
    return MTLPrimitiveTopologyClassLine;
  case PrimitiveType::Triangles:
  case PrimitiveType::TriangleStrip:
    return MTLPrimitiveTopologyClassTriangle;
  }
}

static MTLPrimitiveType Convert(PrimitiveType prim)
{
  // clang-format off
  switch (prim)
  {
  case PrimitiveType::Points:        return MTLPrimitiveTypePoint;
  case PrimitiveType::Lines:         return MTLPrimitiveTypeLine;
  case PrimitiveType::Triangles:     return MTLPrimitiveTypeTriangle;
  case PrimitiveType::TriangleStrip: return MTLPrimitiveTypeTriangleStrip;
  }
  // clang-format on
}

static MTLCullMode Convert(CullMode cull)
{
  switch (cull)
  {
  case CullMode::None:
  case CullMode::All:  // Handled by VertexLoaderManager::RunVertices
    return MTLCullModeNone;
  case CullMode::Front:
    return MTLCullModeFront;
  case CullMode::Back:
    return MTLCullModeBack;
  }
}

static MTLBlendFactor Convert(DstBlendFactor factor, bool usedualsrc)
{
  // clang-format off
  switch (factor)
  {
  case DstBlendFactor::Zero:        return MTLBlendFactorZero;
  case DstBlendFactor::One:         return MTLBlendFactorOne;
  case DstBlendFactor::SrcClr:      return MTLBlendFactorSourceColor;
  case DstBlendFactor::InvSrcClr:   return MTLBlendFactorOneMinusSourceColor;
  case DstBlendFactor::SrcAlpha:    return usedualsrc ? MTLBlendFactorSource1Alpha
                                                      : MTLBlendFactorSourceAlpha;
  case DstBlendFactor::InvSrcAlpha: return usedualsrc ? MTLBlendFactorOneMinusSource1Alpha
                                                      : MTLBlendFactorOneMinusSourceAlpha;
  case DstBlendFactor::DstAlpha:    return MTLBlendFactorDestinationAlpha;
  case DstBlendFactor::InvDstAlpha: return MTLBlendFactorOneMinusDestinationAlpha;
  }
  // clang-format on
}

static MTLBlendFactor Convert(SrcBlendFactor factor, bool usedualsrc)
{
  // clang-format off
  switch (factor)
  {
  case SrcBlendFactor::Zero:        return MTLBlendFactorZero;
  case SrcBlendFactor::One:         return MTLBlendFactorOne;
  case SrcBlendFactor::DstClr:      return MTLBlendFactorDestinationColor;
  case SrcBlendFactor::InvDstClr:   return MTLBlendFactorOneMinusDestinationColor;
  case SrcBlendFactor::SrcAlpha:    return usedualsrc ? MTLBlendFactorSource1Alpha
                                                      : MTLBlendFactorSourceAlpha;
  case SrcBlendFactor::InvSrcAlpha: return usedualsrc ? MTLBlendFactorOneMinusSource1Alpha
                                                      : MTLBlendFactorOneMinusSourceAlpha;
  case SrcBlendFactor::DstAlpha:    return MTLBlendFactorDestinationAlpha;
  case SrcBlendFactor::InvDstAlpha: return MTLBlendFactorOneMinusDestinationAlpha;
  }
  // clang-format on
}

class Metal::ObjectCache::Internal
{
public:
  using StoredPipeline = std::pair<MRCOwned<id<MTLRenderPipelineState>>, PipelineReflection>;
  /// Holds only the things that are actually used in a Metal pipeline
  struct PipelineID
  {
    struct VertexAttribute
    {
      // Just hold the things that might differ while using the same shader
      // (Really only a thing for ubershaders)
      u8 offset : 6;
      u8 components : 2;
      VertexAttribute() = default;
      explicit VertexAttribute(AttributeFormat format)
          : offset(format.offset), components(format.components - 1)
      {
        if (!format.enable)
          offset = 0x3F;  // Set it to something unlikely
      }
    };
    template <size_t N>
    static void CopyAll(std::array<VertexAttribute, N>& output,
                        const std::array<AttributeFormat, N>& input)
    {
      for (size_t i = 0; i < N; ++i)
        output[i] = VertexAttribute(input[i]);
    }
    PipelineID(const AbstractPipelineConfig& cfg)
    {
      memset(this, 0, sizeof(*this));
      if (const NativeVertexFormat* v = cfg.vertex_format)
      {
        const PortableVertexDeclaration& decl = v->GetVertexDeclaration();
        v_stride = v->GetVertexStride();
        v_position = VertexAttribute(decl.position);
        CopyAll(v_normals, decl.normals);
        CopyAll(v_colors, decl.colors);
        CopyAll(v_texcoords, decl.texcoords);
        v_posmtx = VertexAttribute(decl.posmtx);
      }
      vertex_shader = static_cast<const Shader*>(cfg.vertex_shader);
      fragment_shader = static_cast<const Shader*>(cfg.pixel_shader);
      framebuffer.color_texture_format = cfg.framebuffer_state.color_texture_format.Value();
      framebuffer.depth_texture_format = cfg.framebuffer_state.depth_texture_format.Value();
      framebuffer.samples = cfg.framebuffer_state.samples.Value();
      framebuffer.additional_color_attachment_count =
          cfg.framebuffer_state.additional_color_attachment_count.Value();
      blend.colorupdate = cfg.blending_state.colorupdate.Value();
      blend.alphaupdate = cfg.blending_state.alphaupdate.Value();
      if (cfg.blending_state.blendenable)
      {
        // clang-format off
        blend.blendenable = true;
        blend.usedualsrc     = cfg.blending_state.usedualsrc.Value();
        blend.srcfactor      = cfg.blending_state.srcfactor.Value();
        blend.dstfactor      = cfg.blending_state.dstfactor.Value();
        blend.srcfactoralpha = cfg.blending_state.srcfactoralpha.Value();
        blend.dstfactoralpha = cfg.blending_state.dstfactoralpha.Value();
        blend.subtract       = cfg.blending_state.subtract.Value();
        blend.subtractAlpha  = cfg.blending_state.subtractAlpha.Value();
        // clang-format on
      }

      if (cfg.usage != AbstractPipelineUsage::GXUber)
      {
        if (cfg.rasterization_state.primitive == PrimitiveType::Points)
          is_points = true;
        else if (cfg.rasterization_state.primitive == PrimitiveType::Lines)
          is_lines = true;
      }
    }
    PipelineID() { memset(this, 0, sizeof(*this)); }
    PipelineID(const PipelineID& other) { memcpy(this, &other, sizeof(*this)); }
    PipelineID& operator=(const PipelineID& other)
    {
      memcpy(this, &other, sizeof(*this));
      return *this;
    }
    bool operator<(const PipelineID& other) const
    {
      return memcmp(this, &other, sizeof(*this)) < 0;
    }
    bool operator==(const PipelineID& other) const
    {
      return memcmp(this, &other, sizeof(*this)) == 0;
    }

    u8 v_stride;
    VertexAttribute v_position;
    std::array<VertexAttribute, 3> v_normals;
    std::array<VertexAttribute, 2> v_colors;
    std::array<VertexAttribute, 8> v_texcoords;
    VertexAttribute v_posmtx;
    const Shader* vertex_shader;
    const Shader* fragment_shader;
    union
    {
      BlendingState blend;
      // Throw extras in bits we don't otherwise use
      BitField<30, 1, bool, u32> is_points;
      BitField<31, 1, bool, u32> is_lines;
    };
    FramebufferState framebuffer;
  };

  std::mutex m_mtx;
  std::condition_variable m_cv;
  std::map<PipelineID, StoredPipeline> m_pipelines;
  std::map<const Shader*, std::vector<PipelineID>> m_shaders;
  std::array<u32, 3> m_pipeline_counter;

  StoredPipeline CreatePipeline(const AbstractPipelineConfig& config)
  {
    @autoreleasepool
    {
      ASSERT(!config.geometry_shader);
      auto desc = MRCTransfer([MTLRenderPipelineDescriptor new]);
      [desc setVertexFunction:static_cast<const Shader*>(config.vertex_shader)->GetShader()];
      [desc setFragmentFunction:static_cast<const Shader*>(config.pixel_shader)->GetShader()];
      if (config.usage == AbstractPipelineUsage::GXUber)
        [desc setLabel:[NSString stringWithFormat:@"GX Uber Pipeline %d", m_pipeline_counter[0]++]];
      else if (config.usage == AbstractPipelineUsage::GX)
        [desc setLabel:[NSString stringWithFormat:@"GX Pipeline %d", m_pipeline_counter[1]++]];
      else
        [desc setLabel:[NSString stringWithFormat:@"Utility Pipeline %d", m_pipeline_counter[2]++]];
      if (config.vertex_format)
        [desc setVertexDescriptor:static_cast<const VertexFormat*>(config.vertex_format)->Get()];
      RasterizationState rs = config.rasterization_state;
      if (config.usage != AbstractPipelineUsage::GXUber)
        [desc setInputPrimitiveTopology:GetClass(rs.primitive)];
      MTLRenderPipelineColorAttachmentDescriptor* color0 =
          [[desc colorAttachments] objectAtIndexedSubscript:0];
      BlendingState bs = config.blending_state;
      MTLColorWriteMask mask = MTLColorWriteMaskNone;
      if (bs.colorupdate)
        mask |= MTLColorWriteMaskRed | MTLColorWriteMaskGreen | MTLColorWriteMaskBlue;
      if (bs.alphaupdate)
        mask |= MTLColorWriteMaskAlpha;
      [color0 setWriteMask:mask];
      if (bs.blendenable)
      {
        // clang-format off
        [color0 setBlendingEnabled:YES];
        [color0 setSourceRGBBlendFactor:       Convert(bs.srcfactor,      bs.usedualsrc)];
        [color0 setSourceAlphaBlendFactor:     Convert(bs.srcfactoralpha, bs.usedualsrc)];
        [color0 setDestinationRGBBlendFactor:  Convert(bs.dstfactor,      bs.usedualsrc)];
        [color0 setDestinationAlphaBlendFactor:Convert(bs.dstfactoralpha, bs.usedualsrc)];
        [color0 setRgbBlendOperation:  bs.subtract      ? MTLBlendOperationReverseSubtract : MTLBlendOperationAdd];
        [color0 setAlphaBlendOperation:bs.subtractAlpha ? MTLBlendOperationReverseSubtract : MTLBlendOperationAdd];
        // clang-format on
      }
      FramebufferState fs = config.framebuffer_state;
      if (fs.color_texture_format == AbstractTextureFormat::Undefined &&
          fs.depth_texture_format == AbstractTextureFormat::Undefined)
      {
        // Intel HD 4000's Metal driver asserts if you try to make one of these
        PanicAlertFmt("Attempted to create pipeline with no render targets!");
      }
      [desc setRasterSampleCount:fs.samples];
      [color0 setPixelFormat:Util::FromAbstract(fs.color_texture_format)];
      if (u32 cnt = fs.additional_color_attachment_count)
      {
        for (u32 i = 0; i < cnt; i++)
          [[desc colorAttachments] setObject:color0 atIndexedSubscript:i + 1];
      }
      [desc setDepthAttachmentPixelFormat:Util::FromAbstract(fs.depth_texture_format)];
      if (Util::HasStencil(fs.depth_texture_format))
        [desc setStencilAttachmentPixelFormat:Util::FromAbstract(fs.depth_texture_format)];
      NSError* err = nullptr;
      MTLRenderPipelineReflection* reflection = nullptr;
      id<MTLRenderPipelineState> pipe =
          [g_device newRenderPipelineStateWithDescriptor:desc
                                                 options:MTLPipelineOptionArgumentInfo
                                              reflection:&reflection
                                                   error:&err];
      if (err)
      {
        PanicAlertFmt("Failed to compile pipeline for {} and {}: {}",
                      [[[desc vertexFunction] label] UTF8String],
                      [[[desc fragmentFunction] label] UTF8String],
                      [[err localizedDescription] UTF8String]);
        return std::make_pair(nullptr, PipelineReflection());
      }

      return std::make_pair(MRCTransfer(pipe), PipelineReflection(reflection));
    }
  }

  StoredPipeline GetOrCreatePipeline(const AbstractPipelineConfig& config)
  {
    std::unique_lock<std::mutex> lock(m_mtx);
    PipelineID pid(config);
    auto it = m_pipelines.find(pid);
    if (it != m_pipelines.end())
    {
      while (!it->second.first && !it->second.second.textures)
        m_cv.wait(lock);  // Wait for whoever's already compiling this
      return it->second;
    }
    // Reserve the spot now, so other threads know we're making it
    it = m_pipelines.insert({pid, {nullptr, PipelineReflection()}}).first;
    lock.unlock();
    StoredPipeline pipe = CreatePipeline(config);
    lock.lock();
    if (pipe.first)
      it->second = pipe;
    else
      it->second.second.textures = 1;  // Abuse this as a "failed to create pipeline" flag
    m_shaders[pid.vertex_shader].push_back(pid);
    m_shaders[pid.fragment_shader].push_back(pid);
    lock.unlock();
    m_cv.notify_all();  // Wake up anyone who might be waiting
    return pipe;
  }

  void ShaderDestroyed(const Shader* shader)
  {
    std::lock_guard<std::mutex> lock(m_mtx);
    auto it = m_shaders.find(shader);
    if (it == m_shaders.end())
      return;
    // It's unlikely, but if a shader is destroyed, a new one could be made with the same address
    // (Also, we know it won't be used anymore, so there's no reason to keep these around)
    for (const PipelineID& pid : it->second)
      m_pipelines.erase(pid);
    m_shaders.erase(it);
  }
};

std::unique_ptr<AbstractPipeline>
Metal::ObjectCache::CreatePipeline(const AbstractPipelineConfig& config)
{
  Internal::StoredPipeline pipeline = m_internal->GetOrCreatePipeline(config);
  if (!pipeline.first)
    return nullptr;
  return std::make_unique<Pipeline>(config, std::move(pipeline.first), pipeline.second,
                                    Convert(config.rasterization_state.primitive),
                                    Convert(config.rasterization_state.cullmode),
                                    config.depth_state, config.usage);
}

void Metal::ObjectCache::ShaderDestroyed(const Shader* shader)
{
  m_internal->ShaderDestroyed(shader);
}
