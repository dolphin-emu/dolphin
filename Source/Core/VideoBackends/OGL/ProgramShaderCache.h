// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <tuple>

#include "Common/LinearDiskCache.h"
#include "Common/GL/GLUtil.h"

#include "Core/ConfigManager.h"

#include "VideoCommon/GeometryShaderGen.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/VertexShaderGen.h"

namespace OGL
{

class SHADERUID
{
public:
	VertexShaderUid vuid;
	PixelShaderUid puid;
	GeometryShaderUid guid;

	bool operator <(const SHADERUID& r) const
	{
		return std::tie(puid, vuid, guid) <
		       std::tie(r.puid, r.vuid, r.guid);
	}

	bool operator ==(const SHADERUID& r) const
	{
		return std::tie(puid, vuid, guid) ==
		       std::tie(r.puid, r.vuid, r.guid);
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
	GLuint glprogid; // OpenGL program id

	std::string strvprog, strpprog, strgprog;

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


	static PCacheEntry GetShaderProgram();
	static SHADER* SetShader(DSTALPHA_MODE dstAlphaMode, u32 primitive_type);
	static void GetShaderId(SHADERUID *uid, DSTALPHA_MODE dstAlphaMode, u32 primitive_type);

	static bool CompileShader(SHADER &shader, const std::string& vcode, const std::string& pcode, const std::string& gcode = "");
	static GLuint CompileSingleShader(GLuint type, const std::string& code);
	static void UploadConstants();

	static void Init();
	static void Shutdown();
	static void CreateHeader();

private:
	class ProgramShaderCacheInserter : public LinearDiskCacheReader<SHADERUID, u8>
	{
	public:
		void Read(const SHADERUID &key, const u8 *value, u32 value_size) override;
	};

	typedef std::map<SHADERUID, PCacheEntry> PCache;
	static PCache pshaders;
	static PCacheEntry* last_entry;
	static SHADERUID last_uid;

	static UidChecker<PixelShaderUid, ShaderCode> pixel_uid_checker;
	static UidChecker<VertexShaderUid, ShaderCode> vertex_uid_checker;
	static UidChecker<GeometryShaderUid, ShaderCode> geometry_uid_checker;

	static u32 s_ubo_buffer_size;
	static s32 s_ubo_align;
};

}  // namespace OGL
