// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include <set>

#include <d3dx9.h>

#include "Common.h"

namespace DX9
{

namespace D3D
{

// From http://developer.amd.com/gpu_assets/Advanced%20DX9%20Capabilities%20for%20ATI%20Radeon%20Cards.pdf
// Magic FourCC's to unlock undocumented D3D9 features:

// Z texture formats
#define FOURCC_INTZ ((D3DFORMAT)(MAKEFOURCC('I','N','T','Z')))
#define FOURCC_RAWZ ((D3DFORMAT)(MAKEFOURCC('R','A','W','Z')))
#define FOURCC_DF24 ((D3DFORMAT)(MAKEFOURCC('D','F','2','4')))
#define FOURCC_DF16 ((D3DFORMAT)(MAKEFOURCC('D','F','1','6')))

// Depth buffer resolve:
#define FOURCC_RESZ ((D3DFORMAT)(MAKEFOURCC('R','E','S','Z')))
#define RESZ_CODE 0x7fa05000

// Null render target to do Z-only shadow maps: (probably not useful for Dolphin)
#define FOURCC_NULL ((D3DFORMAT)(MAKEFOURCC('N','U','L','L')))

bool IsATIDevice();
bool IsIntelDevice();
HRESULT Init();
HRESULT Create(int adapter, HWND wnd, int resolution, int aa_mode, bool auto_depth);
void Close();
void Shutdown();

// Direct access to the device.
extern LPDIRECT3DDEVICE9 dev;
extern bool bFrameInProgress;

void Reset();
bool BeginFrame();
void EndFrame();
void Present();
bool CanUseINTZ();

int GetBackBufferWidth();
int GetBackBufferHeight();
LPDIRECT3DSURFACE9 GetBackBufferSurface();
LPDIRECT3DSURFACE9 GetBackBufferDepthSurface();
LPDIRECT3DVERTEXBUFFER9  GetquadVB();
LPDIRECT3DVERTEXDECLARATION9 GetBasicvertexDecl();
const D3DCAPS9 &GetCaps();
const char *PixelShaderVersionString();
const char *VertexShaderVersionString();
void ShowD3DError(HRESULT err);

// returns true if size was changed
bool FixTextureSize(int& width, int& height);

// returns true if format is supported
bool CheckTextureSupport(DWORD usage, D3DFORMAT tex_format);
bool CheckDepthStencilSupport(D3DFORMAT target_format, D3DFORMAT depth_format);

D3DFORMAT GetSupportedDepthTextureFormat();
D3DFORMAT GetSupportedDepthSurfaceFormat(D3DFORMAT target_format);

// The following are "filtered" versions of the corresponding D3Ddev-> functions.
void SetTexture(DWORD Stage, IDirect3DBaseTexture9 *pTexture);
void SetRenderState(D3DRENDERSTATETYPE State, DWORD Value);
void RefreshRenderState(D3DRENDERSTATETYPE State);
void ChangeRenderState(D3DRENDERSTATETYPE State, DWORD Value);

void SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value);
void RefreshTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type);
void ChangeTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value);

void SetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value);
void RefreshSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type);
void ChangeSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value);

void RefreshVertexDeclaration();
void SetVertexDeclaration(LPDIRECT3DVERTEXDECLARATION9 decl);
void ChangeVertexDeclaration(LPDIRECT3DVERTEXDECLARATION9 decl);

void RefreshVertexShader();
void SetVertexShader(LPDIRECT3DVERTEXSHADER9 shader);
void ChangeVertexShader(LPDIRECT3DVERTEXSHADER9 shader);

void RefreshPixelShader();
void SetPixelShader(LPDIRECT3DPIXELSHADER9 shader);
void ChangePixelShader(LPDIRECT3DPIXELSHADER9 shader);

void SetStreamSource(UINT StreamNumber,IDirect3DVertexBuffer9* pStreamData,UINT OffsetInBytes,UINT Stride);
void ChangeStreamSource(UINT StreamNumber,IDirect3DVertexBuffer9* pStreamData,UINT OffsetInBytes,UINT Stride);
void RefreshStreamSource(UINT StreamNumber);

void SetIndices(LPDIRECT3DINDEXBUFFER9 pIndexData);
void ChangeIndices(LPDIRECT3DINDEXBUFFER9 pIndexData);
void RefreshIndices();

void ApplyCachedState();

// Utility functions for vendor specific hacks. So far, just the one.
void EnableAlphaToCoverage();

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
	AALevel(const char *n, D3DMULTISAMPLE_TYPE m, int q) {
		strncpy(name, n, 32);
		name[31] = '\0';
		ms_setting = m; 
		qual_setting = q;
	}
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

	// Magic render targets, see the top of this file.
	bool supports_intz;
	bool supports_rawz;
	bool supports_resz;
	bool supports_null;
};

const Adapter &GetAdapter(int i);
const Adapter &GetCurAdapter();
int GetNumAdapters();

}  // namespace

}  // namespace DX9


// Used to not require the SDK and runtime versions to match:
// Linking with d3dx9.lib makes the most recent d3dx9_xx.dll of the
// compiler's SDK an actually unnecessary requirement.
// Add any d3dx9 functions which you want to use here and load them in LoadD3DX9()
typedef HRESULT (WINAPI* D3DXSAVESURFACETOFILEATYPE)(LPCSTR, D3DXIMAGE_FILEFORMAT, LPDIRECT3DSURFACE9, CONST PALETTEENTRY*, CONST RECT*);
typedef HRESULT (WINAPI* D3DXSAVETEXTURETOFILEATYPE)(LPCSTR, D3DXIMAGE_FILEFORMAT, LPDIRECT3DBASETEXTURE9, CONST PALETTEENTRY*);
typedef HRESULT (WINAPI* D3DXCOMPILESHADERTYPE)(LPCSTR, UINT, CONST D3DXMACRO*, LPD3DXINCLUDE, LPCSTR, LPCSTR, DWORD, LPD3DXBUFFER*, LPD3DXBUFFER*, LPD3DXCONSTANTTABLE*);

extern D3DXSAVESURFACETOFILEATYPE PD3DXSaveSurfaceToFileA;
extern D3DXSAVETEXTURETOFILEATYPE PD3DXSaveTextureToFileA;
extern D3DXCOMPILESHADERTYPE PD3DXCompileShader;
