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

namespace OGL
{

GLuint ProgramShaderCache::CurrentFShader = 0, ProgramShaderCache::CurrentVShader = 0, ProgramShaderCache::CurrentProgram = 0;
ProgramShaderCache::PCache ProgramShaderCache::pshaders;
GLuint ProgramShaderCache::s_ps_vs_ubo;
GLintptr ProgramShaderCache::s_vs_data_offset;

LinearDiskCache<ProgramShaderCache::ShaderUID, u8> g_program_disk_cache;
GLenum ProgramFormat;

GLuint ProgramShaderCache::PCacheEntry::prog_format = 0;

std::pair<u32, u32> ProgramShaderCache::CurrentShaderProgram;
const char *UniformNames[NUM_UNIFORMS] =
{
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

void ProgramShaderCache::SetProgramVariables(PCacheEntry &entry)
{
	// Bind UBO 
	if (g_ActiveConfig.backend_info.bSupportsGLSLUBO)
	{
		GLint PSBlock_id = glGetUniformBlockIndex(entry.prog_id, "PSBlock");
		GLint VSBlock_id = glGetUniformBlockIndex(entry.prog_id, "VSBlock");
		
		if(PSBlock_id != -1)
			glUniformBlockBinding(entry.prog_id, PSBlock_id, 1);
		if(VSBlock_id != -1)
			glUniformBlockBinding(entry.prog_id, VSBlock_id, 2);
	}
	
	// We cache our uniform locations for now
	// Once we move up to a newer version of GLSL, ~1.30
	// We can remove this

	// (Sonicadvance): For some reason this fails on my hardware
	//glGetUniformIndices(entry.prog_id, NUM_UNIFORMS, UniformNames, entry.UniformLocations);
	// Got to do it this crappy way.
	if (!g_ActiveConfig.backend_info.bSupportsGLSLUBO)
		for (int a = 8; a < NUM_UNIFORMS; ++a)
			entry.UniformLocations[a] = glGetUniformLocation(entry.prog_id, UniformNames[a]);

	// Bind Texture Sampler
	for (int a = 0; a < 8; ++a)
	{
		// Still need to get sampler locations since we aren't binding them statically in the shaders
		entry.UniformLocations[a] = glGetUniformLocation(entry.prog_id, UniformNames[a]);
		if (entry.UniformLocations[a] != -1)
			glUniform1i(entry.UniformLocations[a], a);
	}

}

void ProgramShaderCache::SetProgramBindings ( ProgramShaderCache::PCacheEntry& entry )
{
	if (g_ActiveConfig.backend_info.bSupportsGLSLBlend)
	{
		// So we do support extended blending
		// So we need to set a few more things here.
		// Bind our out locations
		glBindFragDataLocationIndexed(entry.prog_id, 0, 0, "ocol0");
		glBindFragDataLocationIndexed(entry.prog_id, 0, 1, "ocol1");
	}

	// Need to set some attribute locations
	glBindAttribLocation(entry.prog_id, SHADER_POSITION_ATTRIB, "vposition");
	glBindAttribLocation(entry.prog_id, SHADER_POSMTX_ATTRIB,   "fposmtx");
	glBindAttribLocation(entry.prog_id, SHADER_TEXTURE0_ATTRIB, "texture0");
	glBindAttribLocation(entry.prog_id, SHADER_COLOR0_ATTRIB,   "color0");
	glBindAttribLocation(entry.prog_id, SHADER_NORM1_ATTRIB,    "rawnorm1");
	glBindAttribLocation(entry.prog_id, SHADER_NORM2_ATTRIB,    "rawnorm2");
}


void ProgramShaderCache::SetBothShaders(GLuint PS, GLuint VS)
{
	CurrentFShader = PS;
	CurrentVShader = VS;

	if (CurrentFShader == 0 && CurrentVShader == 0)
	{
		CurrentProgram = 0;
		glUseProgram(0);
		return;
	}

	// Fragment shaders can survive without Vertex Shaders
	// We have a valid fragment shader, let's create our program
	std::pair<u32, u32> ShaderPair = std::make_pair(CurrentFShader, CurrentVShader);
	PCache::iterator iter = pshaders.find(ShaderPair);
	if (iter != pshaders.end())
	{
		PCacheEntry &entry = iter->second;
		glUseProgram(entry.prog_id);
		CurrentShaderProgram = ShaderPair;
		CurrentProgram = entry.prog_id;
		return;
	}

	PCacheEntry entry;
	entry.Create(CurrentFShader, CurrentVShader);

	// Right, the program is created now
	// Let's attach everything
	if (entry.vsid != 0) // attaching zero vertex shader makes it freak out
		glAttachShader(entry.prog_id, entry.vsid);

	glAttachShader(entry.prog_id, entry.psid);

	if (g_ActiveConfig.backend_info.bSupportsGLSLCache)
		glProgramParameteri(entry.prog_id, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);

	SetProgramBindings(entry);
	
	glLinkProgram(entry.prog_id);

	glUseProgram(entry.prog_id);

	SetProgramVariables(entry);

	pshaders[ShaderPair] = entry;
	CurrentShaderProgram = ShaderPair;
	CurrentProgram = entry.prog_id;
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

GLuint ProgramShaderCache::GetCurrentProgram(void)
{
	return CurrentProgram;
}

ProgramShaderCache::PCacheEntry ProgramShaderCache::GetShaderProgram(void)
{
	return pshaders[CurrentShaderProgram];
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

	// Read our shader cache, only if supported
	if (g_ActiveConfig.backend_info.bSupportsGLSLCache)
	{
		PCacheEntry::prog_format = PCacheEntry::SetProgramFormat();

		char cache_filename[MAX_PATH];
		sprintf(cache_filename, "%sogl-%s-shaders.cache", File::GetUserPath(D_SHADERCACHE_IDX).c_str(),
			SConfig::GetInstance().m_LocalCoreStartupParameter.m_strUniqueID.c_str());

		ProgramShaderCacheInserter inserter;
		g_program_disk_cache.OpenAndRead(cache_filename, inserter);
	}
}

void ProgramShaderCache::Shutdown(void)
{
	if (g_ActiveConfig.backend_info.bSupportsGLSLCache)
	{
		PCache::iterator iter = pshaders.begin();
		for (; iter != pshaders.end(); ++iter)
		{
			g_program_disk_cache.Append(iter->second.uid, iter->second.GetProgram(), iter->second.Size());
			iter->second.FreeProgram();
		}

		g_program_disk_cache.Sync();
		g_program_disk_cache.Close();
	}

	PCache::iterator iter = pshaders.begin();
	for (; iter != pshaders.end(); ++iter)
		iter->second.Destroy();
	pshaders.clear();

	if (g_ActiveConfig.backend_info.bSupportsGLSLUBO)
	{
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
		glDeleteBuffers(1, &s_ps_vs_ubo);
		s_ps_vs_ubo = 0;
	}
}

} // namespace OGL
