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

#include "D3DBase.h"
#include "D3DShader.h"
#include "Statistics.h"
#include "Utils.h"
#include "Profiler.h"
#include "PixelShaderGen.h"
#include "PixelShaderManager.h"
#include "PixelShaderCache.h"
#include "VertexLoader.h"
#include "BPMemory.h"
#include "XFMemory.h"

#include <Cg/cg.h>
#include <Cg/cgD3D9.h>

PixelShaderCache::PSCache PixelShaderCache::PixelShaders;

void SetPSConstant4f(int const_number, float f1, float f2, float f3, float f4)
{
	const float f[4] = {f1, f2, f3, f4};
	D3D::dev->SetPixelShaderConstantF(const_number, f, 1);
}

void SetPSConstant4fv(int const_number, const float *f)
{
	D3D::dev->SetPixelShaderConstantF(const_number, f, 1);
}

void PixelShaderCache::Init()
{

}

void PixelShaderCache::Shutdown()
{
	PSCache::iterator iter = PixelShaders.begin();
	for (; iter != PixelShaders.end(); iter++)
		iter->second.Destroy();
	PixelShaders.clear(); 
}

void PixelShaderCache::SetShader()
{
	if (D3D::GetShaderVersion() < 2)
		return; // we are screwed

	static LPDIRECT3DPIXELSHADER9 lastShader = NULL;

	DVSTARTPROFILE();

	PIXELSHADERUID uid;
	GetPixelShaderId(uid, PixelShaderManager::GetTextureMask(), false, false);

	PSCache::iterator iter;
	iter = PixelShaders.find(uid);
	
	if (iter != PixelShaders.end())
	{
		iter->second.frameCount = frameCount;
		PSCacheEntry &entry = iter->second;
		if (!lastShader || entry.shader != lastShader)
		{
			D3D::dev->SetPixelShader(entry.shader);
			lastShader = entry.shader;
		}
		return;
	}

	const char *code = GeneratePixelShader(PixelShaderManager::GetTextureMask(), false, false);
	LPDIRECT3DPIXELSHADER9 shader = CompileCgShader(code);
	if (shader)
	{
		//Make an entry in the table
		PSCacheEntry newentry;
		newentry.shader = shader;
		newentry.frameCount = frameCount;
		PixelShaders[uid] = newentry;

		D3D::dev->SetFVF(NULL);
		D3D::dev->SetPixelShader(shader);

		INCSTAT(stats.numPixelShadersCreated);
		SETSTAT(stats.numPixelShadersAlive, (int)PixelShaders.size());
	} else
		PanicAlert("Failed to compile Pixel Shader:\n\n%s", code);
}

LPDIRECT3DPIXELSHADER9 PixelShaderCache::CompileCgShader(const char *pstrprogram) 
{
	//char stropt[64];
	//sprintf(stropt, "MaxLocalParams=256,MaxInstructions=%d", s_nMaxVertexInstructions);
	const char *opts[] = {"-profileopts", "MaxLocalParams=256", "-O2", "-q", NULL};

	//const char **opts = cgD3D9GetOptimalOptions(g_cgvProf);
	CGprogram tempprog = cgCreateProgram(g_cgcontext, CG_SOURCE, pstrprogram, g_cgfProf, "main", opts);
	if (!cgIsProgram(tempprog) || cgGetError() != CG_NO_ERROR) {
		ERROR_LOG(VIDEO, "Failed to create ps %s:\n", cgGetLastListing(g_cgcontext));
		ERROR_LOG(VIDEO, pstrprogram);
		return false;
	}

	// This looks evil - we modify the program through the const char * we got from cgGetProgramString!
	// It SHOULD not have any nasty side effects though - but you never know...
	char *pcompiledprog = (char*)cgGetProgramString(tempprog, CG_COMPILED_PROGRAM);
	LPD3DXBUFFER shader_binary;
	LPD3DXBUFFER error_msg;

	// Step one - Assemble into binary code. This binary code could be cached.
	if (FAILED(D3DXAssembleShader(pcompiledprog, (UINT)strlen(pcompiledprog), NULL, NULL, 0, &shader_binary, &error_msg)))
		PanicAlert("Asm fail");
	// Destroy Cg program as early as possible - we want as little as possible to do with Cg due to
	// our rather extreme performance requirements.
	cgDestroyProgram(tempprog);
	tempprog = NULL;

	// Create pixel shader from the binary code.
	LPDIRECT3DPIXELSHADER9 pixel_shader = NULL;
	if (SUCCEEDED(D3D::dev->CreatePixelShader((const DWORD *)shader_binary->GetBufferPointer(), &pixel_shader))) {
		// PanicAlert("Success Pixel!");
	} else {
		if (error_msg) {
			PanicAlert("failure pixel %s", error_msg->GetBufferPointer());
			MessageBox(0, pcompiledprog, 0, 0);
		}
		else
			PanicAlert("failure pixel with no error message.");
	}
	if (shader_binary)
		shader_binary->Release();
	if (error_msg)
		error_msg->Release();
	return pixel_shader;
}


void PixelShaderCache::Cleanup()
{
	PSCache::iterator iter;
	iter = PixelShaders.begin();
	while (iter != PixelShaders.end())
	{
		PSCacheEntry &entry = iter->second;
		if (entry.frameCount < frameCount-30)
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