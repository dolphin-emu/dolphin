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

#ifndef __FILESYSTEM_VIEWER_h__
#define __FILESYSTEM_VIEWER_h__

#include <wx/sizer.h>
#include <wx/filepicker.h>
#include <wx/statbmp.h>
#include <wx/imaglist.h>
#include <wx/treectrl.h>
#include <wx/gbsizer.h>
#include <string>

#undef FILESYSTEM_VIEWER_STYLE
#define FILESYSTEM_VIEWER_STYLE wxCAPTION | wxSYSTEM_MENU | wxDIALOG_NO_PARENT | wxCLOSE_BOX

class CFilesystemViewer : public wxDialog
{
	public:

		CFilesystemViewer(const std::string fileName, wxWindow* parent, wxWindowID id = 1, const wxString& title = wxT("Filesystem Viewer"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = FILESYSTEM_VIEWER_STYLE);
		virtual ~CFilesystemViewer();

	private:

		DECLARE_EVENT_TABLE();
		
		wxStaticBoxSizer *sbISODetails;
		wxGridBagSizer *sISODetails;
		wxStaticBoxSizer *sbBannerDetails;
		wxGridBagSizer *sBannerDetails;
		wxStaticBoxSizer *sbTreectrl;

		wxTreeCtrl *m_Treectrl;
		wxButton *m_Close;

		wxStaticText *m_NameText;
		wxStaticText *m_SerialText;
		wxStaticText *m_CountryText;
		wxStaticText *m_MakerIDText;
		wxStaticText *m_DateText;
		wxStaticText *m_TOCText;
		wxStaticText *m_VersionText;
		wxStaticText *m_LangText;
		wxStaticText *m_ShortText;
		wxStaticText *m_LongText;
		wxStaticText *m_MakerText;
		wxStaticText *m_CommentText;
		wxStaticText *m_BannerText;

		wxTextCtrl *m_Name;
		wxTextCtrl *m_Serial;
		wxTextCtrl *m_Country;
		wxTextCtrl *m_MakerID;
		wxTextCtrl *m_Date;
		wxTextCtrl *m_TOC;
		wxTextCtrl *m_Version;
		wxTextCtrl *m_ShortName;
		wxTextCtrl *m_LongName;
		wxTextCtrl *m_Maker;
		wxTextCtrl *m_Comment;
		wxTextCtrl *m_Banner;
		wxTreeItemId RootId;

		wxChoice *m_Lang;
		wxButton *m_SaveBNR;

		enum
		{
			ID_CLOSE = 1000,
			ID_TREECTRL,

			ID_NAME_TEXT,
			ID_SERIAL_TEXT,
			ID_COUNTRY_TEXT,
			ID_MAKERID_TEXT,
			ID_DATE_TEXT,
			ID_TOC_TEXT,
			ID_VERSION_TEXT,
			ID_LANG_TEXT,
			ID_SHORTNAME_TEXT,
			ID_LONGNAME_TEXT,
			ID_MAKER_TEXT,
			ID_COMMENT_TEXT,
			ID_BANNER_TEXT,
			
			ID_NAME,
			ID_SERIAL,
			ID_COUNTRY,
			ID_MAKERID,
			ID_DATE,
			ID_TOC,
			ID_VERSION,
			ID_LANG,
			ID_SHORTNAME,
			ID_LONGNAME,
			ID_MAKER,
			ID_COMMENT,
			ID_BANNER,

			ID_SAVEBNR,

			IDM_EXTRACTFILE,
			IDM_REPLACEFILE,
			IDM_RENAMEFILE,
			IDM_BNRSAVEAS,
			ID_DUMMY_VALUE_ //don't remove this value unless you have other enum values
		};

		void CreateGUIControls();
		void OnClose(wxCloseEvent& event);
		void OnCloseClick(wxCommandEvent& event);
		void OnRightClick(wxMouseEvent& event);
		void OnRightClickOnTree(wxTreeEvent& event);
		void OnSaveBNRClick(wxCommandEvent& event);
		void OnBannerImageSave(wxCommandEvent& event);
		void OnExtractFile(wxCommandEvent& event);
		void OnReplaceFile(wxCommandEvent& event);
		void OnRenameFile(wxCommandEvent& event);
};

#endif
