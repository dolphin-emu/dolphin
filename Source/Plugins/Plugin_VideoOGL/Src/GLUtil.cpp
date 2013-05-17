// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Globals.h"
#include "VideoConfig.h"
#include "IniFile.h"
#include "Core.h"
#include "Host.h"
#include "VideoBackend.h"
#include "ConfigManager.h"

#include "Render.h"
#include "VertexShaderManager.h"

#include "GLUtil.h"

GLWindow GLWin;
cInterfaceBase *GLInterface;

namespace OGL
{

// Draw messages on top of the screen
unsigned int VideoBackend::PeekMessages()
{
	return GLInterface->PeekMessages();
}

// Show the current FPS
void VideoBackend::UpdateFPSDisplay(const char *text)
{
	char temp[100];
	snprintf(temp, sizeof temp, "%s | OpenGL | %s", scm_rev_str, text);
	return GLInterface->UpdateFPSDisplay(temp);
}

}
void InitInterface()
{
	#if defined(USE_EGL) && USE_EGL
		GLInterface = new cInterfaceEGL;
	#elif defined(__APPLE__)
		GLInterface = new cInterfaceAGL;
	#elif defined(_WIN32)
		GLInterface = new cInterfaceWGL;
	#elif defined(HAVE_X11) && HAVE_X11
		GLInterface = new cInterfaceGLX;
	#endif
}

GLuint OpenGL_CompileProgram ( const char* vertexShader, const char* fragmentShader )
{
	// generate objects
	GLuint vertexShaderID = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);
	GLuint programID = glCreateProgram();
	
	// compile vertex shader
	glShaderSource(vertexShaderID, 1, &vertexShader, NULL);
	glCompileShader(vertexShaderID);
#if defined(_DEBUG) || defined(DEBUGFAST) || defined(DEBUG_GLSL)
	GLint Result = GL_FALSE;
	char stringBuffer[1024];
	GLsizei stringBufferUsage = 0;
	glGetShaderiv(vertexShaderID, GL_COMPILE_STATUS, &Result);
	glGetShaderInfoLog(vertexShaderID, 1024, &stringBufferUsage, stringBuffer);
	if(Result && stringBufferUsage) {
		ERROR_LOG(VIDEO, "GLSL vertex shader warnings:\n%s%s", stringBuffer, vertexShader);
	} else if(!Result) {
		ERROR_LOG(VIDEO, "GLSL vertex shader error:\n%s%s", stringBuffer, vertexShader);
	} else {
		DEBUG_LOG(VIDEO, "GLSL vertex shader compiled:\n%s", vertexShader);
	}
	bool shader_errors = !Result;
#endif
	
	// compile fragment shader
	glShaderSource(fragmentShaderID, 1, &fragmentShader, NULL);
	glCompileShader(fragmentShaderID);
#if defined(_DEBUG) || defined(DEBUGFAST) || defined(DEBUG_GLSL)
	glGetShaderiv(fragmentShaderID, GL_COMPILE_STATUS, &Result);
	glGetShaderInfoLog(fragmentShaderID, 1024, &stringBufferUsage, stringBuffer);
	if(Result && stringBufferUsage) {
		ERROR_LOG(VIDEO, "GLSL fragment shader warnings:\n%s%s", stringBuffer, fragmentShader);
	} else if(!Result) {
		ERROR_LOG(VIDEO, "GLSL fragment shader error:\n%s%s", stringBuffer, fragmentShader);
	} else {
		DEBUG_LOG(VIDEO, "GLSL fragment shader compiled:\n%s", fragmentShader);
	}
	shader_errors |= !Result;
#endif
	
	// link them
	glAttachShader(programID, vertexShaderID);
	glAttachShader(programID, fragmentShaderID);
	glLinkProgram(programID);
#if defined(_DEBUG) || defined(DEBUGFAST) || defined(DEBUG_GLSL)
	glGetProgramiv(programID, GL_LINK_STATUS, &Result);
	glGetProgramInfoLog(programID, 1024, &stringBufferUsage, stringBuffer);
	if(Result && stringBufferUsage) {
		ERROR_LOG(VIDEO, "GLSL linker warnings:\n%s%s%s", stringBuffer, vertexShader, fragmentShader);
	} else if(!Result && !shader_errors) {
		ERROR_LOG(VIDEO, "GLSL linker error:\n%s%s%s", stringBuffer, vertexShader, fragmentShader);
	}
#endif
	
	// cleanup
	glDeleteShader(vertexShaderID);
	glDeleteShader(fragmentShaderID);
	
	return programID;
}


GLuint OpenGL_ReportGLError(const char *function, const char *file, int line)
{
	GLint err = glGetError();
	if (err != GL_NO_ERROR)
	{
		ERROR_LOG(VIDEO, "%s:%d: (%s) OpenGL error 0x%x\n",
				file, line, function, err);
	}
	return err;
}

void OpenGL_ReportARBProgramError()
{
#ifndef USE_GLES
	const GLubyte* pstr = glGetString(GL_PROGRAM_ERROR_STRING_ARB);
	if (pstr != NULL && pstr[0] != 0)
	{
		GLint loc = 0;
		glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &loc);
		ERROR_LOG(VIDEO, "Program error at %d: ", loc);
		ERROR_LOG(VIDEO, "%s", (char*)pstr);
		ERROR_LOG(VIDEO, "\n");
	}
#endif
}

bool OpenGL_ReportFBOError(const char *function, const char *file, int line)
{
#ifndef USE_GLES
	unsigned int fbo_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (fbo_status != GL_FRAMEBUFFER_COMPLETE)
	{
		const char *error = "unknown error";
		switch (fbo_status)
		{
			case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
				error = "INCOMPLETE_ATTACHMENT";
				break;
			case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
				error = "INCOMPLETE_MISSING_ATTACHMENT";
				break;
			case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
				error = "INCOMPLETE_DRAW_BUFFER";
				break;
			case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
				error = "INCOMPLETE_READ_BUFFER";
				break;
			case GL_FRAMEBUFFER_UNSUPPORTED:
				error = "UNSUPPORTED";
				break;
		}
		ERROR_LOG(VIDEO, "%s:%d: (%s) OpenGL FBO error - %s\n",
				file, line, function, error);
		return false;
	}
#endif
	return true;
}

