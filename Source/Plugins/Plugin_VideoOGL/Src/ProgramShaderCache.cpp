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

#include "ProgramShaderCache.h"
#include "MathUtil.h"

#include <assert.h>
namespace OGL
{
	GLuint ProgramShaderCache::CurrentFShader = 0, ProgramShaderCache::CurrentVShader = 0, ProgramShaderCache::CurrentProgram = 0;
	ProgramShaderCache::PCache ProgramShaderCache::pshaders;
	GLuint ProgramShaderCache::s_ps_vs_ubo;
	GLintptr ProgramShaderCache::s_vs_data_offset;

	std::pair<u64, u64> ProgramShaderCache::CurrentShaderProgram;
	const char *UniformNames[NUM_UNIFORMS] = {
		// SAMPLERS
			"samp0","samp1","samp2","samp3","samp4","samp5","samp6","samp7",
		// PIXEL SHADER UNIFORMS
			I_COLORS,
			I_KCOLORS,
			I_ALPHA,
			I_TEXDIMS,
			I_ZBIAS ,
			I_INDTEXSCALE ,
			I_INDTEXMTX,
			I_FOG,
			I_PLIGHTS,
			I_PMATERIALS,
		// VERTEX SHADER UNIFORMS
			I_POSNORMALMATRIX,
			I_PROJECTION ,
			I_MATERIALS,
			I_LIGHTS,
			I_TEXMATRICES,
			I_TRANSFORMMATRICES ,
			I_NORMALMATRICES ,
			I_POSTTRANSFORMMATRICES,
			I_DEPTHPARAMS,
		};

	void ProgramShaderCache::SetBothShaders(GLuint PS, GLuint VS)
	{
		PROGRAMUID uid;
		CurrentFShader = PS;
		CurrentVShader = VS;


		GetProgramShaderId(&uid, CurrentVShader, CurrentFShader);

		if(uid.uid.id == 0)
		{
			CurrentProgram = 0;
			glUseProgram(0);
			return;
		}

		// Fragment shaders can survive without Vertex Shaders
		// We have a valid fragment shader, let's create our program
		std::pair<u64, u64> ShaderPair = std::make_pair(uid.uid.psid, uid.uid.vsid);
		PCache::iterator iter = pshaders.find(ShaderPair);
		if (iter != pshaders.end())
		{
			PCacheEntry &entry = iter->second;
			glUseProgram(entry.program.glprogid);
			CurrentShaderProgram = ShaderPair;
			CurrentProgram = entry.program.glprogid;
			return;
		}
		PCacheEntry entry;
		entry.program.vsid = CurrentVShader;
		entry.program.psid = CurrentFShader;
		entry.program.glprogid = glCreateProgram();

		// Right, the program is created now
		// Let's attach everything
		if(entry.program.vsid != 0) // attaching zero vertex shader makes it freak out
			glAttachShader(entry.program.glprogid, entry.program.vsid);
			
		glAttachShader(entry.program.glprogid, entry.program.psid);
		
		glLinkProgram(entry.program.glprogid);
		
		glUseProgram(entry.program.glprogid);

		// Dunno why this is needed when I have the binding
		// points statically set in the shader source
		// We should only need these two functions when we don't support binding but do support UBO
		// Driver Bug? Nvidia GTX 570, 290.xx Driver, Linux x64
		if (g_ActiveConfig.backend_info.bSupportsGLSLUBO)
		{
			glUniformBlockBinding( entry.program.glprogid, 0, 1 );
			glUniformBlockBinding( entry.program.glprogid, 1, 2 );
		}
				
		// We cache our uniform locations for now
		// Once we move up to a newer version of GLSL, ~1.30
		// We can remove this
		
		//For some reason this fails on my hardware     
		//glGetUniformIndices(entry.program.glprogid, NUM_UNIFORMS, UniformNames, entry.program.UniformLocations);
		//Got to do it this crappy way.
		if(!g_ActiveConfig.backend_info.bSupportsGLSLUBO)
			for(int a = 0; a < NUM_UNIFORMS; ++a)
				entry.program.UniformLocations[a] = glGetUniformLocation(entry.program.glprogid, UniformNames[a]);

		// Need to get some attribute locations
		if(uid.uid.vsid != 0) // We have no vertex Shader
		{
			glBindAttribLocation(entry.program.glprogid, SHADER_NORM1_ATTRIB, "rawnorm1");
			glBindAttribLocation(entry.program.glprogid, SHADER_NORM2_ATTRIB, "rawnorm2");
			glBindAttribLocation(entry.program.glprogid, SHADER_POSMTX_ATTRIB, "fposmtx");
		}


		pshaders[ShaderPair] = entry;
		CurrentShaderProgram = ShaderPair;
		CurrentProgram = entry.program.glprogid;
	}

	void ProgramShaderCache::SetMultiPSConstant4fv(unsigned int offset, const float *f, unsigned int count)
	{
		glBufferSubData(GL_UNIFORM_BUFFER, offset * sizeof(float) * 4,
			count * sizeof(float) * 4, f);
	}

	void ProgramShaderCache::SetMultiVSConstant4fv(unsigned int offset, const float *f, unsigned int count)
	{
		glBufferSubData(GL_UNIFORM_BUFFER, s_vs_data_offset + offset * sizeof(float) * 4,
			count * sizeof(float) * 4, f);
	}
	
	GLuint ProgramShaderCache::GetCurrentProgram(void) { return CurrentProgram; }

	PROGRAMSHADER ProgramShaderCache::GetShaderProgram(void)
	{
		return pshaders[CurrentShaderProgram].program;
	}
	void ProgramShaderCache::Init(void)
	{
		// We have to get the UBO alignment here because
		// if we generate a buffer that isn't aligned
		// then the UBO will fail.
		if (g_ActiveConfig.backend_info.bSupportsGLSLUBO)
		{
			GLint Align;
			glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &Align);

			GLintptr const ps_data_size = ROUND_UP(C_PENVCONST_END * sizeof(float) * 4, Align);
			GLintptr const vs_data_size = ROUND_UP(C_VENVCONST_END * sizeof(float) * 4, Align);
			s_vs_data_offset = ps_data_size;
		
			// We multiply by *4*4 because we need to get down to basic machine units.
			// So multiply by four to get how many floats we have from vec4s
			// Then once more to get bytes
			glGenBuffers(1, &s_ps_vs_ubo);
			glBindBuffer(GL_UNIFORM_BUFFER, s_ps_vs_ubo);
			glBufferData(GL_UNIFORM_BUFFER, ps_data_size + vs_data_size, NULL, GL_DYNAMIC_DRAW);
			
			// Now bind the buffer to the index point
			// We know PS is 0 since we have it statically set in the shader
			// Repeat for VS shader
			glBindBufferRange(GL_UNIFORM_BUFFER, 1, s_ps_vs_ubo, 0, ps_data_size);
			glBindBufferRange(GL_UNIFORM_BUFFER, 2, s_ps_vs_ubo, s_vs_data_offset, vs_data_size);
		}
	}
	
	void ProgramShaderCache::Shutdown(void)
	{
		PCache::iterator iter = pshaders.begin();
		for (; iter != pshaders.end(); ++iter)
			iter->second.Destroy();
		pshaders.clear();

		glBindBuffer(GL_UNIFORM_BUFFER, 0);
		glDeleteBuffers(1, &s_ps_vs_ubo);
		s_ps_vs_ubo = 0;
	}
}

void GetProgramShaderId(PROGRAMUID *uid, GLuint _v, GLuint _p)
{
	uid->uid.vsid = _v;
	uid->uid.psid = _p;
}
