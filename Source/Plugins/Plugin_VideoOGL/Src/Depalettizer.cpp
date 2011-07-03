// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "Depalettizer.h"

#include "FramebufferManager.h"
#include "Render.h"
#include "VertexShaderManager.h"

namespace OGL
{

static const char DEPALETTIZE_FS[] =
"//dolphin-emu GLSL depalettizer shader\n"

"#ifndef NUM_COLORS\n"
"#error NUM_COLORS was not defined\n"
"#endif\n"

// Uniforms
"uniform sampler2D u_Base;\n"
"uniform sampler1D u_Palette;\n"

// Shader entry point
"void main()\n"
"{\n"
	"float sample = texture2D(u_Base, gl_TexCoord[0].xy).r;\n"
	"float index = floor(sample * float(NUM_COLORS-1));\n"
	"gl_FragColor = texture1D(u_Palette, (index + 0.5) / float(NUM_COLORS));\n"
"}\n"
;

Depalettizer::Depalettizer()
	: m_fbo(0)
{ }

Depalettizer::~Depalettizer()
{
	glDeleteFramebuffersEXT(1, &m_fbo);
	m_fbo = 0;
}

Depalettizer::DepalProgram::~DepalProgram()
{
	glDeleteProgram(program);
	program = 0;
}

void Depalettizer::Depalettize(BaseType baseType, GLuint dstTex, GLuint baseTex,
	u32 width, u32 height, GLuint paletteTex)
{
	DepalProgram* program = GetProgram(baseType);
	if (!program || !program->program)
		return;

	if (!m_fbo)
		glGenFramebuffersEXT(1, &m_fbo);

	g_renderer->ResetAPIState();

	// Bind destination texture to the framebuffer
	FramebufferManager::SetFramebuffer(m_fbo);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, dstTex, 0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
	GL_REPORT_FBO_ERROR();

	// Bind fragment program and uniforms
	glUseProgram(program->program);
	glUniform1i(program->uBaseLoc, 0); // Set u_Base to sampler 0
	glUniform1i(program->uPaletteLoc, 1); // Set u_Palette to sampler 1

	// Bind base texture to sampler 0
	glActiveTexture(GL_TEXTURE0);
	glDisable(GL_TEXTURE_RECTANGLE_ARB);
	glDisable(GL_TEXTURE_1D);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, baseTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	// Bind palette texture to sampler 1
	glActiveTexture(GL_TEXTURE1);
	glDisable(GL_TEXTURE_RECTANGLE_ARB);
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_TEXTURE_1D);
	glBindTexture(GL_TEXTURE_1D, paletteTex);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glViewport(0, 0, width, height);

	// Depalettize!
	OpenGL_DrawEncoderQuad();

	// Clean up

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_1D, 0);
	glDisable(GL_TEXTURE_1D);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_TEXTURE_2D);

	glUseProgram(0);
	
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, 0, 0);

	g_renderer->RestoreAPIState();

	FramebufferManager::SetFramebuffer(0);
	VertexShaderManager::SetViewportChanged();
}

Depalettizer::DepalProgram* Depalettizer::GetProgram(BaseType type)
{
	switch (type)
	{
	case Unorm4:
		if (!m_unorm4Program.program)
		{
			m_unorm4Program.Compile(
				"#version 110\n"
				"#define NUM_COLORS 16\n");
		}
		return &m_unorm4Program;
	case Unorm8:
		if (!m_unorm8Program.program)
		{
			m_unorm8Program.Compile(
				"#version 110\n"
				"#define NUM_COLORS 256\n");
		}
		return &m_unorm8Program;
	default:
		_assert_msg_(VIDEO, 0, "Invalid depalettizer base type");
		return NULL;
	}
}

bool Depalettizer::DepalProgram::Compile(const GLchar* numColorsLine)
{
	const GLchar* fs[] = {
		numColorsLine,
		DEPALETTIZE_FS
	};

	GLuint shader = OpenGL_CreateShader(GL_FRAGMENT_SHADER, 2, fs);
	if (!shader)
	{
		ERROR_LOG(VIDEO, "Failed to compile depalettizer");
		return false;
	}

	program = glCreateProgram();
	glAttachShader(program, shader);
	if (!OpenGL_LinkProgram(program))
	{
		glDeleteProgram(program);
		glDeleteShader(shader);
		program = 0;
		ERROR_LOG(VIDEO, "Failed to link depalettizer");
		return false;
	}

	// Shader is now embedded in the program
	glDeleteShader(shader);

	// Find uniform locations
	uBaseLoc = glGetUniformLocation(program, "u_Base");
	uPaletteLoc = glGetUniformLocation(program, "u_Palette");

	(void)GL_REPORT_ERROR();

	return true;
}

}
