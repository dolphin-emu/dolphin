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

#include <wx/wx.h>
#include <wx/sizer.h>
#include <wx/filepicker.h>
#include <wx/gbsizer.h>
#include <wx/notebook.h>
#include <wx/mimetype.h>

#include "ConfigDlg.h"
#include "../Globals.h"
#include "VideoConfig.h"
#include "../TextureMngr.h"
#include "VertexShaderManager.h"
#include "../PostProcessing.h"
#include "Render.h"

#include "FileUtil.h"

BEGIN_EVENT_TABLE(GFXConfigDialogOGL,wxDialog)
	EVT_CLOSE(GFXConfigDialogOGL::OnClose)
	EVT_BUTTON(wxID_CLOSE, GFXConfigDialogOGL::CloseClick)
	EVT_BUTTON(wxID_ABOUT, GFXConfigDialogOGL::AboutClick)
	EVT_CHECKBOX(ID_VSYNC, GFXConfigDialogOGL::GeneralSettingsChanged)
	EVT_CHOICE(ID_FULLSCREENRESOLUTION, GFXConfigDialogOGL::GeneralSettingsChanged)
	EVT_CHOICE(ID_MAXANISOTROPY, GFXConfigDialogOGL::GeneralSettingsChanged)
	EVT_CHOICE(ID_MSAAMODECB, GFXConfigDialogOGL::GeneralSettingsChanged)
	EVT_CHECKBOX(ID_NATIVERESOLUTION, GFXConfigDialogOGL::GeneralSettingsChanged)
	EVT_CHECKBOX(ID_2X_RESOLUTION, GFXConfigDialogOGL::GeneralSettingsChanged)
	EVT_CHECKBOX(ID_USEXFB, GFXConfigDialogOGL::GeneralSettingsChanged)
	EVT_CHECKBOX(ID_USEREALXFB, GFXConfigDialogOGL::GeneralSettingsChanged)
	EVT_CHECKBOX(ID_FORCEFILTERING, GFXConfigDialogOGL::GeneralSettingsChanged)
	EVT_CHECKBOX(ID_AUTOSCALE, GFXConfigDialogOGL::GeneralSettingsChanged)
	EVT_CHECKBOX(ID_WIDESCREENHACK, GFXConfigDialogOGL::GeneralSettingsChanged)
	EVT_CHOICE(ID_ASPECT, GFXConfigDialogOGL::GeneralSettingsChanged)
	EVT_CHECKBOX(ID_CROP, GFXConfigDialogOGL::GeneralSettingsChanged)
	EVT_CHECKBOX(ID_WIREFRAME, GFXConfigDialogOGL::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_SHOWFPS, GFXConfigDialogOGL::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_STATISTICS, GFXConfigDialogOGL::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_PROJSTATS, GFXConfigDialogOGL::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_SHOWEFBCOPYREGIONS, GFXConfigDialogOGL::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_SHADERERRORS, GFXConfigDialogOGL::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_TEXFMTOVERLAY, GFXConfigDialogOGL::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_TEXFMTCENTER, GFXConfigDialogOGL::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_DUMPTEXTURES, GFXConfigDialogOGL::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_HIRESTEXTURES, GFXConfigDialogOGL::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_DUMPEFBTARGET, GFXConfigDialogOGL::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_DUMPFRAMES, GFXConfigDialogOGL::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_FREELOOK, GFXConfigDialogOGL::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_DISABLELIGHTING, GFXConfigDialogOGL::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_DISABLETEXTURING, GFXConfigDialogOGL::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_DISABLEFOG, GFXConfigDialogOGL::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_OSDHOTKEY, GFXConfigDialogOGL::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_HACK, GFXConfigDialogOGL::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_SAFETEXTURECACHE,GFXConfigDialogOGL::AdvancedSettingsChanged)
	EVT_RADIOBUTTON(ID_RADIO_SAFETEXTURECACHE_SAFE, GFXConfigDialogOGL::AdvancedSettingsChanged)
	EVT_RADIOBUTTON(ID_RADIO_SAFETEXTURECACHE_NORMAL, GFXConfigDialogOGL::AdvancedSettingsChanged)
	EVT_RADIOBUTTON(ID_RADIO_SAFETEXTURECACHE_FAST, GFXConfigDialogOGL::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_DSTALPHAPASS,GFXConfigDialogOGL::AdvancedSettingsChanged)
	EVT_CHECKBOX(ID_CHECKBOX_DISABLECOPYEFB, GFXConfigDialogOGL::AdvancedSettingsChanged)
	EVT_RADIOBUTTON(ID_RADIO_COPYEFBTORAM, GFXConfigDialogOGL::AdvancedSettingsChanged)
	EVT_RADIOBUTTON(ID_RADIO_COPYEFBTOGL, GFXConfigDialogOGL::AdvancedSettingsChanged)
	EVT_CHOICE(ID_PHACKVALUE, GFXConfigDialogOGL::GeneralSettingsChanged)
	EVT_CHOICE(ID_POSTSHADER, GFXConfigDialogOGL::GeneralSettingsChanged)
	EVT_BUTTON(ID_RELOADSHADER, GFXConfigDialogOGL::ReloadShaderClick)
	EVT_BUTTON(ID_EDITSHADER, GFXConfigDialogOGL::EditShaderClick)
END_EVENT_TABLE()

GFXConfigDialogOGL::GFXConfigDialogOGL(wxWindow *parent, wxWindowID id, const wxString &title, const wxPoint &position, const wxSize& size, long style)
: wxDialog(parent, id, title, position, size, style)
{
}


// Close and unload the window
// ---------------
GFXConfigDialogOGL::~GFXConfigDialogOGL()
{
	INFO_LOG(CONSOLE, "GFXConfigDialogOGL Closed");
}

void GFXConfigDialogOGL::OnClose(wxCloseEvent& event)
{
	//INFO_LOG(CONSOLE, "OnClose");

	// notice that we don't run wxEntryCleanup(); here so the dll will still be loaded
	/* JP: Yes, it seems like Close() does not do that. It only runs EndModal() or something
	    similar to hide the window. And I don't understand the "Window deletion overview" on
		the wxWidgets website. Destroy() doesn't run the destructor either. However running
		wxEntryCleanup() from here crashes the process. But we can run it from CloseClick() */
	//wxEntryCleanup();
	//EndModal(0);

	// Allow wxWidgets to close and unload the window
	//event.Skip();
	CloseWindow();
}

void GFXConfigDialogOGL::CloseClick(wxCommandEvent& WXUNUSED (event))
{
	//INFO_LOG(CONSOLE, "CloseClick");

	// If we run wxEntryCleanup() the class will be entirely deleted, and the destructor will be run
	//g_Config.Save();
	//wxEntryCleanup();

	//Close();
	CloseWindow();
}



// Add avaliable resolutions and other settings
// ---------------
void GFXConfigDialogOGL::AddFSReso(const char *reso)
{
	arrayStringFor_FullscreenCB.Add(wxString::FromAscii(reso));
}

// This one could be used to reload shaders while dolphin is running...
void GFXConfigDialogOGL::LoadShaders()
{
	arrayStringFor_PostShaderCB.Clear();

	arrayStringFor_PostShaderCB.Add(wxT("(off)"));
	if (File::IsDirectory(File::GetUserPath(D_SHADERS_IDX)))
	{
		File::FSTEntry entry;
		File::ScanDirectoryTree(File::GetUserPath(D_SHADERS_IDX), entry);
		for (u32 i = 0; i < entry.children.size(); i++) 
		{
			std::string name = entry.children[i].virtualName.c_str();
			if (!strcasecmp(name.substr(name.size() - 4).c_str(), ".txt"))
				name = name.substr(0, name.size() - 4);
			arrayStringFor_PostShaderCB.Add(wxString::FromAscii(name.c_str()));
		}
	}
	else
	{
		File::CreateDir(File::GetUserPath(D_SHADERS_IDX));
	}
}

void GFXConfigDialogOGL::InitializeGUILists()
{
	// Resolutions
	if (arrayStringFor_FullscreenCB.empty())
		AddFSReso("<No resolutions found>");

	// Keep Aspect Ratio
	arrayStringFor_AspectRatio.Add(wxT("Auto Aspect (recommended)"));
	arrayStringFor_AspectRatio.Add(wxT("Force 16:9 Widescreen"));
	arrayStringFor_AspectRatio.Add(wxT("Force 4:3 Standard"));
	arrayStringFor_AspectRatio.Add(wxT("Stretch to Window"));

	// Antialias (MSAA)
	arrayStringFor_MSAAModeCB.Add(wxT("(off)"));
	arrayStringFor_MSAAModeCB.Add(wxT("2x"));
	arrayStringFor_MSAAModeCB.Add(wxT("4x"));
	arrayStringFor_MSAAModeCB.Add(wxT("8x"));
	arrayStringFor_MSAAModeCB.Add(wxT("8x CSAA"));
	arrayStringFor_MSAAModeCB.Add(wxT("8xQ CSAA"));
	arrayStringFor_MSAAModeCB.Add(wxT("16x CSAA"));
	arrayStringFor_MSAAModeCB.Add(wxT("16xQ CSAA"));

	// Anisotropic filter
	arrayStringFor_MaxAnisotropyCB.Add(wxT("1x"));
	arrayStringFor_MaxAnisotropyCB.Add(wxT("2x"));
	arrayStringFor_MaxAnisotropyCB.Add(wxT("4x"));
	arrayStringFor_MaxAnisotropyCB.Add(wxT("8x"));
	arrayStringFor_MaxAnisotropyCB.Add(wxT("16x"));

	// Post-processing shader
	LoadShaders();

	// Hacks
	arrayStringFor_PhackvalueCB.Add(wxT("None"));
	arrayStringFor_PhackvalueCB.Add(wxT("Zelda Twilight Princess Bloom hack"));
	arrayStringFor_PhackvalueCB.Add(wxT("Sonic and the Black Knight"));
	arrayStringFor_PhackvalueCB.Add(wxT("Bleach Versus Crusade"));
	arrayStringFor_PhackvalueCB.Add(wxT("Skies of Arcadia"));
}

void GFXConfigDialogOGL::InitializeGUIValues()
{
	// General Display Settings
	m_NativeResolution->SetValue(g_Config.bNativeResolution);
	m_2xResolution->SetValue(g_Config.b2xResolution);
	
	int num = 0;
	num = m_WindowFSResolutionCB->FindString(wxString::FromAscii(g_Config.cFSResolution));
	m_WindowFSResolutionCB->SetSelection(num);
	m_KeepAR->SetSelection(g_Config.iAspectRatio);
	m_Crop->SetValue(g_Config.bCrop);

	// Advanced Display Settings
	m_OSDHotKey->SetValue(g_Config.bOSDHotKey);
	m_VSync->SetValue(g_Config.bVSync);
	m_UseXFB->SetValue(g_Config.bUseXFB);
	m_UseRealXFB->SetValue(g_Config.bUseRealXFB);
	m_AutoScale->SetValue(g_Config.bAutoScale);
	m_WidescreenHack->SetValue(g_Config.bWidescreenHack);

	// Enhancements
	m_MaxAnisotropyCB->SetSelection(g_Config.iMaxAnisotropy - 1);
	m_ForceFiltering->SetValue(g_Config.bForceFiltering);
	m_MSAAModeCB->SetSelection(g_Config.iMultisampleMode);

	wxString shader = wxString::FromAscii(g_Config.sPostProcessingShader.c_str());
	if (shader == _(""))
		shader = wxT("(off)");
	m_PostShaderCB->SetStringSelection(shader);

	// Information
	m_ShowFPS->SetValue(g_Config.bShowFPS);
	//m_ShaderErrors->SetValue(g_Config.bShowShaderErrors);
	m_Statistics->SetValue(g_Config.bOverlayStats);
	m_ProjStats->SetValue(g_Config.bOverlayProjStats);
	m_ShowEFBCopyRegions->SetValue(g_Config.bShowEFBCopyRegions);
	m_TexFmtOverlay->SetValue(g_Config.bTexFmtOverlayEnable);
	m_TexFmtCenter->SetValue(g_Config.bTexFmtOverlayCenter);
	m_TexFmtCenter->Enable(m_TexFmtOverlay->IsChecked());

	// Render
	m_Wireframe->SetValue(g_Config.bWireFrame);
	m_DisableLighting->SetValue(g_Config.bDisableLighting);
	m_DisableTexturing->SetValue(g_Config.bDisableTexturing);
	m_DstAlphaPass->SetValue(g_Config.bDstAlphaPass);
	m_DisableFog->SetValue(g_Config.bDisableFog);

	m_CheckBox_DisableCopyEFB->SetValue(g_Config.bEFBCopyDisable);
	g_Config.bCopyEFBToTexture ? m_Radio_CopyEFBToGL->SetValue(true) : m_Radio_CopyEFBToRAM->SetValue(true);

	// Utility
	m_DumpTextures->SetValue(g_Config.bDumpTextures);
	m_HiresTextures->SetValue(g_Config.bHiresTextures);
	m_DumpEFBTarget->SetValue(g_Config.bDumpEFBTarget);
	m_DumpFrames->SetValue(g_Config.bDumpFrames);
	m_FreeLook->SetValue(g_Config.bFreeLook);

	// Hacks controls
	m_PhackvalueCB->SetSelection(g_Config.iPhackvalue);
	m_SafeTextureCache->SetValue(g_Config.bSafeTextureCache);
	if(g_Config.iSafeTextureCache_ColorSamples == 0)
		m_Radio_SafeTextureCache_Safe->SetValue(true);
	else
		if(g_Config.iSafeTextureCache_ColorSamples > 128)
			m_Radio_SafeTextureCache_Normal->SetValue(true);
		else
			m_Radio_SafeTextureCache_Fast->SetValue(true);
}

void GFXConfigDialogOGL::InitializeGUITooltips()
{
	// Tool tips
	m_NativeResolution->SetToolTip(
		wxT("This will use the game's native resolution and stretch it to fill the")
		wxT("\nwindow instead of changing the internal display resolution. It")
		wxT("\nmay result in a blurrier image, but it may also give a higher")
		wxT("\nFPS if you have a slow graphics card.")
		wxT("\n\nApplies instanty during gameplay: <Yes>"));
	m_2xResolution->SetToolTip(wxT(
		"Applies instanty during gameplay: <Yes, if allowed>"));
	m_KeepAR->SetToolTip(
		wxT("This sets the aspect ratio of the image.")
		wxT("\nThe Widescreen hack may cause graphical issues in some games !")
		wxT("\n\nApplies instanty during gameplay: <Yes>"));
	m_Crop->SetToolTip(
		wxT("Crop the picture instead of creating a letterbox. It will assume that your screen")
		wxT("\nis of the 5:4 format if you have selected the 4:3 aspect ratio. It will assume")
		wxT("\nthat your screen is of the 16:10 format if you have selected the 16:9 aspect ratio.")
		wxT("\n\nApplies instanty during gameplay: <Yes>"));
	m_WindowFSResolutionCB->SetToolTip(
		wxT("Select resolution for fullscreen mode")
		wxT("\n\nApplies instantly during gameplay: <No>"));
	m_MSAAModeCB->SetToolTip(wxT(
		"Applies instanty during gameplay: <No>"));
	m_OSDHotKey->SetToolTip(
		wxT("Enable OSD hotkeys '3', '4', '5', '6' and '7' to easily toggle some settings."));

	// Enhancements
	m_ForceFiltering->SetToolTip(
		wxT("Even though it will increase the IQ, it will also break some EFB effects\n")
		wxT("such as Bink FMV in many Wii games or the goo in Mario Sunshine, so be careful :)"));

	// Render
	m_DstAlphaPass->SetToolTip(wxT("This renders a second time to set alpha to a constant value,")
		wxT("\nDisabling it may speed up some games, but could also cause glitches."));
	m_DisableFog->SetToolTip(wxT("This option should not require a restart."));
	m_CheckBox_DisableCopyEFB->SetToolTip(wxT("This may lead to a higher FPS in for example Zelda - TP.")
		wxT(" But it may also cause graphical errors and missing graphics."));
	m_Radio_CopyEFBToRAM->SetToolTip(wxT("[This option will apply immediately and does not require a restart to take effect.]"));
	m_Radio_CopyEFBToGL->SetToolTip(wxT("[This option will apply immediately and does not require a restart to take effect.]"));
	
	// Utility
#ifdef _WIN32
	m_DumpFrames->SetToolTip(
		wxT("When dumping begins, you will be prompted to choose a video codec to")
		wxT(" encode the video in."));
#else
	m_DumpFrames->SetToolTip(wxT(
		"!!WARNING!! This option dumps raw bitmaps of each frame, and will fill up"
		" your hard drive very quickly. Only turn this on if you have a named pipe"
		" set up for the dump or several gigabytes of space available."));
#endif
	m_FreeLook->SetToolTip(
		wxT("Use WASD to move around, 0 and 9 to move faster or slower, and the")
		wxT(" left mouse button to pan the view."));

	// Hacks controls
	m_SafeTextureCache->SetToolTip(wxT("This is useful to prevent Metroid Prime from crashing, but can cause problems in other games.")
		wxT("\n[This option will apply immediately and does not require a restart. However it may not")
		wxT(" be entirely safe to change it midgames.]"));
	m_Radio_SafeTextureCache_Safe->SetToolTip(
		wxT("[This option will apply immediately and does not require a restart to take effect.]"));
	m_Radio_SafeTextureCache_Normal->SetToolTip(
		wxT("[This option will apply immediately and does not require a restart to take effect.]"));
	m_Radio_SafeTextureCache_Fast->SetToolTip(
		wxT("[This option will apply immediately and does not require a restart to take effect.]"));
}

void GFXConfigDialogOGL::CreateGUIControls()
{
	InitializeGUILists();

	// Notebook
	m_Notebook = new wxNotebook(this, ID_NOTEBOOK, wxDefaultPosition, wxDefaultSize);
	m_PageGeneral = new wxPanel(m_Notebook, ID_PAGEGENERAL, wxDefaultPosition, wxDefaultSize);
	m_Notebook->AddPage(m_PageGeneral, wxT("General"));
	m_PageAdvanced = new wxPanel(m_Notebook, ID_PAGEADVANCED, wxDefaultPosition, wxDefaultSize);
	m_Notebook->AddPage(m_PageAdvanced, wxT("Advanced"));

	// Buttons
	m_About = new wxButton(this, wxID_ABOUT, wxT("About"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Close = new wxButton(this, wxID_CLOSE, wxT("Close"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

	// Put notebook and buttons in sizers
	wxBoxSizer* sButtons;
	sButtons = new wxBoxSizer(wxHORIZONTAL);
	sButtons->Add(m_About, 0, wxALL, 5);
	sButtons->AddStretchSpacer();
	sButtons->Add(m_Close, 0, wxALL, 5);

	wxBoxSizer* sMain;
	sMain = new wxBoxSizer(wxVERTICAL);
	sMain->Add(m_Notebook, 1, wxEXPAND|wxALL, 5);
	sMain->Add(sButtons, 0, wxEXPAND, 5);

	this->SetSizer(sMain);
	this->Layout();

	// General Display Settings
	sbBasic = new wxStaticBoxSizer(wxVERTICAL, m_PageGeneral, wxT("Basic Display Settings"));
	wxStaticText *IRText = new wxStaticText(m_PageGeneral, wxID_ANY, wxT("Resolution:"), wxDefaultPosition, wxDefaultSize, 0);
	m_NativeResolution = new wxCheckBox(m_PageGeneral, ID_NATIVERESOLUTION, wxT("Native"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_2xResolution = new wxCheckBox(m_PageGeneral, ID_2X_RESOLUTION, wxT("2x"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	wxStaticText *RText = new wxStaticText(m_PageGeneral, wxID_ANY, wxT("Fullscreen Display Resolution:"), wxDefaultPosition, wxDefaultSize, 0);
	m_WindowFSResolutionCB = new wxChoice(m_PageGeneral, ID_FULLSCREENRESOLUTION, wxDefaultPosition, wxDefaultSize, arrayStringFor_FullscreenCB, 0, wxDefaultValidator, arrayStringFor_FullscreenCB[0]);
	// Aspect ratio / positioning controls
	wxStaticText *KeepARText = new wxStaticText(m_PageGeneral, wxID_ANY, wxT("Keep aspect ratio:"), wxDefaultPosition, wxDefaultSize, 0);
	m_KeepAR = new wxChoice(m_PageGeneral, ID_ASPECT, wxDefaultPosition, wxDefaultSize, arrayStringFor_AspectRatio);
	m_Crop = new wxCheckBox(m_PageGeneral, ID_CROP, wxT("Crop"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

	// Advanced Display Settings
	sbBasicAdvanced = new wxStaticBoxSizer(wxVERTICAL, m_PageGeneral, wxT("Advanced Display Settings"));
	m_OSDHotKey = new wxCheckBox(m_PageGeneral, ID_OSDHOTKEY, wxT("Enable Hotkeys"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
#if !defined(_WIN32) && (!defined(HAVE_X11) || !HAVE_X11)
	// JPeterson set the hot key to be Win32-specific
	// Now linux has this (with X11)
	m_OSDHotKey->Enable(false);
#endif
	m_VSync = new wxCheckBox(m_PageGeneral, ID_VSYNC, wxT("VSync (req. restart)"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_UseXFB = new wxCheckBox(m_PageGeneral, ID_USEXFB, wxT("Use XFB"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_UseRealXFB = new wxCheckBox(m_PageGeneral, ID_USEREALXFB, wxT("Use Real XFB"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_AutoScale = new wxCheckBox(m_PageGeneral, ID_AUTOSCALE, wxT("Auto scale (try to remove borders)"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_WidescreenHack = new wxCheckBox(m_PageGeneral, ID_WIDESCREENHACK, wxT("Wide screen hack"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

	// Enhancements
	sbEnhancements = new wxStaticBoxSizer(wxVERTICAL, m_PageGeneral, wxT("Enhancements"));
	wxStaticText *AnisoText = new wxStaticText(m_PageGeneral, ID_MAXANISOTROPYTEXT, wxT("Anisotropic filter:"), wxDefaultPosition, wxDefaultSize, 0);
	m_MaxAnisotropyCB = new wxChoice(m_PageGeneral, ID_MAXANISOTROPY, wxDefaultPosition, wxDefaultSize, arrayStringFor_MaxAnisotropyCB, 0, wxDefaultValidator);
	m_ForceFiltering = new wxCheckBox(m_PageGeneral, ID_FORCEFILTERING, wxT("Force Linear filter (!)"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	wxStaticText *MSAAText = new wxStaticText(m_PageGeneral, ID_MSAAMODETEXT, wxT("Antialias (MSAA):"), wxDefaultPosition, wxDefaultSize, 0);
	m_MSAAModeCB = new wxChoice(m_PageGeneral, ID_MSAAMODECB, wxDefaultPosition, wxDefaultSize, arrayStringFor_MSAAModeCB, 0, wxDefaultValidator);
	wxStaticText *PostShaderText = new wxStaticText(m_PageGeneral, ID_POSTSHADERTEXT, wxT("Post-processing shader:"), wxDefaultPosition, wxDefaultSize, 0);
	m_PostShaderCB = new wxChoice(m_PageGeneral, ID_POSTSHADER, wxDefaultPosition, wxDefaultSize, arrayStringFor_PostShaderCB, 0, wxDefaultValidator);
	m_ReloadShader = new wxButton(m_PageGeneral, ID_RELOADSHADER, wxT("&Reload"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_EditShader = new wxButton(m_PageGeneral, ID_EDITSHADER, wxT("&Edit"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

	// Sizers
	sGeneral = new wxBoxSizer(wxVERTICAL);
	sBasic = new wxGridBagSizer(0, 0);

	sBasic->Add(IRText, wxGBPosition(0, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL | wxALL, 5);
	sBasic->Add(m_NativeResolution, wxGBPosition(0, 1), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL | wxALL, 5);
	sBasic->Add(m_2xResolution, wxGBPosition(0, 2), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL | wxALL, 5);

	sBasic->Add(RText, wxGBPosition(1, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL | wxALL, 5);
	sBasic->Add(m_WindowFSResolutionCB, wxGBPosition(1, 1), wxGBSpan(1, 1), wxALL, 5);

	sBasic->Add(KeepARText, wxGBPosition(2, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL | wxALL, 5);
	sBasic->Add(m_KeepAR, wxGBPosition(2, 1), wxGBSpan(1, 1), wxALL, 5);
	sBasic->Add(m_Crop, wxGBPosition(2, 2), wxGBSpan(1, 1), wxALL | wxALIGN_CENTER_VERTICAL, 5);

	sbBasic->Add(sBasic);
	sGeneral->Add(sbBasic, 0, wxEXPAND|wxALL, 5);

	sBasicAdvanced = new wxGridBagSizer(0, 0);

	sBasicAdvanced->Add(m_OSDHotKey,			wxGBPosition(0, 0), wxGBSpan(1, 2), wxALL, 5);
	sBasicAdvanced->Add(m_VSync,				wxGBPosition(1, 0), wxGBSpan(1, 2), wxALL, 5);
	sBasicAdvanced->Add(m_UseXFB,				wxGBPosition(2, 0), wxGBSpan(1, 2), wxALL, 5);
	sBasicAdvanced->Add(m_UseRealXFB,			wxGBPosition(3, 0), wxGBSpan(1, 2), wxALL, 5);
	sBasicAdvanced->Add(m_AutoScale,			wxGBPosition(4, 0), wxGBSpan(1, 2), wxALL, 5);
	sBasicAdvanced->Add(m_WidescreenHack,		wxGBPosition(5, 0), wxGBSpan(1, 2), wxALL, 5);

	sbBasicAdvanced->Add(sBasicAdvanced);
	sGeneral->Add(sbBasicAdvanced, 0, wxEXPAND|wxALL, 5);

	sEnhancements = new wxGridBagSizer(0, 0);
	sEnhancements->Add(AnisoText, wxGBPosition(0, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sEnhancements->Add(m_MaxAnisotropyCB, wxGBPosition(0, 1), wxGBSpan(1, 1), wxALL|wxEXPAND, 5);
	sEnhancements->Add(m_ForceFiltering, wxGBPosition(0, 2), wxGBSpan(1, 2), wxALL|wxALIGN_CENTER_VERTICAL, 5);
	sEnhancements->Add(MSAAText, wxGBPosition(1, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sEnhancements->Add(m_MSAAModeCB, wxGBPosition(1, 1), wxGBSpan(1, 1), wxALL|wxEXPAND, 5);
	sEnhancements->Add(PostShaderText, wxGBPosition(2, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sEnhancements->Add(m_PostShaderCB, wxGBPosition(2, 1), wxGBSpan(1, 1), wxALL, 5);
	sEnhancements->Add(m_ReloadShader, wxGBPosition(2, 2), wxGBSpan(1, 1), wxALL, 5);
	sEnhancements->Add(m_EditShader, wxGBPosition(2, 3), wxGBSpan(1, 1), wxALL, 5);
	sbEnhancements->Add(sEnhancements);
	sGeneral->Add(sbEnhancements, 0, wxEXPAND|wxALL, 5);
	m_PageGeneral->SetSizer(sGeneral);
	sGeneral->Layout();

	// Information
	sbInfo = new wxStaticBoxSizer(wxVERTICAL, m_PageAdvanced, wxT("Info"));
	m_ShowFPS = new wxCheckBox(m_PageAdvanced, ID_SHOWFPS, wxT("Overlay FPS"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_ShaderErrors = new wxCheckBox(m_PageAdvanced, ID_SHADERERRORS, wxT("Show shader compilation issues"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_ShaderErrors->Enable(false);
	m_Statistics = new wxCheckBox(m_PageAdvanced, ID_STATISTICS, wxT("Overlay some statistics"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_ProjStats = new wxCheckBox(m_PageAdvanced, ID_PROJSTATS, wxT("Overlay Projection Stats"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_ShowEFBCopyRegions = new wxCheckBox(m_PageAdvanced, ID_SHOWEFBCOPYREGIONS, wxT("Show EFB Copy Regions"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_TexFmtOverlay = new wxCheckBox(m_PageAdvanced, ID_TEXFMTOVERLAY, wxT("Overlay texture format"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_TexFmtCenter = new wxCheckBox(m_PageAdvanced, ID_TEXFMTCENTER, wxT("centered"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

	// Render
	sbRendering = new wxStaticBoxSizer(wxVERTICAL, m_PageAdvanced, wxT("Rendering"));
	m_Wireframe = new wxCheckBox(m_PageAdvanced, ID_WIREFRAME, wxT("Enable Wireframe"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_DisableLighting = new wxCheckBox(m_PageAdvanced, ID_DISABLELIGHTING, wxT("Disable Material Lighting"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_DisableTexturing = new wxCheckBox(m_PageAdvanced, ID_DISABLETEXTURING, wxT("Disable Texturing"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_DstAlphaPass = new wxCheckBox(m_PageAdvanced, ID_DSTALPHAPASS, wxT("Disable Destination Alpha Pass"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_DisableFog = new wxCheckBox(m_PageAdvanced, ID_DISABLEFOG, wxT("Disable Fog"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

	m_StaticBox_EFB = new wxStaticBox(m_PageAdvanced, ID_STATICBOX_EFB, wxT("EFB Copy"));
	m_CheckBox_DisableCopyEFB = new wxCheckBox(m_PageAdvanced, ID_CHECKBOX_DISABLECOPYEFB, wxT("Disable"));
	m_Radio_CopyEFBToRAM = new wxRadioButton(m_PageAdvanced, ID_RADIO_COPYEFBTORAM, wxT("Copy EFB to system RAM (real)"));
	m_Radio_CopyEFBToGL = new wxRadioButton(m_PageAdvanced, ID_RADIO_COPYEFBTOGL, wxT("Copy EFB to GL texture (hack)"));

	// Utility
	sbUtilities = new wxStaticBoxSizer(wxVERTICAL, m_PageAdvanced, wxT("Utilities"));
	m_DumpTextures = new wxCheckBox(m_PageAdvanced, ID_DUMPTEXTURES, wxT("Dump textures"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_HiresTextures = new wxCheckBox(m_PageAdvanced, ID_HIRESTEXTURES, wxT("Load Hires textures"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_DumpEFBTarget = new wxCheckBox(m_PageAdvanced, ID_DUMPEFBTARGET, wxT("Dump EFB Target"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_DumpFrames = new wxCheckBox(m_PageAdvanced, ID_DUMPFRAMES, wxT("Dump Rendered Frames"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_FreeLook = new wxCheckBox(m_PageAdvanced, ID_FREELOOK, wxT("Free Look"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

	// Hacks controls
	sHacks = new wxStaticBoxSizer(wxVERTICAL, m_PageAdvanced, wxT("Hacks"));
	m_PhackvalueCB = new wxChoice(m_PageAdvanced, ID_PHACKVALUE, wxDefaultPosition, wxDefaultSize, arrayStringFor_PhackvalueCB, 0, wxDefaultValidator);
	m_SafeTextureCache = new wxCheckBox(m_PageAdvanced, ID_SAFETEXTURECACHE, wxT("Use Safe texture cache"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Radio_SafeTextureCache_Safe = new wxRadioButton(m_PageAdvanced, ID_RADIO_SAFETEXTURECACHE_SAFE, wxT("Safe"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
	m_Radio_SafeTextureCache_Normal = new wxRadioButton(m_PageAdvanced, ID_RADIO_SAFETEXTURECACHE_NORMAL, wxT("Normal"));
	m_Radio_SafeTextureCache_Fast = new wxRadioButton(m_PageAdvanced, ID_RADIO_SAFETEXTURECACHE_FAST, wxT("Fast"));

	// Sizers
	sHacks->Add(m_PhackvalueCB, 0, wxTOP, 0);
	sbHacks = new wxStaticBoxSizer(wxHORIZONTAL, m_PageAdvanced, wxT("Safe Texture Cache"));
	sbHacks->Add(m_SafeTextureCache, 0, wxALL, 5);
	sbHacks->AddStretchSpacer();
	sbHacks->Add(m_Radio_SafeTextureCache_Safe, 0, wxALL, 5);
	sbHacks->Add(m_Radio_SafeTextureCache_Normal, 0, wxALL, 5);
	sbHacks->Add(m_Radio_SafeTextureCache_Fast, 0, wxALL, 5);
	sHacks->Add(sbHacks, 0, wxEXPAND | (wxTOP), 5);

	// Sizers
	sAdvanced = new wxBoxSizer(wxVERTICAL);
	sInfo = new wxGridBagSizer(0, 0);
	sInfo->Add(m_ShowFPS, wxGBPosition(0, 0), wxGBSpan(1, 2), wxALL, 5);
	sInfo->Add(m_ShaderErrors, wxGBPosition(1, 0), wxGBSpan(1, 2), wxALL, 5);
	sInfo->Add(m_Statistics, wxGBPosition(2, 0), wxGBSpan(1, 2), wxALL, 5);
	sInfo->Add(m_ProjStats, wxGBPosition(3, 0), wxGBSpan(1, 2), wxALL, 5);
	sInfo->Add(m_ShowEFBCopyRegions, wxGBPosition(4, 0), wxGBSpan(1, 2), wxALL, 5);
	sInfo->Add(m_TexFmtOverlay, wxGBPosition(5, 0), wxGBSpan(1, 1), wxALL, 5);
	sInfo->Add(m_TexFmtCenter, wxGBPosition(5, 1), wxGBSpan(1, 1), wxALL, 5);
	sbInfo->Add(sInfo);
	
	wxBoxSizer *sRenderBoxRow1 = new wxBoxSizer(wxHORIZONTAL);
	sRendering = new wxGridBagSizer(0, 0);
	sRendering->Add(m_Wireframe, wxGBPosition(0, 0), wxGBSpan(1, 1), wxALL, 4);
	sRendering->Add(m_DisableLighting, wxGBPosition(1, 0), wxGBSpan(1, 1), wxALL, 4);
	sRendering->Add(m_DisableTexturing, wxGBPosition(2, 0), wxGBSpan(1, 1), wxALL, 4);
	sRendering->Add(m_DstAlphaPass, wxGBPosition(3, 0), wxGBSpan(1, 1), wxALL, 4);
	sRendering->Add(m_DisableFog, wxGBPosition(4, 0), wxGBSpan(1, 1), wxALL, 4);
	sRenderBoxRow1->Add(sRendering, 0, wxALL|wxEXPAND, 1);
		wxStaticBoxSizer *sSBox = new wxStaticBoxSizer(m_StaticBox_EFB, wxVERTICAL);
			wxBoxSizer *sStrip1 = new wxBoxSizer(wxHORIZONTAL);
			sStrip1->Add(m_CheckBox_DisableCopyEFB, 0, wxALL|wxEXPAND, 5);
		sSBox->Add(sStrip1, 0, wxALL|wxEXPAND, 0);
		sSBox->Add(m_Radio_CopyEFBToRAM, 0, wxALL|wxEXPAND, 5);
		sSBox->Add(m_Radio_CopyEFBToGL, 0, wxALL|wxEXPAND, 5);
	sRenderBoxRow1->Add(sSBox, 0, wxALL|wxEXPAND, 5);
	sbRendering->Add(sRenderBoxRow1);
	
	sUtilities = new wxGridBagSizer(0, 0);
	sUtilities->Add(m_DumpTextures, wxGBPosition(0, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sUtilities->Add(m_HiresTextures, wxGBPosition(0, 1), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sUtilities->Add(m_DumpEFBTarget, wxGBPosition(0, 2), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sUtilities->Add(m_DumpFrames, wxGBPosition(1, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sUtilities->Add(m_FreeLook, wxGBPosition(1, 1), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sbUtilities->Add(sUtilities, 1, wxEXPAND);

	// Sizers
	sAdvanced->Add(sbInfo, 0, wxEXPAND | wxALL, 5);
	sAdvanced->Add(sbRendering, 0, wxEXPAND | (wxLEFT | wxDOWN | wxRIGHT), 5);
	sAdvanced->Add(sbUtilities, 1, wxEXPAND | (wxLEFT | wxDOWN | wxRIGHT), 5);
	sAdvanced->Add(sHacks, 0, wxEXPAND | (wxLEFT | wxDOWN | wxRIGHT), 5);

	m_PageAdvanced->SetSizer(sAdvanced);
	sAdvanced->Layout();

	InitializeGUIValues();
	InitializeGUITooltips();

	Fit();
	Center();
	UpdateGUI();
}

void GFXConfigDialogOGL::AboutClick(wxCommandEvent& WXUNUSED (event))
{
	wxMessageBox(wxT("Dolphin OpenGL Plugin\nBy zerofrog(@gmail.com)\n\n")
		wxT("A card supporting Vertex/Pixel Shader 2.0 or higher, framebuffer objects, ")
		wxT("and multiple render targets is required in order to use this plugin."),
		wxT("Dolphin OGL"), wxOK, this);
}

void GFXConfigDialogOGL::ReloadShaderClick(wxCommandEvent& WXUNUSED (event))
{
	PostProcessing::ReloadShader();
}

void GFXConfigDialogOGL::EditShaderClick(wxCommandEvent& WXUNUSED (event))
{
	if (m_PostShaderCB->GetStringSelection() == wxT("(off)"))
		return;
	wxString shader = wxString::FromAscii(File::GetUserPath(D_SHADERS_IDX)) + m_PostShaderCB->GetStringSelection() + _(".txt");
	if (wxFileExists(shader))
	{
		wxFileType* filetype = wxTheMimeTypesManager->GetFileTypeFromExtension(_("txt"));
		if (filetype == NULL) // From extension failed, trying with MIME type now
		{
			filetype = wxTheMimeTypesManager->GetFileTypeFromMimeType(_("text/plain"));
			if (filetype == NULL) // MIME type failed, aborting mission
			{
				PanicAlert("Filetype 'txt' is unknown! Will not open!");
				return;
			}
		}
		wxString OpenCommand;
		OpenCommand = filetype->GetOpenCommand(shader);
		if (OpenCommand.IsEmpty())
			PanicAlert("Couldn't find open command for extension 'ini'!");
		else
			if (wxExecute(OpenCommand, wxEXEC_ASYNC) == -1)
				PanicAlert("wxExecute returned -1 on application run!");
	}
}

void GFXConfigDialogOGL::GeneralSettingsChanged(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case ID_NATIVERESOLUTION:
		g_Config.bNativeResolution = m_NativeResolution->IsChecked();
		// Don't allow 1x and 2x at the same time
		if (g_Config.bNativeResolution) { g_Config.b2xResolution = false; m_2xResolution->SetValue(false); }
		break;
	case ID_2X_RESOLUTION:
		g_Config.b2xResolution = m_2xResolution->IsChecked();
		// Don't allow 1x and 2x at the same time
		if (g_Config.b2xResolution) { g_Config.bNativeResolution = false; m_NativeResolution->SetValue(false); }
		break;
	case ID_VSYNC:
		g_Config.bVSync = m_VSync->IsChecked();
		break;
	case ID_USEXFB:
		g_Config.bUseXFB = m_UseXFB->IsChecked();
		break;
	case ID_USEREALXFB:
		g_Config.bUseRealXFB = m_UseRealXFB->IsChecked();
		break;
	case ID_AUTOSCALE:
		g_Config.bAutoScale = m_AutoScale->IsChecked();
		break;
	case ID_ASPECT:
		g_Config.iAspectRatio = m_KeepAR->GetSelection();
		break;
	case ID_WIDESCREENHACK:
		g_Config.bWidescreenHack = m_WidescreenHack->IsChecked();
		break;
	case ID_CROP:
		g_Config.bCrop = m_Crop->IsChecked();
		break;
	case ID_FORCEFILTERING:
		g_Config.bForceFiltering = m_ForceFiltering->IsChecked();
		break;
	case ID_FULLSCREENRESOLUTION:
		strcpy(g_Config.cFSResolution, m_WindowFSResolutionCB->GetStringSelection().mb_str() );
		break;
	case ID_MAXANISOTROPY:
		g_Config.iMaxAnisotropy = m_MaxAnisotropyCB->GetSelection() + 1;
		break;
	case ID_MSAAMODECB:
		g_Config.iMultisampleMode = m_MSAAModeCB->GetSelection();
		break;
	case ID_PHACKVALUE:
		g_Config.iPhackvalue = m_PhackvalueCB->GetSelection();
		if (g_Config.iPhackvalue >= 0)
		{
			g_Config.UpdateProjectionHack();
		}
		break;
	case ID_POSTSHADER:
		g_Config.sPostProcessingShader = m_PostShaderCB->GetString(m_PostShaderCB->GetSelection()).mb_str();
		if (g_Config.sPostProcessingShader == "(off)")
			g_Config.sPostProcessingShader = "";
		break;
	}

	UpdateGUI();
}

void GFXConfigDialogOGL::AdvancedSettingsChanged(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case ID_SHOWFPS:
		g_Config.bShowFPS = m_ShowFPS->IsChecked();
		break;
	case ID_SHADERERRORS:
		g_Config.bShowShaderErrors = m_ShaderErrors->IsChecked();
		break;
	case ID_STATISTICS:
		g_Config.bOverlayStats = m_Statistics->IsChecked();
		break;
	case ID_TEXFMTOVERLAY:
		g_Config.bTexFmtOverlayEnable = m_TexFmtOverlay->IsChecked();
		m_TexFmtCenter->Enable(m_TexFmtOverlay->IsChecked());
		TextureMngr::Invalidate(false);
		break;
	case ID_TEXFMTCENTER:
		g_Config.bTexFmtOverlayCenter = m_TexFmtCenter->IsChecked();
		TextureMngr::Invalidate(false);
		break;
	case ID_SHOWEFBCOPYREGIONS:
		g_Config.bShowEFBCopyRegions = m_ShowEFBCopyRegions->IsChecked();
		break;
	case ID_WIREFRAME:
		g_Config.bWireFrame = m_Wireframe->IsChecked();
		break;
	case ID_DISABLELIGHTING:
		g_Config.bDisableLighting = m_DisableLighting->IsChecked();
		break;
	case ID_DISABLETEXTURING:
		g_Config.bDisableTexturing = m_DisableTexturing->IsChecked();
		break;
	case ID_DISABLEFOG:
		g_Config.bDisableFog = m_DisableFog->IsChecked();
		break;
	case ID_DSTALPHAPASS:
		g_Config.bDstAlphaPass = m_DstAlphaPass->IsChecked();
		break;
	case ID_DUMPTEXTURES:
		g_Config.bDumpTextures = m_DumpTextures->IsChecked();
		break;
	case ID_HIRESTEXTURES:
		g_Config.bHiresTextures = m_HiresTextures->IsChecked();
		break;
	case ID_DUMPEFBTARGET:
		g_Config.bDumpEFBTarget = m_DumpEFBTarget->IsChecked();
		break;
	case ID_DUMPFRAMES:
		g_Config.bDumpFrames = m_DumpFrames->IsChecked();
		break;
	case ID_FREELOOK:
		g_Config.bFreeLook = m_FreeLook->IsChecked();
		break;
	case ID_TEXTUREPATH:
		break;
	case ID_CHECKBOX_DISABLECOPYEFB:
		g_Config.bEFBCopyDisable = m_CheckBox_DisableCopyEFB->IsChecked();
		break;
	case ID_OSDHOTKEY:
		g_Config.bOSDHotKey = m_OSDHotKey->IsChecked();
		break;
	// Hacks
	case ID_SAFETEXTURECACHE:
		g_Config.bSafeTextureCache = m_SafeTextureCache->IsChecked();
		break;
	case ID_RADIO_SAFETEXTURECACHE_SAFE:
		g_Config.iSafeTextureCache_ColorSamples = 0;
		break;
	case ID_RADIO_SAFETEXTURECACHE_NORMAL:
		if(g_Config.iSafeTextureCache_ColorSamples < 512)
			g_Config.iSafeTextureCache_ColorSamples = 512;
		break;
	case ID_RADIO_SAFETEXTURECACHE_FAST:
		if(g_Config.iSafeTextureCache_ColorSamples > 128 || g_Config.iSafeTextureCache_ColorSamples == 0)
			g_Config.iSafeTextureCache_ColorSamples = 128;
		break;
	case ID_HACK:
		g_Config.bHack = m_Hack->IsChecked();
		break;
	case ID_RADIO_COPYEFBTORAM:
		g_Config.bCopyEFBToTexture = false;
		break;
	case ID_RADIO_COPYEFBTOGL:
		g_Config.bCopyEFBToTexture = true;
		break;
	case ID_PROJSTATS:
		g_Config.bOverlayProjStats = m_ProjStats->IsChecked();
		break;

	default:
		break;
	}

	UpdateGUI();
}

void GFXConfigDialogOGL::CloseWindow()
{
	// Save the config to INI
	g_Config.Save((std::string(File::GetUserPath(D_CONFIG_IDX)) + "gfx_opengl.ini").c_str());

	EndModal(1);
}

void GFXConfigDialogOGL::UpdateGUI()
{
	// This is only used together with the aspect ratio options
	m_Crop->Enable(g_Config.iAspectRatio != ASPECT_STRETCH);
	if (g_Config.bUseRealXFB)
	{
		// must use XFB to use real XFB
		g_Config.bUseXFB = true;
		m_UseXFB->SetValue(true);

		// XFB looks much better if the copy comes from native resolution.
		g_Config.bNativeResolution = true;
		m_NativeResolution->SetValue(true);
		//also disable 2x, since it might leave both checked.
		g_Config.b2xResolution = false; 
		m_2xResolution->SetValue(false);
	}
	m_AutoScale->Enable(!g_Config.bUseRealXFB);
	m_UseXFB->Enable(!g_Config.bUseRealXFB);

	// Resolution settings
	//disable native/2x choice when real xfb is on. native simply looks best, as ector noted above.
	//besides, it would look odd if one disabled native, and it came back on again.
	m_NativeResolution->Enable(!g_Config.bUseRealXFB);
	m_2xResolution->Enable(!g_Config.bUseRealXFB && (!g_Config.bRunning || Renderer::Allow2x()));
	m_WindowFSResolutionCB->Enable(!g_Config.bRunning);

	// Disable the Copy to options when EFBCopy is disabled
	m_Radio_CopyEFBToRAM->Enable(!(g_Config.bEFBCopyDisable));
	m_Radio_CopyEFBToGL->Enable(!(g_Config.bEFBCopyDisable));

	// Disable/Enable Safe Texture Cache options
	m_Radio_SafeTextureCache_Safe->Enable(g_Config.bSafeTextureCache);
	m_Radio_SafeTextureCache_Normal->Enable(g_Config.bSafeTextureCache);
	m_Radio_SafeTextureCache_Fast->Enable(g_Config.bSafeTextureCache);
}

