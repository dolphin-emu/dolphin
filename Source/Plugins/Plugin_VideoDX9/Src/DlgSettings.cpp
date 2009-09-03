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

#include "Config.h"

#include "TextureCache.h"

#define NUMWNDRES 6
int g_Res[NUMWNDRES][2] = 
{
	{640,480},
	{800,600},
	{1024,768},
	{1280,960},
	{1280,1024},
	{1600,1200},
};

struct TabDirect3D : public W32Util::Tab
{
	void Init(HWND hDlg)
	{
		WCHAR tempwstr[2000];

		for (int i=0; i<D3D::GetNumAdapters(); i++)
		{
			const D3D::Adapter &adapter = D3D::GetAdapter(i);
			MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, adapter.ident.Description, -1, tempwstr, 2000);
			ComboBox_AddString(GetDlgItem(hDlg,IDC_ADAPTER),tempwstr);
		}

		const D3D::Adapter &adapter = D3D::GetAdapter(g_Config.iAdapter);

		ComboBox_SetCurSel(GetDlgItem(hDlg, IDC_ADAPTER),g_Config.iAdapter);

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
		}
		ComboBox_SetCurSel(GetDlgItem(hDlg,IDC_RESOLUTION), g_Config.iFSResolution);

		for (int i = 0; i < NUMWNDRES; i++)
		{
			char temp[256];
			sprintf(temp,"%ix%i",g_Res[i][0],g_Res[i][1]);
			MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, temp, -1, tempwstr, 2000);
			ComboBox_AddString(GetDlgItem(hDlg,IDC_RESOLUTIONWINDOWED),tempwstr);
		}
		ComboBox_SetCurSel(GetDlgItem(hDlg,IDC_RESOLUTIONWINDOWED),g_Config.iWindowedRes);

		CheckDlgButton(hDlg, IDC_FULLSCREENENABLE, g_Config.bFullscreen ? TRUE : FALSE);
		CheckDlgButton(hDlg, IDC_VSYNC, g_Config.bVsync ? TRUE : FALSE);
		CheckDlgButton(hDlg, IDC_RENDER_TO_MAINWINDOW, g_Config.renderToMainframe ? TRUE : FALSE);		
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
		g_Config.iAdapter = ComboBox_GetCurSel(GetDlgItem(hDlg, IDC_ADAPTER));
		g_Config.iWindowedRes = ComboBox_GetCurSel(GetDlgItem(hDlg, IDC_RESOLUTIONWINDOWED));
		g_Config.iMultisampleMode = ComboBox_GetCurSel(GetDlgItem(hDlg, IDC_ANTIALIASMODE));
		g_Config.iFSResolution = ComboBox_GetCurSel(GetDlgItem(hDlg,IDC_RESOLUTION));
		g_Config.bFullscreen = Button_GetCheck(GetDlgItem(hDlg, IDC_FULLSCREENENABLE)) ? true : false;
		g_Config.bVsync = Button_GetCheck(GetDlgItem(hDlg, IDC_VSYNC)) ? true : false;
		g_Config.renderToMainframe = Button_GetCheck(GetDlgItem(hDlg, IDC_RENDER_TO_MAINWINDOW)) ? true : false;
		g_Config.Save();
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

		Button_SetCheck(GetDlgItem(hDlg,IDC_OVERLAYSTATS), g_Config.bOverlayStats);
		Button_SetCheck(GetDlgItem(hDlg,IDC_OVERLAYPROJSTATS), g_Config.bOverlayProjStats);
		Button_SetCheck(GetDlgItem(hDlg,IDC_WIREFRAME), g_Config.bWireFrame);
		Button_SetCheck(GetDlgItem(hDlg,IDC_TEXDUMP), g_Config.bDumpTextures);
		Button_SetCheck(GetDlgItem(hDlg,IDC_DUMPFRAMES), g_Config.bDumpFrames);
		Button_SetCheck(GetDlgItem(hDlg,IDC_SHOWSHADERERRORS), g_Config.bShowShaderErrors);

		Button_SetCheck(GetDlgItem(hDlg,IDC_TEXFMT_OVERLAY), g_Config.bTexFmtOverlayEnable);
		Button_SetCheck(GetDlgItem(hDlg,IDC_TEXFMT_CENTER),  g_Config.bTexFmtOverlayCenter);
		Button_GetCheck(GetDlgItem(hDlg,IDC_TEXFMT_OVERLAY)) ? Button_Enable(GetDlgItem(hDlg,IDC_TEXFMT_CENTER), true) : Button_Enable(GetDlgItem(hDlg,IDC_TEXFMT_CENTER), false);

		//SetWindowText(GetDlgItem(hDlg,IDC_TEXDUMPPATH),g_Config.texDumpPath.c_str()); <-- Old method
		//Edit_LimitText(GetDlgItem(hDlg,IDC_TEXDUMPPATH),255); <-- Old method
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
		case IDC_TEXFMT_OVERLAY:
			{
				Button_GetCheck(GetDlgItem(hDlg,IDC_TEXFMT_OVERLAY)) ? Button_Enable(GetDlgItem(hDlg,IDC_TEXFMT_CENTER), true) : Button_Enable(GetDlgItem(hDlg,IDC_TEXFMT_CENTER), false);
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

		g_Config.bOverlayStats = Button_GetCheck(GetDlgItem(hDlg,IDC_OVERLAYSTATS)) ? true : false;
		g_Config.bOverlayProjStats = Button_GetCheck(GetDlgItem(hDlg,IDC_OVERLAYPROJSTATS)) ? true : false;
		g_Config.bWireFrame    = Button_GetCheck(GetDlgItem(hDlg,IDC_WIREFRAME)) ? true : false;
		g_Config.bDumpTextures = Button_GetCheck(GetDlgItem(hDlg,IDC_TEXDUMP)) ? true : false;
		g_Config.bDumpFrames   = Button_GetCheck(GetDlgItem(hDlg,IDC_DUMPFRAMES)) ? true : false;
		g_Config.bShowShaderErrors = Button_GetCheck(GetDlgItem(hDlg,IDC_SHOWSHADERERRORS)) ? true : false;
		//char temp[MAX_PATH];
		//GetWindowText(GetDlgItem(hDlg,IDC_TEXDUMPPATH), temp, MAX_PATH);  <-- Old method
		//g_Config.texDumpPath = temp;
	}
};

struct TabEnhancements : public W32Util::Tab
{
	void Init(HWND hDlg)
	{
		Button_SetCheck(GetDlgItem(hDlg,IDC_FORCEFILTERING),g_Config.bForceFiltering);
		Button_SetCheck(GetDlgItem(hDlg,IDC_FORCEANISOTROPY),g_Config.bForceMaxAniso);
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
		g_Config.bForceMaxAniso = Button_GetCheck(GetDlgItem(hDlg, IDC_FORCEANISOTROPY)) ? true : false;
		g_Config.bForceFiltering = Button_GetCheck(GetDlgItem(hDlg,IDC_FORCEFILTERING)) ? true : false;
		g_Config.iPostprocessEffect = ComboBox_GetCurSel(GetDlgItem(hDlg,IDC_POSTPROCESSEFFECT));
	}
};


void DlgSettings_Show(HINSTANCE hInstance, HWND _hParent)
{
	bool tfoe = g_Config.bTexFmtOverlayEnable;
	bool tfoc = g_Config.bTexFmtOverlayCenter;

	g_Config.Load();
	W32Util::PropSheet sheet;
	sheet.Add(new TabDirect3D,(LPCTSTR)IDD_SETTINGS,_T("Direct3D"));
	sheet.Add(new TabEnhancements,(LPCTSTR)IDD_ENHANCEMENTS,_T("Enhancements"));
	sheet.Add(new TabAdvanced,(LPCTSTR)IDD_ADVANCED,_T("Advanced"));
	sheet.Show(hInstance,_hParent,_T("Graphics Plugin"));
	g_Config.Save();

	if(( tfoe != g_Config.bTexFmtOverlayEnable) ||
		((g_Config.bTexFmtOverlayEnable) && ( tfoc != g_Config.bTexFmtOverlayCenter)))
	{
		TextureCache::Invalidate(false);
	}
}