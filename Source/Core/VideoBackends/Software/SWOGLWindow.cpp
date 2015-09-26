// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/GL/GLInterfaceBase.h"
#include "Common/GL/GLUtil.h"
#include "Common/Logging/Log.h"

#include "VideoBackends/Software/SWOGLWindow.h"

std::unique_ptr<SWOGLWindow> SWOGLWindow::s_instance;

void SWOGLWindow::Init(void *window_handle)
{
	InitInterface();
	GLInterface->SetMode(GLInterfaceMode::MODE_DETECT);
	if (!GLInterface->Create(window_handle, false))
	{
		INFO_LOG(VIDEO, "GLInterface::Create failed.");
	}

	s_instance.reset(new SWOGLWindow());
}

void SWOGLWindow::Shutdown()
{
	GLInterface->Shutdown();
	delete GLInterface;
	GLInterface = nullptr;

	SWOGLWindow::s_instance.release();
}

void SWOGLWindow::Prepare()
{
	if (m_init) return;
	m_init = true;

	// Init extension support.
	if (!GLExtensions::Init())
	{
		ERROR_LOG(VIDEO, "GLExtensions::Init failed!Does your video card support OpenGL 2.0?");
		return;
	}

	static const char *fragShaderText =
		"#ifdef GL_ES\n"
		"precision highp float;\n"
		"#endif\n"
		"varying vec2 TexCoordOut;\n"
		"uniform sampler2D Texture;\n"
		"void main() {\n"
		"	gl_FragColor = texture2D(Texture, TexCoordOut);\n"
		"}\n";
	static const char *vertShaderText =
		"#ifdef GL_ES\n"
		"precision highp float;\n"
		"#endif\n"
		"attribute vec4 pos;\n"
		"attribute vec2 TexCoordIn;\n "
		"varying vec2 TexCoordOut;\n "
		"void main() {\n"
		"	gl_Position = pos;\n"
		"	TexCoordOut = TexCoordIn;\n"
		"}\n";

	m_image_program = OpenGL_CompileProgram(vertShaderText, fragShaderText);

	glUseProgram(m_image_program);

	m_uni_tex = glGetUniformLocation(m_image_program, "Texture");
	m_attr_pos = glGetAttribLocation(m_image_program, "pos");
	m_attr_tex = glGetAttribLocation(m_image_program, "TexCoordIn");

	glUniform1i(m_uni_tex, 0);

	glGenTextures(1, &m_image_texture);
}

void SWOGLWindow::PrintText(const std::string& text, int x, int y, u32 color)
{
	TextData data{text, x, y, color};
	m_text.emplace_back(data);
}

void SWOGLWindow::ShowImage(u8* data, int stride, int width, int height, float aspect)
{
	GLInterface->MakeCurrent();
	GLInterface->Update();
	Prepare();

	GLsizei glWidth = (GLsizei)GLInterface->GetBackBufferWidth();
	GLsizei glHeight = (GLsizei)GLInterface->GetBackBufferHeight();

	glViewport(0, 0, glWidth, glHeight);

	glBindTexture(GL_TEXTURE_2D, m_image_texture);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);  // 4-byte pixel alignment
	//glPixelStorei(GL_UNPACK_ROW_LENGTH, stride);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)width, (GLsizei)height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glUseProgram(m_image_program);
	static const GLfloat verts[4][2] = {
		{ -1, -1}, // Left top
		{ -1,  1}, // left bottom
		{  1,  1}, // right bottom
		{  1, -1} // right top
	};
	static const GLfloat texverts[4][2] = {
		{0, 1},
		{0, 0},
		{1, 0},
		{1, 1}
	};

	glVertexAttribPointer(m_attr_pos, 2, GL_FLOAT, GL_FALSE, 0, verts);
	glVertexAttribPointer(m_attr_tex, 2, GL_FLOAT, GL_FALSE, 0, texverts);
	glEnableVertexAttribArray(m_attr_pos);
	glEnableVertexAttribArray(m_attr_tex);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glDisableVertexAttribArray(m_attr_pos);
	glDisableVertexAttribArray(m_attr_tex);

// TODO: implement OSD
//	for (TextData& text : m_text)
//	{
//	}
	m_text.clear();

	GLInterface->Swap();
	GLInterface->ClearCurrent();
}

int SWOGLWindow::PeekMessages()
{
	return GLInterface->PeekMessages();
}

