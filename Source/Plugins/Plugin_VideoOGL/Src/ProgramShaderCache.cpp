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
#include "StreamBuffer.h"
#include "Debugger.h"
#include "Statistics.h"

namespace OGL
{

static const u32 UBO_LENGTH = 4*1024*1024;

GLintptr ProgramShaderCache::s_vs_data_offset;
u8 *ProgramShaderCache::s_ubo_buffer;
u32 ProgramShaderCache::s_ubo_buffer_size;
bool ProgramShaderCache::s_ubo_dirty;

static StreamBuffer *s_buffer;

LinearDiskCache<SHADERUID, u8> g_program_disk_cache;
static GLuint CurrentProgram = 0;
ProgramShaderCache::PCache ProgramShaderCache::pshaders;
ProgramShaderCache::PCacheEntry* ProgramShaderCache::last_entry;
SHADERUID ProgramShaderCache::last_uid;

static char s_glsl_header[1024] = "";

const char *UniformNames[NUM_UNIFORMS] =
{
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

void SHADER::SetProgramVariables()
{
	// glsl shader must be bind to set samplers
	Bind();
	
	// Bind UBO 
	if (g_ActiveConfig.backend_info.bSupportsGLSLUBO)
	{
		GLint PSBlock_id = glGetUniformBlockIndex(glprogid, "PSBlock");
		GLint VSBlock_id = glGetUniformBlockIndex(glprogid, "VSBlock");
		
		if(PSBlock_id != -1)
			glUniformBlockBinding(glprogid, PSBlock_id, 1);
		if(VSBlock_id != -1)
			glUniformBlockBinding(glprogid, VSBlock_id, 2);
	}
	
	// We cache our uniform locations for now
	// Once we move up to a newer version of GLSL, ~1.30
	// We can remove this

	// (Sonicadvance): For some reason this fails on my hardware
	//glGetUniformIndices(glprogid, NUM_UNIFORMS, UniformNames, UniformLocations);
	// Got to do it this crappy way.
	UniformLocations[0] = glGetUniformLocation(glprogid, UniformNames[0]);
	if (!g_ActiveConfig.backend_info.bSupportsGLSLUBO)
		for (int a = 1; a < NUM_UNIFORMS; ++a)
			UniformLocations[a] = glGetUniformLocation(glprogid, UniformNames[a]);

	// Bind Texture Sampler
	for (int a = 0; a <= 9; ++a)
	{
		char name[8];
		snprintf(name, 8, "samp%d", a);
		
		// Still need to get sampler locations since we aren't binding them statically in the shaders
		int loc = glGetUniformLocation(glprogid, name);
		if (loc != -1)
			glUniform1i(loc, a);
	}

}

void SHADER::SetProgramBindings()
{
	if (g_ActiveConfig.backend_info.bSupportsDualSourceBlend)
	{
		// So we do support extended blending
		// So we need to set a few more things here.
		// Bind our out locations
		glBindFragDataLocationIndexed(glprogid, 0, 0, "ocol0");
		glBindFragDataLocationIndexed(glprogid, 0, 1, "ocol1");
	}
	else
		glBindFragDataLocation(glprogid, 0, "ocol0");

	// Need to set some attribute locations
	glBindAttribLocation(glprogid, SHADER_POSITION_ATTRIB, "rawpos");
	
	glBindAttribLocation(glprogid, SHADER_POSMTX_ATTRIB,   "fposmtx");
	
	glBindAttribLocation(glprogid, SHADER_COLOR0_ATTRIB,   "color0");
	glBindAttribLocation(glprogid, SHADER_COLOR1_ATTRIB,   "color1");
	
	glBindAttribLocation(glprogid, SHADER_NORM0_ATTRIB,    "rawnorm0");
	glBindAttribLocation(glprogid, SHADER_NORM1_ATTRIB,    "rawnorm1");
	glBindAttribLocation(glprogid, SHADER_NORM2_ATTRIB,    "rawnorm2");
	
	for(int i=0; i<8; i++) {
		char attrib_name[8];
		snprintf(attrib_name, 8, "tex%d", i);
		glBindAttribLocation(glprogid, SHADER_TEXTURE0_ATTRIB+i, attrib_name);
	}
}

void SHADER::Bind()
{
	if(CurrentProgram != glprogid)
	{
		glUseProgram(glprogid);
		CurrentProgram = glprogid;
	}
}


void ProgramShaderCache::SetMultiPSConstant4fv(unsigned int offset, const float *f, unsigned int count)
{
	s_ubo_dirty = true;
	memcpy(s_ubo_buffer+(offset*4*sizeof(float)), f, count*4*sizeof(float));
}

void ProgramShaderCache::SetMultiVSConstant4fv(unsigned int offset, const float *f, unsigned int count)
{
	s_ubo_dirty = true;
	memcpy(s_ubo_buffer+(offset*4*sizeof(float))+s_vs_data_offset, f, count*4*sizeof(float));
}

void ProgramShaderCache::UploadConstants()
{
	if(s_ubo_dirty) {
		s_buffer->Alloc(s_ubo_buffer_size);
		size_t offset = s_buffer->Upload(s_ubo_buffer, s_ubo_buffer_size);
		glBindBufferRange(GL_UNIFORM_BUFFER, 1, s_buffer->getBuffer(), offset, s_vs_data_offset);
		glBindBufferRange(GL_UNIFORM_BUFFER, 2, s_buffer->getBuffer(), offset + s_vs_data_offset, s_ubo_buffer_size - s_vs_data_offset);
		s_ubo_dirty = false;
	}
}

GLuint ProgramShaderCache::GetCurrentProgram(void)
{
	return CurrentProgram;
}

SHADER* ProgramShaderCache::SetShader ( DSTALPHA_MODE dstAlphaMode, u32 components )
{
	SHADERUID uid;
	GetShaderId(&uid, dstAlphaMode, components);
	
	// Check if the shader is already set
	if (last_entry)
	{
		if (uid == last_uid)
		{
			GFX_DEBUGGER_PAUSE_AT(NEXT_PIXEL_SHADER_CHANGE, true);
			ValidateShaderIDs(last_entry, dstAlphaMode, components);
			last_entry->shader.Bind();
			return &last_entry->shader;
		}
	}
	
	last_uid = uid;
	
	// Check if shader is already in cache
	PCache::iterator iter = pshaders.find(uid);
	if (iter != pshaders.end())
	{
		PCacheEntry *entry = &iter->second;
		last_entry = entry;

		GFX_DEBUGGER_PAUSE_AT(NEXT_PIXEL_SHADER_CHANGE, true);
		ValidateShaderIDs(entry, dstAlphaMode, components);
		last_entry->shader.Bind();
		return &last_entry->shader;
	}
	
	// Make an entry in the table
	PCacheEntry& newentry = pshaders[uid];
	last_entry = &newentry;
	newentry.in_cache = 0;
	
	const char *vcode = GenerateVertexShaderCode(components, API_OPENGL);
	const char *pcode = GeneratePixelShaderCode(dstAlphaMode, API_OPENGL, components);
	
	if (g_ActiveConfig.bEnableShaderDebugging)
	{
		GetSafeShaderId(&newentry.safe_uid, dstAlphaMode, components);
		newentry.shader.strvprog = vcode;
		newentry.shader.strpprog = pcode;
	}

#if defined(_DEBUG) || defined(DEBUGFAST)
	if (g_ActiveConfig.iLog & CONF_SAVESHADERS) {
		static int counter = 0;
		char szTemp[MAX_PATH];
		sprintf(szTemp, "%svs_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), counter++);
		SaveData(szTemp, vcode);
		sprintf(szTemp, "%sps_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), counter++);
		SaveData(szTemp, pcode);
	}
#endif

	if (!CompileShader(newentry.shader, vcode, pcode)) {
		GFX_DEBUGGER_PAUSE_AT(NEXT_ERROR, true);
		return NULL;
	}

	INCSTAT(stats.numPixelShadersCreated);
	SETSTAT(stats.numPixelShadersAlive, pshaders.size());
	GFX_DEBUGGER_PAUSE_AT(NEXT_PIXEL_SHADER_CHANGE, true);
	
	last_entry->shader.Bind();
	return &last_entry->shader;
}

bool ProgramShaderCache::CompileShader ( SHADER& shader, const char* vcode, const char* pcode )
{
	GLuint vsid = CompileSingleShader(GL_VERTEX_SHADER, vcode);
	GLuint psid = CompileSingleShader(GL_FRAGMENT_SHADER, pcode);
	
	if(!vsid || !psid)
	{
		glDeleteShader(vsid);
		glDeleteShader(psid);
		return false;
	}
	
	GLuint pid = shader.glprogid = glCreateProgram();;
	
	glAttachShader(pid, vsid);
	glAttachShader(pid, psid);

	if (g_ActiveConfig.backend_info.bSupportsGLSLCache)
		glProgramParameteri(pid, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);

	shader.SetProgramBindings();
	
	glLinkProgram(pid);
	
	// original shaders aren't needed any more
	glDeleteShader(vsid);
	glDeleteShader(psid);
	
#if defined(_DEBUG) || defined(DEBUGFAST) || defined(DEBUG_GLSL)
	GLsizei length = 0;
	glGetProgramiv(pid, GL_INFO_LOG_LENGTH, &length);
	if (length > 1)
	{
		GLsizei charsWritten;
		GLchar* infoLog = new GLchar[length];
		glGetProgramInfoLog(pid, length, &charsWritten, infoLog);
		ERROR_LOG(VIDEO, "Program info log:\n%s", infoLog);
		char szTemp[MAX_PATH];
		sprintf(szTemp, "p_%d.txt", pid);
		FILE *fp = fopen(szTemp, "wb");
		fwrite(infoLog, length, 1, fp);
		delete[] infoLog;
		fwrite(s_glsl_header, strlen(s_glsl_header), 1, fp);
		fwrite(vcode, strlen(vcode), 1, fp);
		fwrite(s_glsl_header, strlen(s_glsl_header), 1, fp);
		fwrite(pcode, strlen(pcode), 1, fp);
		fclose(fp);
	}

	GLint linkStatus;
	glGetProgramiv(pid, GL_LINK_STATUS, &linkStatus);
	if (linkStatus != GL_TRUE)
	{
		// Compile failed
		ERROR_LOG(VIDEO, "Program linking failed; see info log");

		// Don't try to use this shader
		glDeleteProgram(pid);
		return false;
	}
#endif

	shader.SetProgramVariables();
	
	return true;
}

GLuint ProgramShaderCache::CompileSingleShader (GLuint type, const char* code )
{
	GLuint result = glCreateShader(type);

	const char *src[] = {s_glsl_header, code};
	
	glShaderSource(result, 2, src, NULL);
	glCompileShader(result);
#if defined(_DEBUG) || defined(DEBUGFAST) || defined(DEBUG_GLSL)
	GLsizei length = 0;
	glGetShaderiv(result, GL_INFO_LOG_LENGTH, &length);
	if (length > 1)
	{
		GLsizei charsWritten;
		GLchar* infoLog = new GLchar[length];
		glGetShaderInfoLog(result, length, &charsWritten, infoLog);
		ERROR_LOG(VIDEO, "PS Shader info log:\n%s", infoLog);
		char szTemp[MAX_PATH];
		sprintf(szTemp, "ps_%d.txt", result);
		FILE *fp = fopen(szTemp, "wb");
		fwrite(infoLog, strlen(infoLog), 1, fp);
		fwrite(s_glsl_header, strlen(s_glsl_header), 1, fp);
		fwrite(code, strlen(code), 1, fp);
		fclose(fp);
		delete[] infoLog;
	}

	GLint compileStatus;
	glGetShaderiv(result, GL_COMPILE_STATUS, &compileStatus);
	if (compileStatus != GL_TRUE)
	{
		// Compile failed
		ERROR_LOG(VIDEO, "Shader compilation failed; see info log");

		// Don't try to use this shader
		glDeleteShader(result);
		return 0;
	}
#endif
	(void)GL_REPORT_ERROR();
	return result;
}



void ProgramShaderCache::GetShaderId ( SHADERUID* uid, DSTALPHA_MODE dstAlphaMode, u32 components )
{
	GetPixelShaderId(&uid->puid, dstAlphaMode, components);
	GetVertexShaderId(&uid->vuid, components);
}

void ProgramShaderCache::GetSafeShaderId ( SHADERUIDSAFE* uid, DSTALPHA_MODE dstAlphaMode, u32 components )
{
	GetSafePixelShaderId(&uid->puid, dstAlphaMode, components);
	GetSafeVertexShaderId(&uid->vuid, components);
}

void ProgramShaderCache::ValidateShaderIDs ( PCacheEntry *entry, DSTALPHA_MODE dstAlphaMode, u32 components )
{
	ValidateVertexShaderIDs(API_OPENGL, entry->safe_uid.vuid, entry->shader.strvprog, components);
	ValidatePixelShaderIDs(API_OPENGL, entry->safe_uid.puid, entry->shader.strpprog, dstAlphaMode, components);
}



ProgramShaderCache::PCacheEntry ProgramShaderCache::GetShaderProgram(void)
{
	return *last_entry;
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
		s_ubo_buffer_size = ps_data_size + vs_data_size;

		// We multiply by *4*4 because we need to get down to basic machine units.
		// So multiply by four to get how many floats we have from vec4s
		// Then once more to get bytes
		s_buffer = new StreamBuffer(GL_UNIFORM_BUFFER, UBO_LENGTH);
		
		s_ubo_buffer = new u8[s_ubo_buffer_size];
		memset(s_ubo_buffer, 0, s_ubo_buffer_size);
		s_ubo_dirty = true;
	}

	// Read our shader cache, only if supported
	if (g_ActiveConfig.backend_info.bSupportsGLSLCache)
	{
		GLint Supported;
		glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS, &Supported);
		if(!Supported)
		{
			ERROR_LOG(VIDEO, "GL_ARB_get_program_binary is supported, but no binary format is known. So disable shader cache.");
			g_ActiveConfig.backend_info.bSupportsGLSLCache = false;
		}
		else
		{
			if (!File::Exists(File::GetUserPath(D_SHADERCACHE_IDX)))
				File::CreateDir(File::GetUserPath(D_SHADERCACHE_IDX).c_str());
		
			char cache_filename[MAX_PATH];
			sprintf(cache_filename, "%sogl-%s-shaders.cache", File::GetUserPath(D_SHADERCACHE_IDX).c_str(),
				SConfig::GetInstance().m_LocalCoreStartupParameter.m_strUniqueID.c_str());
			
			ProgramShaderCacheInserter inserter;
			g_program_disk_cache.OpenAndRead(cache_filename, inserter);
		}
		SETSTAT(stats.numPixelShadersAlive, pshaders.size());
	}
	
	CreateHeader();
	
	CurrentProgram = 0;
	last_entry = NULL;
}

void ProgramShaderCache::Shutdown(void)
{
	// store all shaders in cache on disk
	if (g_ActiveConfig.backend_info.bSupportsGLSLCache)
	{
		PCache::iterator iter = pshaders.begin();
		for (; iter != pshaders.end(); ++iter)
		{
			if(iter->second.in_cache) continue;
			
			GLint binary_size;
			glGetProgramiv(iter->second.shader.glprogid, GL_PROGRAM_BINARY_LENGTH, &binary_size);
			if(!binary_size) continue;
			
			u8 *data = new u8[binary_size+sizeof(GLenum)];
			u8 *binary = data + sizeof(GLenum);
			GLenum *prog_format = (GLenum*)data;
			glGetProgramBinary(iter->second.shader.glprogid, binary_size, NULL, prog_format, binary);
			
			g_program_disk_cache.Append(iter->first, data, binary_size+sizeof(GLenum));
			delete [] data;
		}

		g_program_disk_cache.Sync();
		g_program_disk_cache.Close();
	}

	glUseProgram(0);
	
	PCache::iterator iter = pshaders.begin();
	for (; iter != pshaders.end(); ++iter)
		iter->second.Destroy();
	pshaders.clear();

	if (g_ActiveConfig.backend_info.bSupportsGLSLUBO)
	{
		delete s_buffer;
		s_buffer = 0;
		delete [] s_ubo_buffer;
		s_ubo_buffer = 0;
	}
}

void ProgramShaderCache::CreateHeader ( void )
{
#ifdef _WIN32
	// Intel Windows driver has a issue:
	// their glsl doesn't know about the ubo extension, so we can't load it.
	// but as version 140, ubo is in core and don't have to be loaded in glsl.
	// as sandy do ogl3.1, glsl 140 is supported, so force it in this way. 
	// TODO: remove this again when the issue is fixed:
	// see http://communities.intel.com/thread/36084
	u8 *vendor = glGetString(GL_VENDOR);
	bool intel_windows_hack = strcmp(vendor, "Intel") == 0;
#else
	bool intel_windows_hack = false;
#endif
	
	snprintf(s_glsl_header, sizeof(s_glsl_header), 
		"#version %s\n"
		"#extension GL_ARB_texture_rectangle : enable\n"
		"%s\n" // ubo
		
		"\n"// A few required defines and ones that will make our lives a lot easier
		"#define ATTRIN in\n"
		"#define ATTROUT out\n"
		"#define VARYIN in\n"
		"#define VARYOUT out\n"

		// Silly differences
		"#define float2 vec2\n"
		"#define float3 vec3\n"
		"#define float4 vec4\n"

		// hlsl to glsl function translation
		"#define frac(x) fract(x)\n"
		"#define saturate(x) clamp(x, 0.0f, 1.0f)\n"
		"#define lerp(x, y, z) mix(x, y, z)\n"
		
		
		, intel_windows_hack ? "140" : "130"
		, g_ActiveConfig.backend_info.bSupportsGLSLUBO && !intel_windows_hack ? "#extension GL_ARB_uniform_buffer_object : enable" : "// ubo disabled"
	);
}


void ProgramShaderCache::ProgramShaderCacheInserter::Read ( const SHADERUID& key, const u8* value, u32 value_size )
{
	const u8 *binary = value+sizeof(GLenum);
	GLenum *prog_format = (GLenum*)value;
	GLint binary_size = value_size-sizeof(GLenum);
	
	PCacheEntry entry;
	entry.in_cache = 1;
	entry.shader.glprogid = glCreateProgram();
	glProgramBinary(entry.shader.glprogid, *prog_format, binary, binary_size);

	GLint success;
	glGetProgramiv(entry.shader.glprogid, GL_LINK_STATUS, &success);

	if (success)
	{
		pshaders[key] = entry;
		entry.shader.SetProgramVariables();
	}
	else
		glDeleteProgram(entry.shader.glprogid);
}


} // namespace OGL
