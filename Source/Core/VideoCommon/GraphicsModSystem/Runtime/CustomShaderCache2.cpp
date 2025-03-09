// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/CustomShaderCache2.h"

#include "Common/Assert.h"
#include "Common/VariantUtil.h"

#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/ConstantManager.h"
#include "VideoCommon/DriverDetails.h"
#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/VideoConfig.h"

namespace
{
/// Edits the UID based on driver bugs and other special configurations
static VideoCommon::GXPipelineUid ApplyDriverBugs(const VideoCommon::GXPipelineUid& in)
{
  VideoCommon::GXPipelineUid out;
  // TODO: static_assert(std::is_trivially_copyable_v<GXPipelineUid>);
  // GXPipelineUid is not trivially copyable because RasterizationState and BlendingState aren't
  // either, but we can pretend it is for now. This will be improved after PR #10848 is finished.
  memcpy(static_cast<void*>(&out), static_cast<const void*>(&in), sizeof(out));  // copy padding
  pixel_shader_uid_data* ps = out.ps_uid.GetUidData();
  BlendingState& blend = out.blending_state;

  if (ps->ztest == EmulatedZ::ForcedEarly && !out.depth_state.updateenable)
  {
    // No need to force early depth test if you're not writing z
    ps->ztest = EmulatedZ::Early;
  }

  // If framebuffer fetch is available, we can emulate logic ops in the fragment shader
  // and don't need the below blend approximation
  if (blend.logicopenable && !g_ActiveConfig.backend_info.bSupportsLogicOp &&
      !g_ActiveConfig.backend_info.bSupportsFramebufferFetch)
  {
    if (!blend.LogicOpApproximationIsExact())
      WARN_LOG_FMT(VIDEO,
                   "Approximating logic op with blending, this will produce incorrect rendering.");
    if (blend.LogicOpApproximationWantsShaderHelp())
    {
      ps->emulate_logic_op_with_blend = true;
      ps->logic_op_mode = static_cast<u32>(blend.logicmode.Value());
    }
    blend.ApproximateLogicOpWithBlending();
  }

  const bool benefits_from_ps_dual_source_off =
      (!g_ActiveConfig.backend_info.bSupportsDualSourceBlend &&
       g_ActiveConfig.backend_info.bSupportsFramebufferFetch) ||
      DriverDetails::HasBug(DriverDetails::BUG_BROKEN_DUAL_SOURCE_BLENDING);
  if (benefits_from_ps_dual_source_off && !blend.RequiresDualSrc())
  {
    // Only use dual-source blending when required on drivers that don't support it very well.
    ps->no_dual_src = true;
    blend.usedualsrc = false;
  }

  if (g_ActiveConfig.backend_info.bSupportsFramebufferFetch)
  {
    bool fbfetch_blend = false;
    if ((DriverDetails::HasBug(DriverDetails::BUG_BROKEN_DISCARD_WITH_EARLY_Z) ||
         !g_ActiveConfig.backend_info.bSupportsEarlyZ) &&
        ps->ztest == EmulatedZ::ForcedEarly)
    {
      ps->ztest = EmulatedZ::EarlyWithFBFetch;
      fbfetch_blend |= static_cast<bool>(out.blending_state.blendenable);
      ps->no_dual_src = true;
    }
    fbfetch_blend |= blend.logicopenable && !g_ActiveConfig.backend_info.bSupportsLogicOp;
    fbfetch_blend |= blend.usedualsrc && !g_ActiveConfig.backend_info.bSupportsDualSourceBlend;
    if (fbfetch_blend)
    {
      ps->no_dual_src = true;
      if (blend.logicopenable)
      {
        ps->logic_op_enable = true;
        ps->logic_op_mode = static_cast<u32>(blend.logicmode.Value());
        blend.logicopenable = false;
      }
      if (blend.blendenable)
      {
        ps->blend_enable = true;
        ps->blend_src_factor = blend.srcfactor;
        ps->blend_src_factor_alpha = blend.srcfactoralpha;
        ps->blend_dst_factor = blend.dstfactor;
        ps->blend_dst_factor_alpha = blend.dstfactoralpha;
        ps->blend_subtract = blend.subtract;
        ps->blend_subtract_alpha = blend.subtractAlpha;
        blend.blendenable = false;
      }
    }
  }

  // force dual src off if we can't support it
  if (!g_ActiveConfig.backend_info.bSupportsDualSourceBlend)
  {
    ps->no_dual_src = true;
    blend.usedualsrc = false;
  }

  if (ps->ztest == EmulatedZ::ForcedEarly && !g_ActiveConfig.backend_info.bSupportsEarlyZ)
  {
    // These things should be false
    ASSERT(!ps->zfreeze);
    // ZCOMPLOC HACK:
    // The only way to emulate alpha test + early-z is to force early-z in the shader.
    // As this isn't available on all drivers and as we can't emulate this feature otherwise,
    // we are only able to choose which one we want to respect more.
    // Tests seem to have proven that writing depth even when the alpha test fails is more
    // important that a reliable alpha test, so we just force the alpha test to always succeed.
    // At least this seems to be less buggy.
    ps->ztest = EmulatedZ::EarlyWithZComplocHack;
  }

  if (g_ActiveConfig.UseVSForLinePointExpand() &&
      (out.rasterization_state.primitive == PrimitiveType::Points ||
       out.rasterization_state.primitive == PrimitiveType::Lines))
  {
    // All primitives are expanded to triangles in the vertex shader
    vertex_shader_uid_data* vs = out.vs_uid.GetUidData();
    const PortableVertexDeclaration& decl = out.vertex_format->GetVertexDeclaration();
    vs->position_has_3_elems = decl.position.components >= 3;
    vs->texcoord_elem_count = 0;
    for (int i = 0; i < 8; i++)
    {
      if (decl.texcoords[i].enable)
      {
        ASSERT(decl.texcoords[i].components <= 3);
        vs->texcoord_elem_count |= decl.texcoords[i].components << (i * 2);
      }
    }
    out.vertex_format = nullptr;
    if (out.rasterization_state.primitive == PrimitiveType::Points)
      vs->vs_expand = VSExpand::Point;
    else
      vs->vs_expand = VSExpand::Line;
    PrimitiveType prim = g_ActiveConfig.backend_info.bSupportsPrimitiveRestart ?
                             PrimitiveType::TriangleStrip :
                             PrimitiveType::Triangles;
    out.rasterization_state.primitive = prim;
    out.gs_uid.GetUidData()->primitive_type = static_cast<u32>(prim);
  }

  return out;
}
}  // namespace

CustomShaderCache2::CustomShaderCache2() : m_api_type{APIType::Nothing}
{
}

CustomShaderCache2::~CustomShaderCache2()
{
  Shutdown();
}

void CustomShaderCache2::Initialize()
{
  m_api_type = g_ActiveConfig.backend_info.api_type;
  m_host_config.bits = ShaderHostConfig::GetCurrent().bits;

  m_async_shader_compiler = g_gfx->CreateAsyncShaderCompiler();
  m_async_shader_compiler->StartWorkerThreads(1);  // TODO

  m_frame_end_handler = AfterFrameEvent::Register([this](Core::System&) { RetrieveAsyncShaders(); },
                                                  "CustomShaderCache2EndOfFrame");
}

void CustomShaderCache2::Shutdown()
{
  if (m_async_shader_compiler)
    m_async_shader_compiler->StopWorkerThreads();
}

void CustomShaderCache2::RetrieveAsyncShaders()
{
  m_async_shader_compiler->RetrieveWorkItems();
}

void CustomShaderCache2::Reload()
{
  if (m_async_shader_compiler)
  {
    while (m_async_shader_compiler->HasPendingWork() || m_async_shader_compiler->HasCompletedWork())
    {
      m_async_shader_compiler->RetrieveWorkItems();
    }
  }

  m_ps_cache = {};
  m_vs_cache = {};
  m_pipeline_cache = {};
}

std::optional<const AbstractPipeline*>
CustomShaderCache2::GetPipelineAsync(const VideoCommon::GXPipelineUid& uid,
                                     CustomPipelineMaterial custom_material)
{
  if (auto holder = m_pipeline_cache.GetHolder(uid, custom_material.id))
  {
    if (holder->pending)
      return std::nullopt;
    return holder->value.get();
  }
  AsyncCreatePipeline(uid, std::move(custom_material));
  return std::nullopt;
}

void CustomShaderCache2::TakePipelineResource(const VideoCommon::CustomAssetLibrary::AssetID& id,
                                              std::list<Resource>* resources)
{
  std::list<AbstractPipeline> pipelines;
  auto extracted = m_pipeline_cache.asset_id_to_cachelist.extract(id);
  if (!extracted)
    return;
  for (auto& cache_element : extracted.mapped())
  {
    resources->push_back(std::move(cache_element.second.value));
  }
}

void CustomShaderCache2::TakeVertexShaderResource(const std::string& id,
                                                  std::list<Resource>* resources)
{
  std::list<AbstractShader> shaders;
  auto extracted = m_vs_cache.asset_id_to_cachelist.extract(id);
  if (!extracted)
    return;
  for (auto& cache_element : extracted.mapped())
  {
    resources->push_back(std::move(cache_element.second.value));
  }
}

void CustomShaderCache2::TakePixelShaderResource(const std::string& id,
                                                 std::list<Resource>* resources)
{
  std::list<AbstractShader> shaders;
  auto extracted = m_ps_cache.asset_id_to_cachelist.extract(id);
  if (!extracted)
    return;
  for (auto& cache_element : extracted.mapped())
  {
    resources->push_back(std::move(cache_element.second.value));
  }
}

void CustomShaderCache2::AsyncCreatePipeline(const VideoCommon::GXPipelineUid& uid,
                                             CustomPipelineMaterial custom_material)
{
  class PipelineWorkItem final : public VideoCommon::AsyncShaderCompiler::WorkItem
  {
  public:
    PipelineWorkItem(CustomShaderCache2* shader_cache, const VideoCommon::GXPipelineUid& uid,
                     CustomPipelineMaterial custom_material, PipelineIterator iterator)
        : m_shader_cache(shader_cache), m_uid(uid), m_iterator(iterator),
          m_custom_material(std::move(custom_material))
    {
      SetStagesReady();
    }

    void SetStagesReady()
    {
      m_stages_ready = true;

      VideoCommon::GXPipelineUid new_uid = m_uid;
      if (m_custom_material.blending_state)
        new_uid.blending_state = *m_custom_material.blending_state;

      if (m_custom_material.depth_state)
        new_uid.depth_state = *m_custom_material.depth_state;

      if (m_custom_material.cull_mode)
        new_uid.rasterization_state.cullmode = *m_custom_material.cull_mode;

      const auto actual_uid = ApplyDriverBugs(new_uid);

      PixelShaderUid ps_uid = actual_uid.ps_uid;
      ClearUnusedPixelShaderUidBits(m_shader_cache->m_api_type, m_shader_cache->m_host_config,
                                    &ps_uid);

      if (auto holder =
              m_shader_cache->m_ps_cache.GetHolder(ps_uid, m_custom_material.pixel_shader_id))
      {
        if (!holder->pending && holder->value.get())
        {
          m_config.pixel_shader = holder->value.get();
        }
        m_stages_ready &= !holder->pending;
      }
      else
      {
        m_stages_ready = false;
        m_shader_cache->QueuePixelShaderCompile(ps_uid, m_custom_material);
      }

      VertexShaderUid vs_uid = actual_uid.vs_uid;
      if (auto holder =
              m_shader_cache->m_vs_cache.GetHolder(vs_uid, m_custom_material.vertex_shader_id))
      {
        if (!holder->pending && holder->value.get())
        {
          m_config.vertex_shader = holder->value.get();
        }
        m_stages_ready &= !holder->pending;
      }
      else
      {
        m_stages_ready = false;
        m_shader_cache->QueueVertexShaderCompile(vs_uid, m_custom_material);
      }

      GeometryShaderUid gs_uid = actual_uid.gs_uid;
      if (m_shader_cache->NeedsGeometryShader(gs_uid))
      {
        if (auto holder = m_shader_cache->m_gs_cache.GetHolder(gs_uid, m_custom_material.id))
        {
          if (!holder->pending && holder->value.get())
          {
            m_config.geometry_shader = holder->value.get();
          }
          m_stages_ready &= !holder->pending;
        }
        else
        {
          m_stages_ready = false;
          m_shader_cache->QueueGeometryShaderCompile(gs_uid, m_custom_material);
        }
      }

      if (m_stages_ready)
      {
        if (m_custom_material.blending_state)
          m_config.blending_state = *m_custom_material.blending_state;
        else
          m_config.blending_state = m_uid.blending_state;

        if (m_custom_material.depth_state)
          m_config.depth_state = *m_custom_material.depth_state;
        else
          m_config.depth_state = m_uid.depth_state;

        m_config.framebuffer_state = g_framebuffer_manager->GetEFBFramebufferState();
        if (m_custom_material.additional_color_attachment_count > 0)
        {
          m_config.framebuffer_state.additional_color_attachment_count =
              m_custom_material.additional_color_attachment_count;
        }

        m_config.rasterization_state = m_uid.rasterization_state;
        if (m_custom_material.cull_mode)
          m_config.rasterization_state.cullmode = *m_custom_material.cull_mode;

        m_config.vertex_format = m_uid.vertex_format;
        m_config.usage = AbstractPipelineUsage::GX;
      }
    }

    bool Compile() override
    {
      if (m_stages_ready)
      {
        // If either our vertex shader or pixel shader failed to compile
        // don't even bother with a pipeline
        if (!m_config.vertex_shader || !m_config.pixel_shader)
          m_pipeline = nullptr;
        else
          m_pipeline = g_gfx->CreatePipeline(m_config);
      }
      return true;
    }

    void Retrieve() override
    {
      if (m_stages_ready)
      {
        m_shader_cache->NotifyPipelineFinished(m_iterator, std::move(m_pipeline));
      }
      else
      {
        // Re-queue for next frame.
        auto wi = m_shader_cache->m_async_shader_compiler->CreateWorkItem<PipelineWorkItem>(
            m_shader_cache, std::move(m_uid), std::move(m_custom_material), m_iterator);
        m_shader_cache->m_async_shader_compiler->QueueWorkItem(std::move(wi), 0);
      }
    }

  private:
    CustomShaderCache2* m_shader_cache;
    std::unique_ptr<AbstractPipeline> m_pipeline;
    VideoCommon::GXPipelineUid m_uid;
    PipelineIterator m_iterator;
    AbstractPipelineConfig m_config;
    CustomPipelineMaterial m_custom_material;
    bool m_stages_ready;
  };

  auto list_iter = m_pipeline_cache.InsertElement(uid, custom_material.id);
  auto work_item = m_async_shader_compiler->CreateWorkItem<PipelineWorkItem>(
      this, uid, std::move(custom_material), list_iter);
  m_async_shader_compiler->QueueWorkItem(std::move(work_item), 0);
}

void CustomShaderCache2::NotifyPipelineFinished(PipelineIterator iterator,
                                                std::unique_ptr<AbstractPipeline> pipeline)
{
  iterator->second.pending = false;
  iterator->second.value = std::move(pipeline);
}

bool CustomShaderCache2::NeedsGeometryShader(const GeometryShaderUid& uid) const
{
  return m_host_config.backend_geometry_shaders && !uid.GetUidData()->IsPassthrough();
}

void CustomShaderCache2::QueueGeometryShaderCompile(const GeometryShaderUid& uid,
                                                    const CustomPipelineMaterial& custom_material)
{
  class GeometryShaderWorkItem final : public VideoCommon::AsyncShaderCompiler::WorkItem
  {
  public:
    GeometryShaderWorkItem(CustomShaderCache2* shader_cache, const GeometryShaderUid& uid,
                           const CustomPipelineMaterial& custom_material,
                           GeometryShaderIterator iter)
        : m_shader_cache(shader_cache), m_uid(uid), m_custom_material(custom_material), m_iter(iter)
    {
    }

    bool Compile() override
    {
      m_shader = m_shader_cache->CompileGeometryShader(m_uid);
      return true;
    }

    void Retrieve() override
    {
      m_shader_cache->NotifyGeometryShaderFinished(m_iter, std::move(m_shader));
    }

  private:
    CustomShaderCache2* m_shader_cache;
    std::unique_ptr<AbstractShader> m_shader;
    GeometryShaderUid m_uid;
    CustomPipelineMaterial m_custom_material;
    GeometryShaderIterator m_iter;
  };

  auto list_iter = m_gs_cache.InsertElement(uid, custom_material.id);
  auto work_item = m_async_shader_compiler->CreateWorkItem<GeometryShaderWorkItem>(
      this, uid, custom_material, list_iter);
  m_async_shader_compiler->QueueWorkItem(std::move(work_item), 0);
}

std::unique_ptr<AbstractShader>
CustomShaderCache2::CompileGeometryShader(const GeometryShaderUid& uid) const
{
  const ShaderCode source_code =
      GenerateGeometryShaderCode(m_api_type, m_host_config, uid.GetUidData());
  return g_gfx->CreateShaderFromSource(ShaderStage::Geometry, source_code.GetBuffer(),
                                       fmt::format("Geometry shader: {}", *uid.GetUidData()));
}

void CustomShaderCache2::NotifyGeometryShaderFinished(GeometryShaderIterator iterator,
                                                      std::unique_ptr<AbstractShader> shader)
{
  iterator->second.pending = false;
  iterator->second.value = std::move(shader);
}

void CustomShaderCache2::QueuePixelShaderCompile(const PixelShaderUid& uid,
                                                 const CustomPipelineMaterial& custom_material)
{
  class PixelShaderWorkItem final : public VideoCommon::AsyncShaderCompiler::WorkItem
  {
  public:
    PixelShaderWorkItem(CustomShaderCache2* shader_cache, const PixelShaderUid& uid,
                        const CustomPipelineMaterial& custom_material, PixelShaderIterator iter)
        : m_shader_cache(shader_cache), m_uid(uid), m_custom_material(custom_material), m_iter(iter)
    {
    }

    bool Compile() override
    {
      ShaderCode shader_code;

      const std::size_t custom_sampler_index_offset = 8;
      // Write out the samplers and defines for each texture
      for (const auto& texture_like_resource : m_custom_material.shader.material->textures)
      {
        u32 sampler_index = 0;
        std::visit(
            overloaded{
                [&](const GraphicsModSystem::TextureResource& texture_resource) {
                  sampler_index = texture_resource.sampler_index;
                },
                [&](const GraphicsModSystem::InputRenderTargetResource& render_target_resource) {
                  sampler_index = render_target_resource.sampler_index;
                }},
            texture_like_resource);
        const auto& sampler = m_custom_material.shader.shader_data->m_pixel_samplers[sampler_index];
        std::string_view sampler_type = "";
        switch (sampler.type)
        {
        case AbstractTextureType::Texture_2D:
          sampler_type = "sampler2D";
          break;
        case AbstractTextureType::Texture_2DArray:
          sampler_type = "sampler2DArray";
          break;
        case AbstractTextureType::Texture_CubeMap:
          sampler_type = "samplerCube";
          break;
        };
        shader_code.Write("SAMPLER_BINDING({}) uniform {} samp_{};\n",
                          custom_sampler_index_offset + sampler_index, sampler_type, sampler.name);
        shader_code.Write("#define HAS_{} 1\n", sampler.name);
      }

      const auto& output_targets = m_custom_material.shader.shader_data->m_output_targets;
      if (!output_targets.empty())
      {
        for (std::size_t i = 0; i < output_targets.size(); i++)
        {
          const auto& output_target = m_custom_material.shader.shader_data->m_output_targets[i];
          const u32 texel_size = AbstractTexture::GetTexelSizeForFormat(output_target.format);

          // TODO: vary type based on format
          shader_code.Write("FRAGMENT_OUTPUT_LOCATION({}) out float{} render_target_{};\n", i + 1,
                            texel_size, output_target.name);
        }
      }
      shader_code.Write("\n\n");

      // Now write the custom shader
      shader_code.Write(
          "{}", ReplaceAll(m_custom_material.shader.shader_data->m_pixel_source, "\r\n", "\n"));

      // Write out the uniform data
      ShaderCode uniform_code;
      for (const auto& [name, property] : m_custom_material.shader.shader_data->m_pixel_properties)
      {
        VideoCommon::ShaderProperty2::WriteAsShaderCode(uniform_code, name, property);
      }
      if (!m_custom_material.shader.shader_data->m_pixel_properties.empty())
        uniform_code.Write("\n\n");

      // Compile the shader
      m_shader = m_shader_cache->CompilePixelShader(m_uid, shader_code.GetBuffer(),
                                                    uniform_code.GetBuffer());
      return true;
    }

    void Retrieve() override
    {
      m_shader_cache->NotifyPixelShaderFinished(m_iter, std::move(m_shader));
    }

  private:
    CustomShaderCache2* m_shader_cache;
    std::unique_ptr<AbstractShader> m_shader;
    PixelShaderUid m_uid;
    CustomPipelineMaterial m_custom_material;
    PixelShaderIterator m_iter;
  };

  auto list_iter = m_ps_cache.InsertElement(uid, custom_material.pixel_shader_id);
  auto work_item = m_async_shader_compiler->CreateWorkItem<PixelShaderWorkItem>(
      this, uid, custom_material, list_iter);
  m_async_shader_compiler->QueueWorkItem(std::move(work_item), 0);
}

std::unique_ptr<AbstractShader>
CustomShaderCache2::CompilePixelShader(const PixelShaderUid& uid, std::string_view custom_fragment,
                                       std::string_view custom_uniforms) const
{
  const ShaderCode source_code = PixelShader::WriteFullShader(
      m_api_type, m_host_config, uid.GetUidData(), custom_fragment, custom_uniforms);
  return g_gfx->CreateShaderFromSource(ShaderStage::Pixel, source_code.GetBuffer(),
                                       "Custom Pixel Shader");
}

void CustomShaderCache2::NotifyPixelShaderFinished(PixelShaderIterator iterator,
                                                   std::unique_ptr<AbstractShader> shader)
{
  iterator->second.pending = false;
  iterator->second.value = std::move(shader);
}

void CustomShaderCache2::QueueVertexShaderCompile(const VertexShaderUid& uid,
                                                  const CustomPipelineMaterial& custom_material)
{
  class VertexShaderWorkItem final : public VideoCommon::AsyncShaderCompiler::WorkItem
  {
  public:
    VertexShaderWorkItem(CustomShaderCache2* shader_cache, const VertexShaderUid& uid,
                         const CustomPipelineMaterial& custom_material, VertexShaderIterator iter)
        : m_shader_cache(shader_cache), m_uid(uid), m_custom_material(custom_material), m_iter(iter)
    {
    }

    bool Compile() override
    {
      ShaderCode uniform_output;

      for (const auto& [name, property] : m_custom_material.shader.shader_data->m_vertex_properties)
      {
        VideoCommon::ShaderProperty2::WriteAsShaderCode(uniform_output, name, property);
      }

      // Compile the shader
      m_shader = m_shader_cache->CompileVertexShader(
          m_uid, ReplaceAll(m_custom_material.shader.shader_data->m_vertex_source, "\r\n", "\n"),
          uniform_output.GetBuffer());
      return true;
    }

    void Retrieve() override
    {
      m_shader_cache->NotifyVertexShaderFinished(m_iter, std::move(m_shader));
    }

  private:
    CustomShaderCache2* m_shader_cache;
    std::unique_ptr<AbstractShader> m_shader;
    VertexShaderUid m_uid;
    CustomPipelineMaterial m_custom_material;
    VertexShaderIterator m_iter;
  };

  auto list_iter = m_vs_cache.InsertElement(uid, custom_material.vertex_shader_id);
  auto work_item = m_async_shader_compiler->CreateWorkItem<VertexShaderWorkItem>(
      this, uid, custom_material, list_iter);
  m_async_shader_compiler->QueueWorkItem(std::move(work_item), 0);
}

std::unique_ptr<AbstractShader>
CustomShaderCache2::CompileVertexShader(const VertexShaderUid& uid, std::string_view custom_vertex,
                                        std::string_view custom_uniform) const
{
  const ShaderCode source_code = VertexShader::WriteFullShader(
      m_api_type, m_host_config, uid.GetUidData(), custom_vertex, custom_uniform);
  return g_gfx->CreateShaderFromSource(ShaderStage::Vertex, source_code.GetBuffer(),
                                       "Custom Vertex Shader");
}

void CustomShaderCache2::NotifyVertexShaderFinished(VertexShaderIterator iterator,
                                                    std::unique_ptr<AbstractShader> shader)
{
  iterator->second.pending = false;
  iterator->second.value = std::move(shader);
}
