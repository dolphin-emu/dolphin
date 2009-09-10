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

#include "D3DBase.h"
#include "D3DShader.h"
#include "Statistics.h"
#include "Utils.h"
#include "Profiler.h"
#include "Config.h"
#include "PixelShaderGen.h"
#include "PixelShaderManager.h"
#include "PixelShaderCache.h"
#include "VertexLoader.h"
#include "BPMemory.h"
#include "XFMemory.h"

#include "debugger/debugger.h"

PixelShaderCache::PSCache PixelShaderCache::PixelShaders;
const PixelShaderCache::PSCacheEntry *PixelShaderCache::last_entry;
static float lastPSconstants[C_COLORMATRIX+16][4];

void SetPSConstant4f(int const_number, float f1, float f2, float f3, float f4)
{
	if( lastPSconstants[const_number][0] != f1 || lastPSconstants[const_number][1] != f2 ||
		lastPSconstants[const_number][2] != f3 || lastPSconstants[const_number][3] != f4 )
	{
		const float f[4] = {f1, f2, f3, f4};
		D3D::dev->SetPixelShaderConstantF(const_number, f, 1);
		lastPSconstants[const_number][0] = f1;
		lastPSconstants[const_number][1] = f2;
		lastPSconstants[const_number][2] = f3;
		lastPSconstants[const_number][3] = f4;
	}
}

void SetPSConstant4fv(int const_number, const float *f)
{
	if( lastPSconstants[const_number][0] != f[0] || lastPSconstants[const_number][1] != f[1] ||
		lastPSconstants[const_number][2] != f[2] || lastPSconstants[const_number][3] != f[3] )
	{
		D3D::dev->SetPixelShaderConstantF(const_number, f, 1);
		lastPSconstants[const_number][0] = f[0];
		lastPSconstants[const_number][1] = f[1];
		lastPSconstants[const_number][2] = f[2];
		lastPSconstants[const_number][3] = f[3];
	}
}

void PixelShaderCache::Init()
{
	//memset(lastPSconstants,0xFF,(C_COLORMATRIX+16)*4*sizeof(float));	// why does this not work
	//memset(lastPSconstants,0xFF,sizeof(lastPSconstants));
	for( int i=0;i<(C_COLORMATRIX+16)*4;i++)
		lastPSconstants[i/4][i%4] = -100000000.0f;
	memset(&last_pixel_shader_uid,0xFF,sizeof(last_pixel_shader_uid));
}

void PixelShaderCache::Shutdown()
{
	PSCache::iterator iter = PixelShaders.begin();
	for (; iter != PixelShaders.end(); iter++)
		iter->second.Destroy();
	PixelShaders.clear(); 
}

bool PixelShaderCache::SetShader(bool dstAlpha)
{
	DVSTARTPROFILE();

	PIXELSHADERUID uid;
	GetPixelShaderId(uid, PixelShaderManager::GetTextureMask(), dstAlpha);
	if (uid == last_pixel_shader_uid)
	{
		if (PixelShaders[uid].shader)
			return true;
		else
			return false;
	}

	memcpy(&last_pixel_shader_uid, &uid, sizeof(PIXELSHADERUID));
	
	PSCache::iterator iter;
	iter = PixelShaders.find(uid);
	if (iter != PixelShaders.end())
	{
		iter->second.frameCount = frameCount;
		const PSCacheEntry &entry = iter->second;
		last_entry = &entry;
		

		DEBUGGER_PAUSE_COUNT_N(NEXT_PIXEL_SHADER_CHANGE);

		if (entry.shader)
		{
			D3D::dev->SetPixelShader(entry.shader);
			return true;
		}
		else
			return false;
	}

	const char *code = GeneratePixelShader(PixelShaderManager::GetTextureMask(), dstAlpha, true);
	LPDIRECT3DPIXELSHADER9 shader = D3D::CompilePixelShader(code, (int)strlen(code));

	// Make an entry in the table
	PSCacheEntry newentry;
	newentry.shader = shader;
	newentry.frameCount = frameCount;
#if defined(_DEBUG) || defined(DEBUGFAST)
	newentry.code = code;
#endif
	PixelShaders[uid] = newentry;
	last_entry = &PixelShaders[uid];

	INCSTAT(stats.numPixelShadersCreated);
	SETSTAT(stats.numPixelShadersAlive, (int)PixelShaders.size());

	if (shader)
	{
		D3D::dev->SetPixelShader(shader);
		return true;
	}
	
	if (g_Config.bShowShaderErrors)
	{
		PanicAlert("Failed to compile Pixel Shader:\n\n%s", code);
	}
	return false;
}


void PixelShaderCache::Cleanup()
{
	PSCache::iterator iter;
	iter = PixelShaders.begin();
	while (iter != PixelShaders.end())
	{
		PSCacheEntry &entry = iter->second;
		if (entry.frameCount < frameCount - 1400)
		{
			entry.Destroy();
			iter = PixelShaders.erase(iter);
		}
		else
		{
			iter++;
		}
	}
	SETSTAT(stats.numPixelShadersAlive, (int)PixelShaders.size());
}

#if defined(_DEBUG) || defined(DEBUGFAST)
std::string PixelShaderCache::GetCurrentShaderCode()
{
	if (last_entry)
		return last_entry->code;
	else
		return "(no shader)\n";
}
#endif