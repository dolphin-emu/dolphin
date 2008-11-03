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
	EVT_TREE_ITEM_RIGHT_CLICK(ID_TREECTRL,CFilesystemViewer::OnRightClickOnTree)
	EVT_BUTTON(ID_CLOSE,CFilesystemViewer::OnCloseClick)
	EVT_MENU(IDM_BNRSAVEAS, CFilesystemViewer::OnBannerImageSave)
	EVT_MENU(IDM_EXTRACTFILE, CFilesystemViewer::OnExtractFile)
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

	fileIter beginning = Our_Files.begin(), end = Our_Files.end(), 
		     pos = Our_Files.begin();

	CreateDirectoryTree(RootId, beginning, end, pos, (char *)"\\");	

	m_Treectrl->Expand(RootId);

	// Disk header and apploader
	m_Name->SetValue(wxString(OpenISO->GetName().c_str(), wxConvUTF8));
	m_GameID->SetValue(wxString(OpenISO->GetUniqueID().c_str(), wxConvUTF8));
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
	
	wxString temp;
	temp = _T("0x") + wxString::FromAscii(OpenISO->GetMakerID().c_str());
	m_MakerID->SetValue(temp);
	m_Date->SetValue(wxString(OpenISO->GetApploaderDate().c_str(), wxConvUTF8));
	m_FST->SetValue(wxString::Format(_T("%u"), OpenISO->GetFSTSize()));

	// Banner
	// ...all the BannerLoader functions are bool...gross
	//m_Version;
	//if (OpenISO_.GetBNRVersion() == "BNR1")
		m_Lang->Enable(false);
	m_ShortName->SetValue(wxString(OpenISO_.GetName().c_str(), wxConvUTF8));
	//m_LongName->SetValue(wxString(OpenISO_.GetLongName().c_str(), wxConvUTF8));
	m_Maker->SetValue(wxString(OpenISO_.GetCompany().c_str(), wxConvUTF8));//dev too
	m_Comment->SetValue(wxString(OpenISO_.GetDescription().c_str(), wxConvUTF8));
	m_Banner->SetBitmap(OpenISO_.GetImage());
	m_Banner->Connect(wxID_ANY, wxEVT_RIGHT_DOWN,
		wxMouseEventHandler(CFilesystemViewer::RightClickOnBanner), (wxObject*)NULL, this);

	Fit();
}

CFilesystemViewer::~CFilesystemViewer()
{
	delete pFileSystem;
	delete OpenISO;
}

void CFilesystemViewer::CreateDirectoryTree(wxTreeItemId& parent, 
											fileIter& begin,
											fileIter& end,
											fileIter& iterPos,
											char *directory)
{
	bool bRoot = true;

	if(iterPos == begin)
	 	++iterPos;
	else
		bRoot = false;

	char *name = (char *)((*iterPos)->m_FullPath);

	if(iterPos == end)
		return;

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

			wxTreeItemId item = m_Treectrl->AppendItem(parent, wxString::FromAscii(dirName));
			CreateDirectoryTree(item, begin, end, ++iterPos, name);
		} else {
			char *fileName = strrchr(name, '\\');
			if(!fileName)
				fileName = name;
			else
				fileName++;

			m_Treectrl->AppendItem(parent, wxString::FromAscii(fileName));
			++iterPos;
		}

		if(iterPos == end)
			break;
		
		name = (char *)((*iterPos)->m_FullPath);

	} while(bRoot || strstr(name, directory));
}

void CFilesystemViewer::CreateGUIControls()
{
	m_Close = new wxButton(this, ID_CLOSE, wxT("Close"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	
	// ISO Details
	sbISODetails = new wxStaticBoxSizer(wxVERTICAL, this, wxT("ISO Details"));
	sISODetails = new wxGridBagSizer(0, 0);
	sISODetails->AddGrowableCol(1);
	m_NameText = new wxStaticText(this, ID_NAME_TEXT, wxT("Name:"), wxDefaultPosition, wxDefaultSize);
	m_Name = new wxTextCtrl(this, ID_NAME, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	m_GameIDText = new wxStaticText(this, ID_GAMEID_TEXT, wxT("Game ID:"), wxDefaultPosition, wxDefaultSize);
	m_GameID = new wxTextCtrl(this, ID_GAMEID, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	m_CountryText = new wxStaticText(this, ID_COUNTRY_TEXT, wxT("Country:"), wxDefaultPosition, wxDefaultSize);
	m_Country = new wxTextCtrl(this, ID_COUNTRY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	m_MakerIDText = new wxStaticText(this, ID_MAKERID_TEXT, wxT("Maker ID:"), wxDefaultPosition, wxDefaultSize);
	m_MakerID = new wxTextCtrl(this, ID_MAKERID, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	m_DateText = new wxStaticText(this, ID_DATE_TEXT, wxT("Date:"), wxDefaultPosition, wxDefaultSize);
	m_Date = new wxTextCtrl(this, ID_DATE, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	m_FSTText = new wxStaticText(this, ID_FST_TEXT, wxT("FST Size:"), wxDefaultPosition, wxDefaultSize);	
	m_FST = new wxTextCtrl(this, ID_FST, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);

	sISODetails->Add(m_NameText, wxGBPosition(0, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sISODetails->Add(m_Name, wxGBPosition(0, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	sISODetails->Add(m_GameIDText, wxGBPosition(1, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sISODetails->Add(m_GameID, wxGBPosition(1, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	sISODetails->Add(m_CountryText, wxGBPosition(2, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sISODetails->Add(m_Country, wxGBPosition(2, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	sISODetails->Add(m_MakerIDText, wxGBPosition(3, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sISODetails->Add(m_MakerID, wxGBPosition(3, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	sISODetails->Add(m_DateText, wxGBPosition(4, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sISODetails->Add(m_Date, wxGBPosition(4, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	sISODetails->Add(m_FSTText, wxGBPosition(5, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sISODetails->Add(m_FST, wxGBPosition(5, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);

	sbISODetails->Add(sISODetails, 0, wxEXPAND, 5);

	// Banner Details
	sbBannerDetails = new wxStaticBoxSizer(wxVERTICAL, this, wxT("Banner Details"));
	sBannerDetails = new wxGridBagSizer(0, 0);
	m_VersionText = new wxStaticText(this, ID_VERSION_TEXT, wxT("Version:"), wxDefaultPosition, wxDefaultSize);
	m_Version = new wxTextCtrl(this, ID_VERSION, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	m_LangText = new wxStaticText(this, ID_LANG_TEXT, wxT("Show Language:"), wxDefaultPosition, wxDefaultSize);
	arrayStringFor_Lang.Add(wxT("English"));
	arrayStringFor_Lang.Add(wxT("German"));
	arrayStringFor_Lang.Add(wxT("French"));
	arrayStringFor_Lang.Add(wxT("Spanish"));
	arrayStringFor_Lang.Add(wxT("Italian"));
	arrayStringFor_Lang.Add(wxT("Dutch"));
	m_Lang = new wxChoice(this, ID_LANG, wxDefaultPosition, wxDefaultSize, arrayStringFor_Lang, 0, wxDefaultValidator);
	m_Lang->SetSelection(0);
	m_ShortText = new wxStaticText(this, ID_SHORTNAME_TEXT, wxT("Short Name:"), wxDefaultPosition, wxDefaultSize);
	m_ShortName = new wxTextCtrl(this, ID_SHORTNAME, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	m_LongText = new wxStaticText(this, ID_LONGNAME_TEXT, wxT("Long Name:"), wxDefaultPosition, wxDefaultSize);
	m_LongName = new wxTextCtrl(this, ID_LONGNAME, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	m_MakerText = new wxStaticText(this, ID_MAKER_TEXT, wxT("Maker:"), wxDefaultPosition, wxDefaultSize);
	m_Maker = new wxTextCtrl(this, ID_MAKER, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	m_CommentText = new wxStaticText(this, ID_COMMENT_TEXT, wxT("Comment:"), wxDefaultPosition, wxDefaultSize);
	m_Comment = new wxTextCtrl(this, ID_COMMENT, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE|wxTE_READONLY);
	m_BannerText = new wxStaticText(this, ID_BANNER_TEXT, wxT("Banner:"), wxDefaultPosition, wxDefaultSize);
	m_Banner = new wxStaticBitmap(this, ID_BANNER, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0);

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

	sbBannerDetails->Add(sBannerDetails, 0, wxEXPAND, 0);
	
	// Filesystem tree
	sbTreectrl = new wxStaticBoxSizer(wxVERTICAL, this, wxT("Filesytem"));
	m_Treectrl = new wxTreeCtrl(this, ID_TREECTRL, wxDefaultPosition, wxSize(350, -1), wxTR_DEFAULT_STYLE, wxDefaultValidator);
	sbTreectrl->Add(m_Treectrl, 1, wxEXPAND);

	RootId = m_Treectrl->AddRoot(wxT("Root"), -1, -1, 0);

	wxGridBagSizer* sMain;
	sMain = new wxGridBagSizer(0, 0);
	sMain->Add(sbISODetails, wxGBPosition(0, 0), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	sMain->Add(sbBannerDetails, wxGBPosition(1, 0), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	sMain->Add(sbTreectrl, wxGBPosition(0, 1), wxGBSpan(2, 1), wxEXPAND|wxALL, 5);
	sMain->Add(m_Close, wxGBPosition(2, 1), wxGBSpan(1, 1), wxALL|wxALIGN_RIGHT, 5);

	this->SetSizer(sMain);
	this->Layout();
}

void CFilesystemViewer::OnClose(wxCloseEvent& WXUNUSED (event))
{
	Destroy();
}

void CFilesystemViewer::OnCloseClick(wxCommandEvent& WXUNUSED (event))
{
	Close();
}

void CFilesystemViewer::RightClickOnBanner(wxMouseEvent& event)
{
	wxMenu popupMenu;
	popupMenu.Append(IDM_BNRSAVEAS, _("Save as..."));
	PopupMenu(&popupMenu);

	event.Skip();
}

void CFilesystemViewer::OnBannerImageSave(wxCommandEvent& event)
{
	wxString dirHome;

	wxFileDialog dialog(this, _("Save as..."), wxGetHomeDir(&dirHome), wxString::Format(_("%s.png"), m_GameID->GetLabel()),
		_("*.*"), wxFD_SAVE|wxFD_OVERWRITE_PROMPT, wxDefaultPosition, wxDefaultSize);
	if (dialog.ShowModal() == wxID_OK)
	{
		m_Banner->GetBitmap().ConvertToImage().SaveFile(dialog.GetPath());
	}
}

void CFilesystemViewer::OnRightClickOnTree(wxTreeEvent& event)
{
	m_Treectrl->SelectItem(event.GetItem());

	wxMenu popupMenu;
	if (m_Treectrl->ItemHasChildren(m_Treectrl->GetSelection()))
		;//popupMenu.Append(IDM_EXTRACTFILE, wxString::FromAscii("Extract Directory..."));
	else
		popupMenu.Append(IDM_EXTRACTFILE, wxString::FromAscii("Extract File..."));
	PopupMenu(&popupMenu);

	event.Skip();
}

void CFilesystemViewer::OnExtractFile(wxCommandEvent& WXUNUSED (event))
{
	wxString Path;
	wxString File;

	File = m_Treectrl->GetItemText(m_Treectrl->GetSelection());
	
	Path = wxFileSelector(
		_T("Export File"),
		wxEmptyString, File, wxEmptyString,
		wxString::Format
		(
			_T("All files (%s)|%s"),
			wxFileSelectorDefaultWildcardStr,
			wxFileSelectorDefaultWildcardStr
		),
		wxFD_SAVE,
		this);

	if (!Path || !File)
		return;

	while (m_Treectrl->GetItemParent(m_Treectrl->GetSelection()) != m_Treectrl->GetRootItem())
	{
		wxString temp;
		temp = m_Treectrl->GetItemText(m_Treectrl->GetItemParent(m_Treectrl->GetSelection()));
		File = temp + _T("\\") + File;

		m_Treectrl->SelectItem(m_Treectrl->GetItemParent(m_Treectrl->GetSelection()));
	}

	pFileSystem->ExportFile(File.mb_str(), Path.mb_str());
}
