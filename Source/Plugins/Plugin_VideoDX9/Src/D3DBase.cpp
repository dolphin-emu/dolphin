// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "D3DBase.h"
#include "VideoConfig.h"
#include "Render.h"
#include "XFStructs.h"
#include "StringUtil.h"
#include "VideoCommon.h"

D3DXSAVESURFACETOFILEATYPE PD3DXSaveSurfaceToFileA = NULL;
D3DXSAVETEXTURETOFILEATYPE PD3DXSaveTextureToFileA = NULL;
D3DXCOMPILESHADERTYPE PD3DXCompileShader = NULL;

namespace DX9
{
static char vsVersions[5][7] = {"ERROR", "vs_1_4", "vs_2_a", "vs_3_0", "vs_4_0"};
static char psVersions[5][7] = {"ERROR", "ps_1_4", "ps_2_a", "ps_3_0", "ps_4_0"};
// D3DX
HINSTANCE hD3DXDll = NULL;
int d3dx_dll_ref = 0;

typedef IDirect3D9* (WINAPI* DIRECT3DCREATE9)(UINT);
DIRECT3DCREATE9 PDirect3DCreate9 = NULL;
HINSTANCE hD3DDll = NULL;
int d3d_dll_ref = 0;

namespace D3D
{

LPDIRECT3D9        D3D = NULL; // Used to create the D3DDevice
LPDIRECT3DDEVICE9  dev = NULL; // Our rendering device
LPDIRECT3DSURFACE9 back_buffer;
LPDIRECT3DSURFACE9 back_buffer_z;
D3DCAPS9 caps;
HWND hWnd;

static int multisample;
static int resolution;
static int xres, yres;
static bool auto_depth_stencil = false;

#define VENDOR_NVIDIA 4318
#define VENDOR_ATI    4098
#define VENDOR_INTEL  32902

bool bFrameInProgress = false;

#define MAX_ADAPTERS 4
static Adapter adapters[MAX_ADAPTERS];
static int numAdapters;
static int cur_adapter;

// Value caches for state filtering
const int MaxStreamSources = 16;
const int MaxTextureStages = 9;
const int MaxRenderStates = 210 + 46;
const int MaxTextureTypes = 33;
const int MaxSamplerSize = 13;
const int MaxSamplerTypes = 15;
static bool m_RenderStatesSet[MaxRenderStates];
static DWORD m_RenderStates[MaxRenderStates];
static bool m_RenderStatesChanged[MaxRenderStates];

static DWORD m_TextureStageStates[MaxTextureStages][MaxTextureTypes];
static bool m_TextureStageStatesSet[MaxTextureStages][MaxTextureTypes];
static bool m_TextureStageStatesChanged[MaxTextureStages][MaxTextureTypes];

static DWORD m_SamplerStates[MaxSamplerSize][MaxSamplerTypes];
static bool m_SamplerStatesSet[MaxSamplerSize][MaxSamplerTypes];
static bool m_SamplerStatesChanged[MaxSamplerSize][MaxSamplerTypes];

static LPDIRECT3DBASETEXTURE9 m_Textures[16];
static LPDIRECT3DVERTEXDECLARATION9 m_VtxDecl;
static bool m_VtxDeclChanged;
static LPDIRECT3DPIXELSHADER9 m_PixelShader;
static bool m_PixelShaderChanged;
static LPDIRECT3DVERTEXSHADER9 m_VertexShader;
static bool m_VertexShaderChanged;
struct StreamSourceDescriptor
{
	LPDIRECT3DVERTEXBUFFER9 pStreamData;
	UINT OffsetInBytes;
	UINT Stride;
};
static StreamSourceDescriptor m_stream_sources[MaxStreamSources];
static bool m_stream_sources_Changed[MaxStreamSources];
static LPDIRECT3DINDEXBUFFER9 m_index_buffer;
static bool m_index_buffer_Changed;

// Z buffer formats to be used for EFB depth surface
D3DFORMAT DepthFormats[] = {
	FOURCC_INTZ,
	FOURCC_DF24,
	FOURCC_RAWZ,
	FOURCC_DF16,
	D3DFMT_D24X8,
	D3DFMT_D24X4S4,
	D3DFMT_D24S8,
	D3DFMT_D24FS8,
	D3DFMT_D32,		// too much precision, but who cares
	D3DFMT_D16,		// much lower precision, but better than nothing
	D3DFMT_D15S1,
};


void Enumerate();

int GetNumAdapters() { return numAdapters; }
const Adapter &GetAdapter(int i) { return adapters[i]; }
const Adapter &GetCurAdapter() { return adapters[cur_adapter]; }

bool IsATIDevice()
{
	return GetCurAdapter().ident.VendorId == VENDOR_ATI;
}
bool IsIntelDevice()
{
	return GetCurAdapter().ident.VendorId == VENDOR_INTEL;
}


HRESULT Init()
{
	if (d3d_dll_ref++ > 0) return S_OK;

	hD3DDll = LoadLibraryA("d3d9.dll");
	if (!hD3DDll)
	{
		MessageBoxA(NULL, "Failed to load d3d9.dll", "Critical error", MB_OK | MB_ICONERROR);
		return E_FAIL;
	}
	PDirect3DCreate9 = (DIRECT3DCREATE9)GetProcAddress(hD3DDll, "Direct3DCreate9");
	if (PDirect3DCreate9 == NULL) MessageBoxA(NULL, "GetProcAddress failed for Direct3DCreate9!", "Critical error", MB_OK | MB_ICONERROR);

	// Create the D3D object, which is needed to create the D3DDevice.
	D3D = PDirect3DCreate9(D3D_SDK_VERSION);
	if (!D3D)
	{
		--d3d_dll_ref;
		return E_FAIL;
	}

	// Init the caps structure using data from the currently selected device
	int adapter = g_Config.iAdapter;
	D3D->GetDeviceCaps((adapter >= 0 && adapter < std::min(MAX_ADAPTERS, numAdapters)) ? adapter : D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps);
	Enumerate();
	if(IsIntelDevice()){
		// Murder the a because Intel doesn't support 2.0a because the 'a' part was a ATI and Nvidia war going on
		psVersions[2][5] = '0';
		vsVersions[2][5] = '0';
	}

	return S_OK;
}

void Shutdown()
{
	if (!d3d_dll_ref) return;
	if (--d3d_dll_ref != 0) return;

	if (D3D) D3D->Release();
	D3D = NULL;

	if (hD3DDll) FreeLibrary(hD3DDll);
	PDirect3DCreate9 = NULL;
}

void EnableAlphaToCoverage()
{
	// Each vendor has their own specific little hack.
	if (GetCurAdapter().ident.VendorId == VENDOR_ATI)
		SetRenderState(D3DRS_POINTSIZE, (D3DFORMAT)MAKEFOURCC('A', '2', 'M', '1'));
	else
		SetRenderState(D3DRS_ADAPTIVETESS_Y, (D3DFORMAT)MAKEFOURCC('A', 'T', 'O', 'C'));
}

void InitPP(int adapter, int f, int aa_mode, D3DPRESENT_PARAMETERS *pp)
{
	ZeroMemory(pp, sizeof(D3DPRESENT_PARAMETERS));
	pp->hDeviceWindow = hWnd;
	if (auto_depth_stencil)
	{
		pp->EnableAutoDepthStencil = TRUE;
		pp->AutoDepthStencilFormat = D3DFMT_D24S8;
	} else {
		pp->EnableAutoDepthStencil = FALSE;
		pp->AutoDepthStencilFormat = D3DFMT_UNKNOWN;
	}
	pp->BackBufferFormat = D3DFMT_X8R8G8B8;
	if (aa_mode >= (int)adapters[adapter].aa_levels.size())
		aa_mode = 0;

	pp->MultiSampleType = adapters[adapter].aa_levels[aa_mode].ms_setting;
	pp->MultiSampleQuality = adapters[adapter].aa_levels[aa_mode].qual_setting;

	pp->Flags = auto_depth_stencil ? D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL : 0;

	if(g_Config.b3DVision)
	{
		xres = pp->BackBufferWidth  = adapters[adapter].resolutions[f].xres;
		yres = pp->BackBufferHeight = adapters[adapter].resolutions[f].yres;
	}
	else
	{
		RECT client;
		GetClientRect(hWnd, &client);
		xres = pp->BackBufferWidth  = client.right - client.left;
		yres = pp->BackBufferHeight = client.bottom - client.top;
	}
	pp->SwapEffect = D3DSWAPEFFECT_DISCARD;
	pp->PresentationInterval = g_Config.IsVSync() ? D3DPRESENT_INTERVAL_DEFAULT : D3DPRESENT_INTERVAL_IMMEDIATE;
	pp->Windowed = !g_Config.b3DVision;
}

void Enumerate()
{
	numAdapters = D3D->GetAdapterCount();

	for (int i = 0; i < std::min(MAX_ADAPTERS, numAdapters); i++)
	{
		Adapter &a = adapters[i];
		a.aa_levels.clear();
		a.resolutions.clear();
		D3D->GetAdapterIdentifier(i, 0, &a.ident);
		bool isNvidia = a.ident.VendorId == VENDOR_NVIDIA;

		// Add SuperSamples modes
		a.aa_levels.push_back(AALevel("None", D3DMULTISAMPLE_NONE, 0));
		a.aa_levels.push_back(AALevel("4x SSAA", D3DMULTISAMPLE_NONE, 0));
		a.aa_levels.push_back(AALevel("9x SSAA", D3DMULTISAMPLE_NONE, 0));
		//Add multisample modes
		//disable them will they are not implemnted
		/*
		DWORD qlevels = 0;
		if (D3DERR_NOTAVAILABLE != D3D->CheckDeviceMultiSampleType(
			i, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, TRUE, D3DMULTISAMPLE_2_SAMPLES, &qlevels))
			if (qlevels > 0)
				a.aa_levels.push_back(AALevel("2x MSAA", D3DMULTISAMPLE_2_SAMPLES, 0));

		if (D3DERR_NOTAVAILABLE != D3D->CheckDeviceMultiSampleType(
			i, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, TRUE, D3DMULTISAMPLE_2_SAMPLES, &qlevels))
			if (qlevels > 0)
				a.aa_levels.push_back(AALevel("4x MSAA", D3DMULTISAMPLE_4_SAMPLES, 0));

		if (D3DERR_NOTAVAILABLE != D3D->CheckDeviceMultiSampleType(
			i, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, TRUE, D3DMULTISAMPLE_8_SAMPLES, &qlevels))
			if (qlevels > 0)
				a.aa_levels.push_back(AALevel("8x MSAA", D3DMULTISAMPLE_8_SAMPLES, 0));

		if (isNvidia)
		{
			// CSAA support
			if (D3DERR_NOTAVAILABLE != D3D->CheckDeviceMultiSampleType(
				i, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, TRUE, D3DMULTISAMPLE_4_SAMPLES, &qlevels))
			{
				if (qlevels > 2)
				{
					// 8x, 8xQ are available
					// See http://developer.nvidia.com/object/coverage-sampled-aa.html
					a.aa_levels.push_back(AALevel("8x CSAA", D3DMULTISAMPLE_4_SAMPLES, 2));
					a.aa_levels.push_back(AALevel("8xQ CSAA", D3DMULTISAMPLE_8_SAMPLES, 0));
				}
			}
			if (D3DERR_NOTAVAILABLE != D3D->CheckDeviceMultiSampleType(
				i, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, TRUE, D3DMULTISAMPLE_8_SAMPLES, &qlevels))
			{
				if (qlevels > 2)
				{
					// 16x, 16xQ are available
					// See http://developer.nvidia.com/object/coverage-sampled-aa.html
					a.aa_levels.push_back(AALevel("16x CSAA", D3DMULTISAMPLE_4_SAMPLES, 4));
					a.aa_levels.push_back(AALevel("16xQ CSAA", D3DMULTISAMPLE_8_SAMPLES, 2));
				}
			}
		}
		*/
		// Determine if INTZ is supported. Code from ATI's doc.
		// http://developer.amd.com/gpu_assets/Advanced%20DX9%20Capabilities%20for%20ATI%20Radeon%20Cards.pdf
		a.supports_intz = D3D_OK == D3D->CheckDeviceFormat(
			i, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8,
			D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_TEXTURE, FOURCC_INTZ);
		// Also check for RAWZ (nvidia only, but the only option to get Z24 textures on sub GF8800
		a.supports_rawz = D3D_OK == D3D->CheckDeviceFormat(
			i, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8,
			D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_TEXTURE, FOURCC_RAWZ);
		// Might as well check for RESZ and NULL too.
		a.supports_resz = D3D_OK == D3D->CheckDeviceFormat(
			i, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8,
			D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_TEXTURE, FOURCC_RESZ);
		a.supports_null = D3D_OK == D3D->CheckDeviceFormat(
			i, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8,
			D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_TEXTURE, FOURCC_NULL);

		if (a.aa_levels.size() == 1)
		{
			strcpy(a.aa_levels[0].name, "(Not supported on this device)");
		}
		int numModes = D3D->GetAdapterModeCount(i, D3DFMT_X8R8G8B8);
		for (int m = 0; m < numModes; m++)
		{
			D3DDISPLAYMODE mode;
			D3D->EnumAdapterModes(i, D3DFMT_X8R8G8B8, m, &mode);

			int found = -1;
			for (int x = 0; x < (int)a.resolutions.size(); x++)
			{
				if (a.resolutions[x].xres == mode.Width && a.resolutions[x].yres == mode.Height)
				{
					found = x;
					break;
				}
			}

			Resolution temp;
			Resolution &r = found==-1 ? temp : a.resolutions[found];

			sprintf(r.name, "%ix%i", mode.Width, mode.Height);
			r.bitdepths.insert(mode.Format);
			r.refreshes.insert(mode.RefreshRate);
			if (found == -1 && mode.Width >= 640 && mode.Height >= 480)
			{
				r.xres = mode.Width;
				r.yres = mode.Height;
				a.resolutions.push_back(r);
			}
		}
	}
}

// dynamically picks one of the available d3dx9 dlls and loads it.
// we're first trying to load the dll Dolphin was compiled with, otherwise the most up-to-date one
HRESULT LoadD3DX9()
{
	if (d3dx_dll_ref++ > 0) return S_OK;

	HRESULT hr = E_FAIL;
	hD3DXDll = LoadLibraryA(StringFromFormat("d3dx9_%d.dll", D3DX_SDK_VERSION).c_str());
	if (hD3DXDll != NULL)
	{
		hr = S_OK;
	}
	else
	{
		// if that fails, try loading older dlls (no need to look for newer ones)
		for (unsigned int num = D3DX_SDK_VERSION-1; num >= 24; --num)
		{
			hD3DXDll = LoadLibraryA(StringFromFormat("d3dx9_%d.dll", num).c_str());
			if (hD3DXDll != NULL)
			{
				NOTICE_LOG(VIDEO, "Successfully loaded %s. If you're having trouble, try updating your DX runtime first.", StringFromFormat("d3dx9_%d.dll", num).c_str());
				hr = S_OK;
				break;
			}
		}
		if (FAILED(hr))
		{
			MessageBoxA(NULL, "Failed to load any D3DX9 dll, update your DX9 runtime, please", "Critical error", MB_OK | MB_ICONERROR);
			return hr;
		}
	}
	PD3DXCompileShader = (D3DXCOMPILESHADERTYPE)GetProcAddress(hD3DXDll, "D3DXCompileShader");
	if (PD3DXCompileShader == NULL)
	{
		MessageBoxA(NULL, "GetProcAddress failed for D3DXCompileShader!", "Critical error", MB_OK | MB_ICONERROR);
		goto fail;
	}

	PD3DXSaveSurfaceToFileA = (D3DXSAVESURFACETOFILEATYPE)GetProcAddress(hD3DXDll, "D3DXSaveSurfaceToFileA");
	if (PD3DXSaveSurfaceToFileA == NULL)
	{
		MessageBoxA(NULL, "GetProcAddress failed for D3DXSaveSurfaceToFileA!", "Critical error", MB_OK | MB_ICONERROR);
		goto fail;
	}

	PD3DXSaveTextureToFileA = (D3DXSAVETEXTURETOFILEATYPE)GetProcAddress(hD3DXDll, "D3DXSaveTextureToFileA");
	if (PD3DXSaveTextureToFileA == NULL)
	{
		MessageBoxA(NULL, "GetProcAddress failed for D3DXSaveTextureToFileA!", "Critical error", MB_OK | MB_ICONERROR);
		goto fail;
	}
	return S_OK;

fail:
	--d3dx_dll_ref;
	FreeLibrary(hD3DXDll);
	PD3DXCompileShader = NULL;
	PD3DXSaveSurfaceToFileA = NULL;
	PD3DXSaveTextureToFileA = NULL;
	return E_FAIL;
}

void UnloadD3DX9()
{
	if (!d3dx_dll_ref) return;
	if (--d3dx_dll_ref != 0) return;

	FreeLibrary(hD3DXDll);
	PD3DXCompileShader = NULL;
	PD3DXSaveSurfaceToFileA = NULL;
	PD3DXSaveTextureToFileA = NULL;
}

HRESULT Create(int adapter, HWND wnd, int _resolution, int aa_mode, bool auto_depth)
{
	hWnd = wnd;
	multisample = aa_mode;
	resolution = _resolution;
	auto_depth_stencil = auto_depth;
	cur_adapter = adapter;
	D3DPRESENT_PARAMETERS d3dpp;

	HRESULT hr = LoadD3DX9();
	if (FAILED(hr)) return hr;

	InitPP(adapter, resolution, aa_mode, &d3dpp);

	if (FAILED(D3D->CreateDevice(
		adapter,
		D3DDEVTYPE_HAL,
		wnd,
		D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE,  //doesn't seem to make a difference
		&d3dpp, &dev)))
	{
		if (FAILED(D3D->CreateDevice(
			adapter,
			D3DDEVTYPE_HAL,
			wnd,
			D3DCREATE_SOFTWARE_VERTEXPROCESSING,
			&d3dpp, &dev)))
		{
			MessageBox(wnd,
				_T("Failed to initialize Direct3D."),
				_T("Dolphin Direct3D Backend"), MB_OK | MB_ICONERROR);
			return E_FAIL;
		}
	}

	dev->GetDeviceCaps(&caps);
	dev->GetRenderTarget(0, &back_buffer);
	if (dev->GetDepthStencilSurface(&back_buffer_z) == D3DERR_NOTFOUND)
		back_buffer_z = NULL;
	SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE );
	SetRenderState(D3DRS_FILLMODE, g_Config.bWireFrame ? D3DFILL_WIREFRAME : D3DFILL_SOLID);
	memset(m_Textures, 0, sizeof(m_Textures));
	memset(m_TextureStageStatesSet, 0, sizeof(m_TextureStageStatesSet));
	memset(m_RenderStatesSet, 0, sizeof(m_RenderStatesSet));
	memset(m_SamplerStatesSet, 0, sizeof(m_SamplerStatesSet));
	memset(m_TextureStageStatesChanged, 0, sizeof(m_TextureStageStatesChanged));
	memset(m_RenderStatesChanged, 0, sizeof(m_RenderStatesChanged));
	memset(m_SamplerStatesChanged, 0, sizeof(m_SamplerStatesChanged));
	m_VtxDecl = NULL;
	m_PixelShader = NULL;
	m_VertexShader = NULL;
	m_index_buffer = NULL;
	memset(m_stream_sources, 0, sizeof(m_stream_sources));
	m_index_buffer = NULL;
	
	m_VtxDeclChanged = false;
	m_PixelShaderChanged = false;
	m_VertexShaderChanged = false;
	memset(m_stream_sources_Changed, 0 , sizeof(m_stream_sources_Changed));
	m_index_buffer_Changed = false;
	// Device state would normally be set here
	return S_OK;
}

void Close()
{
	UnloadD3DX9();

	if (back_buffer_z)
		back_buffer_z->Release();
	back_buffer_z = NULL;
	if( back_buffer )
		back_buffer->Release();
	back_buffer = NULL;

	ULONG references = dev->Release();
	if (references)
		ERROR_LOG(VIDEO, "Unreleased references: %i.", references);

	dev = NULL;
}

const D3DCAPS9 &GetCaps()
{
	return caps;
}

// returns true if size was changed
bool FixTextureSize(int& width, int& height)
{
	int oldw = width, oldh = height;

	// conditional nonpow2 support should work fine for us
	if ((caps.TextureCaps & D3DPTEXTURECAPS_POW2) && !(caps.TextureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL))
	{
		// all texture dimensions need to be powers of two
		width = (int)MakePow2((u32)width);
		height = (int)MakePow2((u32)height);
	}
	if (caps.TextureCaps & D3DPTEXTURECAPS_SQUAREONLY)
	{
		width = height = max(width, height);
	}

	width = min(width, (int)caps.MaxTextureWidth);
	height = min(height, (int)caps.MaxTextureHeight);

	return (width != oldw) || (height != oldh);
}

// returns true if format is supported
bool CheckTextureSupport(DWORD usage, D3DFORMAT tex_format)
{
	return D3D_OK == D3D->CheckDeviceFormat(cur_adapter, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, usage, D3DRTYPE_TEXTURE, tex_format);
}

bool CheckDepthStencilSupport(D3DFORMAT target_format, D3DFORMAT depth_format)
{
	return D3D_OK == D3D->CheckDepthStencilMatch(cur_adapter, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, target_format, depth_format);
}

D3DFORMAT GetSupportedDepthTextureFormat()
{
	for (int i = 0; i < sizeof(DepthFormats)/sizeof(D3DFORMAT); ++i)
		if (CheckTextureSupport(D3DUSAGE_DEPTHSTENCIL, DepthFormats[i]))
			return DepthFormats[i];

	return D3DFMT_UNKNOWN;
}

D3DFORMAT GetSupportedDepthSurfaceFormat(D3DFORMAT target_format)
{
	for (int i = 0; i < sizeof(DepthFormats)/sizeof(D3DFORMAT); ++i)
		if (CheckDepthStencilSupport(target_format, DepthFormats[i]))
			return DepthFormats[i];

	return D3DFMT_UNKNOWN;
}

const char *VertexShaderVersionString()
{
	int version = ((caps.VertexShaderVersion >> 8) & 0xFF);
	return vsVersions[std::min(4, version)];
}

const char *PixelShaderVersionString()
{
	int version = ((caps.PixelShaderVersion >> 8) & 0xFF);
	return psVersions[std::min(4, version)];
}

LPDIRECT3DSURFACE9 GetBackBufferSurface()
{
	return back_buffer;
}

LPDIRECT3DSURFACE9 GetBackBufferDepthSurface()
{
	return back_buffer_z;
}

void ShowD3DError(HRESULT err)
{
	switch (err)
	{
	case D3DERR_DEVICELOST:
		PanicAlert("Device Lost");
		break;
	case D3DERR_INVALIDCALL:
		PanicAlert("Invalid Call");
		break;
	case D3DERR_DRIVERINTERNALERROR:
		PanicAlert("Driver Internal Error");
		break;
	case D3DERR_OUTOFVIDEOMEMORY:
		PanicAlert("Out of vid mem");
		break;
	default:
		// MessageBox(0,_T("Other error or success"),_T("ERROR"),0);
		break;
	}
}

void Reset()
{
	if (dev)
	{
		// ForgetCachedState();

		// Can't keep a pointer around to the backbuffer surface when resetting.
		if (back_buffer_z)
			back_buffer_z->Release();
		back_buffer_z = NULL;
		back_buffer->Release();
		back_buffer = NULL;

		D3DPRESENT_PARAMETERS d3dpp;
		InitPP(cur_adapter, resolution, multisample, &d3dpp);
		HRESULT hr = dev->Reset(&d3dpp);
		ShowD3DError(hr);

		dev->GetRenderTarget(0, &back_buffer);
		if (dev->GetDepthStencilSurface(&back_buffer_z) == D3DERR_NOTFOUND)
			back_buffer_z = NULL;
		ApplyCachedState();
	}
}

int GetBackBufferWidth()
{
	return xres;
}

int GetBackBufferHeight()
{
	return yres;
}

bool BeginFrame()
{
	if (bFrameInProgress)
	{
		PanicAlert("BeginFrame WTF");
		return false;
	}
	bFrameInProgress = true;
	if (dev)
	{
		dev->BeginScene();
		return true;
	}
	else
		return false;
}

void EndFrame()
{
	if (!bFrameInProgress)
	{
		PanicAlert("EndFrame WTF");
		return;
	}
	bFrameInProgress = false;
	dev->EndScene();
}

void Present()
{
	if (dev)
	{
		dev->Present(NULL, NULL, NULL, NULL);
	}
}

void ApplyCachedState()
{
	for (int sampler = 0; sampler < 8; sampler++)
	{
		for (int type = 0; type < MaxSamplerTypes; type++)
		{
			if(m_SamplerStatesSet[sampler][type])
				dev->SetSamplerState(sampler, (D3DSAMPLERSTATETYPE)type, m_SamplerStates[sampler][type]);
		}
	}

	for (int rs = 0; rs < MaxRenderStates; rs++)
	{
		if (m_RenderStatesSet[rs])
			dev->SetRenderState((D3DRENDERSTATETYPE)rs, m_RenderStates[rs]);
	}

	// We don't bother restoring these so let's just wipe the state copy
	// so no stale state is around.
	memset(m_Textures, 0, sizeof(m_Textures));
	memset(m_TextureStageStatesSet, 0, sizeof(m_TextureStageStatesSet));
	memset(m_TextureStageStatesChanged, 0, sizeof(m_TextureStageStatesChanged));
	m_VtxDecl = NULL;
	m_PixelShader = NULL;
	m_VertexShader = NULL;
	memset(m_stream_sources, 0, sizeof(m_stream_sources));
	m_index_buffer = NULL;
	m_VtxDeclChanged = false;
	m_PixelShaderChanged = false;
	m_VertexShaderChanged = false;
	memset(m_stream_sources_Changed, 0 , sizeof(m_stream_sources_Changed));
	m_index_buffer_Changed = false;
}

void SetTexture(DWORD Stage, LPDIRECT3DBASETEXTURE9 pTexture)
{
	if (m_Textures[Stage] != pTexture)
	{
		m_Textures[Stage] = pTexture;
		dev->SetTexture(Stage, pTexture);
	}
}

void RefreshRenderState(D3DRENDERSTATETYPE State)
{
	if(m_RenderStatesSet[State] && m_RenderStatesChanged[State])
	{
		dev->SetRenderState(State, m_RenderStates[State]);
		m_RenderStatesChanged[State] = false;
	}
}

void SetRenderState(D3DRENDERSTATETYPE State, DWORD Value)
{
	if (m_RenderStates[State] != Value || !m_RenderStatesSet[State])
	{
		m_RenderStates[State] = Value;
		m_RenderStatesSet[State] = true;
		m_RenderStatesChanged[State] = false;
		dev->SetRenderState(State, Value);
	}
}

void ChangeRenderState(D3DRENDERSTATETYPE State, DWORD Value)
{
	if (m_RenderStates[State] != Value || !m_RenderStatesSet[State])
	{
		m_RenderStatesChanged[State] = m_RenderStatesSet[State];
		dev->SetRenderState(State, Value);
	}
	else
	{
		m_RenderStatesChanged[State] = false;
	}
}

void SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value)
{
	if (m_TextureStageStates[Stage][Type] != Value || !m_TextureStageStatesSet[Stage][Type])
	{
		m_TextureStageStates[Stage][Type] = Value;
		m_TextureStageStatesSet[Stage][Type]=true;
		m_TextureStageStatesChanged[Stage][Type]=false;
		dev->SetTextureStageState(Stage, Type, Value);
	}
}

void RefreshTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type)
{
	if(m_TextureStageStatesSet[Stage][Type] && m_TextureStageStatesChanged[Stage][Type])
	{
		dev->SetTextureStageState(Stage, Type, m_TextureStageStates[Stage][Type]);
		m_TextureStageStatesChanged[Stage][Type] = false;
	}
}

void ChangeTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value)
{
	if (m_TextureStageStates[Stage][Type] != Value || !m_TextureStageStatesSet[Stage][Type])
	{
		m_TextureStageStatesChanged[Stage][Type] = m_TextureStageStatesSet[Stage][Type];
		dev->SetTextureStageState(Stage, Type, Value);
	}
	else
	{
		m_TextureStageStatesChanged[Stage][Type] = false;
	}
}

void SetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value)
{
	if (m_SamplerStates[Sampler][Type] != Value || !m_SamplerStatesSet[Sampler][Type])
	{
		m_SamplerStates[Sampler][Type] = Value;
		m_SamplerStatesSet[Sampler][Type] = true;
		m_SamplerStatesChanged[Sampler][Type] = false;
		dev->SetSamplerState(Sampler, Type, Value);
	}
}

void RefreshSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type)
{
	if(m_SamplerStatesSet[Sampler][Type] && m_SamplerStatesChanged[Sampler][Type])
	{
		dev->SetSamplerState(Sampler, Type, m_SamplerStates[Sampler][Type]);
		m_SamplerStatesChanged[Sampler][Type] = false;
	}
}

void ChangeSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value)
{
	if (m_SamplerStates[Sampler][Type] != Value || !m_SamplerStatesSet[Sampler][Type])
	{
		m_SamplerStatesChanged[Sampler][Type] = m_SamplerStatesSet[Sampler][Type];
		dev->SetSamplerState(Sampler, Type, Value);
	}
	else
	{
		m_SamplerStatesChanged[Sampler][Type] = false;
	}
}

void RefreshVertexDeclaration()
{
	if (m_VtxDeclChanged)
	{
		dev->SetVertexDeclaration(m_VtxDecl);
		m_VtxDeclChanged = false;
	}
}

void SetVertexDeclaration(LPDIRECT3DVERTEXDECLARATION9 decl)
{
	if (decl != m_VtxDecl)
	{
		dev->SetVertexDeclaration(decl);
		m_VtxDecl = decl;
		m_VtxDeclChanged = false;
	}
}

void ChangeVertexDeclaration(LPDIRECT3DVERTEXDECLARATION9 decl)
{
	if (decl != m_VtxDecl) {
		dev->SetVertexDeclaration(decl);
		m_VtxDeclChanged = true;
	}
}

void ChangeVertexShader(LPDIRECT3DVERTEXSHADER9 shader)
{
	if (shader != m_VertexShader)
	{
		dev->SetVertexShader(shader);
		m_VertexShaderChanged = true;
	}
}

void RefreshVertexShader()
{
	if (m_VertexShaderChanged)
	{
		dev->SetVertexShader(m_VertexShader);
		m_VertexShaderChanged = false;
	}
}

void SetVertexShader(LPDIRECT3DVERTEXSHADER9 shader)
{
	if (shader != m_VertexShader)
	{
		dev->SetVertexShader(shader);
		m_VertexShader = shader;
		m_VertexShaderChanged = false;
	}
}

void RefreshPixelShader()
{
	if (m_PixelShaderChanged)
	{
		dev->SetPixelShader(m_PixelShader);
		m_PixelShaderChanged = false;
	}
}

void SetPixelShader(LPDIRECT3DPIXELSHADER9 shader)
{
	if (shader != m_PixelShader)
	{
		dev->SetPixelShader(shader);
		m_PixelShader = shader;
		m_PixelShaderChanged = false;
	}
}

void ChangePixelShader(LPDIRECT3DPIXELSHADER9 shader)
{
	if (shader != m_PixelShader)
	{
		dev->SetPixelShader(shader);
		m_PixelShaderChanged = true;
	}
}

void SetStreamSource(UINT StreamNumber,IDirect3DVertexBuffer9* pStreamData,UINT OffsetInBytes,UINT Stride)
{
	if (m_stream_sources[StreamNumber].OffsetInBytes != OffsetInBytes
		|| m_stream_sources[StreamNumber].pStreamData != pStreamData
		|| m_stream_sources[StreamNumber].Stride != Stride)
	{
		m_stream_sources[StreamNumber].OffsetInBytes = OffsetInBytes;
		m_stream_sources[StreamNumber].pStreamData = pStreamData;
		m_stream_sources[StreamNumber].Stride = Stride;
		dev->SetStreamSource(StreamNumber, pStreamData, OffsetInBytes, Stride);
		m_stream_sources_Changed[StreamNumber] = false;
	}
}

void ChangeStreamSource(UINT StreamNumber,IDirect3DVertexBuffer9* pStreamData,UINT OffsetInBytes,UINT Stride)
{
	if (m_stream_sources[StreamNumber].OffsetInBytes != OffsetInBytes
		|| m_stream_sources[StreamNumber].pStreamData != pStreamData
		|| m_stream_sources[StreamNumber].Stride != Stride)
	{
		dev->SetStreamSource(StreamNumber, pStreamData, OffsetInBytes, Stride);
		m_stream_sources_Changed[StreamNumber] = true;
	}
	
}

void RefreshStreamSource(UINT StreamNumber)
{
	if (m_PixelShaderChanged)
	{
		dev->SetStreamSource(
			StreamNumber, 
			m_stream_sources[StreamNumber].pStreamData, 
			m_stream_sources[StreamNumber].OffsetInBytes, 
			m_stream_sources[StreamNumber].Stride);
		m_stream_sources_Changed[StreamNumber] = false;
	}
}

void SetIndices(LPDIRECT3DINDEXBUFFER9 pIndexData)
{
	if(pIndexData != m_index_buffer)
	{
		m_index_buffer = pIndexData;
		dev->SetIndices(pIndexData);
		m_index_buffer_Changed = false;
	}
}

void ChangeIndices(LPDIRECT3DINDEXBUFFER9 pIndexData)
{
	if(pIndexData != m_index_buffer)
	{
		dev->SetIndices(pIndexData);
		m_index_buffer_Changed = true;
	}
}

void RefreshIndices()
{
	if (m_index_buffer_Changed)
	{
		dev->SetIndices(m_index_buffer);
		m_index_buffer_Changed = false;
	}
}



}  // namespace

}  // namespace DX9
