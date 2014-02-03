// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Null/ShaderCache.h"

#include "VideoCommon/Debugger.h"
#include "VideoCommon/Statistics.h"

namespace Null
{

template <typename Uid>
ShaderCache<Uid>::ShaderCache()
{
	Clear();

	SETSTAT(stats.numPixelShadersCreated, 0);
	SETSTAT(stats.numPixelShadersAlive, 0);

}

template <typename Uid>
ShaderCache<Uid>::~ShaderCache()
{
	Clear();
}

template <typename Uid>
void ShaderCache<Uid>::Clear()
{
	s_shaders.clear();
	s_last_entry = nullptr;
}

template <typename Uid>
bool ShaderCache<Uid>::SetShader(DSTALPHA_MODE dstAlphaMode, u32 primitive_type)
{
	Uid uid = GetUid(dstAlphaMode, primitive_type, API_OPENGL);

	// Check if the shader is already set
	if (s_last_entry)
	{
		if (uid == s_last_uid)
		{
			return true;
		}
	}

	s_last_uid = uid;

	// Check if the shader is already in the cache
	typename SCache::iterator iter;
	iter = s_shaders.find(uid);
	if (iter != s_shaders.end())
	{
		const std::string &entry = iter->second;
		s_last_entry = &entry;

		GFX_DEBUGGER_PAUSE_AT(NEXT_PIXEL_SHADER_CHANGE,true);
		return true;
	}

	// Need to compile a new shader
	ShaderCode code = GenerateCode(dstAlphaMode, primitive_type, API_OPENGL);
	s_shaders.insert(std::make_pair(uid, code.GetBuffer()));

	GFX_DEBUGGER_PAUSE_AT(NEXT_PIXEL_SHADER_CHANGE, true);
	return true;
}

template class ShaderCache<VertexShaderUid>;
template class ShaderCache<GeometryShaderUid>;
template class ShaderCache<PixelShaderUid>;

std::unique_ptr<VertexShaderCache> VertexShaderCache::s_instance;
std::unique_ptr<GeometryShaderCache> GeometryShaderCache::s_instance;
std::unique_ptr<PixelShaderCache> PixelShaderCache::s_instance;

}
