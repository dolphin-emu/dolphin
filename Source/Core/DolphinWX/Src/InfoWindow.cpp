// Copyright (C) 2003-2009 Dolphin Project.

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
#include "InfoWindow.h"
#include "CPUDetect.h"
#include "Core.h"
#include "ConfigManager.h"
#include "CDUtils.h"
#include "OGLconfig.h"
#include "D3Dconfig.h"

BEGIN_EVENT_TABLE(wxInfoWindow, wxWindow)
	EVT_SIZE(                                            wxInfoWindow::OnEvent_Window_Resize)
	EVT_CLOSE(                                           wxInfoWindow::OnEvent_Window_Close)
    EVT_BUTTON(ID_BUTTON_CLOSE,                          wxInfoWindow::OnEvent_ButtonClose_Press)
END_EVENT_TABLE()

wxInfoWindow::wxInfoWindow(wxFrame* parent, const wxPoint& pos, const wxSize& size) :
	wxFrame(parent, wxID_ANY, _T("System Information"), pos, size, wxDEFAULT_FRAME_STYLE | wxNO_FULL_REPAINT_ON_RESIZE)
{
	// Create the GUI controls
	Init_ChildControls();

	// Setup Window
	SetBackgroundColour(wxColour(COLOR_GRAY));
	SetSize(size);
	SetPosition(pos);
	Layout();
	Show();
}

wxInfoWindow::~wxInfoWindow()
{
	// On Disposal
}

std::string Summarize_Plug()
{
	return StringFromFormat(
		"Default GFX Plugin: %s\n"
		"Default DSP Plugin: %s\n"
		"Default PAD Plugin: %s\n"
		"Default WiiMote Plugin: %s\n\n"
		"Current GFX Plugin: %s\n"
		"Current DSP Plugin: %s\n"
		"Current PAD Plugin[0]: %s\n"
		"Current PAD Plugin[1]: %s\n"
		"Current PAD Plugin[2]: %s\n"
		"Current PAD Plugin[3]: %s\n"
		"Current WiiMote Plugin[0]: %s\n",
		SConfig::GetInstance().m_DefaultGFXPlugin.c_str(),
		SConfig::GetInstance().m_DefaultDSPPlugin.c_str(),
		SConfig::GetInstance().m_DefaultPADPlugin.c_str(),
		SConfig::GetInstance().m_DefaultWiiMotePlugin.c_str(),
		Core::GetStartupParameter().m_strVideoPlugin.c_str(),
		Core::GetStartupParameter().m_strDSPPlugin.c_str(),
		Core::GetStartupParameter().m_strPadPlugin[0].c_str(),
		Core::GetStartupParameter().m_strPadPlugin[1].c_str(),
		Core::GetStartupParameter().m_strPadPlugin[2].c_str(),
		Core::GetStartupParameter().m_strPadPlugin[3].c_str(),
		Core::GetStartupParameter().m_strWiimotePlugin[0].c_str()
		);

}

std::string Summarize_Settings()
{
	return StringFromFormat(
		"Dolphin Settings\n\n"
		"Always HLE Bios: %s\n"
		"Use Dynarec: %s\n"
		"Use Dual Core: %s\n"
		"DSP Thread: %s\n"
		"Skip Idle: %s\n"
		"Lock Threads: %s\n"
		"Use Dual Core: %s\n"
		"Default GCM: %s\n"
		"DVD Root: %s\n"
		"Optimize Quantizers: %s\n"
		"Enable Cheats: %s\n"
		"Selected Language: %d\n"
		"Memcard A: %s\n"
		"Memcard B: %s\n"
		"Slot A: %d\n"
		"Slot B: %d\n"
		"Serial Port 1: %d\n"
		"[Wii]Widescreen: %s\n"
		"[Wii]Progressive Scan: %s\n"
		"Run Compare Server: %s\n"
		"Run Compare Client: %s\n"
		"TLB Hack: %s\n",

		Core::GetStartupParameter().bHLEBios?"True":"False",
		Core::GetStartupParameter().bUseJIT?"True":"False",
		Core::GetStartupParameter().bUseDualCore?"True":"False",
		Core::GetStartupParameter().bDSPThread?"True":"False",
		Core::GetStartupParameter().bSkipIdle?"True":"False",
		Core::GetStartupParameter().bLockThreads?"True":"False",
		Core::GetStartupParameter().bUseDualCore?"True":"False",
		Core::GetStartupParameter().m_strDefaultGCM.c_str(),
		Core::GetStartupParameter().m_strDVDRoot.c_str(),
		Core::GetStartupParameter().bOptimizeQuantizers?"True":"False",
		Core::GetStartupParameter().bEnableCheats?"True":"False",
		Core::GetStartupParameter().SelectedLanguage, //FIXME show language based on index
		SConfig::GetInstance().m_strMemoryCardA.c_str(),
		SConfig::GetInstance().m_strMemoryCardB.c_str(),
		SConfig::GetInstance().m_EXIDevice[0], //FIXME
		SConfig::GetInstance().m_EXIDevice[1], //FIXME
		SConfig::GetInstance().m_EXIDevice[2], //FIXME
		Core::GetStartupParameter().bWidescreen?"True":"False",
		Core::GetStartupParameter().bProgressiveScan?"True":"False",
		Core::GetStartupParameter().bRunCompareServer?"True":"False",
		Core::GetStartupParameter().bRunCompareClient?"True":"False",
		Core::GetStartupParameter().iTLBHack?"True":"False"//Fix me(Ment to be interger but caues problems with no ?"True":"False")
		);
}

std::string Summarize_OpenGlPlug()
{
	OGLConfig OGLconfig;
	OGLconfig.Load();
	return StringFromFormat(
		"OpenGl Plugin Settings\n\n"
//General
		"Fullscreen: %s\n"
		"Hide Cursor: %s\n"
		"Render to main frame: %s\n"
		"V Sync: %s\n"
//Resolution control

//char iFSResolution[16];
//char iWindowedRes[16];

        "Use Native Resolution: %s\n"
		"Keep 4:3 Aspect Ratio: %s\n"
		"Keep 16:9 Aspect Ratio: %s\n"
		"Crop screen: %s\n"
//Enhancements
		"Multi Sample Mode: %s\n"
		"Force Filtering: %s\n"
		"Max Anisotropy: %s\n"
//Information
        "Show FPS: %s\n"
		"Overlay Stats: %s\n"
		"Overlay Blend Stats: %s\n"
		"Overlay Proj Stats: %s\n"
		"Tex Fmt Overlay Enable: %s\n"
		"Tex Fmt Overlay Center: %s\n"
		"Show EFB Copy Regions: %s\n"
//Render
        "Wire Frame: %s\n"
		"Disable Lighting: %s\n"
		"Disable Texturing: %s\n"
		"Dst Alpha Pass: %s\n"
		"Disable Fog: %s\n"
//Utility
		"Dump Textures: %s\n"
		"Dump EFB Target: %s\n"
		"Dump Frames: %s\n"
		"Free Look: %s\n"
//Hacks
		"EFB Copy Disable: %s\n"
		"EFB Copy Disable Hot Key: %s\n"
		"Projection Hax 1: %s\n"
		"SMG hack: %s\n"
		"Dump Frames: %s\n"
		"Free Look: %s\n",

//--------------------------------------\\
///////////////////\\\\\\\\\\\\\\\\\\\\\\\


//General
		OGLconfig.bFullscreen?"True":"False",
		OGLconfig.bHideCursor?"True":"False",
		OGLconfig.renderToMainframe?"True":"False",
		OGLconfig.bVSync?"True":"False",
//Resolution control
//char iFSResolution[16];
//char iWindowedRes[16];
		OGLconfig.bNativeResolution?"True":"False",
		OGLconfig.bKeepAR43?"True":"False",
		OGLconfig.bKeepAR169?"True":"False",
		OGLconfig.bCrop?"True":"False",
		OGLconfig.bAutoScale?"True":"False",
//Enhancements
		OGLconfig.iMultisampleMode?"True":"False",//Fix me(Ment to be interger but caues problems with no ?"True":"False")
		OGLconfig.bForceFiltering?"True":"False",
		OGLconfig.iMaxAnisotropy?"True":"False",//Fix me(Ment to be interger but caues problems with no ?"True":"False")
//Information
		OGLconfig.bShowFPS?"True":"False",
		OGLconfig.bOverlayStats?"True":"False",
		OGLconfig.bOverlayBlendStats?"True":"False",
		OGLconfig.bOverlayProjStats?"True":"False",
		OGLconfig.bTexFmtOverlayEnable?"True":"False",
		OGLconfig.bTexFmtOverlayCenter?"True":"False",
		OGLconfig.bShowEFBCopyRegions?"True":"False",
//Render
		OGLconfig.bWireFrame?"True":"False",
		OGLconfig.bDisableTexturing?"True":"False",
		OGLconfig.bDstAlphaPass?"True":"False",
		OGLconfig.bDisableFog?"True":"False",
//Utility
		OGLconfig.bDumpTextures?"True":"False",
		OGLconfig.bDumpEFBTarget?"True":"False",
		OGLconfig.bDumpFrames?"True":"False",		
		OGLconfig.bFreeLook?"True":"False",
//Hacks
		OGLconfig.bEFBCopyDisable?"True":"False",
		OGLconfig.bEFBCopyDisableHotKey?"True":"False",
		OGLconfig.bProjectionHax1?"True":"False",
		OGLconfig.bSMGhack?"True":"False",
		OGLconfig.bCopyEFBToRAM?"True":"False",		
		OGLconfig.bSafeTextureCache?"True":"False"
		);
}

std::string Summarize_D3DPlug()
{
	D3DConfig D3Dconfig;

	D3Dconfig.Load();

	return StringFromFormat(
		"D3D Plugin Settings\n\n"

		"Adapter: %s\n"
		"Full Screen Resolution: %s\n"
		"Post Process Effect: %s\n"
		"Render to main frame: %s\n"
		"Full Screen: %s\n"
		"Vsync: %s\n"
		"Wire frame: %s\n"
		"Overlay Stats: %s\n"
		"Overlay Proj Stats: %s\n"
		"Dump Textures: %s\n"
		"Dump Frames: %s\n"
		"Old Card: %s\n"
		"Show Shader Errors: %s\n"
		"Force Max Aniso: %s\n"
		"Pre Upscale: %s\n"
		"Pre Upscale Filter: %s\n"
		"True form: %s\n"
		"True form Level: %s\n"
		"Window Resolution: %s\n"
//psProfile[16]
//vsProfile[16]
		"Tex Fmt Overlay Enable: %s\n"
		"Tex Fmt Overlay Center: %s\n",
//--------------------------------------\\

		D3Dconfig.iAdapter?"True":"False",
		D3Dconfig.iFSResolution?"True":"False",
		D3Dconfig.iMultisampleMode?"True":"False",
		D3Dconfig.iPostprocessEffect?"True":"False",
		//Above have problems since they use integers and i have no idea how to get the working properly removing ?"True":"False" creates problems
		D3Dconfig.renderToMainframe?"True":"False",
		D3Dconfig.bFullscreen?"True":"False",
		D3Dconfig.bVsync?"True":"False",
		D3Dconfig.bWireFrame?"True":"False",
		D3Dconfig.bOverlayStats?"True":"False",
		D3Dconfig.bOverlayProjStats?"True":"False",
		D3Dconfig.bDumpTextures?"True":"False",
		D3Dconfig.bDumpFrames?"True":"False",
		D3Dconfig.bOldCard?"True":"False",
		D3Dconfig.bShowShaderErrors?"True":"False",
		//Enhancements
		D3Dconfig.bForceFiltering?"True":"False",
		D3Dconfig.bForceMaxAniso?"True":"False",
		D3Dconfig.bPreUpscale?"True":"False",
		D3Dconfig.iPreUpscaleFilter?"True":"False",//May not work because of integar removing ?"True":"False" causes problems
		D3Dconfig.bTruform?"True":"False",
	    D3Dconfig.iTruformLevel?"True":"False",//May not work because of integar removing ?"True":"False" causes problems
	    D3Dconfig.iWindowedRes?"True":"False",//May not work because of integar removing ?"True":"False" causes problems
//psProfile[16]
//vsProfile[16]
		D3Dconfig.bTexFmtOverlayEnable?"True":"False",
		D3Dconfig.bTexFmtOverlayCenter?"True":"False"
		);
}

void wxInfoWindow::Init_ChildControls()
{
	std::string Info;
	Info = StringFromFormat("Dolphin Revision: %s", SVN_REV_STR);

	char ** drives = cdio_get_devices();
	for (int i = 0; drives[i] != NULL && i < 24; i++)
	{

		Info.append(StringFromFormat("\nCD/DVD Drive %d: %s", i+1, drives[i]));
	}
	Info.append(StringFromFormat(
		"\n\nPlugin Information\n\n%s\n%s\n%s\n%s\n"
		"Processor Information:\n%s\n",
		Summarize_Plug().c_str(),
		Summarize_Settings().c_str(),
		Summarize_OpenGlPlug().c_str(),
		Summarize_D3DPlug().c_str(),
		cpu_info.Summarize().c_str()));
	
	// Main Notebook
	m_Notebook_Main = new wxNotebook(this, ID_NOTEBOOK_MAIN, wxDefaultPosition, wxDefaultSize);
		// --- Tabs ---

		// $ Log Tab
		m_Tab_Log = new wxPanel(m_Notebook_Main, ID_TAB_LOG, wxDefaultPosition, wxDefaultSize);
		m_TextCtrl_Log = new wxTextCtrl(m_Tab_Log, ID_TEXTCTRL_LOG, wxString::FromAscii(Info.c_str()), wxDefaultPosition, wxSize(100, 600),
										wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
		wxBoxSizer *HStrip1 = new wxBoxSizer(wxHORIZONTAL);
		wxBoxSizer *sTabLog = new wxBoxSizer(wxVERTICAL);
		sTabLog->Add(HStrip1, 0, wxALL, 5);
		sTabLog->Add(m_TextCtrl_Log, 1, wxALL|wxEXPAND, 5);

		m_Tab_Log->SetSizer(sTabLog);
		m_Tab_Log->Layout();

	// Add Tabs to Notebook
	m_Notebook_Main->AddPage(m_Tab_Log, _T("System Information"));

	// Button Strip
	m_Button_Close = new wxButton(this, ID_BUTTON_CLOSE, _T("Close"), wxDefaultPosition, wxDefaultSize);
	wxBoxSizer* sButtons = new wxBoxSizer(wxHORIZONTAL);
	sButtons->Add(m_Button_Close, 0, wxALL, 5);

	wxBoxSizer* sMain = new wxBoxSizer(wxVERTICAL);
	sMain->Add(m_Notebook_Main, 1, wxEXPAND|wxALL, 5);
	sMain->Add(sButtons, 0, wxALL, 5);
	SetSizer(sMain);
	Layout();

	Fit();
}



void wxInfoWindow::OnEvent_Window_Resize(wxSizeEvent& WXUNUSED (event))
{
	Layout();
}
void wxInfoWindow::OnEvent_ButtonClose_Press(wxCommandEvent& WXUNUSED (event))
{
	Destroy();
}
void wxInfoWindow::OnEvent_Window_Close(wxCloseEvent& WXUNUSED (event))
{
	Destroy();
}


