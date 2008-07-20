#pragma once

#include "D3DBase.h"

#include <map>
#include "PixelShader.h"
#include "VertexShader.h"

typedef u32 tevhash;

tevhash GetCurrentTEV();


class PShaderCache
{
	struct PSCacheEntry
	{
		LPDIRECT3DPIXELSHADER9 shader;
		//CGPShader shader;

		int frameCount;
		PSCacheEntry()
		{
			shader=0;
			frameCount=0;
		}
		void Destroy()
		{
			if (shader)
				shader->Release();
		}
	};


	typedef std::map<tevhash,PSCacheEntry> PSCache;

	static PSCache pshaders;

public:
	static void Init();
	static void Cleanup();
	static void Shutdown();
	static void SetShader();
};



class VShaderCache
{
	struct VSCacheEntry
	{ 
		LPDIRECT3DVERTEXSHADER9 shader;
		//CGVShader shader;
		int frameCount;
		VSCacheEntry()
		{
			shader=0;
			frameCount=0;
		}
		void Destroy()
		{
			if (shader)
				shader->Release();
		}
	};

	typedef std::map<xformhash,VSCacheEntry> VSCache;

	static VSCache vshaders;


public:
	static void Init();
	static void Cleanup();
	static void Shutdown();
	static void SetShader();
};

void InitCG();
void ShutdownCG();