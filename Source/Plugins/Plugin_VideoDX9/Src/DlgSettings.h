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

#ifndef _DX_DLGSETTINGS_H_
#define _DX_DLGSETTINGS_H_
 
#include <wx/wx.h>
#include <wx/dialog.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/choice.h>
#include <wx/combobox.h>
#include <wx/checkbox.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/filepicker.h>
#include <wx/gbsizer.h>

class GFXConfigDialogDX : public wxDialog
{
	public:
		GFXConfigDialogDX(wxWindow *parent, wxWindowID id = 1,
#ifdef DEBUGFAST
			const wxString &title = wxT("DX9 (DEBUGFAST) Plugin Configuration"),
#else
#ifndef _DEBUG
			const wxString &title = wxT("DX9 Plugin Configuration"),
#else
			const wxString &title = wxT("DX9 (DEBUG) Plugin Configuration"),
#endif
#endif
			const wxPoint& pos = wxDefaultPosition,
			const wxSize& size = wxDefaultSize,
			long style = wxDEFAULT_DIALOG_STYLE);
		virtual ~GFXConfigDialogDX();
		void CreateGUIControls();
		void CloseClick(wxCommandEvent& WXUNUSED (event));

	private:
		DECLARE_EVENT_TABLE();

		wxBoxSizer* sGeneral;
		wxStaticBoxSizer* sbBasic;
		wxGridBagSizer* sBasic;
		wxStaticBoxSizer* sbSTC;
		wxGridBagSizer* sSTC;

		wxBoxSizer* sEnhancements;
		wxStaticBoxSizer* sbTextureFilter;
		wxGridBagSizer* sTextureFilter;
		wxStaticBoxSizer* sbEFBHacks;
		wxGridBagSizer* sEFBHacks;

		wxBoxSizer* sAdvanced;
		wxStaticBoxSizer* sbSettings;
		wxGridBagSizer* sSettings;
		wxStaticBoxSizer* sbDataDumping;
		wxGridBagSizer* sDataDumping;
		wxStaticBoxSizer* sbDebuggingTools;
		wxGridBagSizer* sDebuggingTools;

		
		wxButton *m_Close;

		wxNotebook *m_Notebook;
		wxPanel *m_PageDirect3D;
		wxPanel *m_PageEnhancements;
		wxPanel *m_PageAdvanced;

		//Direct3D Tab
		wxStaticText* m_AdapterText;
		wxChoice *m_AdapterCB;
		wxArrayString arrayStringFor_AdapterCB;
		wxArrayString arrayStringFor_MSAAModeCB;
		wxCheckBox *m_VSync;
		wxCheckBox *m_WidescreenHack;
		wxStaticText* m_staticARText;
		wxChoice *m_KeepAR;
		wxStaticText* m_staticMSAAText;
		wxChoice *m_MSAAModeCB;
		wxStaticText* m_EFBScaleText;
		wxChoice *m_EFBScaleMode;
		wxCheckBox *m_EnableEFBAccess;
		wxCheckBox *m_SafeTextureCache;
		wxRadioButton *m_Radio_SafeTextureCache_Fast;
		wxRadioButton *m_Radio_SafeTextureCache_Normal;
		wxRadioButton *m_Radio_SafeTextureCache_Safe;
		wxCheckBox *m_DlistCaching;
		//Enhancements Tab
		wxCheckBox *m_ForceFiltering;
		wxCheckBox *m_MaxAnisotropy;
		wxCheckBox *m_HiresTextures;
		wxCheckBox *m_EFBScaledCopy;
		wxCheckBox *m_Anaglyph;
		wxCheckBox *m_PixelLighting;
		wxStaticText* m_AnaglyphSeparationText;
		wxSlider *m_AnaglyphSeparation;
		wxStaticText* m_AnaglyphFocalAngleText;
		wxSlider *m_AnaglyphFocalAngle;
		//Advanced Tab
		wxCheckBox *m_DisableFog;
		wxCheckBox *m_OverlayFPS;
		wxCheckBox *m_CopyEFB;
		wxRadioButton *m_Radio_CopyEFBToRAM;
		wxRadioButton *m_Radio_CopyEFBToGL;
		wxCheckBox *m_EnableHotkeys;
		wxCheckBox *m_WireFrame;
		wxCheckBox *m_EnableXFB;
		wxCheckBox *m_EnableRealXFB;
		wxCheckBox *m_UseNativeMips;
		wxCheckBox *m_DumpTextures;
		wxCheckBox *m_DumpFrames;
		wxCheckBox *m_OverlayStats;
		wxCheckBox *m_ProjStats;
		wxCheckBox *m_ShaderErrors;
		wxCheckBox *m_TexfmtOverlay;
		wxCheckBox *m_TexfmtCenter;
		wxCheckBox *m_Enable3dVision;

		enum
		{
			ID_CLOSE,
			ID_ADAPTER,
			ID_VSYNC,
			ID_WIDESCREEN_HACK,
			ID_ASPECT,
			ID_FULLSCREENRESOLUTION,
			ID_ANTIALIASMODE,
			ID_EFBSCALEMODE,
			ID_EFB_ACCESS_ENABLE,
			ID_SAFETEXTURECACHE,
			ID_RADIO_SAFETEXTURECACHE_SAFE,
			ID_RADIO_SAFETEXTURECACHE_NORMAL,
			ID_RADIO_SAFETEXTURECACHE_FAST,
			ID_DLISTCACHING,
			ID_FORCEFILTERING,
			ID_FORCEANISOTROPY,
			ID_LOADHIRESTEXTURES,
			ID_EFBSCALEDCOPY,
			ID_DISABLEFOG,
			ID_OVERLAYFPS,
			ID_ENABLEEFBCOPY,
			ID_EFBTORAM,
			ID_EFBTOTEX,
			ID_ENABLEHOTKEY,
			ID_WIREFRAME,
			ID_ENABLEXFB,
			ID_ENABLEREALXFB,
			ID_USENATIVEMIPS,
			ID_TEXDUMP,
			ID_DUMPFRAMES,
			ID_OVERLAYSTATS,
			ID_PROJSTATS,
			ID_SHADERERRORS,
			ID_TEXFMT_OVERLAY,
			ID_TEXFMT_CENTER,
			ID_DEBUGSTEP,
			ID_REGISTERS,
			ID_ENABLEDEBUGGING,
			ID_REGISTERSELECT,
			ID_ARTEXT,
			ID_NOTEBOOK = 1000,
			ID_DEBUGGER,
			ID_ABOUT,
			ID_DIRERCT3D,
			ID_PAGEENHANCEMENTS,
			ID_PAGEADVANCED,
			ID_PIXELLIGHTING,
			ID_ANAGLYPH,
			ID_ANAGLYPHSEPARATION,
			ID_ANAGLYPHFOCALANGLE,
			ID_ENABLE_3DVISION,
		};
		void InitializeAdapters();
		void OnClose(wxCloseEvent& event);
		void InitializeGUIValues();
		void DirectXSettingsChanged(wxCommandEvent& event);
		void EnhancementsSettingsChanged(wxCommandEvent& event);
		void AdvancedSettingsChanged(wxCommandEvent& event);
		void CloseWindow();
		void UpdateGUI();

};
#endif //_DX_DLGSETTINGS_H_
