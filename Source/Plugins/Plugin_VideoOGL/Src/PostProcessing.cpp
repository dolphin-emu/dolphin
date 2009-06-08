// Copyright (C) 2003-2009 Dolphin Project.

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

#include "VideoCommon.h"
#include "FileUtil.h"
#include "Config.h"
#include "GLUtil.h"
#include "PostProcessing.h"
#include "PixelShaderCache.h"

namespace PostProcessing
{

static std::string s_currentShader;
static FRAGMENTSHADER s_shader;

void Init()
{
	s_currentShader = "";
}

void Shutdown()
{
	s_shader.Destroy();
}

void ApplyShader()
{
#ifdef _WIN32
	if (GetAsyncKeyState(VK_LSHIFT))
		s_currentShader = "";
#endif
	if (s_currentShader != "User/Shaders/" + g_Config.sPostProcessingShader + ".txt")
	{
		// Set immediately to prevent endless recompiles on failure.
		s_currentShader = "User/Shaders/" + g_Config.sPostProcessingShader + ".txt";

		s_shader.Destroy();

		if (!s_currentShader.empty())
		{
			std::string code;
			if (File::ReadFileToString(true, s_currentShader.c_str(), code))
			{
				if (!PixelShaderCache::CompilePixelShader(s_shader, code.c_str()))
				{
					ERROR_LOG(VIDEO, "Failed to compile post-processing shader %s", s_currentShader.c_str());
				}
			}
			else 
			{
				ERROR_LOG(VIDEO, "Failed to load post-processing shader %s - does not exist?", s_currentShader.c_str());
			}
		}
		else
		{
			ERROR_LOG(VIDEO, "No post-processing shader selected.");
		}
	}

	if (s_shader.glprogid == 0)
	{
		ERROR_LOG(VIDEO, "WTF");
	}
	else
	{
		glEnable(GL_FRAGMENT_PROGRAM_ARB);
	}
	// If anything went wrong above, glprogid will be 0, which is OK.
	glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, s_shader.glprogid);
}

}  // namespace
