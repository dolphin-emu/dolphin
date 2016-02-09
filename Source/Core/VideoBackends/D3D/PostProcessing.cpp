// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Common.h"
#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

#include "VideoBackends/D3D/D3DShader.h"
#include "VideoBackends/D3D/D3DState.h"
#include "VideoBackends/D3D/D3DUtil.h"
#include "VideoBackends/D3D/FramebufferManager.h"
#include "VideoBackends/D3D/GeometryShaderCache.h"
#include "VideoBackends/D3D/PixelShaderCache.h"
#include "VideoBackends/D3D/PostProcessing.h"
#include "VideoBackends/D3D/Render.h"
#include "VideoBackends/D3D/VertexShaderCache.h"

#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/Statistics.h"

namespace DX11
{

static const u32 FIRST_INPUT_BINDING_SLOT = 9;

static const char* s_shader_common = R"(

struct VS_INPUT
{
	float4 position		: POSITION;
	float4 texCoord		: TEXCOORD0;
};

struct VS_OUTPUT
{
	float4 position		: SV_Position;
	float2 srcTexCoord	: TEXCOORD0;
	float2 dstTexCoord	: TEXCOORD1;
	float layer			: TEXCOORD2;
};

struct GS_OUTPUT
{
	float4 position		: SV_Position;
	float2 srcTexCoord	: TEXCOORD0;
	float2 dstTexCoord	: TEXCOORD1;
	float layer			: TEXCOORD2;
	uint slice			: SV_RenderTargetArrayIndex;
};

)";

static const char* s_vertex_shader = R"(

void main(in VS_INPUT input, out VS_OUTPUT output)
{
	output.position = input.position;
	output.srcTexCoord = input.texCoord;
	output.dstTexCoord = float2(input.position.x * 0.5f + 0.5f, 1.0f - (input.position.y * 0.5f + 0.5f));
	output.layer = input.texCoord.z;
}

)";

static const char* s_geometry_shader = R"(

[maxvertexcount(6)]
void main(triangle VS_OUTPUT input[3], inout TriangleStream<GS_OUTPUT> output)
{
	for (int layer = 0; layer < 2; layer++)
	{
		for (int i = 0; i < 3; i++)
		{
			GS_OUTPUT vert;
			vert.position = input[i].position;
			vert.srcTexCoord = input[i].srcTexCoord;
			vert.dstTexCoord = input[i].dstTexCoord;
			vert.layer = float(layer);
			vert.slice = layer;
			output.Append(vert);
		}

		output.RestartStrip();
	}
}

)";

PostProcessingShader::~PostProcessingShader()
{
	for (RenderPassData& pass : m_passes)
	{
		for (InputBinding& input : pass.inputs)
		{
			// External textures
			if (input.texture != nullptr)
				input.texture->Release();
		}

		if (pass.output_texture != nullptr)
			pass.output_texture->Release();

		pass.pixel_shader.Reset();
	}
}

D3DTexture2D* PostProcessingShader::GetLastPassOutputTexture() const
{
	return m_passes[m_last_pass_index].output_texture;
}

bool PostProcessingShader::IsLastPassScaled() const
{
	return (m_passes[m_last_pass_index].output_size != m_internal_size);
}

bool PostProcessingShader::Initialize(const PostProcessingShaderConfiguration* config, int target_layers)
{
	m_internal_layers = target_layers;
	m_config = config;
	m_ready = false;

	if (!CreatePasses())
		return false;

	// Set size to zero, that way it'll always be reconfigured on first use
	m_internal_size.Set(0, 0);
	m_ready = RecompileShaders();
	return m_ready;
}

bool PostProcessingShader::Reconfigure(const TargetSize& new_size)
{
	m_ready = false;

	bool size_changed = (m_internal_size != new_size);
	if (size_changed)
		ResizeOutputTextures(new_size);

	// Re-link on size change due to the input pointer changes
	if (m_config->IsDirty() || size_changed)
		LinkPassOutputs();

	// Recompile shaders if compile-time constants have changed
	if (m_config->IsCompileTimeConstantsDirty() && !RecompileShaders())
		return false;

	m_ready = true;
	return true;
}

bool PostProcessingShader::CreatePasses()
{
	m_passes.reserve(m_config->GetPasses().size());
	for (const auto& pass_config : m_config->GetPasses())
	{
		RenderPassData pass;
		pass.output_texture = nullptr;
		pass.output_scale = pass_config.output_scale;
		pass.enabled = true;
		pass.inputs.reserve(pass_config.inputs.size());

		for (const auto& input_config : pass_config.inputs)
		{
			InputBinding input;
			input.type = input_config.type;
			input.texture_unit = input_config.texture_unit;
			input.texture = nullptr;
			input.texture_srv = nullptr;
			input.texture_sampler = nullptr;

			if (input.type == POST_PROCESSING_INPUT_TYPE_IMAGE)
			{
				// Copy the external image across all layers
				Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
				CD3D11_TEXTURE2D_DESC texture_desc(DXGI_FORMAT_R8G8B8A8_UNORM,
					input_config.external_image_size.width, input_config.external_image_size.height, 1, 1,
					D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, 1, 0, 0);

				D3D11_SUBRESOURCE_DATA initial_data = { input_config.external_image_data.get(),
					static_cast<UINT>(input_config.external_image_size.width * 4), 0 };

				std::unique_ptr<D3D11_SUBRESOURCE_DATA[]> initial_data_layers = std::make_unique<D3D11_SUBRESOURCE_DATA[]>(m_internal_layers);
				for (int i = 0; i < m_internal_layers; i++)
					memcpy(&initial_data_layers[i], &initial_data, sizeof(initial_data));

				HRESULT hr = D3D::device->CreateTexture2D(&texture_desc, initial_data_layers.get(), &texture);
				if (FAILED(hr))
				{
					ERROR_LOG(VIDEO, "Failed to upload post-processing external image for shader %s (pass %s)", m_config->GetShaderName().c_str(), pass_config.entry_point.c_str());
					return false;
				}

				input.texture = new D3DTexture2D(texture.Get(), (D3D11_BIND_FLAG)texture_desc.BindFlags, texture_desc.Format);
				input.texture_srv = input.texture->GetSRV();
				input.size = input_config.external_image_size;
			}

			// If set to previous pass, but we are the first pass, use the color buffer instead.
			if (input.type == POST_PROCESSING_INPUT_TYPE_PREVIOUS_PASS_OUTPUT && m_passes.empty())
				input.type = POST_PROCESSING_INPUT_TYPE_COLOR_BUFFER;

			// Lookup tables for samplers
			static const D3D11_FILTER d3d_sampler_filters[] = { D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT };
			static const D3D11_TEXTURE_ADDRESS_MODE d3d_address_modes[] = { D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_BORDER };
			static const float d3d_border_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

			// Create sampler object matching the values from config
			CD3D11_SAMPLER_DESC sampler_desc(d3d_sampler_filters[input_config.filter],
											 d3d_address_modes[input_config.address_mode],
											 d3d_address_modes[input_config.address_mode],
											 D3D11_TEXTURE_ADDRESS_CLAMP,
											 0.0f, 1, D3D11_COMPARISON_ALWAYS,
											 d3d_border_color, 0.0f, 0.0f);

			HRESULT hr = D3D::device->CreateSamplerState(&sampler_desc, input.texture_sampler.ReleaseAndGetAddressOf());
			if (FAILED(hr))
			{
				ERROR_LOG(VIDEO, "Failed to create post-processing sampler for shader %s (pass %s)", m_config->GetShaderName().c_str(), pass_config.entry_point.c_str());
				return false;
			}

			pass.inputs.push_back(std::move(input));
		}

		m_passes.push_back(std::move(pass));
	}

	return true;
}

bool PostProcessingShader::RecompileShaders()
{
	for (size_t i = 0; i < m_passes.size(); i++)
	{
		RenderPassData& pass = m_passes[i];
		const PostProcessingShaderConfiguration::RenderPass& pass_config = m_config->GetPass(i);

		std::string hlsl_source = PostProcessor::GetPassFragmentShaderSource(API_D3D, m_config, &pass_config);
		ID3D11PixelShader* pixel_shader = D3D::CompileAndCreatePixelShader(hlsl_source);
		if (pixel_shader == nullptr)
		{
			ERROR_LOG(VIDEO, "Failed to compile post-processing shader %s (pass %s)", m_config->GetShaderName().c_str(), pass_config.entry_point.c_str());
			m_ready = false;
			return false;
		}

		pass.pixel_shader = pixel_shader;
		pixel_shader->Release();
	}

	return true;
}

bool PostProcessingShader::ResizeOutputTextures(const TargetSize& new_size)
{
	for (size_t pass_index = 0; pass_index < m_passes.size(); pass_index++)
	{
		RenderPassData& pass = m_passes[pass_index];
		const PostProcessingShaderConfiguration::RenderPass& pass_config = m_config->GetPass(pass_index);
		pass.output_size = PostProcessor::ScaleTargetSize(new_size, pass_config.output_scale);

		if (pass.output_texture != nullptr)
		{
			pass.output_texture->Release();
			pass.output_texture = nullptr;
		}

		Microsoft::WRL::ComPtr<ID3D11Texture2D> color_texture;
		CD3D11_TEXTURE2D_DESC color_texture_desc(DXGI_FORMAT_R8G8B8A8_UNORM, pass.output_size.width, pass.output_size.height, m_internal_layers, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);
		HRESULT hr = D3D::device->CreateTexture2D(&color_texture_desc, nullptr, &color_texture);
		if (FAILED(hr))
		{
			ERROR_LOG(VIDEO, "Failed to create post-processing intermediate buffers (dimensions: %ux%u)", pass.output_size.width, pass.output_size.height);
			return false;
		}

		pass.output_texture = new D3DTexture2D(color_texture.Get(), (D3D11_BIND_FLAG)color_texture_desc.BindFlags, color_texture_desc.Format);
	}

	m_internal_size = new_size;
	return true;
}

void PostProcessingShader::LinkPassOutputs()
{
	m_last_pass_index = 0;
	m_last_pass_uses_color_buffer = false;

	// Update dependant options (enable/disable passes)
	for (size_t pass_index = 0; pass_index < m_passes.size(); pass_index++)
	{
		const PostProcessingShaderConfiguration::RenderPass& pass_config = m_config->GetPass(pass_index);
		RenderPassData& pass = m_passes[pass_index];
		pass.enabled = true;

		// Check if the option this pass is dependant on is disabled
		if (!pass_config.dependent_option.empty())
		{
			const auto& it = m_config->GetOptions().find(pass_config.dependent_option);
			if (it != m_config->GetOptions().end())
			{
				pass.enabled = it->second.m_bool_value;
				if (!pass.enabled)
					continue;
			}
		}

		size_t previous_pass_index = m_last_pass_index;
		m_last_pass_index = pass_index;
		m_last_pass_uses_color_buffer = false;
		for (size_t input_index = 0; input_index < pass_config.inputs.size(); input_index++)
		{
			InputBinding& input_binding = pass.inputs[input_index];
			switch (input_binding.type)
			{
			case POST_PROCESSING_INPUT_TYPE_PASS_OUTPUT:
				{
					u32 pass_output_index = pass_config.inputs[input_index].pass_output_index;
					input_binding.texture_srv = m_passes[pass_output_index].output_texture->GetSRV();
					input_binding.size = m_passes[pass_output_index].output_size;
				}
				break;

			case POST_PROCESSING_INPUT_TYPE_PREVIOUS_PASS_OUTPUT:
				{
					input_binding.texture_srv = m_passes[previous_pass_index].output_texture->GetSRV();
					input_binding.size = m_passes[previous_pass_index].output_size;
				}
				break;

			case POST_PROCESSING_INPUT_TYPE_COLOR_BUFFER:
				m_last_pass_uses_color_buffer = true;
				break;

			default:
				break;
			}
		}
	}
}

void PostProcessingShader::Draw(D3DPostProcessor* parent,
								const TargetRectangle& dst_rect, const TargetSize& dst_size, D3DTexture2D* dst_texture,
								const TargetRectangle& src_rect, const TargetSize& src_size, D3DTexture2D* src_texture,
								D3DTexture2D* src_depth_texture, int src_layer)
{
	_dbg_assert_(VIDEO, m_ready && m_internal_size == src_size);

	// Determine whether we can skip the final copy by writing directly to the output texture, if the last pass is not scaled.
	bool skip_final_copy = !IsLastPassScaled() && (dst_texture != src_texture || !m_last_pass_uses_color_buffer);

	// Draw each pass.
	PostProcessor::InputTextureSizeArray input_sizes;
	TargetRectangle output_rect = {};
	TargetSize output_size;
	for (size_t pass_index = 0; pass_index < m_passes.size(); pass_index++)
	{
		const RenderPassData& pass = m_passes[pass_index];
		bool is_last_pass = (pass_index == m_last_pass_index);
		if (!pass.enabled)
			continue;

		// If this is the last pass and we can skip the final copy, write directly to output texture.
		if (is_last_pass && skip_final_copy)
		{
			// The target rect may differ from the source.
			output_rect = dst_rect;
			output_size = dst_size;
			D3D::context->OMSetRenderTargets(1, &dst_texture->GetRTV(), nullptr);
		}
		else
		{
			output_rect = PostProcessor::ScaleTargetRectangle(API_D3D, src_rect, pass.output_scale);
			output_size = pass.output_size;
			D3D::context->OMSetRenderTargets(1, &pass.output_texture->GetRTV(), nullptr);
		}

		// Bind inputs to pipeline
		for (size_t i = 0; i < pass.inputs.size(); i++)
		{
			const InputBinding& input = pass.inputs[i];
			ID3D11ShaderResourceView* input_srv;

			switch (input.type)
			{
			case POST_PROCESSING_INPUT_TYPE_COLOR_BUFFER:
				input_srv = src_texture->GetSRV();
				input_sizes[i] = src_size;
				break;

			case POST_PROCESSING_INPUT_TYPE_DEPTH_BUFFER:
				input_srv = (src_depth_texture != nullptr) ? src_depth_texture->GetSRV() : nullptr;
				input_sizes[i] = src_size;
				break;

			default:
				input_srv = input.texture_srv;
				input_sizes[i] = input.size;
				break;
			}

			D3D::stateman->SetTexture(FIRST_INPUT_BINDING_SLOT + input.texture_unit, input_srv);
			D3D::stateman->SetSampler(FIRST_INPUT_BINDING_SLOT + input.texture_unit, input.texture_sampler.Get());
		}

		// Set viewport based on target rect
		CD3D11_VIEWPORT output_viewport((float)output_rect.left, (float)output_rect.top,
			(float)output_rect.GetWidth(), (float)output_rect.GetHeight());

		D3D::context->RSSetViewports(1, &output_viewport);

		parent->MapAndUpdateUniformBuffer(m_config, input_sizes, output_rect, output_size, src_rect, src_size, src_layer);

		// Select geometry shader based on layers
		ID3D11GeometryShader* geometry_shader = nullptr;
		if (src_layer < 0 && m_internal_layers > 1)
			geometry_shader = parent->GetGeometryShader();

		// Draw pass
		D3D::drawShadedTexQuad(nullptr, src_rect.AsRECT(), src_size.width, src_size.height,
			pass.pixel_shader.Get(), parent->GetVertexShader(), VertexShaderCache::GetSimpleInputLayout(),
			geometry_shader, 1.0f, std::max(src_layer, 0));
	}

	// Unbind input textures after rendering, so that they can safely be used as outputs again.
	for (u32 i = 0; i < POST_PROCESSING_MAX_TEXTURE_INPUTS; i++)
		D3D::stateman->SetTexture(FIRST_INPUT_BINDING_SLOT + i, nullptr);
	D3D::stateman->Apply();

	// Copy the last pass output to the target if not done already
	if (!skip_final_copy)
	{
		RenderPassData& final_pass = m_passes[m_last_pass_index];
		D3DPostProcessor::CopyTexture(dst_rect, dst_texture, output_rect, final_pass.output_texture, final_pass.output_size, src_layer);
	}
}

D3DPostProcessor::~D3DPostProcessor()
{
	SAFE_RELEASE(m_color_copy_texture);
	SAFE_RELEASE(m_depth_copy_texture);
	SAFE_RELEASE(m_stereo_buffer_texture);
}

bool D3DPostProcessor::Initialize()
{
	// Create VS/GS
	if (!CreateCommonShaders())
		return false;

	// Uniform buffer
	if (!CreateUniformBuffer())
		return false;

	// Load the currently-configured shader (this may fail, and that's okay)
	ReloadShaders();
	return true;
}

void D3DPostProcessor::ReloadShaders()
{
	m_reload_flag.Clear();
	m_post_processing_shaders.clear();
	m_scaling_shader.reset();
	m_stereo_shader.reset();
	m_active = false;

	ReloadShaderConfigs();

	if (g_ActiveConfig.bPostProcessingEnable)
		CreatePostProcessingShaders();

	CreateScalingShader();

	if (m_stereo_config)
		CreateStereoShader();

	// Set initial sizes to 0,0 to force texture creation on next draw
	m_copy_size.Set(0, 0);
	m_stereo_buffer_size.Set(0, 0);
}

bool D3DPostProcessor::CreateCommonShaders()
{
	ID3D11VertexShader* vs = D3D::CompileAndCreateVertexShader(std::string(s_shader_common) + std::string(s_vertex_shader));
	ID3D11GeometryShader* gs = D3D::CompileAndCreateGeometryShader(std::string(s_shader_common) + std::string(s_geometry_shader));
	if (!vs || !gs)
	{
		SAFE_RELEASE(vs);
		SAFE_RELEASE(gs);
		return false;
	}

	m_vertex_shader = vs;
	m_geometry_shader = gs;
	vs->Release();
	gs->Release();

	return true;
}

bool D3DPostProcessor::CreateUniformBuffer()
{
	CD3D11_BUFFER_DESC buffer_desc(UNIFORM_BUFFER_SIZE, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE, 0, 0);
	HRESULT hr = D3D::device->CreateBuffer(&buffer_desc, nullptr, m_uniform_buffer.ReleaseAndGetAddressOf());
	if (FAILED(hr))
	{
		ERROR_LOG(VIDEO, "Failed to create post-processing uniform buffer (hr=%X)", hr);
		return false;
	}

	return true;
}

std::unique_ptr<PostProcessingShader> D3DPostProcessor::CreateShader(const PostProcessingShaderConfiguration* config)
{
	std::unique_ptr<PostProcessingShader> shader = std::make_unique<PostProcessingShader>();
	if (!shader->Initialize(config, FramebufferManager::GetEFBLayers()))
		shader.reset();

	return shader;
}

void D3DPostProcessor::CreatePostProcessingShaders()
{
	for (const std::string& shader_name : m_shader_names)
	{
		const auto& it = m_shader_configs.find(shader_name);
		if (it == m_shader_configs.end())
			continue;

		std::unique_ptr<PostProcessingShader> shader = CreateShader(it->second.get());
		if (!shader)
		{
			ERROR_LOG(VIDEO, "Failed to initialize postprocessing shader ('%s'). This shader will be ignored.", shader_name.c_str());
			OSD::AddMessage(StringFromFormat("Failed to initialize postprocessing shader ('%s'). This shader will be ignored.", shader_name.c_str()));
			continue;
		}

		m_post_processing_shaders.push_back(std::move(shader));
	}

	// If no shaders initialized successfully, disable post processing
	m_active = !m_post_processing_shaders.empty();
	if (m_active)
	{
		DEBUG_LOG(VIDEO, "Postprocessing is enabled with %u shaders in sequence.", (u32)m_post_processing_shaders.size());
		OSD::AddMessage(StringFromFormat("Postprocessing is enabled with %u shaders in sequence.", (u32)m_post_processing_shaders.size()));
	}
}

void D3DPostProcessor::CreateScalingShader()
{
	if (!m_scaling_config)
		return;

	m_scaling_shader = std::make_unique<PostProcessingShader>();
	if (!m_scaling_shader->Initialize(m_scaling_config.get(), FramebufferManager::GetEFBLayers()))
	{
		ERROR_LOG(VIDEO, "Failed to initialize scaling shader ('%s'). Falling back to copy shader.", m_scaling_config->GetShaderName().c_str());
		OSD::AddMessage(StringFromFormat("Failed to initialize scaling shader ('%s'). Falling back to copy shader.", m_scaling_config->GetShaderName().c_str()));
		m_scaling_shader.reset();
	}
}

void D3DPostProcessor::CreateStereoShader()
{
	if (!m_stereo_config)
		return;

	m_stereo_shader = std::make_unique<PostProcessingShader>();
	if (!m_stereo_shader->Initialize(m_stereo_config.get(), FramebufferManager::GetEFBLayers()))
	{
		ERROR_LOG(VIDEO, "Failed to initialize stereoscopy shader ('%s'). Falling back to blit.", m_scaling_config->GetShaderName().c_str());
		OSD::AddMessage(StringFromFormat("Failed to initialize stereoscopy shader ('%s'). Falling back to blit.", m_scaling_config->GetShaderName().c_str()));
		m_stereo_shader.reset();
	}
}

void D3DPostProcessor::PostProcessEFB()
{
	// Apply normal post-process process, but to the EFB buffers.
	// Uses the current viewport as the "visible" region to post-process.
	g_renderer->ResetAPIState();

	// Copied from Renderer::SetViewport
	int scissorXOff = bpmem.scissorOffset.x * 2;
	int scissorYOff = bpmem.scissorOffset.y * 2;

	float X = Renderer::EFBToScaledXf(xfmem.viewport.xOrig - xfmem.viewport.wd - scissorXOff);
	float Y = Renderer::EFBToScaledYf(xfmem.viewport.yOrig + xfmem.viewport.ht - scissorYOff);
	float Wd = Renderer::EFBToScaledXf(2.0f * xfmem.viewport.wd);
	float Ht = Renderer::EFBToScaledYf(-2.0f * xfmem.viewport.ht);
	if (Wd < 0.0f)
	{
		X += Wd;
		Wd = -Wd;
	}
	if (Ht < 0.0f)
	{
		Y += Ht;
		Ht = -Ht;
	}

	// In D3D, the viewport rectangle must fit within the render target.
	TargetRectangle target_rect;
	TargetSize target_size(g_renderer->GetTargetWidth(), g_renderer->GetTargetHeight());
	target_rect.left = static_cast<int>((X >= 0.f) ? X : 0.f);
	target_rect.top = static_cast<int>((Y >= 0.f) ? Y : 0.f);
	target_rect.right = target_rect.left + static_cast<int>((X + Wd <= g_renderer->GetTargetWidth()) ? Wd : (g_renderer->GetTargetWidth() - X));
	target_rect.bottom = target_rect.bottom + static_cast<int>((Y + Ht <= g_renderer->GetTargetHeight()) ? Ht : (g_renderer->GetTargetHeight() - Y));

	// Source and target textures, if MSAA is enabled, this needs to be resolved
	D3DTexture2D* color_texture = FramebufferManager::GetResolvedEFBColorTexture();
	D3DTexture2D* depth_texture = nullptr;
	if (m_requires_depth_buffer)
		depth_texture = FramebufferManager::GetResolvedEFBDepthTexture();

	// Invoke post process process
	PostProcess(nullptr, nullptr, nullptr,
		target_rect, target_size, reinterpret_cast<uintptr_t>(color_texture),
		target_rect, target_size, reinterpret_cast<uintptr_t>(depth_texture));

	// Copy back to EFB buffer when multisampling is enabled
	if (g_ActiveConfig.iMultisamples > 1)
		CopyTexture(target_rect, FramebufferManager::GetEFBColorTexture(), target_rect, color_texture, target_size, -1, true);

	g_renderer->RestoreAPIState();

	// Restore EFB target
	D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(),
		FramebufferManager::GetEFBDepthTexture()->GetDSV());
}

void D3DPostProcessor::BlitScreen(const TargetRectangle& dst_rect, const TargetSize& dst_size, uintptr_t dst_texture,
										 const TargetRectangle& src_rect, const TargetSize& src_size, uintptr_t src_texture,
										 int src_layer)
{
	D3DTexture2D* real_dst_texture = reinterpret_cast<D3DTexture2D*>(dst_texture);
	D3DTexture2D* real_src_texture = reinterpret_cast<D3DTexture2D*>(src_texture);
	_dbg_assert_msg_(VIDEO, src_layer >= 0, "BlitToFramebuffer should always be called with a single source layer");

	ReconfigureScalingShader(src_size);
	ReconfigureStereoShader(dst_size);

	// Bind constants early (shared across everything)
	D3D::stateman->SetPixelConstants(m_uniform_buffer.Get());

	// Use stereo shader if enabled, otherwise invoke scaling shader, if that is invalid, fall back to blit.
	if (m_stereo_shader)
		DrawStereoBuffers(dst_rect, dst_size, real_dst_texture, src_rect, src_size, real_src_texture);
	else if (m_scaling_shader)
		m_scaling_shader->Draw(this, dst_rect, dst_size, real_dst_texture, src_rect, src_size, real_src_texture, nullptr, src_layer);
	else
		CopyTexture(dst_rect, real_dst_texture, src_rect, real_src_texture, src_size, src_layer);

	// Restore constants
	D3D::stateman->SetPixelConstants(PixelShaderCache::GetConstantBuffer(), g_ActiveConfig.bEnablePixelLighting ? VertexShaderCache::GetConstantBuffer() : nullptr);
	D3D::stateman->Apply();
}

void D3DPostProcessor::PostProcess(TargetRectangle* output_rect, TargetSize* output_size, uintptr_t* output_texture,
								   const TargetRectangle& src_rect, const TargetSize& src_size, uintptr_t src_texture,
								   const TargetRectangle& src_depth_rect, const TargetSize& src_depth_size, uintptr_t src_depth_texture)
{
	D3DTexture2D* real_src_texture = reinterpret_cast<D3DTexture2D*>(src_texture);
	D3DTexture2D* real_depth_texture = reinterpret_cast<D3DTexture2D*>(src_depth_texture);
	if (!m_active)
		return;

	// Setup copy buffers first, and update compile-time constants.
	TargetSize buffer_size(src_rect.GetWidth(), src_rect.GetHeight());
	if (!ResizeCopyBuffers(buffer_size, FramebufferManager::GetEFBLayers()) ||
		!ReconfigurePostProcessingShaders(buffer_size))
	{
		ERROR_LOG(VIDEO, "Failed to update post-processor state. Disabling post processor.");

		// We need to fill in an output texture if we're bailing out, so set it to the source.
		if (output_texture)
		{
			*output_rect = src_rect;
			*output_size = src_size;
			*output_texture = src_texture;
		}

		DisablePostProcessor();
		return;
	}

	// Copy only the visible region to our buffers.
	TargetRectangle buffer_rect(0, 0, buffer_size.width, buffer_size.height);
	CopyTexture(buffer_rect, m_color_copy_texture, src_rect, real_src_texture, src_size, -1, false);
	if (real_depth_texture != nullptr)
	{
		// Depth buffers can only be completely CopySubresourced, so use a shader copy instead.
		CopyTexture(buffer_rect, m_depth_copy_texture, src_depth_rect, real_depth_texture, src_depth_size, -1, true);
	}

	// Uniform buffer is shared across all shaders, bind it early.
	D3D::stateman->SetPixelConstants(m_uniform_buffer.Get());

	// Loop through the shader list, applying each of them in sequence.
	D3DTexture2D* input_color_texture = m_color_copy_texture;
	for (size_t shader_index = 0; shader_index < m_post_processing_shaders.size(); shader_index++)
	{
		PostProcessingShader* shader = m_post_processing_shaders[shader_index].get();

		// To save a copy, we use the output of one shader as the input to the next.
		// This works except when the last pass is scaled, as the next shader expects a full-size input, so re-use the copy buffer for this case.
		D3DTexture2D* output_color_texture = (shader->IsLastPassScaled()) ? m_color_copy_texture : shader->GetLastPassOutputTexture();

		// Last shader in the sequence? If so, write to the output texture.
		if (shader_index == (m_post_processing_shaders.size() - 1))
		{
			// Are we returning our temporary texture, or storing back to the original buffer?
			if (output_texture)
			{
				// Use the same texture as if it was a previous pass, and return it.
				shader->Draw(this, buffer_rect, buffer_size, output_color_texture, buffer_rect, buffer_size, input_color_texture, m_depth_copy_texture, -1);
				*output_rect = buffer_rect;
				*output_size = buffer_size;
				*output_texture = reinterpret_cast<uintptr_t>(output_color_texture);
			}
			else
			{
				// Write to original texture.
				shader->Draw(this, src_rect, src_size, real_src_texture, buffer_rect, buffer_size, input_color_texture, m_depth_copy_texture, -1);
			}
		}
		else
		{
			shader->Draw(this, buffer_rect, buffer_size, output_color_texture, buffer_rect, buffer_size, input_color_texture, m_depth_copy_texture, -1);
			input_color_texture = output_color_texture;
		}
	}

	// Restore constants
	D3D::stateman->SetPixelConstants(PixelShaderCache::GetConstantBuffer(), g_ActiveConfig.bEnablePixelLighting ? VertexShaderCache::GetConstantBuffer() : nullptr);
	D3D::stateman->Apply();
}

bool D3DPostProcessor::ResizeCopyBuffers(const TargetSize& size, int layers)
{
	HRESULT hr;
	if (m_copy_size == size && m_copy_layers == layers)
		return true;

	// reset before creating, in case it fails
	SAFE_RELEASE(m_color_copy_texture);
	SAFE_RELEASE(m_depth_copy_texture);
	m_copy_size.Set(0, 0);
	m_copy_layers = 0;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> color_texture;
	CD3D11_TEXTURE2D_DESC color_desc(DXGI_FORMAT_R8G8B8A8_UNORM, size.width, size.height, layers, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, D3D11_USAGE_DEFAULT, 0, 1, 0, 0);
	if (FAILED(hr = D3D::device->CreateTexture2D(&color_desc, nullptr, &color_texture)))
	{
		ERROR_LOG(VIDEO, "CreateTexture2D failed, hr=0x%X", hr);
		return false;
	}

	Microsoft::WRL::ComPtr<ID3D11Texture2D> depth_texture;
	CD3D11_TEXTURE2D_DESC depth_desc(DXGI_FORMAT_R32_FLOAT, size.width, size.height, layers, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, D3D11_USAGE_DEFAULT, 0, 1, 0, 0);
	if (FAILED(hr = D3D::device->CreateTexture2D(&depth_desc, nullptr, &depth_texture)))
	{
		ERROR_LOG(VIDEO, "CreateTexture2D failed, hr=0x%X", hr);
		return false;
	}

	m_copy_size = size;
	m_copy_layers = layers;
	m_color_copy_texture = new D3DTexture2D(color_texture.Get(), (D3D11_BIND_FLAG)color_desc.BindFlags, color_desc.Format);
	m_depth_copy_texture = new D3DTexture2D(depth_texture.Get(), (D3D11_BIND_FLAG)depth_desc.BindFlags, depth_desc.Format);
	return true;
}

bool D3DPostProcessor::ResizeStereoBuffer(const TargetSize& size)
{
	if (m_stereo_buffer_size == size)
		return true;

	SAFE_RELEASE(m_stereo_buffer_texture);
	m_stereo_buffer_size.Set(0, 0);

	Microsoft::WRL::ComPtr<ID3D11Texture2D> stereo_buffer_texture;
	CD3D11_TEXTURE2D_DESC stereo_buffer_desc(DXGI_FORMAT_R8G8B8A8_UNORM, size.width, size.height, 2, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, D3D11_USAGE_DEFAULT, 0, 1, 0, 0);
	HRESULT hr = D3D::device->CreateTexture2D(&stereo_buffer_desc, nullptr, &stereo_buffer_texture);
	if (FAILED(hr))
	{
		ERROR_LOG(VIDEO, "CreateTexture2D failed, hr=0x%X", hr);
		return false;
	}

	m_stereo_buffer_size = size;
	m_stereo_buffer_texture = new D3DTexture2D(stereo_buffer_texture.Get(), (D3D11_BIND_FLAG)stereo_buffer_desc.BindFlags, stereo_buffer_desc.Format);
	return true;
}

bool D3DPostProcessor::ReconfigurePostProcessingShaders(const TargetSize& size)
{
	for (const auto& shader : m_post_processing_shaders)
	{
		if (!shader->IsReady() || !shader->Reconfigure(size))
			return false;
	}

	// Remove dirty flags afterwards. This is because we can have the same shader twice.
	for (auto& it : m_shader_configs)
		it.second->ClearDirty();

	return true;
}

bool D3DPostProcessor::ReconfigureScalingShader(const TargetSize& size)
{
	if (m_scaling_shader)
	{
		if (!m_scaling_shader->IsReady() ||
			!m_scaling_shader->Reconfigure(size))
		{
			m_scaling_shader.reset();
			return false;
		}

		m_scaling_config->ClearDirty();
	}

	return true;
}

bool D3DPostProcessor::ReconfigureStereoShader(const TargetSize& size)
{
	if (m_stereo_shader)
	{
		if (!m_stereo_shader->IsReady() ||
			!m_stereo_shader->Reconfigure(size) ||
			!ResizeStereoBuffer(size))
		{
			m_stereo_shader.reset();
			return false;
		}

		m_stereo_config->ClearDirty();
	}

	return true;
}

void D3DPostProcessor::DrawStereoBuffers(const TargetRectangle& dst_rect, const TargetSize& dst_size, D3DTexture2D* dst_texture,
										 const TargetRectangle& src_rect, const TargetSize& src_size, D3DTexture2D* src_texture)
{
	D3DTexture2D* stereo_buffer = (m_scaling_shader) ? m_stereo_buffer_texture : src_texture;
	TargetRectangle stereo_buffer_rect(src_rect);
	TargetSize stereo_buffer_size(src_size);

	// Apply scaling shader if enabled, otherwise just use the source buffers
	if (m_scaling_shader)
	{
		stereo_buffer_rect = TargetRectangle(0, 0, dst_size.width, dst_size.height);
		stereo_buffer_size = dst_size;
		m_scaling_shader->Draw(this, stereo_buffer_rect, stereo_buffer_size, stereo_buffer, src_rect, src_size, src_texture, nullptr, -1);
	}

	m_stereo_shader->Draw(this, dst_rect, dst_size, dst_texture, stereo_buffer_rect, stereo_buffer_size, stereo_buffer, nullptr, 0);
}

void D3DPostProcessor::DisablePostProcessor()
{
	m_post_processing_shaders.clear();
	m_active = false;
}

void D3DPostProcessor::MapAndUpdateUniformBuffer(const PostProcessingShaderConfiguration* config,
												 const InputTextureSizeArray& input_sizes,
												 const TargetRectangle& dst_rect, const TargetSize& dst_size,
												 const TargetRectangle& src_rect, const TargetSize& src_size,
												 int src_layer)
{
	// Skip writing to buffer if there were no changes
	u32 buffer_size;
	if (!UpdateUniformBuffer(API_D3D, config, input_sizes, dst_rect, dst_size, src_rect, src_size, src_layer, &buffer_size))
		return;

	D3D11_MAPPED_SUBRESOURCE mapped_cbuf;
	HRESULT hr = D3D::context->Map(m_uniform_buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_cbuf);
	CHECK(SUCCEEDED(hr), "Map post processing constant buffer failed, hr=%X", hr);
	memcpy(mapped_cbuf.pData, m_current_constants.data(), buffer_size);
	D3D::context->Unmap(m_uniform_buffer.Get(), 0);

	ADDSTAT(stats.thisFrame.bytesUniformStreamed, buffer_size);
}

void D3DPostProcessor::CopyTexture(const TargetRectangle& dst_rect, D3DTexture2D* dst_texture,
								   const TargetRectangle& src_rect, D3DTexture2D* src_texture,
								   const TargetSize& src_size, int src_layer,
								   bool force_shader_copy)
{
	// If the dimensions are the same, we can copy instead of using a shader.
	bool scaling = (dst_rect.GetWidth() != src_rect.GetWidth() || dst_rect.GetHeight() != src_rect.GetHeight());
	if (!scaling && !force_shader_copy)
	{
		CD3D11_BOX copy_box(src_rect.left, src_rect.top, 0, src_rect.right, src_rect.bottom, 1);
		if (src_layer < 0)
		{
			// Copy all layers
			for (unsigned int layer = 0; layer < FramebufferManager::GetEFBLayers(); layer++)
				D3D::context->CopySubresourceRegion(dst_texture->GetTex(), layer, dst_rect.left, dst_rect.top, 0, src_texture->GetTex(), layer, &copy_box);
		}
		else
		{
			// Copy single layer to layer 0
			D3D::context->CopySubresourceRegion(dst_texture->GetTex(), 0, dst_rect.left, dst_rect.top, 0, src_texture->GetTex(), src_layer, &copy_box);
		}
	}
	else
	{
		CD3D11_VIEWPORT target_viewport((float)dst_rect.left, (float)dst_rect.top, (float)dst_rect.GetWidth(), (float)dst_rect.GetHeight());

		D3D::context->RSSetViewports(1, &target_viewport);
		D3D::context->OMSetRenderTargets(1, &dst_texture->GetRTV(), nullptr);
		if (scaling)
			D3D::SetLinearCopySampler();
		else
			D3D::SetPointCopySampler();

		D3D::drawShadedTexQuad(src_texture->GetSRV(), src_rect.AsRECT(), src_size.width, src_size.height,
			PixelShaderCache::GetColorCopyProgram(false), VertexShaderCache::GetSimpleVertexShader(), VertexShaderCache::GetSimpleInputLayout(),
			(src_layer < 0) ? GeometryShaderCache::GetCopyGeometryShader() : nullptr, 1.0f, std::max(src_layer, 0));
	}
}

}  // namespace DX11
