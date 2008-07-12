#ifndef _D3DBASE_H
#define _D3DBASE_H

#include <d3d9.h>

#include <vector>
#include <set>

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if(p) { (p)->Release(); (p)=NULL; } }
#endif

#include "Common.h"

namespace D3D
{
	enum ShaderVersion
	{
		PSNONE=0,
		PS20=2,
		PS30,
		PS40,
	};

	HRESULT Init();
	HRESULT Create(int adapter, HWND wnd, bool fullscreen, int resolution, int aa_mode);
	void Close();
	void Shutdown();

	void Reset();
	bool BeginFrame(bool clear=true, u32 color=0, float z=1.0f);
	void EndFrame();
	void SwitchFullscreen(bool fullscreen);
	bool IsFullscreen();
	int GetDisplayWidth();
	int GetDisplayHeight();
	ShaderVersion GetShaderVersion();
	LPDIRECT3DSURFACE9 GetBackBufferSurface();
	const D3DCAPS9 &GetCaps();
	extern IDirect3DDevice9 *dev;
	void ShowD3DError(HRESULT err);
	void EnableAlphaToCoverage();

	extern int psMajor;
	extern int psMinor;
	extern int vsMajor;
	extern int vsMinor;

	struct Resolution
	{
		char name[32];
		int xres;
		int yres;
		std::set<D3DFORMAT> bitdepths;
		std::set<int> refreshes;
	};

	struct AALevel
	{
		AALevel(const char *n, D3DMULTISAMPLE_TYPE m, int q) {strcpy(name, n); ms_setting=m; qual_setting=q;}
	    char name[32];
		D3DMULTISAMPLE_TYPE ms_setting;
		int qual_setting;
	};

	struct Adapter
	{
		D3DADAPTER_IDENTIFIER9 ident;
		std::vector<Resolution> resolutions;
		std::vector<AALevel> aa_levels;
		bool supports_alpha_to_coverage;
	};
	
	const Adapter &GetAdapter(int i);
	const Adapter &GetCurAdapter();
	int GetNumAdapters();
}

#endif