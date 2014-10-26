// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/IniFile.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"

#include "VideoBackends/OGL/GLInterfaceBase.h"
#include "VideoBackends/OGL/GLUtil.h"
#include "VideoBackends/OGL/Render.h"
#include "VideoBackends/OGL/VideoBackend.h"

#include "VideoCommon/VideoConfig.h"

cInterfaceBase *GLInterface;

namespace OGL
{

// Draw messages on top of the screen
unsigned int VideoBackend::PeekMessages()
{
	return GLInterface->PeekMessages();
}

}
void InitInterface()
{
	GLInterface = HostGL_CreateGLInterface();
}

GLuint OpenGL_CompileProgram(const char* vertexShader, const char* fragmentShader)
{
	// generate objects
	GLuint vertexShaderID = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);
	GLuint programID = glCreateProgram();

	// compile vertex shader
	glShaderSource(vertexShaderID, 1, &vertexShader, nullptr);
	glCompileShader(vertexShaderID);
#if defined(_DEBUG) || defined(DEBUGFAST) || defined(DEBUG_GLSL)
	GLint Result = GL_FALSE;
	char stringBuffer[1024];
	GLsizei stringBufferUsage = 0;
	glGetShaderiv(vertexShaderID, GL_COMPILE_STATUS, &Result);
	glGetShaderInfoLog(vertexShaderID, 1024, &stringBufferUsage, stringBuffer);

	if (Result && stringBufferUsage)
	{
		ERROR_LOG(VIDEO, "GLSL vertex shader warnings:\n%s%s", stringBuffer, vertexShader);
	}
	else if (!Result)
	{
		ERROR_LOG(VIDEO, "GLSL vertex shader error:\n%s%s", stringBuffer, vertexShader);
	}
	else
	{
		DEBUG_LOG(VIDEO, "GLSL vertex shader compiled:\n%s", vertexShader);
	}

	bool shader_errors = !Result;
#endif

	// compile fragment shader
	glShaderSource(fragmentShaderID, 1, &fragmentShader, nullptr);
	glCompileShader(fragmentShaderID);
#if defined(_DEBUG) || defined(DEBUGFAST) || defined(DEBUG_GLSL)
	glGetShaderiv(fragmentShaderID, GL_COMPILE_STATUS, &Result);
	glGetShaderInfoLog(fragmentShaderID, 1024, &stringBufferUsage, stringBuffer);

	if (Result && stringBufferUsage)
	{
		ERROR_LOG(VIDEO, "GLSL fragment shader warnings:\n%s%s", stringBuffer, fragmentShader);
	}
	else if (!Result)
	{
		ERROR_LOG(VIDEO, "GLSL fragment shader error:\n%s%s", stringBuffer, fragmentShader);
	}
	else
	{
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

	if (Result && stringBufferUsage)
	{
		ERROR_LOG(VIDEO, "GLSL linker warnings:\n%s%s%s", stringBuffer, vertexShader, fragmentShader);
	}
	else if (!Result && !shader_errors)
	{
		ERROR_LOG(VIDEO, "GLSL linker error:\n%s%s%s", stringBuffer, vertexShader, fragmentShader);
	}
#endif

	// cleanup
	glDeleteShader(vertexShaderID);
	glDeleteShader(fragmentShaderID);

	return programID;
}

