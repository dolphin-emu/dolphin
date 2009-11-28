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

#include "D3DBase.h"

#include "VideoConfig.h"

#include "TextureCache.h"
// TODO: remove if/when ini files use unicode
#define ComboBox_GetTextA(hwndCtl, lpch, cchMax) GetWindowTextA((hwndCtl), (lpch), (cchMax))

struct TabDirect3D : public W32Util::Tab
{
	void Init(HWND hDlg)
	{
		WCHAR tempwstr[2000];

		for (int i = 0; i < D3D::GetNumAdapters(); i++)
		{
			const D3D::Adapter &adapter = D3D::GetAdapter(i);
			MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, adapter.ident.Description, -1, tempwstr, 2000);
			ComboBox_AddString(GetDlgItem(hDlg,IDC_ADAPTER),tempwstr);
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
			EnableWindow(GetDlgItem(hDlg, IDC_ANTIALIASMODE), FALSE);
		}

		for (int i = 0; i < (int)adapter.resolutions.size(); i++)
		{
			const D3D::Resolution &r = adapter.resolutions[i];
			MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, r.name, -1, tempwstr, 2000);
			ComboBox_AddString(GetDlgItem(hDlg,IDC_RESOLUTION), tempwstr);
			ComboBox_AddString(GetDlgItem(hDlg,IDC_RESOLUTIONWINDOWED),tempwstr);
		}
		
		for (int i = 0; i <16; i++) tempwstr[i] = g_Config.cFSResolution[i];
		ComboBox_SelectString(GetDlgItem(hDlg,IDC_RESOLUTION), -1, tempwstr);

		for (int i = 0; i < 16; i++) tempwstr[i] = g_Config.cInternalRes[i];		
		ComboBox_SelectString(GetDlgItem(hDlg,IDC_RESOLUTIONWINDOWED), -1, tempwstr);

		CheckDlgButton(hDlg, IDC_FULLSCREENENABLE, g_Config.bFullscreen ? TRUE : FALSE);
		CheckDlgButton(hDlg, IDC_VSYNC, g_Config.bVSync ? TRUE : FALSE);
		CheckDlgButton(hDlg, IDC_RENDER_TO_MAINWINDOW, g_Config.RenderToMainframe ? TRUE : FALSE);		
		CheckDlgButton(hDlg, IDC_ASPECT_16_9, g_Config.bKeepAR169 ? TRUE : FALSE);
		CheckDlgButton(hDlg, IDC_ASPECT_4_3, g_Config.bKeepAR43 ? TRUE : FALSE);
		CheckDlgButton(hDlg, IDC_WIDESCREEN_HACK, g_Config.bWidescreenHack ? TRUE : FALSE);
		CheckDlgButton(hDlg, IDC_SAFE_TEXTURE_CACHE, g_Config.bSafeTextureCache ? TRUE : FALSE);
		CheckDlgButton(hDlg, IDC_EFB_ACCESS_ENABLE, g_Config.bEFBAccessEnable ? TRUE : FALSE);
		Button_GetCheck(GetDlgItem(hDlg,IDC_RENDER_TO_MAINWINDOW)) ? Button_Enable(GetDlgItem(hDlg,IDC_FULLSCREENENABLE), false) : Button_Enable(GetDlgItem(hDlg,IDC_FULLSCREENENABLE), true);
	}

	void Command(HWND hDlg,WPARAM wParam)
	{
		switch (LOWORD(wParam))
		{
		case IDC_ASPECT_4_3:
			Button_SetCheck(GetDlgItem(hDlg, IDC_ASPECT_16_9), FALSE);
			g_Config.bKeepAR43 = Button_GetCheck(GetDlgItem(hDlg, IDC_ASPECT_4_3)) ? true : false;
			g_Config.bKeepAR169 = false;
			break;
		case IDC_ASPECT_16_9:
			Button_SetCheck(GetDlgItem(hDlg, IDC_ASPECT_4_3), FALSE);
			g_Config.bKeepAR169 = Button_GetCheck(GetDlgItem(hDlg, IDC_ASPECT_16_9)) ? true : false;
			g_Config.bKeepAR43 = false;
			break;
		case IDC_WIDESCREEN_HACK:
			g_Config.bWidescreenHack = Button_GetCheck(GetDlgItem(hDlg, IDC_WIDESCREEN_HACK)) ? true : false;
			break;
		case IDC_WIREFRAME:
			g_Config.bWireFrame = Button_GetCheck(GetDlgItem(hDlg,IDC_WIREFRAME)) ? true : false;
			break;
		case IDC_SAFE_TEXTURE_CACHE:
			g_Config.bSafeTextureCache = Button_GetCheck(GetDlgItem(hDlg, IDC_SAFE_TEXTURE_CACHE)) == 0 ? false : true;
			break;
		case IDC_EFB_ACCESS_ENABLE:
			g_Config.bEFBAccessEnable = Button_GetCheck(GetDlgItem(hDlg, IDC_EFB_ACCESS_ENABLE)) == 0 ? false : true;
			break;
		case IDC_RENDER_TO_MAINWINDOW:
			Button_Enable(GetDlgItem(hDlg, IDC_FULLSCREENENABLE), !Button_GetCheck(GetDlgItem(hDlg, IDC_RENDER_TO_MAINWINDOW)));
			if (Button_GetCheck(GetDlgItem(hDlg, IDC_RENDER_TO_MAINWINDOW)))
				Button_SetCheck(GetDlgItem(hDlg, IDC_FULLSCREENENABLE),  false);
			break;
		default:
			break;
		}
	}

	void Apply(HWND hDlg)
	{
		ComboBox_GetTextA(GetDlgItem(hDlg, IDC_RESOLUTIONWINDOWED), g_Config.cInternalRes, 16);
		ComboBox_GetTextA(GetDlgItem(hDlg, IDC_RESOLUTION), g_Config.cFSResolution, 16);

		g_Config.iAdapter = ComboBox_GetCurSel(GetDlgItem(hDlg, IDC_ADAPTER));
		g_Config.iMultisampleMode = ComboBox_GetCurSel(GetDlgItem(hDlg, IDC_ANTIALIASMODE));
		g_Config.bFullscreen = Button_GetCheck(GetDlgItem(hDlg, IDC_FULLSCREENENABLE)) ? true : false;
		g_Config.bVSync = Button_GetCheck(GetDlgItem(hDlg, IDC_VSYNC)) ? true : false;
		g_Config.RenderToMainframe = Button_GetCheck(GetDlgItem(hDlg, IDC_RENDER_TO_MAINWINDOW)) ? true : false;
		g_Config.Save(FULL_CONFIG_DIR "gfx_dx9.ini");
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

		Button_SetCheck(GetDlgItem(hDlg,IDC_OVERLAYFPS), g_Config.bShowFPS);
		Button_SetCheck(GetDlgItem(hDlg,IDC_OVERLAYSTATS), g_Config.bOverlayStats);
		Button_SetCheck(GetDlgItem(hDlg,IDC_OVERLAYPROJSTATS), g_Config.bOverlayProjStats);
		Button_SetCheck(GetDlgItem(hDlg,IDC_WIREFRAME), g_Config.bWireFrame);
		Button_SetCheck(GetDlgItem(hDlg,IDC_TEXDUMP), g_Config.bDumpTextures);
		Button_SetCheck(GetDlgItem(hDlg,IDC_DUMPFRAMES), g_Config.bDumpFrames);
		Button_SetCheck(GetDlgItem(hDlg,IDC_SHOWSHADERERRORS), g_Config.bShowShaderErrors);
		Button_SetCheck(GetDlgItem(hDlg,IDC_DISABLEFOG), g_Config.bDisableFog);
		Button_SetCheck(GetDlgItem(hDlg,IDC_ENABLEEFBCOPY), !g_Config.bEFBCopyDisable);
		if(g_Config.bCopyEFBToRAM)
			Button_SetCheck(GetDlgItem(hDlg,IDC_EFBTORAM), TRUE);
		else
			Button_SetCheck(GetDlgItem(hDlg,IDC_EFBTOTEX), TRUE);

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
	//	case IDC_BROWSETEXDUMPPATH:  <-- Old method
	//		{
	//			std::string path = W32Util::BrowseForFolder(hDlg,"Choose texture dump path:"); 
	//			SetWindowText(GetDlgItem(hDlg,IDC_TEXDUMPPATH),path.c_str());
	//		}
	//		break;
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

		g_Config.bShowFPS = Button_GetCheck(GetDlgItem(hDlg,IDC_OVERLAYFPS)) ? true : false;
		g_Config.bOverlayStats = Button_GetCheck(GetDlgItem(hDlg,IDC_OVERLAYSTATS)) ? true : false;
		g_Config.bOverlayProjStats = Button_GetCheck(GetDlgItem(hDlg,IDC_OVERLAYPROJSTATS)) ? true : false;
		g_Config.bWireFrame = Button_GetCheck(GetDlgItem(hDlg,IDC_WIREFRAME)) ? true : false;
		g_Config.bDumpTextures = Button_GetCheck(GetDlgItem(hDlg,IDC_TEXDUMP)) ? true : false;
		g_Config.bDumpFrames   = Button_GetCheck(GetDlgItem(hDlg,IDC_DUMPFRAMES)) ? true : false;
		g_Config.bShowShaderErrors = Button_GetCheck(GetDlgItem(hDlg,IDC_SHOWSHADERERRORS)) ? true : false;
		g_Config.bDisableFog = Button_GetCheck(GetDlgItem(hDlg,IDC_DISABLEFOG)) ? true : false;
		g_Config.bEFBCopyDisable = Button_GetCheck(GetDlgItem(hDlg,IDC_ENABLEEFBCOPY)) ? false : true;
		g_Config.bCopyEFBToRAM = Button_GetCheck(GetDlgItem(hDlg,IDC_EFBTORAM)) ? true : false;
		//char temp[MAX_PATH];
		//GetWindowText(GetDlgItem(hDlg,IDC_TEXDUMPPATH), temp, MAX_PATH);  <-- Old method
		//g_Config.texDumpPath = temp;
		g_Config.Save(FULL_CONFIG_DIR "gfx_dx9.ini");

		if( D3D::dev != NULL ) {
			D3D::dev->SetRenderState( D3DRS_FILLMODE, g_Config.bWireFrame ? D3DFILL_WIREFRAME : D3DFILL_SOLID );
		}
	}
};

struct TabEnhancements : public W32Util::Tab
{
	void Init(HWND hDlg)
	{
		Button_SetCheck(GetDlgItem(hDlg,IDC_FORCEFILTERING),g_Config.bForceFiltering);
		Button_SetCheck(GetDlgItem(hDlg,IDC_FORCEANISOTROPY),g_Config.iMaxAnisotropy > 1);
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
		g_Config.Save(FULL_CONFIG_DIR "gfx_dx9.ini");
	}
};


void DlgSettings_Show(HINSTANCE hInstance, HWND _hParent)
{
	bool tfoe = g_Config.bTexFmtOverlayEnable;
	bool tfoc = g_Config.bTexFmtOverlayCenter;

	g_Config.Load(FULL_CONFIG_DIR "gfx_dx9.ini");
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