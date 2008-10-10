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

#include "ISOFile.h"
#include "VolumeCreator.h"
#include "Filesystem.h"
#include "FilesystemViewer.h"

BEGIN_EVENT_TABLE(CFilesystemViewer, wxDialog)
	EVT_CLOSE(CFilesystemViewer::OnClose)
	EVT_RIGHT_DOWN(CFilesystemViewer::OnRightClick)
	EVT_TREE_ITEM_RIGHT_CLICK(ID_TREECTRL,CFilesystemViewer::OnRightClickOnTree)
	EVT_BUTTON(ID_CLOSE,CFilesystemViewer::OnCloseClick)
	EVT_BUTTON(ID_SAVEBNR,CFilesystemViewer::OnSaveBNRClick)
	EVT_MENU(IDM_BNRSAVEAS, CFilesystemViewer::OnBannerImageSave)
	EVT_MENU(IDM_EXTRACTFILE, CFilesystemViewer::OnExtractFile)
	EVT_MENU(IDM_REPLACEFILE, CFilesystemViewer::OnReplaceFile)
	EVT_MENU(IDM_RENAMEFILE, CFilesystemViewer::OnRenameFile)
END_EVENT_TABLE()

DiscIO::IVolume *OpenISO = NULL;
DiscIO::IFileSystem *pFileSystem = NULL;

CFilesystemViewer::CFilesystemViewer(const std::string fileName, wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& position, const wxSize& size, long style)
	: wxDialog(parent, id, title, position, size, style)
{
	std::vector<const DiscIO::SFileInfo *> Our_Files;

	OpenISO = DiscIO::CreateVolumeFromFilename(fileName);
	pFileSystem = DiscIO::CreateFileSystem(OpenISO);
	pFileSystem->GetFileList(Our_Files);

	GameListItem OpenISO_(fileName);

	CreateGUIControls();

	// shuffle2: things only appear in the tree for me when using debug build; why? :<
	wxTreeItemId dirId = NULL;
	fileIter beginning = Our_Files.begin(), pos = Our_Files.begin();

	CreateDirectoryTree(RootId, beginning, pos, "\\");

	m_Treectrl->Expand(RootId);

	// Disk header and apploader
	m_Name->SetValue(wxString(OpenISO->GetName().c_str(), wxConvUTF8));
	m_Serial->SetValue(wxString(OpenISO->GetUniqueID().c_str(), wxConvUTF8));
	switch (OpenISO->GetCountry())
	{
	case DiscIO::IVolume::COUNTRY_EUROPE:
	case DiscIO::IVolume::COUNTRY_FRANCE:
		m_Country->SetValue(wxString::FromAscii("EUR"));
		break;
	case DiscIO::IVolume::COUNTRY_USA:
		m_Country->SetValue(wxString::FromAscii("USA"));
		break;
	case DiscIO::IVolume::COUNTRY_JAP:
		m_Country->SetValue(wxString::FromAscii("JAP"));
		break;
	default:
		m_Country->SetValue(wxString::FromAscii("UNKNOWN"));
		break;
	}
	m_MakerID->SetValue(wxString::Format(_T("0x%s"), OpenISO->GetMakerID().c_str()));
	m_Date->SetValue(wxString(OpenISO->GetApploaderDate().c_str(), wxConvUTF8));
	m_TOC->SetValue(wxString::Format(_T("%u"), OpenISO->GetFSTSize()));

	// Banner
	// ...all the BannerLoader functions are bool...gross
	//m_Version;
	m_ShortName->SetValue(wxString(OpenISO_.GetName().c_str(), wxConvUTF8));
	//m_LongName->SetValue(wxString(OpenISO_.GetLongName().c_str(), wxConvUTF8));
	m_Maker->SetValue(wxString(OpenISO_.GetCompany().c_str(), wxConvUTF8));//dev too
	m_Comment->SetValue(wxString(OpenISO_.GetDescription().c_str(), wxConvUTF8));
}

CFilesystemViewer::~CFilesystemViewer()
{
	delete pFileSystem;
	delete OpenISO;
}

void CFilesystemViewer::CreateDirectoryTree(wxTreeItemId& parent, 
											fileIter& begin,
											fileIter& iterPos,
											char *directory)
{
	//TODO(XK): Fix more than one folder/file not appearing in the root
	if(iterPos == begin)
	 	++iterPos;

	char *name = (char *)((*iterPos)->m_FullPath);

	do
	{
		if((*iterPos)->IsDirectory()) {
			char *dirName;
			name[strlen(name) - 1] = '\0';
			dirName = strrchr(name, '\\');
			if(!dirName)
				dirName = name;
			else
				dirName++;
			//filename = strrchr(name, '\\'); ++filename;
			wxTreeItemId item = m_Treectrl->AppendItem(parent, wxT(dirName));
			CreateDirectoryTree(item, begin, ++iterPos, name);
		} else {

		//filename = strrchr(name, '\\'); ++filename;
			m_Treectrl->AppendItem(parent, wxT(strrchr(name, '\\') + 1));
		}
		++iterPos;
		name = (char *)((*iterPos)->m_FullPath);
	} while(strstr(name, directory));
}

void CFilesystemViewer::CreateGUIControls()
{
	m_Close = new wxButton(this, ID_CLOSE, wxT("Close"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	
	// ISO Details
	sbISODetails = new wxStaticBoxSizer(wxVERTICAL, this, wxT("ISO Details:"));
	sISODetails = new wxGridBagSizer(0, 0);
	m_NameText = new wxStaticText(this, ID_NAME_TEXT, wxT("Name:"), wxDefaultPosition, wxDefaultSize);
	m_Name = new wxTextCtrl(this, ID_NAME, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	m_SerialText = new wxStaticText(this, ID_SERIAL_TEXT, wxT("Serial:"), wxDefaultPosition, wxDefaultSize);
	m_Serial = new wxTextCtrl(this, ID_SERIAL, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	m_CountryText = new wxStaticText(this, ID_COUNTRY_TEXT, wxT("Country:"), wxDefaultPosition, wxDefaultSize);
	m_Country = new wxTextCtrl(this, ID_COUNTRY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	m_MakerIDText = new wxStaticText(this, ID_MAKERID_TEXT, wxT("Maker ID:"), wxDefaultPosition, wxDefaultSize);
	m_MakerID = new wxTextCtrl(this, ID_MAKERID, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	m_DateText = new wxStaticText(this, ID_DATE_TEXT, wxT("Date:"), wxDefaultPosition, wxDefaultSize);
	m_Date = new wxTextCtrl(this, ID_DATE, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	m_TOCText = new wxStaticText(this, ID_TOC_TEXT, wxT("TOC Size:"), wxDefaultPosition, wxDefaultSize);	
	m_TOC = new wxTextCtrl(this, ID_TOC, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);

	sISODetails->Add(m_NameText, wxGBPosition(0, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sISODetails->Add(m_Name, wxGBPosition(0, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	sISODetails->Add(m_SerialText, wxGBPosition(1, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sISODetails->Add(m_Serial, wxGBPosition(1, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	sISODetails->Add(m_CountryText, wxGBPosition(2, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sISODetails->Add(m_Country, wxGBPosition(2, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	sISODetails->Add(m_MakerIDText, wxGBPosition(3, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sISODetails->Add(m_MakerID, wxGBPosition(3, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	sISODetails->Add(m_DateText, wxGBPosition(4, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sISODetails->Add(m_Date, wxGBPosition(4, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	sISODetails->Add(m_TOCText, wxGBPosition(5, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sISODetails->Add(m_TOC, wxGBPosition(5, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);

	sbISODetails->Add(sISODetails, 0, wxEXPAND, 5);

	// Banner Details
	wxArrayString arrayStringFor_Lang;
	sbBannerDetails = new wxStaticBoxSizer(wxVERTICAL, this, wxT("Banner Details:"));
	sBannerDetails = new wxGridBagSizer(0, 0);
	m_VersionText = new wxStaticText(this, ID_VERSION_TEXT, wxT("Version:"), wxDefaultPosition, wxDefaultSize);
	m_Version = new wxTextCtrl(this, ID_VERSION, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	m_LangText = new wxStaticText(this, ID_LANG_TEXT, wxT("Show Language:"), wxDefaultPosition, wxDefaultSize);
	m_Lang = new wxChoice(this, ID_LANG, wxDefaultPosition, wxDefaultSize, arrayStringFor_Lang, 0, wxDefaultValidator);
	m_ShortText = new wxStaticText(this, ID_SHORTNAME_TEXT, wxT("Short Name:"), wxDefaultPosition, wxDefaultSize);
	m_ShortName = new wxTextCtrl(this, ID_SHORTNAME, wxEmptyString, wxDefaultPosition, wxDefaultSize);
	m_LongText = new wxStaticText(this, ID_LONGNAME_TEXT, wxT("Long Name:"), wxDefaultPosition, wxDefaultSize);
	m_LongName = new wxTextCtrl(this, ID_LONGNAME, wxEmptyString, wxDefaultPosition, wxDefaultSize);
	m_MakerText = new wxStaticText(this, ID_MAKER_TEXT, wxT("Maker:"), wxDefaultPosition, wxDefaultSize);
	m_Maker = new wxTextCtrl(this, ID_MAKER, wxEmptyString, wxDefaultPosition, wxDefaultSize);
	m_CommentText = new wxStaticText(this, ID_COMMENT_TEXT, wxT("Comment:"), wxDefaultPosition, wxDefaultSize);
	m_Comment = new wxTextCtrl(this, ID_COMMENT, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE);
	m_BannerText = new wxStaticText(this, ID_BANNER_TEXT, wxT("Banner:"), wxDefaultPosition, wxDefaultSize);
	// Needs to be image:
	m_Banner = new wxTextCtrl(this, ID_BANNER, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE);
	m_SaveBNR = new wxButton(this, ID_SAVEBNR, wxT("Save Changes to BNR"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_SaveBNR->Disable();

	sBannerDetails->Add(m_VersionText, wxGBPosition(0, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sBannerDetails->Add(m_Version, wxGBPosition(0, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	sBannerDetails->Add(m_LangText, wxGBPosition(0, 2), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sBannerDetails->Add(m_Lang, wxGBPosition(0, 3), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	sBannerDetails->Add(m_ShortText, wxGBPosition(1, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sBannerDetails->Add(m_ShortName, wxGBPosition(1, 1), wxGBSpan(1, 3), wxEXPAND|wxALL, 5);
	sBannerDetails->Add(m_LongText, wxGBPosition(2, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sBannerDetails->Add(m_LongName, wxGBPosition(2, 1), wxGBSpan(1, 3), wxEXPAND|wxALL, 5);
	sBannerDetails->Add(m_MakerText, wxGBPosition(3, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sBannerDetails->Add(m_Maker, wxGBPosition(3, 1), wxGBSpan(1, 3), wxEXPAND|wxALL, 5);
	sBannerDetails->Add(m_CommentText, wxGBPosition(4, 0), wxGBSpan(1, 1), wxALL, 5);
	sBannerDetails->Add(m_Comment, wxGBPosition(4, 1), wxGBSpan(1, 3), wxEXPAND|wxALL, 5);
	sBannerDetails->Add(m_BannerText, wxGBPosition(5, 0), wxGBSpan(1, 1), wxALL, 5);
	sBannerDetails->Add(m_Banner, wxGBPosition(5, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	sBannerDetails->Add(m_SaveBNR, wxGBPosition(5, 2), wxGBSpan(1, 2), wxEXPAND|wxALL, 5);

	sbBannerDetails->Add(sBannerDetails, 0, wxEXPAND, 0);
	
	// Filesystem tree
	sbTreectrl = new wxStaticBoxSizer(wxVERTICAL, this, wxT("Filesytem:"));
	m_Treectrl = new wxTreeCtrl(this, ID_TREECTRL, wxDefaultPosition, wxSize(350, 450)/*wxDefaultSize*/, wxTR_DEFAULT_STYLE, wxDefaultValidator);
	sbTreectrl->Add(m_Treectrl, 1, wxEXPAND);

	RootId = m_Treectrl->AddRoot(wxT("Root"), -1, -1, 0);

	wxGridBagSizer* sMain;
	sMain = new wxGridBagSizer(0, 0);
	sMain->Add(sbISODetails, wxGBPosition(0, 0), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	sMain->Add(sbBannerDetails, wxGBPosition(1, 0), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	sMain->Add(sbTreectrl, wxGBPosition(0, 1), wxGBSpan(2, 1), wxALL, 5);
	sMain->Add(m_Close, wxGBPosition(2, 1), wxGBSpan(1, 1), wxALL|wxALIGN_RIGHT, 5);

	this->SetSizer(sMain);
	this->Layout();
	Fit();
}

void CFilesystemViewer::OnClose(wxCloseEvent& WXUNUSED (event))
{
	Destroy();
}

void CFilesystemViewer::OnCloseClick(wxCommandEvent& WXUNUSED (event))
{
	Close();
}

void CFilesystemViewer::OnRightClick(wxMouseEvent& WXUNUSED (event))
{
	//check for right click on banner image
	//if(event.GetId() == ID_SAVEBNR)
	//{
	//	//on banner then save as.
	//	wxMenu popupMenu;
	//	popupMenu.Append(IDM_BNRSAVEAS, wxString::FromAscii("Save as..."));
	//	PopupMenu(&popupMenu);
	//}
	//event.Skip();
}

void CFilesystemViewer::OnRightClickOnTree(wxTreeEvent& event)
{
	wxMenu popupMenu;
	popupMenu.Append(IDM_EXTRACTFILE, wxString::FromAscii("Extract File..."));
	popupMenu.Append(IDM_REPLACEFILE, wxString::FromAscii("Replace File..."));
	popupMenu.Append(IDM_RENAMEFILE, wxString::FromAscii("Rename File..."));
	PopupMenu(&popupMenu);

	event.Skip();
}

void CFilesystemViewer::OnSaveBNRClick(wxCommandEvent& WXUNUSED (event))
{

}

void CFilesystemViewer::OnBannerImageSave(wxCommandEvent& WXUNUSED (event))
{

}

void CFilesystemViewer::OnExtractFile(wxCommandEvent& WXUNUSED (event))
{
	wxString Path;
	wxString File;
	Path = wxFileSelector(
		_T("Export File"),
		wxEmptyString, wxEmptyString, wxEmptyString,
		wxString::Format
		(
		_T("All files (%s)|%s"),
		wxFileSelectorDefaultWildcardStr
		),
		wxFD_SAVE,
		this);
		
	File = m_Treectrl->GetItemText(m_Treectrl->GetSelection());
	if (!Path || !File)
		return;
	pFileSystem->ExportFile(File.mb_str(), Path.mb_str());
}

void CFilesystemViewer::OnReplaceFile(wxCommandEvent& WXUNUSED (event))
{

}

void CFilesystemViewer::OnRenameFile(wxCommandEvent& WXUNUSED (event))
{

}
