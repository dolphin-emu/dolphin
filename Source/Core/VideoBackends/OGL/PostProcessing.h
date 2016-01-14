// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <string>
#include <unordered_map>

#include "Common/GL/GLUtil.h"

#include "VideoBackends/OGL/ProgramShaderCache.h"
#include "VideoBackends/OGL/StreamBuffer.h"

#include "VideoCommon/PostProcessing.h"
#include "VideoCommon/VideoCommon.h"

namespace OGL
{
// Forward declaration needed for PostProcessingShader::Draw()
class OGLPostProcessor;

// PostProcessingShader comprises of all the resources needed to render a shader, including
// temporary buffers, external images, shader programs, and configurations.
class PostProcessingShader final
{
public:
	PostProcessingShader() = default;
	~PostProcessingShader();

	GLuint GetLastPassOutputTexture() const;
	bool IsLastPassScaled() const;

	bool IsReady() const { return m_ready; }

	bool Initialize(const PostProcessingShaderConfiguration* config, int target_layers);
	bool Reconfigure(const TargetSize& new_size);

	void Draw(OGLPostProcessor* parent,
			  const TargetRectangle& dst_rect, const TargetSize& dst_size, GLuint dst_texture,
			  const TargetRectangle& src_rect, const TargetSize& src_size, GLuint src_texture,
			  GLuint src_depth_texture, int src_layer);

private:
	struct InputBinding final
	{
		PostProcessingInputType type;
		GLuint texture_unit;
		GLuint texture_id;
		GLuint sampler_id;
		TargetSize size;
		bool owned;
	};

	struct RenderPassData final
	{
		std::unique_ptr<SHADER> program;
		std::unique_ptr<SHADER> gs_program;

		std::vector<InputBinding> inputs;

		GLuint output_texture_id;
		TargetSize output_size;
		float output_scale;

		bool enabled;
	};

	bool CreatePasses();
	bool RecompileShaders();
	bool ResizeOutputTextures(const TargetSize& new_size);
	void LinkPassOutputs();

	const PostProcessingShaderConfiguration* m_config;

	TargetSize m_internal_size;
	int m_internal_layers = 0;

	GLuint m_framebuffer = 0;

	std::vector<RenderPassData> m_passes;
	size_t m_last_pass_index = 0;
	bool m_last_pass_uses_color_buffer = false;
	bool m_ready = false;
};

class OGLPostProcessor final : public PostProcessor
{
public:
	OGLPostProcessor() = default;
	~OGLPostProcessor();

	bool Initialize() override;

	void ReloadShaders() override;

	void PostProcessEFB() override;

	void BlitToFramebuffer(const TargetRectangle& dst_rect, const TargetSize& dst_size, uintptr_t dst_texture,
						   const TargetRectangle& src_rect, const TargetSize& src_size, uintptr_t src_texture,
						   int src_layer) override;

	void PostProcess(const TargetRectangle& visible_rect, const TargetSize& tex_size, int tex_layers,
					 uintptr_t texture, uintptr_t depth_texture) override;

	void MapAndUpdateUniformBuffer(const PostProcessingShaderConfiguration* config, const InputTextureSizeArray& input_sizes,
								   const TargetSize& target_size, const TargetRectangle& src_rect, const TargetSize& src_size,
								   int src_layer);

	// NOTE: Can modify the bindings of draw_framebuffer/read_framebuffer.
	// If src_layer <0, copy all layers, otherwise, copy src_layer to layer 0.
	void CopyTexture(const TargetRectangle& dst_rect, GLuint dst_texture,
					 const TargetRectangle& src_rect, GLuint src_texture,
					 int src_layer, bool is_depth_texture,
					 bool force_blit = false);

protected:
	std::unique_ptr<PostProcessingShader> CreateShader(const PostProcessingShaderConfiguration* config);
	void CreatePostProcessingShaders();
	void CreateScalingShader();
	void CreateStereoShader();

	bool ResizeCopyBuffers(const TargetSize& size, int layers);
	bool ResizeStereoBuffer(const TargetSize& size);
	bool ReconfigurePostProcessingShaders(const TargetSize& size);
	void DisablePostProcessor();

	GLuint m_draw_framebuffer = 0;
	GLuint m_read_framebuffer = 0;

	TargetSize m_copy_size;
	int m_copy_layers = 0;
	GLuint m_color_copy_texture = 0;
	GLuint m_depth_copy_texture = 0;

	TargetSize m_stereo_buffer_size;
	GLuint m_stereo_buffer_texture;

	std::unique_ptr<StreamBuffer> m_uniform_buffer;
	std::unique_ptr<PostProcessingShader> m_scaling_shader;
	std::unique_ptr<PostProcessingShader> m_stereo_shader;
	std::vector<std::unique_ptr<PostProcessingShader>> m_post_processing_shaders;
};

}  // namespace
