// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <unordered_map>

#include "VideoBackends/OGL/GLUtil.h"
#include "VideoBackends/OGL/ProgramShaderCache.h"

#include "VideoCommon/PostProcessing.h"
#include "VideoCommon/VideoCommon.h"

namespace OGL
{

class OpenGLPostProcessing : public PostProcessingShaderImplementation
{
public:
	OpenGLPostProcessing();
	~OpenGLPostProcessing();

	void BindTargetFramebuffer() override;
	void BlitToScreen() override;
	void Update(u32 width, u32 height) override;
	void ApplyShader() override;

private:
	GLuint m_fbo;
	GLuint m_texture;
	SHADER m_shader;
	GLuint m_uniform_resolution;
	GLuint m_uniform_time;
	std::string m_glsl_header;

	std::unordered_map<std::string, GLuint> m_uniform_bindings;

	void CreateHeader();
	std::string LoadShaderOptions(const std::string& code);
};

}  // namespace
