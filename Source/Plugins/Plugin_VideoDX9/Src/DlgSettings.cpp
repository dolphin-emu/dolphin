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

#include <windowsx.h>

#include "resource.h"
#include "W32Util/PropertySheet.h"
#include "W32Util/ShellUtil.h"
#include "FileUtil.h"

#include "D3DBase.h"

#include "VideoConfig.h"

#include "TextureCache.h"
// TODO: remove if/when ini files use unicode
#define ComboBox_GetTextA(hwndCtl, lpch, cchMax) GetWindowTextA((hwndCtl), (lpch), (cchMax))

const char *aspect_ratio_names[4] = {
	"Auto",
	"Force 16:9 Widescreen",
	"Force 4:3 Standard",
	"Stretch to Window",
};

struct TabDirect3D : public W32Util::Tab
{
	void Init(HWND hDlg)
	{
		WCHAR tempwstr[2000];

		for (int i = 0; i < D3D::GetNumAdapters(); i++)
		{
			const D3D::Adapter &adapter = D3D::GetAdapter(i);
			MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, adapter.ident.Description, -1, tempwstr, 2000);
			ComboBox_AddString(GetDlgItem(hDlg, IDC_ADAPTER),tempwstr);
		}

		const D3D::Adapter &adapter = D3D::GetAdapter(g_Config.iAdapter);
		ComboBox_SetCurSel(GetDlgItem(hDlg, IDC_ADAPTER), g_Config.iAdapter);

		for (int i = 0; i < (int)adapter.aa_levels.size(); i++)
		{
			MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, adapter.aa_levels[i].name, -1, tempwstr, 2000);
			ComboBox_AddString(GetDlgItem(hDlg, IDC_ANTIALIASMODE), tempwstr);
		}

		ComboBox_SetCurSel(GetDlgItem(hDlg, IDC_ANTIALIASMODE), g_Config.iMultisampleMode);
		if (adapter.aa_levels.size() == 1) 
		{
			ComboBox_Enable(GetDlgItem(hDlg, IDC_ANTIALIASMODE), FALSE);
		}

		for (int i = 0; i < 4; i++)
		{
			MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, aspect_ratio_names[i], -1, tempwstr, 2000);
			ComboBox_AddString(GetDlgItem(hDlg, IDC_ASPECTRATIO), tempwstr);
		}
		ComboBox_SetCurSel(GetDlgItem(hDlg, IDC_ASPECTRATIO), g_Config.iAspectRatio);

		for (int i = 0; i < (int)adapter.resolutions.size(); i++)
		{
			const D3D::Resolution &r = adapter.resolutions[i];
			MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, r.name, -1, tempwstr, 2000);
			ComboBox_AddString(GetDlgItem(hDlg, IDC_RESOLUTION), tempwstr);
		}
		
		for (int i = 0; i < 16; i++) {
			tempwstr[i] = g_Config.cFSResolution[i];
		}
		ComboBox_SelectString(GetDlgItem(hDlg,IDC_RESOLUTION), -1, tempwstr);

		Button_SetCheck(GetDlgItem(hDlg, IDC_VSYNC), g_Config.bVSync);
		Button_SetCheck(GetDlgItem(hDlg, IDC_WIDESCREEN_HACK), g_Config.bWidescreenHack);
		Button_SetCheck(GetDlgItem(hDlg, IDC_SAFE_TEXTURE_CACHE), g_Config.bSafeTextureCache);
		
		if(g_Config.iSafeTextureCache_ColorSamples == 0)
		{
			Button_SetCheck(GetDlgItem(hDlg, IDC_SAFE_TEXTURE_CACHE_SAFE), true);
		}
		else
		{
			if(g_Config.iSafeTextureCache_ColorSamples > 128)
			{
				Button_SetCheck(GetDlgItem(hDlg, IDC_SAFE_TEXTURE_CACHE_NORMAL), true);	
			}
			else
			{
				Button_SetCheck(GetDlgItem(hDlg, IDC_SAFE_TEXTURE_CACHE_FAST), true);
			}
		}
		Button_Enable(GetDlgItem(hDlg, IDC_SAFE_TEXTURE_CACHE_SAFE),g_Config.bSafeTextureCache);
		Button_Enable(GetDlgItem(hDlg, IDC_SAFE_TEXTURE_CACHE_NORMAL),g_Config.bSafeTextureCache);
		Button_Enable(GetDlgItem(hDlg, IDC_SAFE_TEXTURE_CACHE_FAST),g_Config.bSafeTextureCache);
		
		Button_SetCheck(GetDlgItem(hDlg, IDC_EFB_ACCESS_ENABLE), g_Config.bEFBAccessEnable);
	}

	void Command(HWND hDlg,WPARAM wParam)
	{
		switch (LOWORD(wParam))
		{
		case IDC_ASPECTRATIO:
			g_Config.iAspectRatio = ComboBox_GetCurSel(GetDlgItem(hDlg, IDC_ASPECTRATIO));
			break;
		case IDC_WIDESCREEN_HACK:
			g_Config.bWidescreenHack = Button_GetCheck(GetDlgItem(hDlg, IDC_WIDESCREEN_HACK)) ? true : false;
			break;
		case IDC_SAFE_TEXTURE_CACHE:
			g_Config.bSafeTextureCache = Button_GetCheck(GetDlgItem(hDlg, IDC_SAFE_TEXTURE_CACHE)) == 0 ? false : true;
			Button_Enable(GetDlgItem(hDlg, IDC_SAFE_TEXTURE_CACHE_SAFE),g_Config.bSafeTextureCache);
			Button_Enable(GetDlgItem(hDlg, IDC_SAFE_TEXTURE_CACHE_NORMAL),g_Config.bSafeTextureCache);
			Button_Enable(GetDlgItem(hDlg, IDC_SAFE_TEXTURE_CACHE_FAST),g_Config.bSafeTextureCache);
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
		ComboBox_GetTextA(GetDlgItem(hDlg, IDC_RESOLUTION), g_Config.cFSResolution, 16);

		g_Config.iAdapter = ComboBox_GetCurSel(GetDlgItem(hDlg, IDC_ADAPTER));
		g_Config.iMultisampleMode = ComboBox_GetCurSel(GetDlgItem(hDlg, IDC_ANTIALIASMODE));
		g_Config.bVSync = Button_GetCheck(GetDlgItem(hDlg, IDC_VSYNC)) ? true : false;
		if(Button_GetCheck(GetDlgItem(hDlg, IDC_SAFE_TEXTURE_CACHE_SAFE)))
		{
			g_Config.iSafeTextureCache_ColorSamples = 0;			
		}
		else
		{
			if(Button_GetCheck(GetDlgItem(hDlg, IDC_SAFE_TEXTURE_CACHE_NORMAL)))
			{
				if(g_Config.iSafeTextureCache_ColorSamples < 512)
				{
					g_Config.iSafeTextureCache_ColorSamples = 512;
				}				
			}
			else
			{
				if(g_Config.iSafeTextureCache_ColorSamples > 128 || g_Config.iSafeTextureCache_ColorSamples == 0)
				{
					g_Config.iSafeTextureCache_ColorSamples = 128;
				}				
			}
		}
		g_Config.Save((std::string(File::GetUserPath(D_CONFIG_IDX)) + "gfx_dx9.ini").c_str());
	}
};

struct TabAdvanced : public W32Util::Tab
{
	void Init(HWND hDlg)
	{
//		HWND opt = GetDlgItem(hDlg,IDC_DLOPTLEVEL);
//		ComboBox_AddString(opt,"0: Interpret (slowest, most compatible)");
//		ComboBox_AddString(opt,"1: Compile lists and decode vertex lists");
		//ComboBox_AddString(opt,"2: Compile+decode to vbufs and use hw xform");
		//ComboBox_AddString(opt,"Recompile to vbuffers and shaders");
//		ComboBox_SetCurSel(opt,g_Config.iCompileDLsLevel);

		Button_SetCheck(GetDlgItem(hDlg,IDC_OSDHOTKEY), g_Config.bOSDHotKey);
		Button_SetCheck(GetDlgItem(hDlg,IDC_OVERLAYFPS), g_Config.bShowFPS);
		Button_SetCheck(GetDlgItem(hDlg,IDC_OVERLAYSTATS), g_Config.bOverlayStats);
		Button_SetCheck(GetDlgItem(hDlg,IDC_OVERLAYPROJSTATS), g_Config.bOverlayProjStats);
		Button_SetCheck(GetDlgItem(hDlg,IDC_WIREFRAME), g_Config.bWireFrame);
		Button_SetCheck(GetDlgItem(hDlg,IDC_TEXDUMP), g_Config.bDumpTextures);
		Button_SetCheck(GetDlgItem(hDlg,IDC_DUMPFRAMES), g_Config.bDumpFrames);
		Button_SetCheck(GetDlgItem(hDlg,IDC_SHOWSHADERERRORS), g_Config.bShowShaderErrors);
		Button_SetCheck(GetDlgItem(hDlg,IDC_DISABLEFOG), g_Config.bDisableFog);
		Button_SetCheck(GetDlgItem(hDlg,IDC_ENABLEEFBCOPY), !g_Config.bEFBCopyDisable);
		Button_SetCheck(GetDlgItem(hDlg,IDC_ENABLEXFB),g_Config.bUseXFB);
		Button_SetCheck(GetDlgItem(hDlg,IDC_ENABLEREALXFB),g_Config.bUseRealXFB);

		if(g_Config.bCopyEFBToTexture)
			Button_SetCheck(GetDlgItem(hDlg,IDC_EFBTOTEX), true);
		else
			Button_SetCheck(GetDlgItem(hDlg,IDC_EFBTORAM), true);

		
		Button_SetCheck(GetDlgItem(hDlg,IDC_TEXFMT_OVERLAY), g_Config.bTexFmtOverlayEnable);
		Button_SetCheck(GetDlgItem(hDlg,IDC_TEXFMT_CENTER),  g_Config.bTexFmtOverlayCenter);
		Button_GetCheck(GetDlgItem(hDlg,IDC_TEXFMT_OVERLAY)) ? Button_Enable(GetDlgItem(hDlg,IDC_TEXFMT_CENTER), true) : Button_Enable(GetDlgItem(hDlg,IDC_TEXFMT_CENTER), false);
		Button_GetCheck(GetDlgItem(hDlg,IDC_ENABLEEFBCOPY)) ? Button_Enable(GetDlgItem(hDlg,IDC_EFBTORAM), true) : Button_Enable(GetDlgItem(hDlg,IDC_EFBTORAM), false);
		Button_GetCheck(GetDlgItem(hDlg,IDC_ENABLEEFBCOPY)) ? Button_Enable(GetDlgItem(hDlg,IDC_EFBTOTEX), true) : Button_Enable(GetDlgItem(hDlg,IDC_EFBTOTEX), false);
	}
	void Command(HWND hDlg,WPARAM wParam)
	{
		switch (LOWORD(wParam))
		{
		case IDC_ENABLEXFB:
			{
				g_Config.bUseXFB = Button_GetCheck(GetDlgItem(hDlg, IDC_ENABLEXFB)) ? true : false;
			}
			break;
		case IDC_ENABLEREALXFB:
		{
			g_Config.bUseXFB = Button_GetCheck(GetDlgItem(hDlg, IDC_ENABLEREALXFB)) ? true : (Button_GetCheck(GetDlgItem(hDlg, IDC_ENABLEXFB)) ? true : false);
			g_Config.bUseRealXFB = Button_GetCheck(GetDlgItem(hDlg, IDC_ENABLEREALXFB)) ? true : false;
			Button_GetCheck(GetDlgItem(hDlg, IDC_ENABLEREALXFB)) ? Button_Enable(GetDlgItem(hDlg,IDC_ENABLEXFB), false) : Button_Enable(GetDlgItem(hDlg,IDC_ENABLEXFB), true);
		}
		break;
		case IDC_ENABLEEFBCOPY:
			{
				Button_GetCheck(GetDlgItem(hDlg,IDC_ENABLEEFBCOPY)) ? Button_Enable(GetDlgItem(hDlg,IDC_EFBTORAM), true) : Button_Enable(GetDlgItem(hDlg,IDC_EFBTORAM), false);
				Button_GetCheck(GetDlgItem(hDlg,IDC_ENABLEEFBCOPY)) ? Button_Enable(GetDlgItem(hDlg,IDC_EFBTOTEX), true) : Button_Enable(GetDlgItem(hDlg,IDC_EFBTOTEX), false);
			}
			break;
		case IDC_TEXFMT_OVERLAY:
			{
				Button_GetCheck(GetDlgItem(hDlg, IDC_TEXFMT_OVERLAY)) ? Button_Enable(GetDlgItem(hDlg,IDC_TEXFMT_CENTER), true) : Button_Enable(GetDlgItem(hDlg,IDC_TEXFMT_CENTER), false);
			}
			break;
		default:
			break;
		}
	}
	void Apply(HWND hDlg)
	{
		g_Config.bTexFmtOverlayEnable = Button_GetCheck(GetDlgItem(hDlg,IDC_TEXFMT_OVERLAY)) ? true : false;
		g_Config.bTexFmtOverlayCenter = Button_GetCheck(GetDlgItem(hDlg,IDC_TEXFMT_CENTER)) ? true : false;

		g_Config.bOSDHotKey = Button_GetCheck(GetDlgItem(hDlg,IDC_OSDHOTKEY)) ? true : false;
		g_Config.bShowFPS = Button_GetCheck(GetDlgItem(hDlg,IDC_OVERLAYFPS)) ? true : false;
		g_Config.bOverlayStats = Button_GetCheck(GetDlgItem(hDlg,IDC_OVERLAYSTATS)) ? true : false;
		g_Config.bOverlayProjStats = Button_GetCheck(GetDlgItem(hDlg,IDC_OVERLAYPROJSTATS)) ? true : false;
		g_Config.bWireFrame = Button_GetCheck(GetDlgItem(hDlg,IDC_WIREFRAME)) ? true : false;
		g_Config.bDumpTextures = Button_GetCheck(GetDlgItem(hDlg,IDC_TEXDUMP)) ? true : false;
		g_Config.bDumpFrames   = Button_GetCheck(GetDlgItem(hDlg,IDC_DUMPFRAMES)) ? true : false;
		g_Config.bShowShaderErrors = Button_GetCheck(GetDlgItem(hDlg,IDC_SHOWSHADERERRORS)) ? true : false;
		g_Config.bDisableFog = Button_GetCheck(GetDlgItem(hDlg,IDC_DISABLEFOG)) ? true : false;
		g_Config.bEFBCopyDisable = Button_GetCheck(GetDlgItem(hDlg,IDC_ENABLEEFBCOPY)) ? false : true;
		g_Config.bCopyEFBToTexture = Button_GetCheck(GetDlgItem(hDlg,IDC_EFBTORAM)) ? false : true;
		g_Config.bUseXFB = Button_GetCheck(GetDlgItem(hDlg, IDC_ENABLEXFB)) ? true : false;
		g_Config.bUseRealXFB = Button_GetCheck(GetDlgItem(hDlg, IDC_ENABLEREALXFB)) ? true : false;		
		g_Config.Save((std::string(File::GetUserPath(D_CONFIG_IDX)) + "gfx_dx9.ini").c_str());

		if( D3D::dev != NULL ) {
			D3D::SetRenderState( D3DRS_FILLMODE, g_Config.bWireFrame ? D3DFILL_WIREFRAME : D3DFILL_SOLID );
		}
	}
};

struct TabEnhancements : public W32Util::Tab
{
	void Init(HWND hDlg)
	{
		Button_SetCheck(GetDlgItem(hDlg,IDC_FORCEFILTERING),g_Config.bForceFiltering);
		Button_SetCheck(GetDlgItem(hDlg,IDC_FORCEANISOTROPY),g_Config.iMaxAnisotropy > 1);
		Button_SetCheck(GetDlgItem(hDlg, IDC_LOADHIRESTEXTURE), g_Config.bHiresTextures);
		Button_SetCheck(GetDlgItem(hDlg,IDC_EFBSCALEDCOPY), g_Config.bCopyEFBScaled);

		/*
		Temporarily disabled the old postprocessing code since it wasn't working anyway.
		New postprocessing code will come sooner or later, sharing shaders and framework with
		the GL postprocessing.

		HWND pp = GetDlgItem(hDlg, IDC_POSTPROCESSEFFECT);
		const char **names = Postprocess::GetPostprocessingNames();
		int i = 0;
		while (true)
		{
			if (!names[i])
				break;

			MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, names[i], -1, tempwstr, 2000);
			ComboBox_AddString(pp, tempwstr);
			i++;
		}
		ComboBox_SetCurSel(pp, g_Config.iPostprocessEffect);
		*/
	}
	void Command(HWND hDlg,WPARAM wParam)
	{
		/*
		switch (LOWORD(wParam))
		{
		default:
			break;
		}
		*/
	}
	void Apply(HWND hDlg)
	{
		g_Config.iMaxAnisotropy = Button_GetCheck(GetDlgItem(hDlg, IDC_FORCEANISOTROPY)) ? 8 : 1;
		g_Config.bForceFiltering = Button_GetCheck(GetDlgItem(hDlg, IDC_FORCEFILTERING)) ? true : false;
		g_Config.bHiresTextures = Button_GetCheck(GetDlgItem(hDlg, IDC_LOADHIRESTEXTURE)) ? true : false;
		g_Config.bCopyEFBScaled = Button_GetCheck(GetDlgItem(hDlg,IDC_EFBSCALEDCOPY)) ? true : false;
		g_Config.Save((std::string(File::GetUserPath(D_CONFIG_IDX)) + "gfx_dx9.ini").c_str());
	}
};


void DlgSettings_Show(HINSTANCE hInstance, HWND _hParent)
{
	bool tfoe = g_Config.bTexFmtOverlayEnable;
	bool tfoc = g_Config.bTexFmtOverlayCenter;

	g_Config.Load((std::string(File::GetUserPath(D_CONFIG_IDX)) + "gfx_dx9.ini").c_str());
	W32Util::PropSheet sheet;
	sheet.Add(new TabDirect3D, (LPCTSTR)IDD_SETTINGS,_T("Direct3D"));
	sheet.Add(new TabEnhancements, (LPCTSTR)IDD_ENHANCEMENTS,_T("Enhancements"));
	sheet.Add(new TabAdvanced, (LPCTSTR)IDD_ADVANCED,_T("Advanced"));

#ifdef DEBUGFAST
	sheet.Show(hInstance,_hParent,_T("DX9 Graphics Plugin (DEBUGFAST)"));
#else
#ifndef _DEBUG
	sheet.Show(hInstance,_hParent,_T("DX9 Graphics Plugin"));
#else
	sheet.Show(hInstance,_hParent,_T("DX9 Graphics Plugin (DEBUG)"));
#endif
#endif

	if ((tfoe != g_Config.bTexFmtOverlayEnable) ||
		((g_Config.bTexFmtOverlayEnable) && ( tfoc != g_Config.bTexFmtOverlayCenter)))
	{
		TextureCache::Invalidate(false);
	}
}
