#include <windowsx.h>

#include "resource.h"
#include "W32Util/PropertySheet.h"
#include "W32Util/ShellUtil.h"

#include "D3DBase.h"
#include "D3DPostprocess.h"

#include "Globals.h"

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
		for (int i=0; i<D3D::GetNumAdapters(); i++)
		{
			const D3D::Adapter &adapter = D3D::GetAdapter(i);
			ComboBox_AddString(GetDlgItem(hDlg,IDC_ADAPTER),adapter.ident.Description);
		}

		const D3D::Adapter &adapter = D3D::GetAdapter(g_Config.iAdapter);

		ComboBox_SetCurSel(GetDlgItem(hDlg, IDC_ADAPTER),g_Config.iAdapter);

		for (int i = 0; i < (int)adapter.aa_levels.size(); i++)
		{
			ComboBox_AddString(GetDlgItem(hDlg, IDC_ANTIALIASMODE), adapter.aa_levels[i].name);
		}

		ComboBox_SetCurSel(GetDlgItem(hDlg, IDC_ANTIALIASMODE), g_Config.iMultisampleMode);
		if (adapter.aa_levels.size() == 1) 
		{
			EnableWindow(GetDlgItem(hDlg, IDC_ANTIALIASMODE), FALSE);
		}

		for (int i = 0; i < (int)adapter.resolutions.size(); i++)
		{
			const D3D::Resolution &r = adapter.resolutions[i];
			ComboBox_AddString(GetDlgItem(hDlg,IDC_RESOLUTION), r.name);
		}
		ComboBox_SetCurSel(GetDlgItem(hDlg,IDC_RESOLUTION), g_Config.iFSResolution);

		for (int i = 0; i < NUMWNDRES; i++)
		{
			char temp[256];
			sprintf(temp,"%ix%i",g_Res[i][0],g_Res[i][1]);
			ComboBox_AddString(GetDlgItem(hDlg,IDC_RESOLUTIONWINDOWED),temp);
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

		Button_SetCheck(GetDlgItem(hDlg,IDC_TEXFMT_OVERLAY), g_Config.bTexFmtOverlayEnable);
		Button_SetCheck(GetDlgItem(hDlg,IDC_TEXFMT_CENTER),  g_Config.bTexFmtOverlayCenter);
		Button_GetCheck(GetDlgItem(hDlg,IDC_TEXFMT_OVERLAY)) ? Button_Enable(GetDlgItem(hDlg,IDC_TEXFMT_CENTER), true) : Button_Enable(GetDlgItem(hDlg,IDC_TEXFMT_CENTER), false);

		SetWindowText(GetDlgItem(hDlg,IDC_TEXDUMPPATH),g_Config.texDumpPath.c_str());
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
		g_Config.bWireFrame    = Button_GetCheck(GetDlgItem(hDlg,IDC_WIREFRAME)) ? true : false;
		g_Config.bDumpTextures = Button_GetCheck(GetDlgItem(hDlg,IDC_TEXDUMP)) ? true : false;
		g_Config.iCompileDLsLevel = (int)ComboBox_GetCurSel(GetDlgItem(hDlg,IDC_DLOPTLEVEL));
		g_Config.bShowShaderErrors = Button_GetCheck(GetDlgItem(hDlg,IDC_SHOWSHADERERRORS)) ? true : false;
		char temp[MAX_PATH];
		GetWindowText(GetDlgItem(hDlg,IDC_TEXDUMPPATH), temp, MAX_PATH);
		g_Config.texDumpPath = temp;
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
		Button_SetCheck(GetDlgItem(hDlg,IDC_FORCEANISOTROPY),g_Config.bForceMaxAniso);
		HWND pp = GetDlgItem(hDlg,IDC_POSTPROCESSEFFECT);
		const char **names = Postprocess::GetPostprocessingNames();
		int i = 0;
		while (true)
		{
			if (!names[i])
				break;

			ComboBox_AddString(pp, names[i]);
			i++;
		}
		ComboBox_SetCurSel(pp, g_Config.iPostprocessEffect);
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
	sheet.Add(new TabDirect3D,(LPCTSTR)IDD_SETTINGS,"Direct3D");
	sheet.Add(new TabEnhancements,(LPCTSTR)IDD_ENHANCEMENTS,"Enhancements");
	sheet.Add(new TabAdvanced,(LPCTSTR)IDD_ADVANCED,"Advanced");
	//sheet.Add(new TabDebug,(LPCTSTR)IDD_DEBUGGER,"Debugger");
	sheet.Show(hInstance,_hParent,"Graphics Plugin");
	g_Config.Save();

	if(( tfoe != g_Config.bTexFmtOverlayEnable) ||
		((g_Config.bTexFmtOverlayEnable) && ( tfoc != g_Config.bTexFmtOverlayCenter)))
	{
		TextureCache::Invalidate();
	}
}