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
	private:
		DECLARE_EVENT_TABLE();
		
	public:
		ConfigDialog(wxWindow *parent, wxWindowID id = 1, const wxString &title = wxT("OpenGL Plugin Configuration"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = ConfigDialog_STYLE);
		virtual ~ConfigDialog();
		void ConfigDialogActivate(wxActivateEvent& event);
		void OKClick(wxCommandEvent& event);
		void FullScreenCheck(wxCommandEvent& event);
		void RenderMainCheck(wxCommandEvent& event);
		void AddFSReso(char *reso);
		void FSCB(wxCommandEvent& event);
		void AddWindowReso(char *reso);
		void WMCB(wxCommandEvent& event);
		void AddAAMode(int mode);
		void AACB(wxCommandEvent& event);
		void ForceFilteringCheck(wxCommandEvent& event);
		void ForceAnisotropyCheck(wxCommandEvent& event);
		void WireframeCheck(wxCommandEvent& event);
		void OverlayCheck(wxCommandEvent& event);
		void ShowShaderErrorsCheck(wxCommandEvent& event);
		void TexFmtOverlayChange(wxCommandEvent& event);
		void DumpTexturesChange(wxCommandEvent& event);
		void TexturePathChange(wxFileDirPickerEvent& event);

	private:

		wxButton *m_Cancel;
		wxButton *m_OK;
		wxDirPickerCtrl *m_TexturePath;
		wxCheckBox *m_DumpTextures;
		wxCheckBox *m_TexFmtCenter;
		wxCheckBox *m_TexFmtOverlay;
		wxCheckBox *m_Statistics;
		wxCheckBox *m_ShaderErrors;
		wxCheckBox *m_Wireframe;
		wxCheckBox *m_ForceAnisotropy;
		wxCheckBox *m_ForceFiltering;
		wxComboBox *m_AliasModeCB;
		wxComboBox *m_WindowResolutionCB;
		wxComboBox *m_FullscreenCB;
		wxCheckBox *m_RenderToMainWindow;
		wxCheckBox *m_Fullscreen;
		wxPanel *m_PageAdvanced;
		wxPanel *m_PageEnhancements;
		wxPanel *m_PageVideo;
		wxNotebook *m_Notebook;
		
	private:

		enum
		{
			////GUI Enum Control ID Start
			ID_CANCEL = 1000,
			ID_OK,
			ID_TEXTUREPATH,
			ID_SHADERERRORS,
			ID_TEXFMTCENTER,
			ID_TEXFMTOVERLAY,
			ID_STATISTICS,
			ID_DUMPTEXTURES,
			ID_WIREFRAME,
			ID_FORCEANISOTROPY,
			ID_FORCEFILTERING,
			ID_ALIASMODECB,
			ID_AATEXT,
			ID_WINDOWRESOLUTIONCB,
			ID_WMTEXT,
			ID_FULLSCREENCB,
			ID_FSTEXT,
			ID_RENDERTOMAINWINDOW,
			ID_FULLSCREEN,
			ID_PAGEADVANCED,
			ID_NOTEBOOK,
			ID_PAGEVIDEO,
			ID_PAGEENHANCEMENTS,
			////GUI Enum Control ID End
			ID_DUMMY_VALUE_ //don't remove this value unless you have other enum values
		};
	
	private:
		void OnClose(wxCloseEvent& event);
		void CreateGUIControls();
};

#endif
