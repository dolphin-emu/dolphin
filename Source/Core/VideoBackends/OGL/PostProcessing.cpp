// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Common.h"
#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

#include "Common/GL/GLUtil.h"

#include "VideoBackends/OGL/FramebufferManager.h"
#include "VideoBackends/OGL/PostProcessing.h"
#include "VideoBackends/OGL/ProgramShaderCache.h"
#include "VideoBackends/OGL/Render.h"
#include "VideoBackends/OGL/SamplerCache.h"
#include "VideoBackends/OGL/TextureCache.h"

#include "VideoCommon/DriverDetails.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

namespace OGL
{

static const u32 FIRST_INPUT_TEXTURE_UNIT = 9;
static const u32 UNIFORM_BUFFER_BIND_POINT = 4;

static const char* s_vertex_shader = R"(
out vec2 v_source_uv;
out vec2 v_target_uv;
flat out float v_layer;
void main(void)
{
	vec2 rawpos = vec2(gl_VertexID&1, gl_VertexID&2);
	gl_Position = vec4(rawpos*2.0-1.0, 0.0, 1.0);
	v_source_uv = rawpos * u_source_rect.zw + u_source_rect.xy;
	v_target_uv = rawpos;
	v_layer = u_src_layer;
}
)";

static const char* s_layered_vertex_shader = R"(
out vec2 i_source_uv;
out vec2 i_target_uv;
void main(void)
{
	vec2 rawpos = vec2(gl_VertexID&1, gl_VertexID&2);
	gl_Position = vec4(rawpos*2.0-1.0, 0.0, 1.0);
	i_source_uv = rawpos * u_source_rect.zw + u_source_rect.xy;
	i_target_uv = rawpos;
}
)";

static const char* s_geometry_shader = R"(

layout(triangles) in;
layout(triangle_strip, max_vertices = %d) out;

in vec2 i_source_uv[3];
in vec2 i_target_uv[3];
out vec2 v_source_uv;
out vec2 v_target_uv;
flat out float v_layer;

void main()
{
	for (int i = 0; i < %d; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			gl_Position = gl_in[j].gl_Position;
			v_source_uv = i_source_uv[j];
			v_target_uv = i_target_uv[j];
			v_layer = float(i);
			gl_Layer = i;
			EmitVertex();
		}

		EndPrimitive();
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
			if (input.texture_id != 0 && input.owned)
			{
				glDeleteTextures(1, &input.texture_id);
				input.texture_id = 0;
			}
		}

		if (pass.program != nullptr)
		{
			pass.program->Destroy();
			pass.program.reset();
		}

		if (pass.gs_program != nullptr)
		{
			pass.gs_program->Destroy();
			pass.gs_program.reset();
		}

		if (pass.output_texture_id != 0)
		{
			glDeleteTextures(1, &pass.output_texture_id);
			pass.output_texture_id = 0;
		}
	}
}

GLuint PostProcessingShader::GetLastPassOutputTexture() const
{
	return m_passes[m_last_pass_index].output_texture_id;
}

bool PostProcessingShader::IsLastPassScaled() const
{
	return m_passes[m_last_pass_index].output_size != m_internal_size;
}

bool PostProcessingShader::Initialize(const PostProcessingShaderConfiguration* config, int target_layers)
{
	m_config = config;
	m_internal_layers = target_layers;
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
	if (size_changed && !ResizeOutputTextures(new_size))
		return false;

	// Also done on size change due to the input pointer changes
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
	// In case we need to allocate texture objects
	glActiveTexture(GL_TEXTURE0 + FIRST_INPUT_TEXTURE_UNIT);

	m_passes.reserve(m_config->GetPasses().size());
	for (const auto& pass_config : m_config->GetPasses())
	{
		RenderPassData pass;
		pass.output_texture_id = 0;
		pass.output_scale = pass_config.output_scale;
		pass.enabled = true;

		pass.inputs.reserve(pass_config.inputs.size());
		for (const auto& input_config : pass_config.inputs)
		{
			// Non-external textures will be bound at run-time.
			InputBinding input;
			input.type = input_config.type;
			input.texture_unit = input_config.texture_unit;
			input.texture_id = 0;
			input.sampler_id = 0;
			input.owned = false;

			// Only external images have to be set up here
			if (input.type == POST_PROCESSING_INPUT_TYPE_IMAGE)
			{
				input.size = input_config.external_image_size;
				input.owned = true;

				// Copy the image across all layers
				glGenTextures(1, &input.texture_id);
				glActiveTexture(GL_TEXTURE0 + FIRST_INPUT_TEXTURE_UNIT);
				glBindTexture(GL_TEXTURE_2D_ARRAY, input.texture_id);
				glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, input.size.width, input.size.height, m_internal_layers, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
				glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 0);
				for (int layer = 0; layer < m_internal_layers; layer++)
					glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, layer, input.size.width, input.size.height, 1, GL_RGBA, GL_UNSIGNED_BYTE, input_config.external_image_data.get());
			}

			// If set to previous pass, but we are the first pass, use the color buffer instead.
			if (input.type == POST_PROCESSING_INPUT_TYPE_PREVIOUS_PASS_OUTPUT && m_passes.empty())
				input.type = POST_PROCESSING_INPUT_TYPE_COLOR_BUFFER;

			// Lookup tables for samplers, simple due to no mipmaps
			static const GLenum gl_sampler_filters[] = { GL_NEAREST, GL_LINEAR };
			static const GLenum gl_sampler_modes[] = { GL_CLAMP_TO_EDGE, GL_REPEAT,  GL_CLAMP_TO_BORDER };
			static const float gl_border_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

			// Create sampler object matching the values from config
			glGenSamplers(1, &input.sampler_id);
			glSamplerParameteri(input.sampler_id, GL_TEXTURE_MIN_FILTER, gl_sampler_filters[input_config.filter]);
			glSamplerParameteri(input.sampler_id, GL_TEXTURE_MAG_FILTER, gl_sampler_filters[input_config.filter]);
			glSamplerParameteri(input.sampler_id, GL_TEXTURE_WRAP_S, gl_sampler_modes[input_config.address_mode]);
			glSamplerParameteri(input.sampler_id, GL_TEXTURE_WRAP_T, gl_sampler_modes[input_config.address_mode]);
			glSamplerParameterfv(input.sampler_id, GL_TEXTURE_BORDER_COLOR, gl_border_color);

			pass.inputs.push_back(std::move(input));
		}

		m_passes.push_back(std::move(pass));
	}

	// Restore active texture unit
	TextureCache::SetStage();
	return true;
}

bool PostProcessingShader::RecompileShaders()
{
	for (size_t i = 0; i < m_passes.size(); i++)
	{
		RenderPassData& pass = m_passes[i];
		const PostProcessingShaderConfiguration::RenderPass& pass_config = m_config->GetPass(i);

		// Manually destroy old programs (no cleanup in destructor)
		if (pass.program)
		{
			pass.program->Destroy();
			pass.program.reset();
		}
		if (pass.gs_program)
		{
			pass.gs_program->Destroy();
			pass.gs_program.reset();
		}

		// Compile shader for this pass
		pass.program = std::make_unique<SHADER>();
		std::string vertex_shader_source = PostProcessor::GetUniformBufferShaderSource(API_OPENGL, m_config) + s_vertex_shader;
		std::string fragment_shader_source = PostProcessor::GetPassFragmentShaderSource(API_OPENGL, m_config, &pass_config);
		if (!ProgramShaderCache::CompileShader(*pass.program, vertex_shader_source.c_str(), fragment_shader_source.c_str()))
		{
			ERROR_LOG(VIDEO, "Failed to compile post-processing shader %s (pass %s)", m_config->GetShaderName().c_str(), pass_config.entry_point.c_str());
			m_ready = false;
			return false;
		}

		// Bind our uniform block
		GLuint block_index = glGetUniformBlockIndex(pass.program->glprogid, "PostProcessingConstants");
		if (block_index != GL_INVALID_INDEX)
			glUniformBlockBinding(pass.program->glprogid, block_index, UNIFORM_BUFFER_BIND_POINT);

		// Only generate a GS-expanding program if needed
		std::unique_ptr<SHADER> gs_program;
		if (m_internal_layers > 1)
		{
			pass.gs_program = std::make_unique<SHADER>();
			vertex_shader_source = PostProcessor::GetUniformBufferShaderSource(API_OPENGL, m_config) + s_layered_vertex_shader;
			std::string geometry_shader_source = StringFromFormat(s_geometry_shader, m_internal_layers * 3, m_internal_layers).c_str();

			if (!ProgramShaderCache::CompileShader(*pass.gs_program, vertex_shader_source.c_str(), fragment_shader_source.c_str(), geometry_shader_source.c_str()))
			{
				ERROR_LOG(VIDEO, "Failed to compile GS post-processing shader %s (pass %s)", m_config->GetShaderName().c_str(), pass_config.entry_point.c_str());
				m_ready = false;
				return false;
			}

			block_index = glGetUniformBlockIndex(pass.gs_program->glprogid, "PostProcessingConstants");
			if (block_index != GL_INVALID_INDEX)
				glUniformBlockBinding(pass.gs_program->glprogid, block_index, UNIFORM_BUFFER_BIND_POINT);
		}
	}

	return true;
}

bool PostProcessingShader::ResizeOutputTextures(const TargetSize& new_size)
{
	// Recreate buffers
	glActiveTexture(GL_TEXTURE0 + FIRST_INPUT_TEXTURE_UNIT);
	for (size_t pass_index = 0; pass_index < m_passes.size(); pass_index++)
	{
		RenderPassData& pass = m_passes[pass_index];
		const PostProcessingShaderConfiguration::RenderPass& pass_config = m_config->GetPass(pass_index);
		pass.output_size = PostProcessor::ScaleTargetSize(new_size, pass_config.output_scale);

		// Re-use existing texture object if one already exists
		if (pass.output_texture_id == 0)
			glGenTextures(1, &pass.output_texture_id);

		glBindTexture(GL_TEXTURE_2D_ARRAY, pass.output_texture_id);
		glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, pass.output_size.width, pass.output_size.height, m_internal_layers, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 0);
	}

	m_internal_size = new_size;
	TextureCache::SetStage();
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
					input_binding.texture_id = m_passes[pass_output_index].output_texture_id;
					input_binding.size = m_passes[pass_output_index].output_size;
				}
				break;

			case POST_PROCESSING_INPUT_TYPE_PREVIOUS_PASS_OUTPUT:
				{
					input_binding.texture_id = m_passes[previous_pass_index].output_texture_id;
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

void PostProcessingShader::Draw(OGLPostProcessor* parent,
								const TargetRectangle& dst_rect, const TargetSize& dst_size, GLuint dst_texture,
								const TargetRectangle& src_rect, const TargetSize& src_size, GLuint src_texture,
								GLuint src_depth_texture, int src_layer)
{
	_dbg_assert_(VIDEO, m_ready && m_internal_size == src_size);
	OpenGL_BindAttributelessVAO();

	// Determine whether we can skip the final copy by writing directly to the output texture, if the last pass is not scaled.
	bool skip_final_copy = !IsLastPassScaled() && (dst_texture != src_texture || !m_last_pass_uses_color_buffer);

	// If the last pass is not at full scale, we can't skip the copy.
	if (m_passes[m_last_pass_index].output_size != src_size)
		skip_final_copy = false;

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
		GLuint output_texture;
		if (is_last_pass && skip_final_copy)
		{
			output_rect = dst_rect;
			output_size = dst_size;
			output_texture = dst_texture;
		}
		else
		{
			output_rect = PostProcessor::ScaleTargetRectangle(API_OPENGL, src_rect, pass.output_scale);
			output_size = pass.output_size;
			output_texture = pass.output_texture_id;
		}

		// Setup framebuffer
		if (output_texture != 0)
		{
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, parent->GetDrawFramebuffer());
			if (src_layer < 0 && m_internal_layers > 1)
				glFramebufferTexture(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, output_texture, 0);
			else if (src_layer >= 0)
				glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, output_texture, 0, src_layer);
			else
				glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, output_texture, 0, 0);
		}
		else
		{
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		}

		// Bind program and texture units here
		if (src_layer < 0 && m_internal_layers > 1)
			pass.gs_program->Bind();
		else
			pass.program->Bind();

		for (size_t i = 0; i < pass.inputs.size(); i++)
		{
			const InputBinding& input = pass.inputs[i];
			glActiveTexture(GL_TEXTURE0 + FIRST_INPUT_TEXTURE_UNIT + input.texture_unit);

			switch (input.type)
			{
			case POST_PROCESSING_INPUT_TYPE_COLOR_BUFFER:
				glBindTexture(GL_TEXTURE_2D_ARRAY, src_texture);
				input_sizes[i] = src_size;
				break;

			case POST_PROCESSING_INPUT_TYPE_DEPTH_BUFFER:
				glBindTexture(GL_TEXTURE_2D_ARRAY, src_depth_texture);
				input_sizes[i] = src_size;
				break;

			default:
				glBindTexture(GL_TEXTURE_2D_ARRAY, input.texture_id);
				input_sizes[i] = input.size;
				break;
			}

			glBindSampler(FIRST_INPUT_TEXTURE_UNIT + input.texture_unit, input.sampler_id);
		}

		parent->MapAndUpdateUniformBuffer(m_config, input_sizes, output_rect, output_size, src_rect, src_size, src_layer);
		glViewport(output_rect.left, output_rect.bottom, output_rect.GetWidth(), output_rect.GetHeight());
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	}

	// Unbind input textures after rendering, so that they can safely be used as outputs again.
	for (u32 i = 0; i < POST_PROCESSING_MAX_TEXTURE_INPUTS; i++)
	{
		glActiveTexture(GL_TEXTURE0 + FIRST_INPUT_TEXTURE_UNIT + i);
		glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	}

	// Copy the last pass output to the target if not done already
	if (!skip_final_copy)
		parent->CopyTexture(dst_rect, dst_texture, output_rect, m_passes[m_last_pass_index].output_texture_id, src_layer, false);
}

OGLPostProcessor::~OGLPostProcessor()
{
	if (m_read_framebuffer != 0)
		glDeleteFramebuffers(1, &m_read_framebuffer);
	if (m_draw_framebuffer != 0)
		glDeleteFramebuffers(1, &m_draw_framebuffer);
	if (m_color_copy_texture != 0)
		glDeleteTextures(1, &m_color_copy_texture);
	if (m_depth_copy_texture != 0)
		glDeleteTextures(1, &m_depth_copy_texture);
	if (m_stereo_buffer_texture != 0)
		glDeleteTextures(1, &m_stereo_buffer_texture);

	// Need to change latched buffer before freeing uniform buffer, see MapAndUpdateUniformBuffer for why.
	glBindBuffer(GL_UNIFORM_BUFFER, m_uniform_buffer->m_buffer);
	m_uniform_buffer.reset();
	ProgramShaderCache::BindUniformBuffer();
}

bool OGLPostProcessor::Initialize()
{
	// Create our framebuffer objects, since these are needed regardless of whether we're enabled.
	glGenFramebuffers(1, &m_draw_framebuffer);
	glGenFramebuffers(1, &m_read_framebuffer);
	if (glGetError() != GL_NO_ERROR)
	{
		ERROR_LOG(VIDEO, "Failed to create postprocessing framebuffer objects.");
		return false;
	}

	m_uniform_buffer = StreamBuffer::Create(GL_UNIFORM_BUFFER, UNIFORM_BUFFER_SIZE * 16);
	if (m_uniform_buffer == nullptr)
	{
		ERROR_LOG(VIDEO, "Failed to create postprocessing uniform buffer.");
		return false;
	}

	// Allocate copy texture names, the actual storage is done in ResizeCopyBuffers
	glGenTextures(1, &m_color_copy_texture);
	glGenTextures(1, &m_depth_copy_texture);
	glGenTextures(1, &m_stereo_buffer_texture);
	if (m_color_copy_texture == 0 || m_depth_copy_texture == 0 || m_stereo_buffer_texture == 0)
	{
		ERROR_LOG(VIDEO, "Failed to create copy textures.");
		return false;
	}

	// Load the currently-configured shader (this may fail, and that's okay)
	ReloadShaders();
	return true;
}

void OGLPostProcessor::ReloadShaders()
{
	// Delete current shaders
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

std::unique_ptr<PostProcessingShader> OGLPostProcessor::CreateShader(const PostProcessingShaderConfiguration* config)
{
	std::unique_ptr<PostProcessingShader> shader = std::make_unique<PostProcessingShader>();
	if (!shader->Initialize(config, FramebufferManager::GetEFBLayers()))
		shader.reset();

	return shader;
}

void OGLPostProcessor::CreatePostProcessingShaders()
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

void OGLPostProcessor::CreateScalingShader()
{
	if (!m_scaling_config)
		return;

	m_scaling_shader = std::make_unique<PostProcessingShader>();
	if (!m_scaling_shader->Initialize(m_scaling_config.get(), FramebufferManager::GetEFBLayers()))
	{
		ERROR_LOG(VIDEO, "Failed to initialize scaling shader ('%s'). Falling back to blit.", m_scaling_config->GetShaderName().c_str());
		OSD::AddMessage(StringFromFormat("Failed to initialize scaling shader ('%s'). Falling back to blit.", m_scaling_config->GetShaderName().c_str()));
		m_scaling_shader.reset();
	}
}

void OGLPostProcessor::CreateStereoShader()
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

void OGLPostProcessor::PostProcessEFB()
{
	// Apply normal post-process process, but to the EFB buffers.
	// Uses the current viewport as the "visible" region to post-process.
	g_renderer->ResetAPIState();

	// Copied from Renderer::SetViewport
	int scissorXOff = bpmem.scissorOffset.x * 2;
	int scissorYOff = bpmem.scissorOffset.y * 2;
	float X = Renderer::EFBToScaledXf(xfmem.viewport.xOrig - xfmem.viewport.wd - (float)scissorXOff);
	float Y = Renderer::EFBToScaledYf((float)EFB_HEIGHT - xfmem.viewport.yOrig + xfmem.viewport.ht + (float)scissorYOff);
	float Width = Renderer::EFBToScaledXf(2.0f * xfmem.viewport.wd);
	float Height = Renderer::EFBToScaledYf(-2.0f * xfmem.viewport.ht);
	if (Width < 0)
	{
		X += Width;
		Width *= -1;
	}
	if (Height < 0)
	{
		Y += Height;
		Height *= -1;
	}

	EFBRectangle efb_rect(0, EFB_HEIGHT, EFB_WIDTH, 0);
	TargetSize target_size(g_renderer->GetTargetWidth(), g_renderer->GetTargetHeight());
	TargetRectangle target_rect(static_cast<int>(X), static_cast<int>(Y + Height),
		static_cast<int>(X + Width), static_cast<int>(Y));

	// Source and target textures, if MSAA is enabled, this needs to be resolved
	GLuint efb_color_texture = FramebufferManager::GetEFBColorTexture(efb_rect);
	GLuint efb_depth_texture = 0;
	if (m_requires_depth_buffer)
		efb_depth_texture = FramebufferManager::GetEFBDepthTexture(efb_rect);

	// Invoke post-process process, this will write back to efb_color_texture
	PostProcess(nullptr, nullptr, nullptr, target_rect, target_size, efb_color_texture, target_rect, target_size, efb_depth_texture);

	// Restore EFB framebuffer
	FramebufferManager::SetFramebuffer(0);

	// In msaa mode, we need to blit back to the original framebuffer.
	// An accessor for the texture name means we could use CopyTexture here.
	if (g_ActiveConfig.iMultisamples > 1)
	{
		glBindFramebuffer(GL_READ_FRAMEBUFFER, m_read_framebuffer);
		FramebufferManager::FramebufferTexture(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D_ARRAY, efb_color_texture, 0);

		glBlitFramebuffer(target_rect.left, target_rect.bottom, target_rect.right, target_rect.top,
			target_rect.left, target_rect.bottom, target_rect.right, target_rect.top,
			GL_COLOR_BUFFER_BIT, GL_NEAREST);

		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	}

	g_renderer->RestoreAPIState();
}

void OGLPostProcessor::BlitScreen(const TargetRectangle& dst_rect, const TargetSize& dst_size, uintptr_t dst_texture,
										 const TargetRectangle& src_rect, const TargetSize& src_size, uintptr_t src_texture,
										 int src_layer)
{
	GLuint real_dst_texture = static_cast<GLuint>(dst_texture);
	GLuint real_src_texture = static_cast<GLuint>(src_texture);
	_dbg_assert_msg_(VIDEO, src_layer >= 0, "BlitToFramebuffer should always be called with a single source layer");

	ReconfigureScalingShader(src_size);
	ReconfigureStereoShader(dst_size);

	// Use stereo shader if enabled, otherwise invoke scaling shader, if that is invalid, fall back to blit.
	if (m_stereo_shader)
		DrawStereoBuffers(dst_rect, dst_size, real_dst_texture, src_rect, src_size, real_src_texture);
	else if (m_scaling_shader)
		m_scaling_shader->Draw(this, dst_rect, dst_size, real_dst_texture, src_rect, src_size, real_src_texture, 0, 0);
	else
		CopyTexture(dst_rect, real_dst_texture, src_rect, real_src_texture, src_layer, false);
}

void OGLPostProcessor::PostProcess(TargetRectangle* output_rect, TargetSize* output_size, uintptr_t* output_texture,
								   const TargetRectangle& src_rect, const TargetSize& src_size, uintptr_t src_texture,
								   const TargetRectangle& src_depth_rect, const TargetSize& src_depth_size, uintptr_t src_depth_texture)
{
	GLuint real_src_texture = static_cast<GLuint>(src_texture);
	GLuint real_src_depth_texture = static_cast<GLuint>(src_depth_texture);
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

	// Copy the visible region to our buffers.
	TargetRectangle buffer_rect(0, buffer_size.height, buffer_size.width, 0);
	CopyTexture(buffer_rect, m_color_copy_texture, src_rect, real_src_texture, -1, false);
	if (real_src_depth_texture != 0)
		CopyTexture(buffer_rect, m_depth_copy_texture, src_depth_rect, real_src_depth_texture, -1, false);

	// Loop through the shader list, applying each of them in sequence.
	GLuint input_color_texture = m_color_copy_texture;
	for (size_t shader_index = 0; shader_index < m_post_processing_shaders.size(); shader_index++)
	{
		PostProcessingShader* shader = m_post_processing_shaders[shader_index].get();

		// To save a copy, we use the output of one shader as the input to the next.
		// This works except when the last pass is scaled, as the next shader expects a full-size input, so re-use the copy buffer for this case.
		GLuint output_color_texture = (shader->IsLastPassScaled()) ? m_color_copy_texture : shader->GetLastPassOutputTexture();

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
				*output_texture = static_cast<uintptr_t>(output_color_texture);
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
}

bool OGLPostProcessor::ResizeCopyBuffers(const TargetSize& size, int layers)
{
	if (m_copy_size == size && m_copy_layers == layers)
		return true;

	m_copy_size = size;
	m_copy_layers = layers;

	glActiveTexture(GL_TEXTURE0 + FIRST_INPUT_TEXTURE_UNIT);
	glBindTexture(GL_TEXTURE_2D_ARRAY, m_color_copy_texture);
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, size.width, size.height, FramebufferManager::GetEFBLayers(), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D_ARRAY, m_depth_copy_texture);
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT32F, size.width, size.height, FramebufferManager::GetEFBLayers(), 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	TextureCache::SetStage();
	return true;
}

bool OGLPostProcessor::ResizeStereoBuffer(const TargetSize& size)
{
	if (m_stereo_buffer_size == size)
		return true;

	m_stereo_buffer_size = size;

	glActiveTexture(GL_TEXTURE0 + FIRST_INPUT_TEXTURE_UNIT);
	glBindTexture(GL_TEXTURE_2D_ARRAY, m_stereo_buffer_texture);
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, size.width, size.height, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	TextureCache::SetStage();
	return true;
}

bool OGLPostProcessor::ReconfigurePostProcessingShaders(const TargetSize& size)
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

bool OGLPostProcessor::ReconfigureScalingShader(const TargetSize& size)
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

bool OGLPostProcessor::ReconfigureStereoShader(const TargetSize& size)
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

void OGLPostProcessor::DrawStereoBuffers(const TargetRectangle& dst_rect, const TargetSize& dst_size, GLuint dst_texture,
										 const TargetRectangle& src_rect, const TargetSize& src_size, GLuint src_texture)
{
	GLuint stereo_buffer = (m_scaling_shader) ? m_stereo_buffer_texture : src_texture;
	TargetRectangle stereo_buffer_rect(src_rect);
	TargetSize stereo_buffer_size(src_size);

	// Apply scaling shader if enabled, otherwise just use the source buffers
	if (m_scaling_shader)
	{
		stereo_buffer_rect = TargetRectangle(0, dst_size.height, dst_size.width, 0);
		stereo_buffer_size = dst_size;
		m_scaling_shader->Draw(this, stereo_buffer_rect, stereo_buffer_size, stereo_buffer, src_rect, src_size, src_texture, 0, -1);
	}

	m_stereo_shader->Draw(this, dst_rect, dst_size, dst_texture, stereo_buffer_rect, stereo_buffer_size, stereo_buffer, 0, 0);
}

void OGLPostProcessor::DisablePostProcessor()
{
	m_post_processing_shaders.clear();
	m_active = false;
}

void OGLPostProcessor::CopyTexture(const TargetRectangle& dst_rect, GLuint dst_texture,
								   const TargetRectangle& src_rect, GLuint src_texture,
								   int src_layer, bool is_depth_texture,
								   bool force_blit)
{
	// Can we copy the image?
	bool scaling = (dst_rect.GetWidth() != src_rect.GetWidth() || dst_rect.GetHeight() != src_rect.GetHeight());
	int layers_to_copy = (src_layer < 0) ? FramebufferManager::GetEFBLayers() : 1;

	// Copy each layer individually.
	for (int i = 0; i < layers_to_copy; i++)
	{
		int layer = (src_layer < 0) ? i : src_layer;
		if (g_ogl_config.bSupportsCopySubImage && dst_texture != 0 && !force_blit)
		{
			// use (ARB|NV)_copy_image, but only for non-window-framebuffer cases
			glCopyImageSubData(src_texture, GL_TEXTURE_2D_ARRAY, 0, src_rect.left, src_rect.bottom, layer,
				dst_texture, GL_TEXTURE_2D_ARRAY, 0, dst_rect.left, dst_rect.bottom, layer,
				src_rect.GetWidth(), src_rect.GetHeight(), 1);
		}
		else
		{
			// fallback to glBlitFramebuffer path
			GLenum filter = (scaling) ? GL_LINEAR : GL_NEAREST;
			GLbitfield bits = (!is_depth_texture) ? GL_COLOR_BUFFER_BIT : GL_DEPTH_BUFFER_BIT;
			if (dst_texture != 0)
			{
				glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_draw_framebuffer);
				if (!is_depth_texture)
					glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, dst_texture, 0, layer);
				else
					glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, dst_texture, 0, layer);
			}
			else
			{
				// window framebuffer
				glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
			}

			glBindFramebuffer(GL_READ_FRAMEBUFFER, m_read_framebuffer);
			if (!is_depth_texture)
				glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, src_texture, 0, layer);
			else
				glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, src_texture, 0, layer);

			glBlitFramebuffer(src_rect.left, src_rect.bottom, src_rect.right, src_rect.top,
				dst_rect.left, dst_rect.bottom, dst_rect.right, dst_rect.top,
				bits, filter);
		}
	}
}

void OGLPostProcessor::MapAndUpdateUniformBuffer(const PostProcessingShaderConfiguration* config,
												 const InputTextureSizeArray& input_sizes,
												 const TargetRectangle& dst_rect, const TargetSize& dst_size,
												 const TargetRectangle& src_rect, const TargetSize& src_size,
												 int src_layer)
{
	// Skip writing to buffer if there were no changes
	u32 buffer_size;
	if (!UpdateUniformBuffer(API_OPENGL, config, input_sizes, dst_rect, dst_size, src_rect, src_size, src_layer, &buffer_size))
		return;

	// Annoyingly, due to latched state, we have to bind our private uniform buffer here, then restore the
	// ProgramShaderCache uniform buffer afterwards, otherwise we'll end up flushing the wrong buffer.
	glBindBuffer(GL_UNIFORM_BUFFER, m_uniform_buffer->m_buffer);

	auto mapped_buffer = m_uniform_buffer->Map(buffer_size, ProgramShaderCache::GetUniformBufferAlignment());
	memcpy(mapped_buffer.first, m_current_constants.data(), buffer_size);
	m_uniform_buffer->Unmap(buffer_size);

	glBindBufferRange(GL_UNIFORM_BUFFER, UNIFORM_BUFFER_BIND_POINT, m_uniform_buffer->m_buffer, mapped_buffer.second, buffer_size);

	ProgramShaderCache::BindUniformBuffer();

	ADDSTAT(stats.thisFrame.bytesUniformStreamed, buffer_size);
}

}  // namespace OGL
