// Copyright (C) 2003-2008 Dolphin Project.

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

#include <map>

#include "D3DBase.h"
#include "Statistics.h"
#include "Utils.h"
#include "Profiler.h"
#include "VertexShaderCache.h"
#include "VertexLoader.h"
#include "BPMemory.h"
#include "XFMemory.h"

VShaderCache::VSCache VShaderCache::vshaders;

void VShaderCache::Init()
{

}


void VShaderCache::Shutdown()
{
	VSCache::iterator iter = vshaders.begin();
	for (; iter != vshaders.end(); iter++)
		iter->second.Destroy();
	vshaders.clear();
}


void VShaderCache::SetShader()
{
	static LPDIRECT3DVERTEXSHADER9 shader = NULL;
	if (D3D::GetShaderVersion() < 2)
		return; // we are screwed

	if (shader) {
		//D3D::dev->SetVertexShader(shader);
		return;
	}
	
	static LPDIRECT3DVERTEXSHADER9 lastShader = 0;
	DVSTARTPROFILE();

	u32 currentHash = 0x1337; // GetCurrentTEV();

	VSCache::iterator iter;
	iter = vshaders.find(currentHash);

	if (iter != vshaders.end())
	{
		iter->second.frameCount=frameCount;
		VSCacheEntry &entry = iter->second;
		if (!lastShader || entry.shader != lastShader)
		{
			D3D::dev->SetVertexShader(entry.shader);
			lastShader = entry.shader;
		}
		return;
	}

	const char *code = GenerateVertexShader();
	shader = D3D::CompileVShader(code, int(strlen(code)));
	if (shader)
	{
		//Make an entry in the table
		VSCacheEntry entry;
		entry.shader = shader;
		entry.frameCount=frameCount;
		vshaders[currentHash] = entry;
	}

	D3D::dev->SetVertexShader(shader);

	INCSTAT(stats.numVertexShadersCreated);
	SETSTAT(stats.numVertexShadersAlive, (int)vshaders.size());
}

void VShaderCache::Cleanup()
{
	for (VSCache::iterator iter = vshaders.begin(); iter != vshaders.end();)
	{
		VSCacheEntry &entry = iter->second;
		if (entry.frameCount < frameCount - 30)
		{
			entry.Destroy();
			iter = vshaders.erase(iter);
		}
		else
		{
			++iter;
		}
	}
	SETSTAT(stats.numVertexShadersAlive, (int)vshaders.size());
}
