// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/LinearDiskCache.h"
#include "Core/ConfigManager.h"
#include "VideoBackends/OGL/GLUtil.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/VertexShaderGen.h"

namespace OGL
{

class SHADERUID
{
public:
	VertexShaderUid vuid;
	PixelShaderUid puid;

	SHADERUID() {}

	SHADERUID(const SHADERUID& r) : vuid(r.vuid), puid(r.puid) {}

	bool operator <(const SHADERUID& r) const
	{
		if (puid < r.puid) return true;
		if (r.puid < puid) return false;
		if (vuid < r.vuid) return true;
		return false;
	}

	bool operator ==(const SHADERUID& r) const
	{
		return puid == r.puid && vuid == r.vuid;
	}
};


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
		bool in_cache;

		void Destroy()
		{
			shader.Destroy();
		}
	};

	typedef std::map<SHADERUID, PCacheEntry> PCache;

	static PCacheEntry GetShaderProgram(void);
	static GLuint GetCurrentProgram(void);
	static SHADER* SetShader(DSTALPHA_MODE dstAlphaMode, u32 components);
	static void GetShaderId(SHADERUID *uid, DSTALPHA_MODE dstAlphaMode, u32 components);

	static bool CompileShader(SHADER &shader, const char* vcode, const char* pcode);
	static GLuint CompileSingleShader(GLuint type, const char *code);
	static void UploadConstants();

	static void Init(void);
	static void Shutdown(void);
	static void CreateHeader(void);

private:
	class ProgramShaderCacheInserter : public LinearDiskCacheReader<SHADERUID, u8>
	{
	public:
		void Read(const SHADERUID &key, const u8 *value, u32 value_size) override;
	};

	static PCache pshaders;
	static PCacheEntry* last_entry;
	static SHADERUID last_uid;

	static UidChecker<PixelShaderUid,PixelShaderCode> pixel_uid_checker;
	static UidChecker<VertexShaderUid,VertexShaderCode> vertex_uid_checker;

	static u32 s_ubo_buffer_size;
	static s32 s_ubo_align;
};

}  // namespace OGL
