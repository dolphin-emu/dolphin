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

#ifndef __CONFIGDIALOG_h__
#define __CONFIGDIALOG_h__

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


#undef ConfigDialog_STYLE
#define ConfigDialog_STYLE wxCAPTION | wxSYSTEM_MENU | wxDIALOG_NO_PARENT | wxCLOSE_BOX


class ConfigDialog : public wxDialog
{
	public:
		ConfigDialog(wxWindow *parent, wxWindowID id = 1, const wxString &title = wxT("OpenGL Plugin Configuration"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = ConfigDialog_STYLE);
		virtual ~ConfigDialog();
		void CloseClick(wxCommandEvent& event);

		void AddFSReso(char *reso);
		void AddWindowReso(char *reso);
		void AddAAMode(int mode);

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
		wxCheckBox *m_RenderToMainWindow;
		wxCheckBox *m_StretchToFit;
		wxCheckBox *m_KeepAR;
		wxCheckBox *m_HideCursor;
		wxArrayString arrayStringFor_FullscreenCB;
		wxComboBox *m_FullscreenCB;
		wxArrayString arrayStringFor_WindowResolutionCB;
		wxComboBox *m_WindowResolutionCB;

		wxCheckBox *m_ForceFiltering; // advanced
		wxChoice *m_MaxAnisotropyCB;
		wxArrayString arrayStringFor_MaxAnisotropyCB;
		wxComboBox *m_AliasModeCB;
		wxCheckBox *m_ShowFPS;
		wxCheckBox *m_ShaderErrors;
		wxCheckBox *m_Statistics;
		wxCheckBox *m_TexFmtOverlay;
		wxCheckBox *m_TexFmtCenter;
		wxCheckBox *m_UseXFB;
		wxCheckBox *m_Wireframe;
		wxCheckBox *m_DisableLighting;
		wxCheckBox *m_DisableTexturing;
		wxCheckBox *m_DumpTextures;
		wxDirPickerCtrl *m_TexturePath;
		wxCheckBox *m_EFBToTextureDisable, *m_EFBToTextureDisableHotKey;
		wxCheckBox *m_ProjectionHax1;
		wxCheckBox *m_ProjectionHax2;
		wxCheckBox *m_SafeTextureCache;

		enum
		{
			ID_CLOSE = 1000,
			ID_ABOUTOGL,

			ID_NOTEBOOK,
			ID_PAGEGENERAL,
			ID_PAGEADVANCED,

			ID_FULLSCREEN,
			ID_RENDERTOMAINWINDOW,
			ID_STRETCHTOFIT,
			ID_KEEPAR,
			ID_HIDECURSOR,
			ID_FSTEXT,
			ID_FULLSCREENCB,
			ID_WMTEXT,
			ID_WINDOWRESOLUTIONCB,

			ID_FORCEFILTERING,
			ID_MAXANISOTROPY,
			ID_AATEXT,
			ID_ALIASMODECB,

			ID_SHOWFPS,
			ID_SHADERERRORS,
			ID_STATISTICS,
			ID_TEXFMTOVERLAY,
			ID_TEXFMTCENTER,

			ID_USEXFB,
			ID_WIREFRAME,
			ID_DISABLELIGHTING,
			ID_DISABLETEXTURING,
			ID_SAFETEXTURECACHE,

			ID_DUMPTEXTURES,
			ID_TEXTUREPATH,

			ID_EFBTOTEXTUREDISABLE, ID_EFBTOTEXTUREDISABLEHOTKEY,
			ID_PROJECTIONHACK1,
			ID_PROJECTIONHACK2
		};

		void OnClose(wxCloseEvent& event);
		void CreateGUIControls();

		void AboutClick(wxCommandEvent& event);
		void GeneralSettingsChanged(wxCommandEvent& event);
		void AdvancedSettingsChanged(wxCommandEvent& event);
		void TexturePathChange(wxFileDirPickerEvent& event);
};

#endif
