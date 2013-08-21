// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "ProgramShaderCache.h"
#include "DriverDetails.h"
#include "MathUtil.h"
#include "StreamBuffer.h"
#include "Debugger.h"
#include "Statistics.h"
#include "ImageWrite.h"
#include "Render.h"

namespace OGL
{

static const u32 UBO_LENGTH = 32*1024*1024;

GLintptr ProgramShaderCache::s_vs_data_size;
GLintptr ProgramShaderCache::s_ps_data_size;
GLintptr ProgramShaderCache::s_vs_data_offset;
u8 *ProgramShaderCache::s_ubo_buffer;
u32 ProgramShaderCache::s_ubo_buffer_size;
bool ProgramShaderCache::s_ubo_dirty;

static StreamBuffer *s_buffer;
static int num_failures = 0;

LinearDiskCache<SHADERUID, u8> g_program_disk_cache;
static GLuint CurrentProgram = 0;
ProgramShaderCache::PCache ProgramShaderCache::pshaders;
ProgramShaderCache::PCacheEntry* ProgramShaderCache::last_entry;
SHADERUID ProgramShaderCache::last_uid;
UidChecker<PixelShaderUid,PixelShaderCode> ProgramShaderCache::pixel_uid_checker;
UidChecker<VertexShaderUid,VertexShaderCode> ProgramShaderCache::vertex_uid_checker;

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
#ifndef USE_GLES3
		glBindFragDataLocationIndexed(glprogid, 0, 0, "ocol0");
		glBindFragDataLocationIndexed(glprogid, 0, 1, "ocol1");
#endif
	}
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
		glBindBufferRange(GL_UNIFORM_BUFFER, 1, s_buffer->getBuffer(), offset, s_ps_data_size);
		glBindBufferRange(GL_UNIFORM_BUFFER, 2, s_buffer->getBuffer(), offset + s_vs_data_offset, s_vs_data_size);
		s_ubo_dirty = false;
		
		ADDSTAT(stats.thisFrame.bytesUniformStreamed, s_ubo_buffer_size);
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
		last_entry->shader.Bind();
		return &last_entry->shader;
	}

	// Make an entry in the table
	PCacheEntry& newentry = pshaders[uid];
	last_entry = &newentry;
	newentry.in_cache = 0;

	VertexShaderCode vcode;
	PixelShaderCode pcode;
	GenerateVertexShaderCode(vcode, components, API_OPENGL);
	GeneratePixelShaderCode(pcode, dstAlphaMode, API_OPENGL, components);

	if (g_ActiveConfig.bEnableShaderDebugging)
	{
		newentry.shader.strvprog = vcode.GetBuffer();
		newentry.shader.strpprog = pcode.GetBuffer();
	}

#if defined(_DEBUG) || defined(DEBUGFAST)
	if (g_ActiveConfig.iLog & CONF_SAVESHADERS) {
		static int counter = 0;
		char szTemp[MAX_PATH];
		sprintf(szTemp, "%svs_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), counter++);
		SaveData(szTemp, vcode.GetBuffer());
		sprintf(szTemp, "%sps_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), counter++);
		SaveData(szTemp, pcode.GetBuffer());
	}
#endif

	if (!CompileShader(newentry.shader, vcode.GetBuffer(), pcode.GetBuffer())) {
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

	if (g_ogl_config.bSupportsGLSLCache)
		glProgramParameteri(pid, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);

	shader.SetProgramBindings();
	
	glLinkProgram(pid);
	
	// original shaders aren't needed any more
	glDeleteShader(vsid);
	glDeleteShader(psid);
	
	GLint linkStatus;
	glGetProgramiv(pid, GL_LINK_STATUS, &linkStatus);
	GLsizei length = 0;
	glGetProgramiv(pid, GL_INFO_LOG_LENGTH, &length);
	if (linkStatus != GL_TRUE || (length > 1 && DEBUG_GLSL))
	{
		GLsizei charsWritten;
		GLchar* infoLog = new GLchar[length];
		glGetProgramInfoLog(pid, length, &charsWritten, infoLog);
		ERROR_LOG(VIDEO, "Program info log:\n%s", infoLog);
		char szTemp[MAX_PATH];
		sprintf(szTemp, "%sbad_p_%d.txt", File::GetUserPath(D_DUMP_IDX).c_str(), num_failures++);
		std::ofstream file;
		OpenFStream(file, szTemp, std::ios_base::out);
		file << s_glsl_header << vcode << s_glsl_header << pcode << infoLog;
		file.close();
		
		PanicAlert("Failed to link shaders!\nThis usually happens when trying to use Dolphin with an outdated GPU or integrated GPU like the Intel GMA series.\n\nIf you're sure this is Dolphin's error anyway, post the contents of %s along with this error message at the forums.\n\nDebug info (%s, %s, %s):\n%s",
			szTemp,
			g_ogl_config.gl_vendor,
			g_ogl_config.gl_renderer,
			g_ogl_config.gl_version,
			infoLog);
		
		delete [] infoLog;
	}
	if (linkStatus != GL_TRUE)
	{
		// Compile failed
		ERROR_LOG(VIDEO, "Program linking failed; see info log");

		// Don't try to use this shader
		glDeleteProgram(pid);
		return false;
	}

	shader.SetProgramVariables();
	
	return true;
}

GLuint ProgramShaderCache::CompileSingleShader (GLuint type, const char* code )
{
	GLuint result = glCreateShader(type);

	const char *src[] = {s_glsl_header, code};
	
	glShaderSource(result, 2, src, NULL);
	glCompileShader(result);
	GLint compileStatus;
	glGetShaderiv(result, GL_COMPILE_STATUS, &compileStatus);
	GLsizei length = 0;
	glGetShaderiv(result, GL_INFO_LOG_LENGTH, &length);

	if (DriverDetails::HasBug(DriverDetails::BUG_BROKENINFOLOG))
		length = 1024;

	if (compileStatus != GL_TRUE || (length > 1 && DEBUG_GLSL))
	{
		GLsizei charsWritten;
		GLchar* infoLog = new GLchar[length];
		glGetShaderInfoLog(result, length, &charsWritten, infoLog);
		ERROR_LOG(VIDEO, "PS Shader info log:\n%s", infoLog);
		char szTemp[MAX_PATH];
		sprintf(szTemp, 
			"%sbad_%s_%04i.txt", 
			File::GetUserPath(D_DUMP_IDX).c_str(), 
			type==GL_VERTEX_SHADER ? "vs" : "ps", 
			num_failures++);
		std::ofstream file;
		OpenFStream(file, szTemp, std::ios_base::out);
		file << s_glsl_header << code << infoLog;
		file.close();
		
		PanicAlert("Failed to compile %s shader!\nThis usually happens when trying to use Dolphin with an outdated GPU or integrated GPU like the Intel GMA series.\n\nIf you're sure this is Dolphin's error anyway, post the contents of %s along with this error message at the forums.\n\nDebug info (%s, %s, %s):\n%s",
			type==GL_VERTEX_SHADER ? "vertex" : "pixel",
			szTemp,
			g_ogl_config.gl_vendor,
			g_ogl_config.gl_renderer,
			g_ogl_config.gl_version,
			infoLog);
		
		delete[] infoLog;
	}
	if (compileStatus != GL_TRUE)
	{
		// Compile failed
		ERROR_LOG(VIDEO, "Shader compilation failed; see info log");

		// Don't try to use this shader
		glDeleteShader(result);
		return 0;
	}
	(void)GL_REPORT_ERROR();
	return result;
}

void ProgramShaderCache::GetShaderId(SHADERUID* uid, DSTALPHA_MODE dstAlphaMode, u32 components)
{
	GetPixelShaderUid(uid->puid, dstAlphaMode, API_OPENGL, components);
	GetVertexShaderUid(uid->vuid, components, API_OPENGL);

	if (g_ActiveConfig.bEnableShaderDebugging)
	{
		PixelShaderCode pcode;
		GeneratePixelShaderCode(pcode, dstAlphaMode, API_OPENGL, components);
		pixel_uid_checker.AddToIndexAndCheck(pcode, uid->puid, "Pixel", "p");

		VertexShaderCode vcode;
		GenerateVertexShaderCode(vcode, components, API_OPENGL);
		vertex_uid_checker.AddToIndexAndCheck(vcode, uid->vuid, "Vertex", "v");
	}
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

		s_ps_data_size = C_PENVCONST_END * sizeof(float) * 4;
		s_vs_data_size = C_VENVCONST_END * sizeof(float) * 4;
		s_vs_data_offset = ROUND_UP(s_ps_data_size, Align);
		s_ubo_buffer_size = ROUND_UP(s_ps_data_size, Align) + ROUND_UP(s_vs_data_size, Align);

		// We multiply by *4*4 because we need to get down to basic machine units.
		// So multiply by four to get how many floats we have from vec4s
		// Then once more to get bytes
		s_buffer = new StreamBuffer(GL_UNIFORM_BUFFER, UBO_LENGTH);
		
		s_ubo_buffer = new u8[s_ubo_buffer_size];
		memset(s_ubo_buffer, 0, s_ubo_buffer_size);
		s_ubo_dirty = true;
	}

	// Read our shader cache, only if supported
	if (g_ogl_config.bSupportsGLSLCache)
	{
		GLint Supported;
		glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS, &Supported);
		if(!Supported)
		{
			ERROR_LOG(VIDEO, "GL_ARB_get_program_binary is supported, but no binary format is known. So disable shader cache.");
			g_ogl_config.bSupportsGLSLCache = false;
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
	if (g_ogl_config.bSupportsGLSLCache)
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

	pixel_uid_checker.Invalidate();
	vertex_uid_checker.Invalidate();

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
	GLSL_VERSION v = g_ogl_config.eSupportedGLSLVersion;
	snprintf(s_glsl_header, sizeof(s_glsl_header), 
		"#version %s\n"
		"%s\n" // default precision
		"%s\n" // ubo
		"%s\n" // early-z
		
		"\n"// A few required defines and ones that will make our lives a lot easier
		"#define ATTRIN %s\n"
		"#define ATTROUT %s\n"
		"#define VARYIN %s\n"
		"#define VARYOUT %s\n"

		// Silly differences
		"#define float2 vec2\n"
		"#define float3 vec3\n"
		"#define float4 vec4\n"

		// hlsl to glsl function translation
		"#define frac fract\n"
		"#define lerp mix\n"

		// glsl 120 hack
		"%s\n"
		"%s\n"
		"%s\n"
		"%s\n"
		"%s\n"
		"#define COLOROUT(name) %s\n"
		
		// texture2d hack
		"%s\n"
		"%s\n"
		"%s\n"
				
		, v==GLSLES3 ? "300 es" : v==GLSL_120 ? "120" : v==GLSL_130 ? "130" : v==GLSL_140 ? "140" : "150"
		, v==GLSLES3 ? "precision highp float;" : ""
		, g_ActiveConfig.backend_info.bSupportsGLSLUBO && v<GLSL_140 ? "#extension GL_ARB_uniform_buffer_object : enable" : ""
		, g_ActiveConfig.backend_info.bSupportsEarlyZ ? "#extension GL_ARB_shader_image_load_store : enable" : ""
		
		, v==GLSL_120 ? "attribute" : "in"
		, v==GLSL_120 ? "attribute" : "out"
		, DriverDetails::HasBug(DriverDetails::BUG_BROKENCENTROID) ? "in" : v==GLSL_120 ? "varying" : "centroid in"
		, DriverDetails::HasBug(DriverDetails::BUG_BROKENCENTROID) ? "out" : v==GLSL_120 ? "varying" : "centroid out"
		
		, v==GLSL_120 ? "#define texture texture2D" : ""
		, v==GLSL_120 ? "#define round(x) floor((x)+0.5f)" : ""
		, v==GLSL_120 ? "#define out " : ""
		, v==GLSL_120 ? "#define ocol0 gl_FragColor" : ""
		, v==GLSL_120 ? "#define ocol1 gl_FragColor" : "" //TODO: implement dual source blend
		, v==GLSL_120 ? "" : "out vec4 name;"
		
		, v==GLSL_120 ? "#extension GL_ARB_texture_rectangle : enable" : ""
		, v==GLSL_120 ? "" : "#define texture2DRect(samp, uv)  texelFetch(samp, ivec2(floor(uv)), 0)"
		, v==GLSL_120 ? "" : "#define sampler2DRect sampler2D"
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
