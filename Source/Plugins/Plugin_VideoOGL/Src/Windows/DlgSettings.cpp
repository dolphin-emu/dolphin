// Copyright (C) 2003-2008 Dolphin Project.

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

#include "Globals.h"

#include <windowsx.h>

#include "resource.h"
#include "PropertySheet.h"
#include "ShellUtil.h"
#include "Misc.h"

struct TabOGL : public W32Util::Tab
{
	void Init(HWND hDlg)
	{
        ComboBox_AddString(GetDlgItem(hDlg, IDC_ANTIALIASMODE), "1X");
        ComboBox_AddString(GetDlgItem(hDlg, IDC_ANTIALIASMODE), "2X");
        ComboBox_AddString(GetDlgItem(hDlg, IDC_ANTIALIASMODE), "4X");
        ComboBox_AddString(GetDlgItem(hDlg, IDC_ANTIALIASMODE), "8X");
        ComboBox_AddString(GetDlgItem(hDlg, IDC_ANTIALIASMODE), "16X");
		ComboBox_SetCurSel(GetDlgItem(hDlg, IDC_ANTIALIASMODE), g_Config.iMultisampleMode);

		//EnableWindow(GetDlgItem(hDlg, IDC_ANTIALIASMODE), FALSE);
		
		for (int i = 0; i < NUMWNDRES; i++)
		{
			char temp[256];
			sprintf(temp,"%ix%i",g_Res[i][0],g_Res[i][1]);
			ComboBox_AddString(GetDlgItem(hDlg,IDC_RESOLUTIONWINDOWED),temp);
            ComboBox_AddString(GetDlgItem(hDlg,IDC_RESOLUTION), temp);
		}
		ComboBox_SetCurSel(GetDlgItem(hDlg,IDC_RESOLUTIONWINDOWED),g_Config.iWindowedRes);
        ComboBox_SetCurSel(GetDlgItem(hDlg,IDC_RESOLUTION), g_Config.iFSResolution);

		CheckDlgButton(hDlg, IDC_FULLSCREENENABLE, g_Config.bFullscreen ? TRUE : FALSE);
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
		g_Config.iWindowedRes = ComboBox_GetCurSel(GetDlgItem(hDlg, IDC_RESOLUTIONWINDOWED));
		g_Config.iMultisampleMode = ComboBox_GetCurSel(GetDlgItem(hDlg, IDC_ANTIALIASMODE));
		g_Config.iFSResolution = ComboBox_GetCurSel(GetDlgItem(hDlg,IDC_RESOLUTION));
		g_Config.bFullscreen = Button_GetCheck(GetDlgItem(hDlg, IDC_FULLSCREENENABLE)) ? true : false;
		g_Config.renderToMainframe = Button_GetCheck(GetDlgItem(hDlg, IDC_RENDER_TO_MAINWINDOW)) ? true : false;
		g_Config.Save();
	}
};

struct TabAdvanced : public W32Util::Tab
{
	void Init(HWND hDlg)
	{
		HWND opt = GetDlgItem(hDlg,IDC_DLOPTLEVEL);
		ComboBox_AddString(opt,"0: Interpret (slowest, most compatible)");
		ComboBox_AddString(opt,"1: Compile lists and decode vertex lists");
		//ComboBox_AddString(opt,"2: Compile+decode to vbufs and use hw xform");
		//ComboBox_AddString(opt,"Recompile to vbuffers and shaders");
		ComboBox_SetCurSel(opt,g_Config.iCompileDLsLevel);

		Button_SetCheck(GetDlgItem(hDlg,IDC_OVERLAYSTATS), g_Config.bOverlayStats);
		Button_SetCheck(GetDlgItem(hDlg,IDC_WIREFRAME), g_Config.bWireFrame);
		Button_SetCheck(GetDlgItem(hDlg,IDC_TEXDUMP), g_Config.bDumpTextures);
		Button_SetCheck(GetDlgItem(hDlg,IDC_SHOWSHADERERRORS), g_Config.bShowShaderErrors);
		
		SetWindowText(GetDlgItem(hDlg,IDC_TEXDUMPPATH),g_Config.texDumpPath);
		Edit_LimitText(GetDlgItem(hDlg,IDC_TEXDUMPPATH),255);
	}
	void Command(HWND hDlg,WPARAM wParam)
	{
		switch (LOWORD(wParam))
		{
		case IDC_BROWSETEXDUMPPATH:
			{
				std::string path = W32Util::BrowseForFolder(hDlg,"Choose texture dump path:");
				SetWindowText(GetDlgItem(hDlg,IDC_TEXDUMPPATH),path.c_str());
			}
			break;
		default:
			break;
		}
	}
	void Apply(HWND hDlg)
	{
		g_Config.bOverlayStats = Button_GetCheck(GetDlgItem(hDlg,IDC_OVERLAYSTATS)) ? true : false;
		g_Config.bWireFrame    = Button_GetCheck(GetDlgItem(hDlg,IDC_WIREFRAME)) ? true : false;
		g_Config.bDumpTextures = Button_GetCheck(GetDlgItem(hDlg,IDC_TEXDUMP)) ? true : false;
		g_Config.iCompileDLsLevel = (int)ComboBox_GetCurSel(GetDlgItem(hDlg,IDC_DLOPTLEVEL));
		g_Config.bShowShaderErrors = Button_GetCheck(GetDlgItem(hDlg,IDC_SHOWSHADERERRORS)) ? true : false;
		char temp[MAX_PATH];
		GetWindowText(GetDlgItem(hDlg,IDC_TEXDUMPPATH), temp, MAX_PATH);
		strcpy(g_Config.texDumpPath, temp);
	}
};

struct TabDebug : public W32Util::Tab
{
	void Init(HWND hDlg)
	{
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
	}
};

struct TabEnhancements : public W32Util::Tab
{
	void Init(HWND hDlg)
	{
		Button_SetCheck(GetDlgItem(hDlg,IDC_FORCEFILTERING),g_Config.bForceFiltering);
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
		g_Config.bForceFiltering = Button_GetCheck(GetDlgItem(hDlg,IDC_FORCEFILTERING)) ? true : false;
	}
};


void DlgSettings_Show(HINSTANCE hInstance, HWND _hParent)
{
	g_Config.Load();
	W32Util::PropSheet sheet;
	sheet.Add(new TabOGL,(LPCTSTR)IDD_SETTINGS,"Direct3D");
	sheet.Add(new TabEnhancements,(LPCTSTR)IDD_ENHANCEMENTS,"Enhancements");
	sheet.Add(new TabAdvanced,(LPCTSTR)IDD_ADVANCED,"Advanced");
	//sheet.Add(new TabDebug,(LPCTSTR)IDD_DEBUGGER,"Debugger");
	sheet.Show(hInstance,_hParent,"Graphics Plugin");
	g_Config.Save();
}

// Message handler for about box.
LRESULT CALLBACK AboutProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		W32Util::CenterWindow(hDlg);
		return TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) 
		{
			EndDialog(hDlg, LOWORD(wParam));
			return TRUE;
		}
		break;
	}
	return FALSE;
}
