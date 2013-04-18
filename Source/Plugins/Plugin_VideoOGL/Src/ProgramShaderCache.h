// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef PROGRAM_SHADER_CACHE_H_
#define PROGRAM_SHADER_CACHE_H_

#include "GLUtil.h"

#include "PixelShaderGen.h"
#include "VertexShaderGen.h"

#include "LinearDiskCache.h"
#include "ConfigManager.h"

namespace OGL
{

template<bool safe>
class _SHADERUID
{
public:
	_VERTEXSHADERUID<safe> vuid;
	_PIXELSHADERUID<safe> puid;

	_SHADERUID() {}

	_SHADERUID(const _SHADERUID& r) : vuid(r.vuid), puid(r.puid) {}

	bool operator <(const _SHADERUID& r) const
	{
		if(puid < r.puid) return true;
		if(r.puid < puid) return false;
		if(vuid < r.vuid) return true;
		return false;
	}

	bool operator ==(const _SHADERUID& r) const
	{
		return puid == r.puid && vuid == r.vuid;
	}
};
typedef _SHADERUID<false> SHADERUID;
typedef _SHADERUID<true> SHADERUIDSAFE;


const int NUM_UNIFORMS = 19;
extern const char *UniformNames[NUM_UNIFORMS];

struct SHADER
{
	SHADER() : glprogid(0) { }
	void Destroy()
	{
		glDeleteProgram(glprogid);
		glprogid = 0;
	}
	GLuint glprogid; // opengl program id
	
	std::string strvprog, strpprog;
	GLint UniformLocations[NUM_UNIFORMS];
	
	void SetProgramVariables();
	void SetProgramBindings();
	void Bind();
};

class ProgramShaderCache
{
public:

	struct PCacheEntry
	{
		SHADER shader;
		SHADERUIDSAFE safe_uid;
		bool in_cache;

		void Destroy()
		{
			shader.Destroy();
		}
	};

	static PCacheEntry GetShaderProgram(void);
	static GLuint GetCurrentProgram(void);
	static SHADER* SetShader(DSTALPHA_MODE dstAlphaMode, u32 components);
	static void GetShaderId(SHADERUID *uid, DSTALPHA_MODE dstAlphaMode, u32 components);
	static void GetSafeShaderId(SHADERUIDSAFE *uid, DSTALPHA_MODE dstAlphaMode, u32 components);
	static void ValidateShaderIDs(PCacheEntry *entry, DSTALPHA_MODE dstAlphaMode, u32 components);
	
	static bool CompileShader(SHADER &shader, const char* vcode, const char* pcode);
	static GLuint CompileSingleShader(GLuint type, const char *code);

	static void SetMultiPSConstant4fv(unsigned int offset, const float *f, unsigned int count);
	static void SetMultiVSConstant4fv(unsigned int offset, const float *f, unsigned int count);
	static void UploadConstants();

	static void Init(void);
	static void Shutdown(void);
	static void CreateHeader(void);

private:
	class ProgramShaderCacheInserter : public LinearDiskCacheReader<SHADERUID, u8>
	{
	public:
		void Read(const SHADERUID &key, const u8 *value, u32 value_size);
	};

	typedef std::map<SHADERUID, PCacheEntry> PCache;

	static PCache pshaders;
	static PCacheEntry* last_entry;
	static SHADERUID last_uid;

	static GLintptr s_vs_data_size;
	static GLintptr s_ps_data_size;
	static GLintptr s_vs_data_offset;
	static u8 *s_ubo_buffer;
	static u32 s_ubo_buffer_size;
	static bool s_ubo_dirty;
};

}  // namespace OGL
#endif
