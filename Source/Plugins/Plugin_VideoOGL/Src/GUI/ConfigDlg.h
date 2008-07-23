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


#undef ConfigDialog_STYLE
#define ConfigDialog_STYLE wxCAPTION | wxSYSTEM_MENU | wxDIALOG_NO_PARENT | wxCLOSE_BOX


class ConfigDialog : public wxDialog
{
	private:
		DECLARE_EVENT_TABLE();
		
	public:
		ConfigDialog(wxWindow *parent, wxWindowID id = 1, const wxString &title = wxT("Opengl Plugin Configuration"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = ConfigDialog_STYLE);
		virtual ~ConfigDialog();
		void ConfigDialogActivate(wxActivateEvent& event);
		void BrowseClick(wxCommandEvent& event);
		void OKClick(wxCommandEvent& event);
	
	private:

		wxButton *m_Close;
		wxButton *m_Apply;
		wxButton *m_OK;
		wxTextCtrl *m_TexturePath;
		wxButton *m_Browse;
		wxCheckBox *m_ShaderErrors;
		wxCheckBox *m_Statistics;
		wxCheckBox *m_DumpTextures;
		wxCheckBox *m_Wireframe;
		wxCheckBox *m_ForceAnsitropy;
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
			ID_CLOSE = 1030,
			ID_APPLY = 1029,
			ID_OK = 1028,
			ID_TEXTUREPATH = 1026,
			ID_BROWSE = 1025,
			ID_SHADERERRORS = 1024,
			ID_STATISTICS = 1023,
			ID_DUMPTEXTURES = 1022,
			ID_WIREFRAME = 1021,
			ID_FORCEANSITROPY = 1020,
			ID_FORCEFILTERING = 1019,
			ID_WXSTATICTEXT3 = 1015,
			ID_ALIASMODECB = 1014,
			ID_WXSTATICTEXT2 = 1013,
			ID_WXSTATICTEXT1 = 1012,
			ID_WINDOWRESOLUTIONCB = 1011,
			ID_FULLSCREENCB = 1010,
			ID_RENDERTOMAINWINDOW = 1009,
			ID_FULLSCREEN = 1008,
			ID_PAGEADVANCED = 1007,
			ID_WXNOTEBOOKPAGE3 = 1006,
			ID_PAGEVIDEO = 1005,
			ID_PAGEENHANCEMENTS = 1004,
			ID_WXNOTEBOOKPAGE1 = 1003,
			////GUI Enum Control ID End
			ID_DUMMY_VALUE_ //don't remove this value unless you have other enum values
		};
	
	private:
		void OnClose(wxCloseEvent& event);
		void CreateGUIControls();
};

#endif
