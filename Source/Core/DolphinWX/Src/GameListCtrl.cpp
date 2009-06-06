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

#include <wx/imaglist.h>
#include <wx/fontmap.h>

#include <algorithm>

#include "FileSearch.h"
#include "StringUtil.h"
#include "ConfigManager.h"
#include "GameListCtrl.h"
#include "Blob.h"
#include "Core.h"
#include "ISOProperties.h"
#include "IniFile.h"
#include "FileUtil.h"
#include "CDUtils.h"

#if USE_XPM_BITMAPS
    #include "../resources/Flag_Europe.xpm"
    #include "../resources/Flag_France.xpm"
	#include "../resources/Flag_Italy.xpm"
    #include "../resources/Flag_Japan.xpm"
    #include "../resources/Flag_USA.xpm"
    #include "../resources/Platform_Wii.xpm"
    #include "../resources/Platform_Gamecube.xpm"
#endif // USE_XPM_BITMAPS

// ugly that this lib included code from the main
#include "../../DolphinWX/Src/WxUtils.h"

size_t CGameListCtrl::m_currentItem = 0;
size_t CGameListCtrl::m_numberItem = 0;
std::string CGameListCtrl::m_currentFilename;


static int currentColumn = 0;
bool operator < (const GameListItem &one, const GameListItem &other)
{
	int indexOne = 0;
	int indexOther = 0;

	switch (one.GetCountry())
	{
	case DiscIO::IVolume::COUNTRY_JAP:;
	case DiscIO::IVolume::COUNTRY_USA:indexOne = 0; break;
	default: indexOne = (int)SConfig::GetInstance().m_InterfaceLanguage;
	}

	switch (other.GetCountry())
	{
	case DiscIO::IVolume::COUNTRY_JAP:;
	case DiscIO::IVolume::COUNTRY_USA:indexOther = 0; break;
	default: indexOther = (int)SConfig::GetInstance().m_InterfaceLanguage;
	}

	switch(currentColumn)
	{
	case CGameListCtrl::COLUMN_TITLE:		return strcasecmp(one.GetName(indexOne).c_str(),        other.GetName(indexOther).c_str()) < 0;
	case CGameListCtrl::COLUMN_COMPANY:		return strcasecmp(one.GetCompany().c_str(),     other.GetCompany().c_str()) < 0;
	case CGameListCtrl::COLUMN_NOTES:		return strcasecmp(one.GetDescription(indexOne).c_str(), other.GetDescription(indexOther).c_str()) < 0;
	case CGameListCtrl::COLUMN_COUNTRY:		return (one.GetCountry() < other.GetCountry());
	case CGameListCtrl::COLUMN_SIZE:		return (one.GetFileSize() < other.GetFileSize());
	case CGameListCtrl::COLUMN_PLATFORM:	return (one.GetPlatform() < other.GetPlatform());
	default:								return strcasecmp(one.GetName(indexOne).c_str(), other.GetName(indexOther).c_str()) < 0;
	}
}

BEGIN_EVENT_TABLE(CGameListCtrl, wxListCtrl)

EVT_SIZE(CGameListCtrl::OnSize)
EVT_RIGHT_DOWN(CGameListCtrl::OnRightClick)
EVT_LIST_COL_BEGIN_DRAG(LIST_CTRL, CGameListCtrl::OnColBeginDrag)
EVT_LIST_COL_CLICK(LIST_CTRL, CGameListCtrl::OnColumnClick)
EVT_MENU(IDM_PROPERTIES, CGameListCtrl::OnProperties)
EVT_MENU(IDM_OPENCONTAININGFOLDER, CGameListCtrl::OnOpenContainingFolder)
EVT_MENU(IDM_OPENSAVEFOLDER, CGameListCtrl::OnOpenSaveFolder)
EVT_MENU(IDM_SETDEFAULTGCM, CGameListCtrl::OnSetDefaultGCM)
EVT_MENU(IDM_COMPRESSGCM, CGameListCtrl::OnCompressGCM)
EVT_MENU(IDM_MULTICOMPRESSGCM, CGameListCtrl::OnMultiCompressGCM)
EVT_MENU(IDM_MULTIDECOMPRESSGCM, CGameListCtrl::OnMultiDecompressGCM)
EVT_MENU(IDM_DELETEGCM, CGameListCtrl::OnDeleteGCM)
END_EVENT_TABLE()

CGameListCtrl::CGameListCtrl(wxWindow* parent, const wxWindowID id, const wxPoint& pos, const wxSize& size, long style)
	: wxListCtrl(parent, id, pos, size, style)
{
}

CGameListCtrl::~CGameListCtrl()
{
	if (m_imageListSmall)
		delete m_imageListSmall;
}

void CGameListCtrl::InitBitmaps()
{
	m_imageListSmall = new wxImageList(96, 32);
	SetImageList(m_imageListSmall, wxIMAGE_LIST_SMALL);
	m_FlagImageIndex.resize(DiscIO::IVolume::NUMBER_OF_COUNTRIES);
	wxIcon iconTemp;
	iconTemp.CopyFromBitmap(wxBitmap(Flag_Europe_xpm));
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_EUROPE] = m_imageListSmall->Add(iconTemp);
	iconTemp.CopyFromBitmap(wxBitmap(Flag_France_xpm));
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_FRANCE] = m_imageListSmall->Add(iconTemp);
	iconTemp.CopyFromBitmap(wxBitmap(Flag_Italy_xpm));
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_ITALY] = m_imageListSmall->Add(iconTemp);
	iconTemp.CopyFromBitmap(wxBitmap(Flag_USA_xpm));
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_USA] = m_imageListSmall->Add(iconTemp);
	iconTemp.CopyFromBitmap(wxBitmap(Flag_Japan_xpm));
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_JAP] = m_imageListSmall->Add(iconTemp);
	iconTemp.CopyFromBitmap(wxBitmap(Flag_Europe_xpm));
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_UNKNOWN] = m_imageListSmall->Add(iconTemp);

	m_PlatformImageIndex.resize(2);
	iconTemp.CopyFromBitmap(wxBitmap(Platform_Gamecube_xpm));
	m_PlatformImageIndex[0] = m_imageListSmall->Add(iconTemp);
	iconTemp.CopyFromBitmap(wxBitmap(Platform_Wii_xpm));
	m_PlatformImageIndex[1] = m_imageListSmall->Add(iconTemp);
}

void CGameListCtrl::BrowseForDirectory()
{
	wxString dirHome;
	wxGetHomeDir(&dirHome);

	// browse
	wxDirDialog dialog(this, _("Browse for a directory to add"), dirHome, wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);

	if (dialog.ShowModal() == wxID_OK)
	{
		std::string sPath(dialog.GetPath().ToAscii());
		std::vector<std::string>::iterator itResult = std::find(
			SConfig::GetInstance().m_ISOFolder.begin(), SConfig::GetInstance().m_ISOFolder.end(), sPath
			);

		if (itResult == SConfig::GetInstance().m_ISOFolder.end())
		{
			SConfig::GetInstance().m_ISOFolder.push_back(sPath);
			SConfig::GetInstance().SaveSettings();
		}
		
		Update();
	}
}

void CGameListCtrl::Update()
{
	// Don't let the user refresh it while the a game is running
	if (Core::GetState() != Core::CORE_UNINITIALIZED)
		return;
	if (m_imageListSmall)
	{
		delete m_imageListSmall;
		m_imageListSmall = NULL;
	}

	// NetPlay : Set/Reset the GameList string
	m_gameList.clear();
	m_gamePath.clear();

	Hide();

	ScanForISOs();

	ClearAll();

	if (m_ISOFiles.size() != 0)
	{
		// Don't load bitmaps unless there are games to list
		InitBitmaps();

		// add columns
		InsertColumn(COLUMN_BANNER, _("Banner"));
		InsertColumn(COLUMN_TITLE, _("Title"));
		InsertColumn(COLUMN_COMPANY, _("Company"));
		InsertColumn(COLUMN_NOTES, _("Notes"));
		InsertColumn(COLUMN_COUNTRY, _(""));
		InsertColumn(COLUMN_SIZE, _("Size"));
		InsertColumn(COLUMN_EMULATION_STATE, _("Emulation"));
		InsertColumn(COLUMN_PLATFORM, _("Platform"));

		// set initial sizes for columns
		SetColumnWidth(COLUMN_BANNER, 106);
		SetColumnWidth(COLUMN_TITLE, 150);
		SetColumnWidth(COLUMN_COMPANY, 130);
		SetColumnWidth(COLUMN_NOTES, 150);
		SetColumnWidth(COLUMN_COUNTRY, 32);
		SetColumnWidth(COLUMN_EMULATION_STATE, 130);
		SetColumnWidth(COLUMN_PLATFORM, 90);

		// add all items
		for (int i = 0; i < (int)m_ISOFiles.size(); i++)
		{
			InsertItemInReportView(i);
			if (m_ISOFiles[i].IsCompressed())
				SetItemTextColour(i, wxColour(0xFF0000));
		}

		SetColumnWidth(COLUMN_SIZE, wxLIST_AUTOSIZE);
	}
	else
	{
		InsertColumn(COLUMN_BANNER, _("No ISOs found"));

		long index = InsertItem(0, wxString::FromAscii("msgRow"));
		SetItem(index, COLUMN_BANNER, _("Dolphin could not find any GC/Wii ISOs. Doubleclick here to browse for files..."));
		SetItemFont(index, *wxITALIC_FONT);
		SetColumnWidth(COLUMN_BANNER, wxLIST_AUTOSIZE);
	}

	AutomaticColumnWidth();

	Show();
}

wxString NiceSizeFormat(s64 _size)
{
	const char* sizes[] = {"b", "KB", "MB", "GB", "TB", "PB", "EB"};
	int s = 0;
	int frac = 0;
	
	while (_size > (s64)1024)
	{
		s++;
		frac   = (int)_size & 1023;
		_size /= (s64)1024;
	}

	float f = (float)_size + ((float)frac / 1024.0f);

	wxString NiceString;
	char tempstr[32];
	sprintf(tempstr,"%3.1f %s", f, sizes[s]);
	NiceString = wxString::FromAscii(tempstr);
	return(NiceString);
}

std::string CGameListCtrl::GetGamePaths() const
{ 
	return m_gamePath; 
}
std::string CGameListCtrl::GetGameNames() const
{ 
	return m_gameList; 
}

void CGameListCtrl::InsertItemInReportView(long _Index)
{
	// When using wxListCtrl, there is no hope of per-column text colors.
	// But for reference, here are the old colors that were used: (BGR)
	// title: 0xFF0000
	// company: 0x007030

	wxString name, description;
	GameListItem& rISOFile = m_ISOFiles[_Index];
	m_gamePath.append(std::string(rISOFile.GetFileName()) + '\n');
	
	int ImageIndex = -1;
	
	if (rISOFile.GetImage().IsOk())
	{
		ImageIndex = m_imageListSmall->Add(rISOFile.GetImage());
	}

	// Insert a row with the banner image
	long ItemIndex = InsertItem(_Index, wxEmptyString, ImageIndex);

	switch (rISOFile.GetCountry())
	{
	case DiscIO::IVolume::COUNTRY_JAP:
		// keep these codes, when we move to wx unicode...
		//wxCSConv convFrom(wxFontMapper::GetEncodingName(wxFONTENCODING_SHIFT_JIS));
		//wxCSConv convTo(wxFontMapper::GetEncodingName(wxFONTENCODING_DEFAULT));
		//SetItem(_Index, COLUMN_TITLE, wxString(wxString(rISOFile.GetName()).wc_str(convFrom) , convTo), -1);
		//SetItem(_Index, COLUMN_NOTES, wxString(wxString(rISOFile.GetDescription()).wc_str(convFrom) , convTo), -1);
		if (CopySJISToString(name, rISOFile.GetName(0).c_str()))
			SetItem(_Index, COLUMN_TITLE, name, -1);
		if (CopySJISToString(description, rISOFile.GetDescription(0).c_str()))
			SetItem(_Index, COLUMN_NOTES, description, -1);

		// NetPLay string
		m_gameList.append(std::string(name.mb_str()) + " (J)\n");

		break;
	case DiscIO::IVolume::COUNTRY_USA:
		m_gameList.append(std::string(wxString::From8BitData(rISOFile.GetName(0).c_str()).mb_str()) + " (U)\n");
		SetItem(_Index, COLUMN_TITLE, wxString::From8BitData(rISOFile.GetName(0).c_str()), -1);
		SetItem(_Index, COLUMN_NOTES, wxString::From8BitData(rISOFile.GetDescription(0).c_str()), -1);
		break;
	default:
		m_gameList.append(std::string(wxString::From8BitData(rISOFile.GetName((int)SConfig::GetInstance().m_InterfaceLanguage).c_str()).mb_str()) + " (E)\n");
		SetItem(_Index, COLUMN_TITLE, 
			wxString::From8BitData(rISOFile.GetName((int)SConfig::GetInstance().m_InterfaceLanguage).c_str()), -1);
		SetItem(_Index, COLUMN_NOTES, 
			wxString::From8BitData(rISOFile.GetDescription((int)SConfig::GetInstance().m_InterfaceLanguage).c_str()), -1);
		break;
	}

	SetItem(_Index, COLUMN_COMPANY, wxString::From8BitData(rISOFile.GetCompany().c_str()), -1);
	SetItem(_Index, COLUMN_SIZE, NiceSizeFormat(rISOFile.GetFileSize()), -1);

	// Load the INI file for columns that read from it
	IniFile ini;
	std::string GameIni = FULL_GAMECONFIG_DIR + (rISOFile.GetUniqueID()) + ".ini";
	ini.Load(GameIni.c_str());

	// Emulation status = COLUMN_EMULATION_STATE
	{
		wxListItem item;
		item.SetId(_Index);
		std::string EmuState;
		std::string issues;
		item.SetColumn(COLUMN_EMULATION_STATE);
		ini.Get("EmuState","EmulationStateId",&EmuState);
		if (!EmuState.empty())
		{
			switch(atoi(EmuState.c_str()))
			{
			case 5:
				item.SetText(_("Perfect"));
				break;
			case 4:
				item.SetText(_("In Game"));
				break;
			case 3:
				item.SetText(_("Intro"));
				break;
			case 2:
				//NOTE (Daco): IMO under 2 goes problems like music and games that only work with specific settings
				ini.Get("EmuState","EmulationIssues",&issues);
				if (!issues.empty())
				{
					issues = "Problems: " + issues;
					item.SetText(wxString::FromAscii(issues.c_str()));
				}
				else
					item.SetText(_("Problems: Other"));
				break;
			case 1:
				item.SetText(_("Broken"));
				break;
			case 0:
				item.SetText(_("Not Set"));
				break;
			default:
				//if the EmuState isn't a number between 0 & 5 we dont know the state D:
				item.SetText(_("unknown emu ID"));
				break;
			}
		}
		SetItem(item);
	}

	// Country
	SetItemColumnImage(_Index, COLUMN_COUNTRY, m_FlagImageIndex[rISOFile.GetCountry()]);
	//Platform
	SetItemColumnImage(_Index, COLUMN_PLATFORM, m_PlatformImageIndex[rISOFile.GetPlatform()]);

	// Background color
	SetBackgroundColor();

	// Item data
	SetItemData(_Index, ItemIndex);
}

wxColour blend50(const wxColour& c1, const wxColour& c2)
{
	unsigned char r,g,b,a;
	r = c1.Red()/2   + c2.Red()/2;
	g = c1.Green()/2 + c2.Green()/2;
	b = c1.Blue()/2  + c2.Blue()/2;
	a = c1.Alpha()/2 + c2.Alpha()/2;
	return a << 24 | b << 16 | g << 8 | r;
}

void CGameListCtrl::SetBackgroundColor()
{
	for(long i = 0; i < GetItemCount(); i++)
	{
		wxColour color = (i & 1) ? blend50(wxSystemSettings::GetColour(wxSYS_COLOUR_3DLIGHT), wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW)) : wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
		CGameListCtrl::SetItemBackgroundColour(i, color);
	}
}

void CGameListCtrl::ScanForISOs()
{
	m_ISOFiles.clear();
	CFileSearch::XStringVector Directories(SConfig::GetInstance().m_ISOFolder);

	if (SConfig::GetInstance().m_RecursiveISOFolder)
	{
		for (u32 i = 0; i < Directories.size(); i++)
		{
			File::FSTEntry FST_Temp;
			File::ScanDirectoryTree(Directories.at(i).c_str(), FST_Temp);
			for (u32 j = 0; j < FST_Temp.children.size(); j++)
			{
				if (FST_Temp.children.at(j).isDirectory)
				{
					bool duplicate = false;
					NormalizeDirSep(&(FST_Temp.children.at(j).physicalName));
					for (u32 k = 0; k < Directories.size(); k++)
					{
						NormalizeDirSep(&Directories.at(k));
						if (strcmp(Directories.at(k).c_str(), FST_Temp.children.at(j).physicalName.c_str()) == 0)
						{
							duplicate = true;
							break;
						}
					}
					if (!duplicate)
						Directories.push_back(FST_Temp.children.at(j).physicalName.c_str());
				}
			}
		}
	}

	CFileSearch::XStringVector Extensions;
	Extensions.push_back("*.iso");
	Extensions.push_back("*.gcm");
	Extensions.push_back("*.gcz");

	CFileSearch FileSearch(Extensions, Directories);
	const CFileSearch::XStringVector& rFilenames = FileSearch.GetFileNames();

	if (rFilenames.size() > 0)
	{
		wxProgressDialog dialog(_T("Scanning for ISOs"),
					_T("Scanning..."),
					(int)rFilenames.size(), // range
					this, // parent
					wxPD_APP_MODAL |
					// wxPD_AUTO_HIDE | -- try this as well
					wxPD_ELAPSED_TIME |
					wxPD_ESTIMATED_TIME |
					wxPD_REMAINING_TIME |
					wxPD_SMOOTH // - makes indeterminate mode bar on WinXP very small
					);
		dialog.CenterOnParent();

		for (u32 i = 0; i < rFilenames.size(); i++)
		{
			std::string FileName;
			SplitPath(rFilenames[i], NULL, &FileName, NULL);

			wxString msg;
			char tempstring[128];
			sprintf(tempstring,"Scanning %s", FileName.c_str());
			msg = wxString::FromAscii(tempstring);

			// Update with the progress (i) and the message (msg)
			bool Cont = dialog.Update(i, msg);
			if (!Cont)
			{
				break;
			}
			GameListItem ISOFile(rFilenames[i]);
			if (ISOFile.IsValid())
			{
				bool list = true;

				switch(ISOFile.GetPlatform())
				{
				case GameListItem::WII_DISC:
					if (!SConfig::GetInstance().m_ListWii)
						list = false;
					break;
				default:
					if (!SConfig::GetInstance().m_ListGC)
						list = false;
					break;
				}

				switch(ISOFile.GetCountry())
				{
				case DiscIO::IVolume::COUNTRY_JAP:
					if (!SConfig::GetInstance().m_ListJap)
						list = false;
					break;
				case DiscIO::IVolume::COUNTRY_USA:
					if (!SConfig::GetInstance().m_ListUsa)
						list = false;
					break;
				default:
					if (!SConfig::GetInstance().m_ListPal)
						list = false;
					break;
				}

				if (list) m_ISOFiles.push_back(ISOFile);
			}
		}
	}

	if (SConfig::GetInstance().m_ListDrives)
	{
		char **drives = cdio_get_devices();
		GameListItem * Drive[24];
		for (int i = 0; drives[i] != NULL && i < 24; i++)
		{
			Drive[i] = new GameListItem(drives[i]);
			if (Drive[i]->IsValid())	m_ISOFiles.push_back(*Drive[i]);
		}
	}

	std::sort(m_ISOFiles.begin(), m_ISOFiles.end());
}

void CGameListCtrl::OnColBeginDrag(wxListEvent& event)
{
	if (event.GetColumn() != COLUMN_TITLE && event.GetColumn() != COLUMN_COMPANY
		&& event.GetColumn() != COLUMN_NOTES)
		event.Veto();
}

const GameListItem *CGameListCtrl::GetISO(int index) const
{
	return &m_ISOFiles[index];
}

CGameListCtrl *caller;
int wxCALLBACK wxListCompare(long item1, long item2, long sortData)
{
	//return 1 if item1 > item2
	//return -1 if item1 < item2
	//0 for identity
	const GameListItem *iso1 = caller->GetISO(item1);
	const GameListItem *iso2 = caller->GetISO(item2);
 
	int t = 1;
 
	if (sortData < 0)
	{
		t = -1;
		sortData = -sortData;
	}

	int indexOne = 0;
	int indexOther = 0;

	switch (iso1->GetCountry())
	{
	case DiscIO::IVolume::COUNTRY_JAP:;
	case DiscIO::IVolume::COUNTRY_USA:indexOne = 0; break;
	default: indexOne = (int)SConfig::GetInstance().m_InterfaceLanguage;
	}

	switch (iso2->GetCountry())
	{
	case DiscIO::IVolume::COUNTRY_JAP:;
	case DiscIO::IVolume::COUNTRY_USA:indexOther = 0; break;
	default: indexOther = (int)SConfig::GetInstance().m_InterfaceLanguage;
	}

	switch(sortData)
	{
	case CGameListCtrl::COLUMN_TITLE:
		return strcasecmp(iso1->GetName(indexOne).c_str(),iso2->GetName(indexOther).c_str()) *t;
	case CGameListCtrl::COLUMN_COMPANY:
		return strcasecmp(iso1->GetCompany().c_str(),iso2->GetCompany().c_str()) *t;
	case CGameListCtrl::COLUMN_NOTES:
		return strcasecmp(iso1->GetDescription(indexOne).c_str(),iso2->GetDescription(indexOther).c_str()) *t;
	case CGameListCtrl::COLUMN_COUNTRY:
		if(iso1->GetCountry() > iso2->GetCountry()) return  1 *t;
		if(iso1->GetCountry() < iso2->GetCountry()) return -1 *t;
		return 0;
	case CGameListCtrl::COLUMN_SIZE:
		if (iso1->GetFileSize() > iso2->GetFileSize()) return  1 *t;
		if (iso1->GetFileSize() < iso2->GetFileSize()) return -1 *t;
		return 0;
	case CGameListCtrl::COLUMN_PLATFORM:
		if(iso1->GetPlatform() > iso2->GetPlatform()) return  1 *t;
		if(iso1->GetPlatform() < iso2->GetPlatform()) return -1 *t;
		return 0;
	}
 
	return 0;
}

void CGameListCtrl::OnColumnClick(wxListEvent& event)
{
	if(event.GetColumn() != COLUMN_BANNER && event.GetColumn() != COLUMN_EMULATION_STATE)
	{
		int current_column = event.GetColumn();
 
		if(last_column == current_column)
		{
			last_sort = -last_sort;
		}
		else
		{
			last_column = current_column;
			last_sort = current_column;
		}
 
		caller = this;
		SortItems(wxListCompare, last_sort);
	}

	SetBackgroundColor();

	event.Skip();
}

void CGameListCtrl::OnRightClick(wxMouseEvent& event)
{
	// Focus the clicked item.
	int flags;
    long item = HitTest(event.GetPosition(), flags);
	if (item != wxNOT_FOUND) 
	{
		if (GetItemState(item, wxLIST_STATE_SELECTED) != wxLIST_STATE_SELECTED)
		{
			UnselectAll();
			SetItemState(item, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED );
		}
		SetItemState(item, wxLIST_STATE_FOCUSED, wxLIST_STATE_FOCUSED);
	}

	if (GetSelectedItemCount() == 1)
	{
		const GameListItem *selected_iso = GetSelectedISO();
		if (selected_iso)
		{
			wxMenu popupMenu;
			popupMenu.Append(IDM_PROPERTIES, _("&Properties"));
			popupMenu.AppendSeparator();
			if (selected_iso->GetPlatform() == GameListItem::WII_DISC)
				popupMenu.Append(IDM_OPENSAVEFOLDER, _("Open Wii &save folder"));
			popupMenu.Append(IDM_OPENCONTAININGFOLDER, _("Open &containing folder"));
			popupMenu.AppendCheckItem(IDM_SETDEFAULTGCM, _("Set as &default ISO"));
			
			// First we have to decide a starting value when we append it
			if(selected_iso->GetFileName() == SConfig::GetInstance().
				m_LocalCoreStartupParameter.m_strDefaultGCM)
				popupMenu.FindItem(IDM_SETDEFAULTGCM)->Check();

			popupMenu.AppendSeparator();
			popupMenu.Append(IDM_DELETEGCM, _("&Delete ISO..."));

			if (selected_iso->IsCompressed())
				popupMenu.Append(IDM_COMPRESSGCM, _("Decompress ISO..."));
			else
				popupMenu.Append(IDM_COMPRESSGCM, _("Compress ISO..."));

			PopupMenu(&popupMenu);
		}
	}
	else if (GetSelectedItemCount() > 1)
	{
		wxMenu popupMenu;
		popupMenu.Append(IDM_DELETEGCM, _("&Delete selected ISOs..."));
		popupMenu.AppendSeparator();
		popupMenu.Append(IDM_MULTICOMPRESSGCM, _("Compress selected ISOs..."));
		popupMenu.Append(IDM_MULTIDECOMPRESSGCM, _("Decompress selected ISOs..."));
		PopupMenu(&popupMenu);
	}
}

const GameListItem * CGameListCtrl::GetSelectedISO()
{
	if (m_ISOFiles.size() == 0)
	{
		// There are no detected games, so add a GCMPath
		BrowseForDirectory();
		return 0;
	}
	else
	{
		long item = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED); 
		if (item == -1) // -1 means the selection is bogus, not a gamelistitem
			return 0;
		else
		{
			// Here is a little workaround for multiselections:
			// when > 1 item is selected, return info on the first one
			// and deselect it so the next time GetSelectedISO() is called,
			// the next item's info is returned
			if (GetSelectedItemCount() > 1)
				SetItemState(item, 0, wxLIST_STATE_SELECTED);

			return &m_ISOFiles[GetItemData(item)];
		}
	}
}

void CGameListCtrl::OnOpenContainingFolder(wxCommandEvent& WXUNUSED (event)) 
{
	const GameListItem *iso = GetSelectedISO();
	if (!iso)
		return;
	std::string path;
	SplitPath(iso->GetFileName(), &path, 0, 0);
	WxUtils::Explore(path.c_str());
}

void CGameListCtrl::OnOpenSaveFolder(wxCommandEvent& WXUNUSED (event)) 
{
	const GameListItem *iso = GetSelectedISO();
	if (!iso)
		return;
	if (iso->GetWiiFSPath() != "NULL")
		WxUtils::Explore(iso->GetWiiFSPath().c_str());
}


// =======================================================
// Save this file as the default file
// -------------
void CGameListCtrl::OnSetDefaultGCM(wxCommandEvent& event) 
{
	const GameListItem *iso = GetSelectedISO();
	if (!iso) return;
	
	if (event.IsChecked()) // Write the new default value and save it the ini file
	{
		SConfig::GetInstance().m_LocalCoreStartupParameter.m_strDefaultGCM = iso->GetFileName();
		SConfig::GetInstance().SaveSettings();
	}
	else // Othwerise blank the value and save it
	{
		SConfig::GetInstance().m_LocalCoreStartupParameter.m_strDefaultGCM = "";
		SConfig::GetInstance().SaveSettings();
	}
}
// =============


void CGameListCtrl::OnDeleteGCM(wxCommandEvent& WXUNUSED (event))
{
	if (GetSelectedItemCount() == 1)
	{
		const GameListItem *iso = GetSelectedISO();
		if (!iso)
			return;
		if (wxMessageBox(_("Are you sure you want to delete this file?\nIt will be gone forever!"),
			wxMessageBoxCaptionStr, wxYES_NO | wxICON_EXCLAMATION) == wxYES)
		{
			File::Delete(iso->GetFileName().c_str());
			Update();
		}
	}
	else
	{
		if (wxMessageBox(_("Are you sure you want to delete these files?\nThey will be gone forever!"),
			wxMessageBoxCaptionStr, wxYES_NO | wxICON_EXCLAMATION) == wxYES)
		{
			int selected = GetSelectedItemCount();

			for (int i = 0; i < selected; i++)
			{
				const GameListItem *iso = GetSelectedISO();
				File::Delete(iso->GetFileName().c_str());
			}
			Update();
		}
	}
}

void CGameListCtrl::OnProperties(wxCommandEvent& WXUNUSED (event)) 
{
	const GameListItem *iso = GetSelectedISO();
	if (!iso)
		return;
	CISOProperties ISOProperties(iso->GetFileName(), this);
	if(ISOProperties.ShowModal() == wxID_OK)
		Update();
}

void CGameListCtrl::MultiCompressCB(const char* text, float percent, void* arg)
{
    wxString textString(wxString::Format(wxT("%s (%i/%i) - %s"), m_currentFilename.c_str(), (int)m_currentItem+1, (int)m_numberItem, text));

    percent = (((float)m_currentItem) + percent) / (float)m_numberItem;
    wxProgressDialog* pDialog = (wxProgressDialog*)arg;
    pDialog->Update((int)(percent*1000), textString);
}

void CGameListCtrl::OnMultiCompressGCM(wxCommandEvent& /*event*/)
{
	CompressSelection(true);
}

void CGameListCtrl::OnMultiDecompressGCM(wxCommandEvent& /*event*/)
{
	CompressSelection(false);
}

void CGameListCtrl::CompressSelection(bool _compress)
{
	wxString dirHome;
	wxGetHomeDir(&dirHome);

	wxDirDialog browseDialog(this, _("Browse for output directory"), dirHome, wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
	if (browseDialog.ShowModal() != wxID_OK)
		return;

	wxProgressDialog progressDialog(_compress ? _("Compressing ISO") : _("Decompressing ISO"),
		_("Working..."),
		1000, // range
		this, // parent
		wxPD_APP_MODAL |
		// wxPD_AUTO_HIDE | -- try this as well
		wxPD_ELAPSED_TIME |
		wxPD_ESTIMATED_TIME |
		wxPD_REMAINING_TIME |
		wxPD_SMOOTH // - makes indeterminate mode bar on WinXP very small
		);
	
	progressDialog.SetSize(wxSize(600, 180));
	progressDialog.CenterOnParent();

	m_currentItem = 0;
	m_numberItem = GetSelectedItemCount();
	for (u32 i=0; i<m_numberItem; i++)
	{
		const GameListItem *iso = GetSelectedISO();

			if (!iso->IsCompressed() && _compress)
			{
				std::string FileName;
				SplitPath(iso->GetFileName(), NULL, &FileName, NULL);
				m_currentFilename = FileName;
				FileName.append(".gcz");

				std::string OutputFileName;
				BuildCompleteFilename(OutputFileName, (const char *)browseDialog.GetPath().mb_str(wxConvUTF8), FileName);

				DiscIO::CompressFileToBlob(iso->GetFileName().c_str(), OutputFileName.c_str(), (iso->GetPlatform() == GameListItem::WII_DISC) ? 1 : 0, 16384, &MultiCompressCB, &progressDialog);					
			}
			else if (iso->IsCompressed() && !_compress)
			{
				std::string FileName;
				SplitPath(iso->GetFileName(), NULL, &FileName, NULL);
				m_currentFilename = FileName;
				FileName.append(".gcm");

				std::string OutputFileName;
				BuildCompleteFilename(OutputFileName, (const char *)browseDialog.GetPath().mb_str(wxConvUTF8), FileName);

				DiscIO::DecompressBlobToFile(iso->GetFileName().c_str(), OutputFileName.c_str(), &MultiCompressCB, &progressDialog);
			}
			m_currentItem++;
	}
	Update();
}

void CGameListCtrl::CompressCB(const char* text, float percent, void* arg)
{
	wxProgressDialog* pDialog = (wxProgressDialog*)arg;
	pDialog->Update((int)(percent*1000), wxString::FromAscii(text));
}

void CGameListCtrl::OnCompressGCM(wxCommandEvent& WXUNUSED (event)) 
{
	const GameListItem *iso = GetSelectedISO();
	if (!iso)
		return;

	wxString path;

	std::string FileName;
	SplitPath(iso->GetFileName(), NULL, &FileName, NULL);

	if (iso->IsCompressed())
	{
		 path = wxFileSelector(
			_T("Save decompressed ISO"),
			wxEmptyString, wxString::FromAscii(FileName.c_str()), wxEmptyString,
			wxString::Format
			(
			_T("All GC/Wii ISO files (gcm)|*.gcm|All files (%s)|%s"),
			wxFileSelectorDefaultWildcardStr,
			wxFileSelectorDefaultWildcardStr
			),
			wxFD_SAVE,
			this);

		if (!path)
		{
			return;
		}
	}
	else
	{
		path = wxFileSelector(
			_T("Save compressed ISO"),
			wxEmptyString, wxString::FromAscii(FileName.c_str()), wxEmptyString,
			wxString::Format
			(
			_T("All compressed GC/Wii ISO files (gcz)|*.gcz|All files (%s)|%s"),
			wxFileSelectorDefaultWildcardStr,
			wxFileSelectorDefaultWildcardStr
			),
			wxFD_SAVE,
			this);

		if (!path)
		{
			return;
		}
	}

	wxProgressDialog dialog(iso->IsCompressed() ? _T("Decompressing ISO") : _T("Compressing ISO"),
		_T("Working..."),
		1000, // range
		this, // parent
		wxPD_APP_MODAL |
		// wxPD_AUTO_HIDE | -- try this as well
		wxPD_ELAPSED_TIME |
		wxPD_ESTIMATED_TIME |
		wxPD_REMAINING_TIME |
		wxPD_SMOOTH // - makes indeterminate mode bar on WinXP very small
		);
	
	dialog.SetSize(wxSize(280, 180));
	dialog.CenterOnParent();

	if (iso->IsCompressed())
		DiscIO::DecompressBlobToFile(iso->GetFileName().c_str(), path.char_str(), &CompressCB, &dialog);	
	else
		DiscIO::CompressFileToBlob(iso->GetFileName().c_str(), path.char_str(), (iso->GetPlatform() == GameListItem::WII_DISC) ? 1 : 0, 16384, &CompressCB, &dialog);

	Update();
}

void CGameListCtrl::OnSize(wxSizeEvent& event)
{
	AutomaticColumnWidth();

	event.Skip();
}

void CGameListCtrl::AutomaticColumnWidth()
{
	wxRect rc(GetClientRect());

	if (GetColumnCount() == 1)
	{
		SetColumnWidth(0, rc.GetWidth());
	}
	else if (GetColumnCount() > 4)
	{
		int resizable = rc.GetWidth() - (
			GetColumnWidth(COLUMN_BANNER)
			+ GetColumnWidth(COLUMN_COUNTRY)
			+ GetColumnWidth(COLUMN_SIZE)
			+ GetColumnWidth(COLUMN_EMULATION_STATE)
			+ GetColumnWidth(COLUMN_PLATFORM)
			+ 5); // some pad to keep the horizontal scrollbar away :)

		SetColumnWidth(COLUMN_TITLE, wxMax(0.3*resizable, 100));
		SetColumnWidth(COLUMN_COMPANY, wxMax(0.2*resizable, 90));
		SetColumnWidth(COLUMN_NOTES, wxMax(0.5*resizable, 100));
	}
}

void CGameListCtrl::UnselectAll()
{
	for (int i=0; i<GetItemCount(); i++)
	{
		SetItemState(i, 0, wxLIST_STATE_SELECTED);
	}

}

bool CGameListCtrl::CopySJISToString( wxString& _rDestination, const char* _src )
{
	bool returnCode = false;
#ifdef WIN32
	// HyperIris: because dolphin using "Use Multi-Byte Character Set",
	// we must convert the SJIS chars to unicode then to our windows local by hand
	u32 unicodeNameSize = MultiByteToWideChar(932, MB_PRECOMPOSED, 
		_src, (int)strlen(_src),	NULL, NULL);
	if (unicodeNameSize > 0)
	{
		u16* pUnicodeStrBuffer = new u16[unicodeNameSize + 1];
		if (pUnicodeStrBuffer)
		{
			memset(pUnicodeStrBuffer, 0, (unicodeNameSize + 1) * sizeof(u16));
			if (MultiByteToWideChar(932, MB_PRECOMPOSED, 
				_src, (int)strlen(_src),
				(LPWSTR)pUnicodeStrBuffer, unicodeNameSize))
			{
				u32 ansiNameSize = WideCharToMultiByte(CP_ACP, 0, 
					(LPCWSTR)pUnicodeStrBuffer, unicodeNameSize,
					NULL, NULL, NULL, NULL);
				if (ansiNameSize > 0)
				{
					char* pAnsiStrBuffer = new char[ansiNameSize + 1];
					if (pAnsiStrBuffer)
					{
						memset(pAnsiStrBuffer, 0, (ansiNameSize + 1) * sizeof(char));
						if (WideCharToMultiByte(CP_ACP, 0, 
							(LPCWSTR)pUnicodeStrBuffer, unicodeNameSize,
							pAnsiStrBuffer, ansiNameSize, NULL, NULL))
						{
							_rDestination = pAnsiStrBuffer;
							returnCode = true;
						}
						delete pAnsiStrBuffer;
					}
				}
			}
			delete pUnicodeStrBuffer;
		}		
	}
#else
	_rDestination = wxString(wxString(_src,wxConvLibc),wxConvUTF8);
	returnCode = true;
#endif
	return returnCode;
}

