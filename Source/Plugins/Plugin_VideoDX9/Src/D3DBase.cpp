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
#include "VideoConfig.h"
#include "Render.h"
#include "XFStructs.h"

namespace D3D
{

LPDIRECT3D9        D3D = NULL; // Used to create the D3DDevice
LPDIRECT3DDEVICE9  dev = NULL; // Our rendering device
LPDIRECT3DSURFACE9 back_buffer;
LPDIRECT3DSURFACE9 back_buffer_z;
D3DCAPS9 caps;
HWND hWnd;

static bool fullScreen = false;
static bool nextFullScreen = false;
static int multisample;
static int resolution;
static int xres, yres;
static bool auto_depth_stencil = false;

#define VENDOR_NVIDIA 4318
#define VENDOR_ATI    4098

bool bFrameInProgress = false;

#define MAX_ADAPTERS 4
static Adapter adapters[MAX_ADAPTERS];
static int numAdapters;
static int cur_adapter;

// Value caches for state filtering
const int MaxTextureStages = 9;
const int MaxRenderStates = 210 + 46;
const int MaxTextureTypes = 33;
const int MaxSamplerSize = 13;
const int MaxSamplerTypes = 15;
static bool m_RenderStatesSet[MaxRenderStates];
static DWORD m_RenderStates[MaxRenderStates];
static DWORD m_TextureStageStates[MaxTextureStages][MaxTextureTypes];
static DWORD m_SamplerStates[MaxSamplerSize][MaxSamplerTypes];
LPDIRECT3DBASETEXTURE9 m_Textures[16];
LPDIRECT3DVERTEXDECLARATION9 m_VtxDecl;

void Enumerate();

int GetNumAdapters() { return numAdapters; }
const Adapter &GetAdapter(int i) { return adapters[i]; }
const Adapter &GetCurAdapter() { return adapters[cur_adapter]; }

HRESULT Init()
{
	// Create the D3D object, which is needed to create the D3DDevice.
	D3D = Direct3DCreate9(D3D_SDK_VERSION);
	if (!D3D)
		return E_FAIL;
	Enumerate();
	return S_OK;
}

void Shutdown()
{
	D3D->Release();
	D3D = 0;
}

void EnableAlphaToCoverage()
{
	// Each vendor has their own specific little hack.
	if (GetCurAdapter().ident.VendorId == VENDOR_ATI)
		D3D::SetRenderState(D3DRS_POINTSIZE, (D3DFORMAT)MAKEFOURCC('A', '2', 'M', '1'));
	else
		D3D::SetRenderState(D3DRS_ADAPTIVETESS_Y, (D3DFORMAT)MAKEFOURCC('A', 'T', 'O', 'C'));			
}

void InitPP(int adapter, int f, int aa_mode, D3DPRESENT_PARAMETERS *pp)
{
	int FSResX = adapters[adapter].resolutions[resolution].xres; 
	int FSResY = adapters[adapter].resolutions[resolution].yres;

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
	pp->BackBufferFormat = D3DFMT_A8R8G8B8;
	if (aa_mode >= (int)adapters[adapter].aa_levels.size())
		aa_mode = 0;

	pp->MultiSampleType = adapters[adapter].aa_levels[aa_mode].ms_setting;
	pp->MultiSampleQuality = adapters[adapter].aa_levels[aa_mode].qual_setting;

	pp->Flags = auto_depth_stencil ? D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL : 0;
	if (fullScreen)
	{
		xres = pp->BackBufferWidth = FSResX;
		yres = pp->BackBufferHeight = FSResY;	
		pp->SwapEffect = D3DSWAPEFFECT_DISCARD;
		pp->Windowed = FALSE;
		//if(g_Config.bHideCursor)
		if(!g_Config.RenderToMainframe)
			ShowCursor(FALSE);
	}
	else
	{
		RECT client;
		GetClientRect(hWnd, &client);
		xres = pp->BackBufferWidth  = client.right - client.left;
		yres = pp->BackBufferHeight = client.bottom - client.top;
		pp->SwapEffect = D3DSWAPEFFECT_DISCARD;
		pp->PresentationInterval = g_Config.bVSync ? D3DPRESENT_INTERVAL_DEFAULT : D3DPRESENT_INTERVAL_IMMEDIATE;
		pp->Windowed = TRUE;
		ShowCursor(TRUE);
	}
}

void Enumerate()
{
	numAdapters = D3D::D3D->GetAdapterCount();

	for (int i = 0; i < std::min(MAX_ADAPTERS, numAdapters); i++)
	{
		Adapter &a = adapters[i];
		a.aa_levels.clear();
		a.resolutions.clear();
		D3D::D3D->GetAdapterIdentifier(i, 0, &a.ident);
		bool isNvidia = a.ident.VendorId == VENDOR_NVIDIA;

		// Add multisample modes
		a.aa_levels.push_back(AALevel("None", D3DMULTISAMPLE_NONE, 0));

		DWORD qlevels = 0;
		if (D3DERR_NOTAVAILABLE != D3D::D3D->CheckDeviceMultiSampleType(
			i, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, TRUE, D3DMULTISAMPLE_2_SAMPLES, &qlevels))
			if (qlevels > 0)
				a.aa_levels.push_back(AALevel("2x MSAA", D3DMULTISAMPLE_2_SAMPLES, 0));
		
		if (D3DERR_NOTAVAILABLE != D3D::D3D->CheckDeviceMultiSampleType(
			i, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, TRUE, D3DMULTISAMPLE_2_SAMPLES, &qlevels))
			if (qlevels > 0)
				a.aa_levels.push_back(AALevel("4x MSAA", D3DMULTISAMPLE_4_SAMPLES, 0));

		if (D3DERR_NOTAVAILABLE != D3D::D3D->CheckDeviceMultiSampleType(
			i, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, TRUE, D3DMULTISAMPLE_8_SAMPLES, &qlevels))
			if (qlevels > 0)
				a.aa_levels.push_back(AALevel("8x MSAA", D3DMULTISAMPLE_8_SAMPLES, 0));

		if (isNvidia)
		{
			// CSAA support
			if (D3DERR_NOTAVAILABLE != D3D::D3D->CheckDeviceMultiSampleType(
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
			if (D3DERR_NOTAVAILABLE != D3D::D3D->CheckDeviceMultiSampleType(
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
		int numModes = D3D::D3D->GetAdapterModeCount(i, D3DFMT_X8R8G8B8);
		for (int m = 0; m < numModes; m++)
		{	
			D3DDISPLAYMODE mode;
			D3D::D3D->EnumAdapterModes(i, D3DFMT_X8R8G8B8, m, &mode);
			
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

HRESULT Create(int adapter, HWND wnd, bool _fullscreen, int _resolution, int aa_mode, bool auto_depth)
{
	hWnd = wnd;
	fullScreen = _fullscreen;
	nextFullScreen = _fullscreen;
	multisample = aa_mode;
	resolution = _resolution;
	auto_depth_stencil = auto_depth;
	cur_adapter = adapter;
	D3DPRESENT_PARAMETERS d3dpp; 
	InitPP(adapter, resolution, aa_mode, &d3dpp);

	if (FAILED(D3D->CreateDevice( 
		adapter, 
		D3DDEVTYPE_HAL, 
		wnd,
		D3DCREATE_HARDWARE_VERTEXPROCESSING, // | D3DCREATE_PUREDEVICE,  doesn't seem to make a difference
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
				_T("Dolphin Direct3D plugin"), MB_OK | MB_ICONERROR);
			return E_FAIL;
		}
	}

	dev->GetDeviceCaps(&caps);
	dev->GetRenderTarget(0, &back_buffer);
	if (dev->GetDepthStencilSurface(&back_buffer_z) == D3DERR_NOTFOUND)
		back_buffer_z = NULL;
	dev->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE );
	
	dev->SetRenderState(D3DRS_FILLMODE, g_Config.bWireFrame ? D3DFILL_WIREFRAME : D3DFILL_SOLID);

	// Device state would normally be set here
	return S_OK;
}

void Close()
{
	dev->Release(); 
	dev = 0;
}

const D3DCAPS9 &GetCaps()
{
	return caps;
}

const char *VertexShaderVersionString()
{
	static const char *versions[5] = {"ERROR", "vs_1_4", "vs_2_0", "vs_3_0", "vs_4_0"};
	int version = ((D3D::caps.VertexShaderVersion >> 8) & 0xFF);
	return versions[std::min(4, version)];
}

const char *PixelShaderVersionString()
{
	static const char *versions[5] = {"ERROR", "ps_1_4", "ps_2_0", "ps_3_0", "ps_4_0"};
	int version = ((D3D::caps.PixelShaderVersion >> 8) & 0xFF);
	return versions[std::min(4, version)];
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
	case D3DERR_DEVICELOST: PanicAlert("Device Lost"); break;
	case D3DERR_INVALIDCALL: PanicAlert("Invalid Call"); break;
	case D3DERR_DRIVERINTERNALERROR: PanicAlert("Driver Internal Error"); break;
	case D3DERR_OUTOFVIDEOMEMORY: PanicAlert("Out of vid mem"); break;
	default:
		// MessageBoxA(0,"Other error or success","ERROR",0);
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

bool IsFullscreen()
{
	return fullScreen;
}

int GetBackBufferWidth()
{
	return xres;
}

int GetBackBufferHeight()
{
	return yres;
}

void SwitchFullscreen(bool fullscreen)
{
	nextFullScreen = fullscreen;
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

	if (fullScreen != nextFullScreen)
	{
		fullScreen = nextFullScreen;
		Reset();
	}
}

void ApplyCachedState()
{
	for (int sampler = 0; sampler < 8; sampler++)
	{
		for (int type = 0; type < MaxSamplerTypes; type++)
		{
			D3D::dev->SetSamplerState(sampler, (D3DSAMPLERSTATETYPE)type, m_SamplerStates[sampler][type]);
		}
	}

	for (int rs = 0; rs < MaxRenderStates; rs++)
	{
		if (m_RenderStatesSet[rs])
			D3D::dev->SetRenderState((D3DRENDERSTATETYPE)rs, m_RenderStates[rs]);
	}

	// We don't bother restoring these so let's just wipe the state copy
	// so no stale state is around.
	memset(m_Textures, 0, sizeof(m_Textures));
	memset(m_TextureStageStates, 0xFF, sizeof(m_TextureStageStates));
	m_VtxDecl = NULL;
}

void SetTexture(DWORD Stage, LPDIRECT3DBASETEXTURE9 pTexture)
{
	if (m_Textures[Stage] != pTexture)
	{
		m_Textures[Stage] = pTexture;
		D3D::dev->SetTexture(Stage, pTexture);
	}
}

void RefreshRenderState(D3DRENDERSTATETYPE State)
{
	D3D::dev->SetRenderState(State, m_RenderStates[State]);	
}

void SetRenderState(D3DRENDERSTATETYPE State, DWORD Value)
{
	if (m_RenderStates[State] != Value)
	{
		m_RenderStates[State] = Value;
		m_RenderStatesSet[State] = true;
		D3D::dev->SetRenderState(State, Value);
	}
}

void SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value)
{
	if (m_TextureStageStates[Stage][Type] != Value)
	{
		m_TextureStageStates[Stage][Type] = Value;
		D3D::dev->SetTextureStageState(Stage, Type, Value);
	}
}

void RefreshSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type)
{
	D3D::dev->SetSamplerState(Sampler, Type, m_SamplerStates[Sampler][Type]);	
}

void SetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value)
{
	if (m_SamplerStates[Sampler][Type] != Value)
	{
		m_SamplerStates[Sampler][Type] = Value;
		D3D::dev->SetSamplerState(Sampler, Type, Value);
	}
}

void RefreshVertexDeclaration()
{
	if (m_VtxDecl)
	{
		D3D::dev->SetVertexDeclaration(m_VtxDecl);		
	}
}

void SetVertexDeclaration(LPDIRECT3DVERTEXDECLARATION9 decl)
{
	if (!decl) {
		m_VtxDecl = NULL;
		return;
	}
	if (decl != m_VtxDecl)
	{
		D3D::dev->SetVertexDeclaration(decl);
		m_VtxDecl = decl;
	}
}

}  // namespace
