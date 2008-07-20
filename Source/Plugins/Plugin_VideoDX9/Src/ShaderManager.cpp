#include "D3DBase.h"
#include "Utils.h"
#include "Globals.h"
#include "ShaderManager.h"
#include "BPMemory.h"
#include "XFMemory.h"


//I hope we don't get too many hash collisions :p
//all these magic numbers are primes, it should help a bit
tevhash GetCurrentTEV()
{
	u32 hash = bpmem.genMode.numindstages + bpmem.genMode.numtevstages*11 + bpmem.genMode.numtexgens*8*17;
	for (int i = 0; i < (int)bpmem.genMode.numtevstages+1; i++)
	{
		hash = _rotl(hash,3) ^ (bpmem.combiners[i].colorC.hex*13);
		hash = _rotl(hash,7) ^ ((bpmem.combiners[i].alphaC.hex&0xFFFFFFFC)*3);
		hash = _rotl(hash,9) ^ xfregs.texcoords[i].texmtxinfo.projection*451;
	}
	for (int i = 0; i < (int)bpmem.genMode.numtevstages/2+1; i++)
	{
		hash = _rotl(hash,13) ^ (bpmem.tevorders[i].hex*7);
	}
	for (int i = 0; i < 8; i++)
	{
		hash = _rotl(hash,3) ^ bpmem.tevksel[i].swap1;
		hash = _rotl(hash,3) ^ bpmem.tevksel[i].swap2;
	}
	hash ^= bpmem.dstalpha.enable ^ 0xc0debabe;
	hash = _rotl(hash,4) ^ bpmem.alphaFunc.comp0*7;
	hash = _rotl(hash,4) ^ bpmem.alphaFunc.comp1*13;
	hash = _rotl(hash,4) ^ bpmem.alphaFunc.logic*11;
	return hash;
}


PShaderCache::PSCache PShaderCache::pshaders;
VShaderCache::VSCache VShaderCache::vshaders;

void PShaderCache::Init()
{


}


void PShaderCache::Shutdown()
{
	PSCache::iterator iter = pshaders.begin();
	for (;iter!=pshaders.end();iter++)
		iter->second.Destroy();
	pshaders.clear();
}


void PShaderCache::SetShader()
{
	if (D3D::GetShaderVersion() < 2)
		return; // we are screwed

	static LPDIRECT3DPIXELSHADER9 lastShader = 0;
    DVSTARTPROFILE();

	tevhash currentHash = GetCurrentTEV();

	PSCache::iterator iter;
	iter = pshaders.find(currentHash);
	
	if (iter != pshaders.end())
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

	const char *code = GeneratePixelShader();
	LPDIRECT3DPIXELSHADER9 shader = D3D::CompilePShader(code, strlen(code));
	if (shader)
	{
		//Make an entry in the table
		PSCacheEntry newentry;
		newentry.shader = shader;
		newentry.frameCount = frameCount;
		pshaders[currentHash] = newentry;
	}

	D3D::dev->SetPixelShader(shader);

	INCSTAT(stats.numPixelShadersCreated);
	SETSTAT(stats.numPixelShadersAlive, (int)pshaders.size());
}

void PShaderCache::Cleanup()
{
	PSCache::iterator iter;
	iter = pshaders.begin();

	while(iter != pshaders.end())
	{
		PSCacheEntry &entry = iter->second;
		if (entry.frameCount < frameCount-30)
		{
			entry.Destroy();
			iter = pshaders.erase(iter);
		}
		else
		{
		  iter++;
		}
	}
	SETSTAT(stats.numPixelShadersAlive, (int)pshaders.size());
}


void VShaderCache::Init()
{


}


void VShaderCache::Shutdown()
{
	VSCache::iterator iter = vshaders.begin();
	for (;iter!=vshaders.end();iter++)
		iter->second.Destroy();
	vshaders.clear();
}


void VShaderCache::SetShader()
{
	static LPDIRECT3DVERTEXSHADER9 lastShader = 0;
	xformhash currentHash = GetCurrentXForm();

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

	LPDIRECT3DVERTEXSHADER9 shader = GenerateVertexShader();

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
	for (VSCache::iterator iter=vshaders.begin(); iter!=vshaders.end();iter++)
	{
		VSCacheEntry &entry = iter->second;
		if (entry.frameCount<frameCount-30)
		{
			entry.Destroy();
			iter = vshaders.erase(iter);
		}
	}
	SETSTAT(stats.numPixelShadersAlive, (int)vshaders.size());
}
