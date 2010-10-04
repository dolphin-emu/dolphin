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

#include <vector>
#include <windowsx.h>

#include "resource.h"
#include "W32Util/PropertySheet.h"
#include "FileUtil.h"

#include "D3DBase.h"
#include "VideoConfig.h"
#include "TextureCache.h"
using std::vector;

const char* aspect_ratio_names[4] = {
	"Auto",
	"Force 16:9 Widescreen",
	"Force 4:3 Standard",
	"Stretch to Window",
};

vector<IDXGIAdapter*> CreateAdapterList()
{
	vector<IDXGIAdapter*> adapters;
	IDXGIFactory* factory;
	IDXGIAdapter* ad;
	HRESULT hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory);
	if (FAILED(hr)) MessageBox(NULL, _T("Failed to create IDXGIFactory object"), _T("Dolphin Direct3D 11 plugin"), MB_OK | MB_ICONERROR);

	while (factory->EnumAdapters(adapters.size(), &ad) != DXGI_ERROR_NOT_FOUND)
		adapters.push_back(ad);

	if (adapters.size() == 0) MessageBox(NULL, _T("Couldn't find any devices!"), _T("Dolphin Direct3D 11 plugin"), MB_OK | MB_ICONERROR);
	factory->Release();
	return adapters;
}

void DestroyAdapterList(vector<IDXGIAdapter*> &adapters)
{
	while (!adapters.empty())
	{
		adapters.back()->Release();
		adapters.pop_back();
	}
}

struct TabDirect3D : public W32Util::Tab
{
	void Init(HWND hDlg)
	{
		WCHAR tempwstr[2000];
		HRESULT hr;

		vector<IDXGIAdapter*> adapters = CreateAdapterList();
		for (vector<IDXGIAdapter*>::iterator it = adapters.begin();it != adapters.end();++it)
		{
			DXGI_ADAPTER_DESC desc;
			hr = (*it)->GetDesc(&desc);
			if (SUCCEEDED(hr)) ComboBox_AddString(GetDlgItem(hDlg, IDC_ADAPTER), desc.Description);
			else
			{
				MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, "Unknown device", -1, tempwstr, 2000);
				ComboBox_AddString(GetDlgItem(hDlg, IDC_ADAPTER), tempwstr);
			}
		}
		DestroyAdapterList(adapters);
		ComboBox_SetCurSel(GetDlgItem(hDlg, IDC_ADAPTER), g_Config.iAdapter);

		for (unsigned int i = 0; i < 4; i++)
		{
			MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, aspect_ratio_names[i], -1, tempwstr, 2000);
			ComboBox_AddString(GetDlgItem(hDlg, IDC_ASPECTRATIO), tempwstr);
		}
		ComboBox_SetCurSel(GetDlgItem(hDlg, IDC_ASPECTRATIO), g_Config.iAspectRatio);

		Button_SetCheck(GetDlgItem(hDlg, IDC_WIDESCREEN_HACK), g_Config.bWidescreenHack);
		Button_SetCheck(GetDlgItem(hDlg, IDC_VSYNC), g_Config.bVSync);
		Button_SetCheck(GetDlgItem(hDlg, IDC_SAFE_TEXTURE_CACHE), g_Config.bSafeTextureCache);
		Button_SetCheck(GetDlgItem(hDlg, IDC_DLIST_CACHING), g_Config.bDlistCachingEnable);
		Button_SetCheck(GetDlgItem(hDlg, IDC_ENABLEPIXELLIGHTING), g_Config.bEnablePixelLigting);



		if (g_Config.iSafeTextureCache_ColorSamples == 0)
		{
			Button_SetCheck(GetDlgItem(hDlg, IDC_SAFE_TEXTURE_CACHE_SAFE), true);
		}
		else
		{
			if (g_Config.iSafeTextureCache_ColorSamples > 128)
			{
				Button_SetCheck(GetDlgItem(hDlg, IDC_SAFE_TEXTURE_CACHE_NORMAL), true);	
			}
			else
			{
				Button_SetCheck(GetDlgItem(hDlg, IDC_SAFE_TEXTURE_CACHE_FAST), true);
			}
		}
		Button_Enable(GetDlgItem(hDlg, IDC_SAFE_TEXTURE_CACHE_SAFE), g_Config.bSafeTextureCache);
		Button_Enable(GetDlgItem(hDlg, IDC_SAFE_TEXTURE_CACHE_NORMAL), g_Config.bSafeTextureCache);
		Button_Enable(GetDlgItem(hDlg, IDC_SAFE_TEXTURE_CACHE_FAST), g_Config.bSafeTextureCache);
		
		Button_SetCheck(GetDlgItem(hDlg, IDC_EFB_ACCESS_ENABLE), g_Config.bEFBAccessEnable);
	}

	void Command(HWND hDlg,WPARAM wParam)
	{
		switch (LOWORD(wParam))
		{
		case IDC_ASPECTRATIO:
			g_Config.iAspectRatio = ComboBox_GetCurSel(GetDlgItem(hDlg, IDC_ASPECTRATIO));
			break;
		case IDC_ADAPTER:
			g_Config.iAdapter = ComboBox_GetCurSel(GetDlgItem(hDlg, IDC_ADAPTER));
			break;
		case IDC_VSYNC:
			g_Config.bVSync = Button_GetCheck(GetDlgItem(hDlg, IDC_VSYNC)) ? true : false;
			break;
		case IDC_WIDESCREEN_HACK:
			g_Config.bWidescreenHack = Button_GetCheck(GetDlgItem(hDlg, IDC_WIDESCREEN_HACK)) ? true : false;
			break;
		case IDC_SAFE_TEXTURE_CACHE:
			g_Config.bSafeTextureCache = Button_GetCheck(GetDlgItem(hDlg, IDC_SAFE_TEXTURE_CACHE)) == 0 ? false : true;
			Button_Enable(GetDlgItem(hDlg, IDC_SAFE_TEXTURE_CACHE_SAFE), g_Config.bSafeTextureCache);
			Button_Enable(GetDlgItem(hDlg, IDC_SAFE_TEXTURE_CACHE_NORMAL), g_Config.bSafeTextureCache);
			Button_Enable(GetDlgItem(hDlg, IDC_SAFE_TEXTURE_CACHE_FAST), g_Config.bSafeTextureCache);
			break;
		case IDC_DLIST_CACHING:
			g_Config.bDlistCachingEnable = Button_GetCheck(GetDlgItem(hDlg, IDC_DLIST_CACHING)) == 0 ? false : true;
			break;
		case IDC_ENABLEPIXELLIGHTING:
			g_Config.bEnablePixelLigting = Button_GetCheck(GetDlgItem(hDlg, IDC_ENABLEPIXELLIGHTING)) == 0 ? false : true;
			break;
		case IDC_EFB_ACCESS_ENABLE:
			g_Config.bEFBAccessEnable = Button_GetCheck(GetDlgItem(hDlg, IDC_EFB_ACCESS_ENABLE)) == 0 ? false : true;
			break;
		default:
			break;
		}
	}

	void Apply(HWND hDlg)
	{
		if (Button_GetCheck(GetDlgItem(hDlg, IDC_SAFE_TEXTURE_CACHE_SAFE)))
		{
			g_Config.iSafeTextureCache_ColorSamples = 0;			
		}
		else
		{
			if (Button_GetCheck(GetDlgItem(hDlg, IDC_SAFE_TEXTURE_CACHE_NORMAL)))
			{
				if (g_Config.iSafeTextureCache_ColorSamples < 512)
				{
					g_Config.iSafeTextureCache_ColorSamples = 512;
				}				
			}
			else
			{
				if (g_Config.iSafeTextureCache_ColorSamples > 128 || g_Config.iSafeTextureCache_ColorSamples == 0)
				{
					g_Config.iSafeTextureCache_ColorSamples = 128;
				}				
			}
		}
		g_Config.Save((std::string(File::GetUserPath(D_CONFIG_IDX)) + "gfx_dx11.ini").c_str());
	}
};

struct TabAdvanced : public W32Util::Tab
{
	void Init(HWND hDlg)
	{
		Button_SetCheck(GetDlgItem(hDlg, IDC_OSDHOTKEY), g_Config.bOSDHotKey);
		Button_SetCheck(GetDlgItem(hDlg, IDC_OVERLAYFPS), g_Config.bShowFPS);
		Button_SetCheck(GetDlgItem(hDlg, IDC_OVERLAYSTATS), g_Config.bOverlayStats);
		Button_SetCheck(GetDlgItem(hDlg, IDC_OVERLAYPROJSTATS), g_Config.bOverlayProjStats);
		Button_SetCheck(GetDlgItem(hDlg, IDC_WIREFRAME), g_Config.bWireFrame);
		Button_SetCheck(GetDlgItem(hDlg, IDC_DISABLEFOG), g_Config.bDisableFog);
		Button_SetCheck(GetDlgItem(hDlg, IDC_ENABLEEFBCOPY), !g_Config.bEFBCopyDisable);

		Button_SetCheck(GetDlgItem(hDlg, IDC_TEXFMT_OVERLAY), g_Config.bTexFmtOverlayEnable);
		Button_SetCheck(GetDlgItem(hDlg, IDC_TEXFMT_CENTER),  g_Config.bTexFmtOverlayCenter);
		Button_GetCheck(GetDlgItem(hDlg, IDC_TEXFMT_OVERLAY)) ? Button_Enable(GetDlgItem(hDlg,IDC_TEXFMT_CENTER), true) : Button_Enable(GetDlgItem(hDlg,IDC_TEXFMT_CENTER), false);

		Button_SetCheck(GetDlgItem(hDlg, IDC_FORCEANISOTROPY),g_Config.iMaxAnisotropy > 1);
		Button_SetCheck(GetDlgItem(hDlg, IDC_EFBSCALEDCOPY), g_Config.bCopyEFBScaled);

		Button_SetCheck(GetDlgItem(hDlg, IDC_LOADHIRESTEXTURE),g_Config.bHiresTextures);
		Button_SetCheck(GetDlgItem(hDlg, IDC_DUMPTEXTURES),g_Config.bDumpTextures);

		if (Button_GetCheck(GetDlgItem(hDlg, IDC_ENABLEEFBCOPY))) Button_Enable(GetDlgItem(hDlg,IDC_EFBSCALEDCOPY), true);
		else Button_Enable(GetDlgItem(hDlg, IDC_EFBSCALEDCOPY), false);
	}
	void Command(HWND hDlg,WPARAM wParam)
	{
		switch (LOWORD(wParam))
		{
			case IDC_TEXFMT_OVERLAY:
				Button_GetCheck(GetDlgItem(hDlg, IDC_TEXFMT_OVERLAY)) ? Button_Enable(GetDlgItem(hDlg, IDC_TEXFMT_CENTER), true) : Button_Enable(GetDlgItem(hDlg, IDC_TEXFMT_CENTER), false);
				break;

			case IDC_ENABLEEFBCOPY:
				if (Button_GetCheck(GetDlgItem(hDlg, IDC_ENABLEEFBCOPY))) Button_Enable(GetDlgItem(hDlg, IDC_EFBSCALEDCOPY), true);
				else Button_Enable(GetDlgItem(hDlg, IDC_EFBSCALEDCOPY), false);
				break;

			default: break;
		}
	}
	void Apply(HWND hDlg)
	{
		g_Config.bTexFmtOverlayEnable = Button_GetCheck(GetDlgItem(hDlg, IDC_TEXFMT_OVERLAY)) ? true : false;
		g_Config.bTexFmtOverlayCenter = Button_GetCheck(GetDlgItem(hDlg, IDC_TEXFMT_CENTER)) ? true : false;

		g_Config.bOSDHotKey = Button_GetCheck(GetDlgItem(hDlg, IDC_OSDHOTKEY)) ? true : false;
		g_Config.bShowFPS = Button_GetCheck(GetDlgItem(hDlg, IDC_OVERLAYFPS)) ? true : false;
		g_Config.bOverlayStats = Button_GetCheck(GetDlgItem(hDlg, IDC_OVERLAYSTATS)) ? true : false;
		g_Config.bOverlayProjStats = Button_GetCheck(GetDlgItem(hDlg, IDC_OVERLAYPROJSTATS)) ? true : false;
		g_Config.bWireFrame = Button_GetCheck(GetDlgItem(hDlg, IDC_WIREFRAME)) ? true : false;
		g_Config.bDisableFog = Button_GetCheck(GetDlgItem(hDlg, IDC_DISABLEFOG)) ? true : false;
		g_Config.bEFBCopyDisable = Button_GetCheck(GetDlgItem(hDlg, IDC_ENABLEEFBCOPY)) ? false : true;
		g_Config.bCopyEFBToTexture = !g_Config.bEFBCopyDisable;
		g_Config.bDumpFrames = false;
		g_Config.bShowShaderErrors = true;
		g_Config.bUseNativeMips = true;

		g_Config.iMaxAnisotropy = Button_GetCheck(GetDlgItem(hDlg, IDC_FORCEANISOTROPY)) ? 16 : 1;
		g_Config.bForceFiltering = false;
		g_Config.bHiresTextures = Button_GetCheck(GetDlgItem(hDlg, IDC_LOADHIRESTEXTURE)) ? true : false;
		g_Config.bDumpTextures = Button_GetCheck(GetDlgItem(hDlg, IDC_DUMPTEXTURES)) ? true : false;
		g_Config.bCopyEFBScaled = Button_GetCheck(GetDlgItem(hDlg, IDC_EFBSCALEDCOPY)) ? true : false;
		g_Config.Save((std::string(File::GetUserPath(D_CONFIG_IDX)) + "gfx_dx11.ini").c_str());
	}
};

struct TabAbout : public W32Util::Tab
{
	void Init(HWND hDlg) {}
	void Command(HWND hDlg,WPARAM wParam) {}
	void Apply(HWND hDlg) {}
};

void DlgSettings_Show(HINSTANCE hInstance, HWND _hParent)
{
	bool tfoe = g_Config.bTexFmtOverlayEnable;
	bool tfoc = g_Config.bTexFmtOverlayCenter;

	g_Config.Load((std::string(File::GetUserPath(D_CONFIG_IDX)) + "gfx_dx11.ini").c_str());
	W32Util::PropSheet sheet;
	sheet.Add(new TabDirect3D, (LPCTSTR)IDD_SETTINGS, _T("Direct3D"));
	sheet.Add(new TabAdvanced, (LPCTSTR)IDD_ADVANCED, _T("Advanced"));
	sheet.Add(new TabAbout,    (LPCTSTR)IDD_ABOUT,    _T("About"));

#ifdef DEBUGFAST
	sheet.Show(hInstance,_hParent,_T("DX11 Graphics Plugin (DEBUGFAST)"));
#elif defined _DEBUG
	sheet.Show(hInstance,_hParent,_T("DX11 Graphics Plugin (DEBUG)"));
#else
	sheet.Show(hInstance,_hParent,_T("DX11 Graphics Plugin"));
#endif

	if ((tfoe != g_Config.bTexFmtOverlayEnable) ||
		((g_Config.bTexFmtOverlayEnable) && ( tfoc != g_Config.bTexFmtOverlayCenter)))
	{
		TextureCache::Invalidate(false);
	}
}
