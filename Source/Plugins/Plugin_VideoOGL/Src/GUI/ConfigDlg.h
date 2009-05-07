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

#ifndef _OGL_CONFIGDIALOG_H_
#define _OGL_CONFIGDIALOG_H_

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


class ConfigDialog : public wxDialog
{
	public:
		ConfigDialog(wxWindow *parent, wxWindowID id = 1,
			const wxString &title = wxT("OpenGL Plugin Configuration"),
			const wxPoint& pos = wxDefaultPosition,
			const wxSize& size = wxDefaultSize,
			long style = wxDEFAULT_DIALOG_STYLE);
		virtual ~ConfigDialog();
		void CloseClick(wxCommandEvent& event);

		void AddFSReso(char *reso);
		void AddWindowReso(char *reso);
		void CreateGUIControls();

		// Combo box lists, this one needs to be public
		wxArrayString arrayStringFor_FullscreenCB;

	private:
		DECLARE_EVENT_TABLE();

		wxBoxSizer* sGeneral;
		wxStaticBoxSizer* sbBasic;
		wxGridBagSizer* sBasic;
		wxStaticBoxSizer* sbEnhancements;
		wxGridBagSizer* sEnhancements;
		wxBoxSizer* sAdvanced;
		wxStaticBoxSizer* sbInfo;
		wxGridBagSizer* sInfo;
		wxStaticBoxSizer* sbRendering;
		wxGridBagSizer* sRendering;
		wxStaticBoxSizer* sbUtilities;
		wxGridBagSizer* sUtilities;
		wxStaticBoxSizer* sbHacks;
		wxGridBagSizer* sHacks;
		
		wxButton *m_About;
		wxButton *m_Close;
		wxNotebook *m_Notebook;
		wxPanel *m_PageGeneral;
		wxPanel *m_PageAdvanced;
		wxCheckBox *m_Fullscreen;
		wxCheckBox *m_VSync;
		wxCheckBox *m_RenderToMainWindow;
		wxCheckBox *m_NativeResolution;
		wxCheckBox *m_ForceFiltering;
		wxCheckBox *m_KeepAR43, *m_KeepAR169, *m_Crop;
		wxCheckBox *m_UseXFB;
		wxCheckBox *m_AutoScale;
		#ifndef _WIN32
			wxCheckBox *m_HideCursor;
		#endif
		wxComboBox *m_FullscreenCB;
		wxArrayString arrayStringFor_WindowResolutionCB;
		wxComboBox *m_WindowResolutionCB;
		wxArrayString arrayStringFor_MaxAnisotropyCB;
		wxChoice *m_MaxAnisotropyCB;
		wxArrayString arrayStringFor_MSAAModeCB;
		wxChoice *m_MSAAModeCB;

		wxCheckBox *m_ShowFPS;
		wxCheckBox *m_ShaderErrors;
		wxCheckBox *m_Statistics;
		wxCheckBox *m_BlendStats;
		wxCheckBox *m_ProjStats;
		wxCheckBox *m_ShowEFBCopyRegions;
		wxCheckBox *m_TexFmtOverlay;
		wxCheckBox *m_TexFmtCenter;
		wxCheckBox *m_Wireframe;
		wxCheckBox *m_DisableLighting;
		wxCheckBox *m_DisableTexturing;
		wxCheckBox *m_DisableFog;
        wxCheckBox *m_DstAlphaPass;
		wxCheckBox *m_DumpTextures;
		wxCheckBox *m_HiresTextures;
		wxCheckBox *m_DumpEFBTarget;
		wxCheckBox *m_DumpFrames;
        wxCheckBox *m_FreeLook;
		wxStaticBox * m_StaticBox_EFB;
		wxCheckBox *m_CheckBox_DisableCopyEFB;
		wxRadioButton *m_Radio_CopyEFBToRAM, *m_Radio_CopyEFBToGL;
		wxCheckBox *m_EFBCopyDisableHotKey;
		wxCheckBox *m_ProjectionHax1;
		wxCheckBox *m_SMGh;
		wxCheckBox *m_SafeTextureCache;
		// Screen size
		wxStaticText *m_TextScreenWidth, *m_TextScreenHeight, *m_TextScreenLeft, *m_TextScreenTop;
		wxSlider *m_SliderWidth, *m_SliderHeight, *m_SliderLeft, *m_SliderTop;
		wxCheckBox *m_ScreenSize;

		enum
		{
			ID_CLOSE = 1000,
			ID_ABOUTOGL,

			ID_NOTEBOOK,
			ID_PAGEGENERAL,
			ID_PAGEADVANCED,

			ID_FULLSCREEN,
			ID_VSYNC,
			ID_RENDERTOMAINWINDOW,
			ID_NATIVERESOLUTION,
			ID_KEEPAR_4_3, ID_KEEPAR_16_9, ID_CROP,
			ID_USEXFB,
			ID_AUTOSCALE,

			ID_HIDECURSOR,
			ID_FSTEXT,
			ID_FULLSCREENCB,
			ID_WMTEXT,
			ID_WINDOWRESOLUTIONCB,
			ID_FORCEFILTERING,
			ID_MAXANISOTROPY,
			ID_MAXANISOTROPYTEXT,
			ID_MSAAMODECB,
			ID_MSAAMODETEXT,

			ID_SHOWFPS,
			ID_SHADERERRORS,
			ID_STATISTICS,
			ID_BLENDSTATS,
			ID_PROJSTATS,
			ID_SHOWEFBCOPYREGIONS,
			ID_TEXFMTOVERLAY,
			ID_TEXFMTCENTER,

			ID_WIREFRAME,
			ID_DISABLELIGHTING,
			ID_DISABLETEXTURING,
			ID_DISABLEFOG,
			ID_STATICBOX_EFB,
			ID_SAFETEXTURECACHE,
			ID_SMGHACK,

			ID_DUMPTEXTURES,
			ID_HIRESTEXTURES,
			ID_DUMPEFBTARGET,
			ID_DUMPFRAMES,
            ID_FREELOOK,
			ID_TEXTUREPATH,

			ID_CHECKBOX_DISABLECOPYEFB, 
			ID_EFBCOPYDISABLEHOTKEY,
			ID_PROJECTIONHACK1,
            ID_DSTALPHAPASS,
			ID_RADIO_COPYEFBTORAM,
			ID_RADIO_COPYEFBTOGL,
		};

		void OnClose(wxCloseEvent& event);
		void UpdateGUI();

		void AboutClick(wxCommandEvent& event);
		void GeneralSettingsChanged(wxCommandEvent& event);
		void AdvancedSettingsChanged(wxCommandEvent& event); 
};

#endif // _OGL_CONFIGDIALOG_H_
