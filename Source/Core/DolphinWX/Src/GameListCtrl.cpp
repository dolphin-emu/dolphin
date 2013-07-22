// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Globals.h"

#include <wx/imaglist.h>
#include <wx/fontmap.h>
#include <wx/filename.h>

#include <algorithm>
#include <memory>

#include "FileSearch.h"
#include "StringUtil.h"
#include "ConfigManager.h"
#include "GameListCtrl.h"
#include "Blob.h"
#include "Core.h"
#include "ISOProperties.h"
#include "FileUtil.h"
#include "CDUtils.h"
#include "WxUtils.h"
#include "Main.h"
#include "MathUtil.h"
#include "HW/DVDInterface.h"

#include "../resources/Flag_Europe.xpm"
#include "../resources/Flag_Germany.xpm"
#include "../resources/Flag_France.xpm"
#include "../resources/Flag_Italy.xpm"
#include "../resources/Flag_Japan.xpm"
#include "../resources/Flag_USA.xpm"
#include "../resources/Flag_Taiwan.xpm"
#include "../resources/Flag_Korea.xpm"
#include "../resources/Flag_Unknown.xpm"
#include "../resources/Flag_SDK.xpm"

#include "../resources/Platform_Wad.xpm"
#include "../resources/Platform_Wii.xpm"
#include "../resources/Platform_Gamecube.xpm"
#include "../resources/rating_gamelist.h"

size_t CGameListCtrl::m_currentItem = 0;
size_t CGameListCtrl::m_numberItem = 0;
std::string CGameListCtrl::m_currentFilename;
bool sorted = false;

extern CFrame* main_frame;

static int CompareGameListItems(const GameListItem* iso1, const GameListItem* iso2,
								long sortData = CGameListCtrl::COLUMN_TITLE)
{
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
		case DiscIO::IVolume::COUNTRY_JAPAN:
		case DiscIO::IVolume::COUNTRY_USA:
			indexOne = 0;
			break;
		default:
			indexOne = SConfig::GetInstance().m_LocalCoreStartupParameter.SelectedLanguage;
	}

	switch (iso2->GetCountry())
	{
		case DiscIO::IVolume::COUNTRY_JAPAN:
		case DiscIO::IVolume::COUNTRY_USA:
			indexOther = 0;
			break;
		default:
			indexOther = SConfig::GetInstance().m_LocalCoreStartupParameter.SelectedLanguage;
	}

	switch(sortData)
	{
		case CGameListCtrl::COLUMN_TITLE:
			if (!strcasecmp(iso1->GetName(indexOne).c_str(),iso2->GetName(indexOther).c_str()))
			{
				if (iso1->IsDiscTwo())
					return 1 * t;
				else if (iso2->IsDiscTwo())
					return -1 * t;
			}
			return strcasecmp(iso1->GetName(indexOne).c_str(),
					iso2->GetName(indexOther).c_str()) * t;
		case CGameListCtrl::COLUMN_NOTES:
			{
				std::string cmp1 =
					(iso1->GetPlatform() == GameListItem::GAMECUBE_DISC) ?
					iso1->GetCompany() : iso1->GetDescription(indexOne);
				std::string cmp2 =
					(iso2->GetPlatform() == GameListItem::GAMECUBE_DISC) ?
					iso2->GetCompany() : iso2->GetDescription(indexOther);
				return strcasecmp(cmp1.c_str(), cmp2.c_str()) * t;
			}
		case CGameListCtrl::COLUMN_COUNTRY:
			if(iso1->GetCountry() > iso2->GetCountry())
				return  1 * t;
			if(iso1->GetCountry() < iso2->GetCountry())
				return -1 * t;
			return 0;
		case CGameListCtrl::COLUMN_SIZE:
			if (iso1->GetFileSize() > iso2->GetFileSize())
				return  1 * t;
			if (iso1->GetFileSize() < iso2->GetFileSize())
				return -1 * t;
			return 0;
		case CGameListCtrl::COLUMN_PLATFORM:
			if(iso1->GetPlatform() > iso2->GetPlatform())
				return  1 * t;
			if(iso1->GetPlatform() < iso2->GetPlatform())
				return -1 * t;
			return 0;

		case CGameListCtrl::COLUMN_EMULATION_STATE:
		{
			const int
				nState1 = iso1->GetEmuState(),
				nState2 = iso2->GetEmuState();

			if (nState1 > nState2)
				return  1 * t;
			if (nState1 < nState2)
				return -1 * t;
			else
				return 0;
		}
			break;
	}

	return 0;
}

bool operator < (const GameListItem &one, const GameListItem &other)
{
	return CompareGameListItems(&one, &other) < 0;
}

BEGIN_EVENT_TABLE(wxEmuStateTip, wxTipWindow)
	EVT_KEY_DOWN(wxEmuStateTip::OnKeyDown)
END_EVENT_TABLE()

BEGIN_EVENT_TABLE(CGameListCtrl, wxListCtrl)
	EVT_SIZE(CGameListCtrl::OnSize)
	EVT_RIGHT_DOWN(CGameListCtrl::OnRightClick)
	EVT_LEFT_DOWN(CGameListCtrl::OnLeftClick)
	EVT_LIST_KEY_DOWN(LIST_CTRL, CGameListCtrl::OnKeyPress)
	EVT_MOTION(CGameListCtrl::OnMouseMotion)
	EVT_LIST_COL_BEGIN_DRAG(LIST_CTRL, CGameListCtrl::OnColBeginDrag)
	EVT_LIST_COL_CLICK(LIST_CTRL, CGameListCtrl::OnColumnClick)
	EVT_MENU(IDM_PROPERTIES, CGameListCtrl::OnProperties)
	EVT_MENU(IDM_GAMEWIKI, CGameListCtrl::OnWiki)
	EVT_MENU(IDM_OPENCONTAININGFOLDER, CGameListCtrl::OnOpenContainingFolder)
	EVT_MENU(IDM_OPENSAVEFOLDER, CGameListCtrl::OnOpenSaveFolder)
	EVT_MENU(IDM_EXPORTSAVE, CGameListCtrl::OnExportSave)
	EVT_MENU(IDM_SETDEFAULTGCM, CGameListCtrl::OnSetDefaultGCM)
	EVT_MENU(IDM_COMPRESSGCM, CGameListCtrl::OnCompressGCM)
	EVT_MENU(IDM_MULTICOMPRESSGCM, CGameListCtrl::OnMultiCompressGCM)
	EVT_MENU(IDM_MULTIDECOMPRESSGCM, CGameListCtrl::OnMultiDecompressGCM)
	EVT_MENU(IDM_DELETEGCM, CGameListCtrl::OnDeleteGCM)
END_EVENT_TABLE()

CGameListCtrl::CGameListCtrl(wxWindow* parent, const wxWindowID id, const
		wxPoint& pos, const wxSize& size, long style)
	: wxListCtrl(parent, id, pos, size, style), toolTip(0)
{
	DragAcceptFiles(true);
	Connect(wxEVT_DROP_FILES, wxDropFilesEventHandler(CGameListCtrl::OnDropFiles), NULL, this);
}

CGameListCtrl::~CGameListCtrl()
{
	if (m_imageListSmall)
		delete m_imageListSmall;

	ClearIsoFiles();
}

void CGameListCtrl::InitBitmaps()
{
	m_imageListSmall = new wxImageList(96, 32);
	SetImageList(m_imageListSmall, wxIMAGE_LIST_SMALL);

	m_FlagImageIndex.resize(DiscIO::IVolume::NUMBER_OF_COUNTRIES);
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_EUROPE] =
		m_imageListSmall->Add(wxBitmap(Flag_Europe_xpm), wxNullBitmap);
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_GERMANY] =
		m_imageListSmall->Add(wxBitmap(Flag_Germany_xpm), wxNullBitmap);
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_FRANCE] =
		m_imageListSmall->Add(wxBitmap(Flag_France_xpm), wxNullBitmap);
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_USA] =
		m_imageListSmall->Add(wxBitmap(Flag_USA_xpm), wxNullBitmap);
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_JAPAN] =
		m_imageListSmall->Add(wxBitmap(Flag_Japan_xpm), wxNullBitmap);
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_KOREA] =
		m_imageListSmall->Add(wxBitmap(Flag_Korea_xpm), wxNullBitmap);
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_ITALY] =
		m_imageListSmall->Add(wxBitmap(Flag_Italy_xpm), wxNullBitmap);
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_TAIWAN] =
		m_imageListSmall->Add(wxBitmap(Flag_Taiwan_xpm), wxNullBitmap);
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_SDK] =
		m_imageListSmall->Add(wxBitmap(Flag_SDK_xpm), wxNullBitmap);
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_UNKNOWN] =
		m_imageListSmall->Add(wxBitmap(Flag_Unknown_xpm), wxNullBitmap);

	m_PlatformImageIndex.resize(3);
	m_PlatformImageIndex[0] =
		m_imageListSmall->Add(wxBitmap(Platform_Gamecube_xpm), wxNullBitmap);
	m_PlatformImageIndex[1] =
		m_imageListSmall->Add(wxBitmap(Platform_Wii_xpm), wxNullBitmap);
	m_PlatformImageIndex[2] =
		m_imageListSmall->Add(wxBitmap(Platform_Wad_xpm), wxNullBitmap);

	m_EmuStateImageIndex.resize(6);
	m_EmuStateImageIndex[0] =
		m_imageListSmall->Add(wxBitmap(rating_0), wxNullBitmap);
	m_EmuStateImageIndex[1] =
		m_imageListSmall->Add(wxBitmap(rating_1), wxNullBitmap);
	m_EmuStateImageIndex[2] =
		m_imageListSmall->Add(wxBitmap(rating_2), wxNullBitmap);
	m_EmuStateImageIndex[3] =
		m_imageListSmall->Add(wxBitmap(rating_3), wxNullBitmap);
	m_EmuStateImageIndex[4] =
		m_imageListSmall->Add(wxBitmap(rating_4), wxNullBitmap);
	m_EmuStateImageIndex[5] =
		m_imageListSmall->Add(wxBitmap(rating_5), wxNullBitmap);
}

void CGameListCtrl::BrowseForDirectory()
{
	wxString dirHome;
	wxGetHomeDir(&dirHome);

	// browse
	wxDirDialog dialog(this, _("Browse for a directory to add"), dirHome,
			wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);

	if (dialog.ShowModal() == wxID_OK)
	{
		std::string sPath(WxStrToStr(dialog.GetPath()));
		std::vector<std::string>::iterator itResult = std::find(
				SConfig::GetInstance().m_ISOFolder.begin(),
				SConfig::GetInstance().m_ISOFolder.end(), sPath);

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
	int scrollPos = wxWindow::GetScrollPos(wxVERTICAL);
	// Don't let the user refresh it while a game is running
	if (Core::GetState() != Core::CORE_UNINITIALIZED)
		return;

	if (m_imageListSmall)
	{
		delete m_imageListSmall;
		m_imageListSmall = NULL;
	}

	Hide();

	ScanForISOs();

	ClearAll();

	if (m_ISOFiles.size() != 0)
	{
		// Don't load bitmaps unless there are games to list
		InitBitmaps();

		// add columns
		InsertColumn(COLUMN_DUMMY,_T(""));
		InsertColumn(COLUMN_PLATFORM, _T(""));
		InsertColumn(COLUMN_BANNER, _("Banner"));
		InsertColumn(COLUMN_TITLE, _("Title"));

		// Instead of showing the notes + the company, which is unknown with
		// wii titles We show in the same column : company for GC games and
		// description for wii/wad games
		InsertColumn(COLUMN_NOTES, _("Notes"));
		InsertColumn(COLUMN_COUNTRY, _T(""));
		InsertColumn(COLUMN_SIZE, _("Size"));
		InsertColumn(COLUMN_EMULATION_STATE, _("State"));

#ifdef __WXMSW__
		const int platform_padding = 0;
#else
		const int platform_padding = 8;
#endif
		
		// set initial sizes for columns
		SetColumnWidth(COLUMN_DUMMY,0);
		SetColumnWidth(COLUMN_PLATFORM, 35 + platform_padding);
		SetColumnWidth(COLUMN_BANNER, 96 + platform_padding);
		SetColumnWidth(COLUMN_TITLE, 200 + platform_padding);
		SetColumnWidth(COLUMN_NOTES, 200 + platform_padding);
		SetColumnWidth(COLUMN_COUNTRY, 32 + platform_padding);
		SetColumnWidth(COLUMN_EMULATION_STATE, 50 + platform_padding);

		// add all items
		for (int i = 0; i < (int)m_ISOFiles.size(); i++)
		{
			InsertItemInReportView(i);
			if (m_ISOFiles[i]->IsCompressed())
				SetItemTextColour(i, wxColour(0xFF0000));
		}

		// Sort items by Title
		if (!sorted)
			last_column = 0;
		sorted = false;
		wxListEvent event;
		event.m_col = SConfig::GetInstance().m_ListSort2;
		OnColumnClick(event);

		event.m_col = SConfig::GetInstance().m_ListSort;
		OnColumnClick(event);
		sorted = true;

		SetColumnWidth(COLUMN_SIZE, wxLIST_AUTOSIZE);
	}
	else
	{
		wxString errorString;
		// We just check for one hide setting to be enabled, as we may only
		// have GC games for example, and hide them, so we should show the
		// second message instead
		if ((SConfig::GetInstance().m_ListGC  &&
			SConfig::GetInstance().m_ListWii  &&
			SConfig::GetInstance().m_ListWad) &&
			(SConfig::GetInstance().m_ListJap &&
			SConfig::GetInstance().m_ListUsa  &&
			SConfig::GetInstance().m_ListPal))
		{
			errorString = _("Dolphin could not find any GC/Wii ISOs.  Doubleclick here to browse for files...");
		}
		else
		{
			errorString = _("Dolphin is currently set to hide all games.  Doubleclick here to show all games...");
		}
		InsertColumn(0, _("No ISOs or WADS found"));
		long index = InsertItem(0, errorString);
		SetItemFont(index, *wxITALIC_FONT);
		SetColumnWidth(0, wxLIST_AUTOSIZE);
	}
	if (GetSelectedISO() == NULL)
		main_frame->UpdateGUI();
	Show();

	AutomaticColumnWidth();
	ScrollLines(scrollPos);
	SetFocus();
}

wxString NiceSizeFormat(u64 _size)
{
	const char* const unit_symbols[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB"};
	
	auto const unit = Log2(std::max<u64>(_size, 1)) / 10;
	auto const unit_size = (1 << (unit * 10));
	
	// ugly rounding integer math
	auto const value = (_size + unit_size / 2) / unit_size;
	auto const frac = (_size % unit_size * 10 + unit_size / 2) / unit_size % 10;

	return StrToWxStr(StringFromFormat("%llu.%llu %s", value, frac, unit_symbols[unit]));
}

void CGameListCtrl::InsertItemInReportView(long _Index)
{
	// When using wxListCtrl, there is no hope of per-column text colors.
	// But for reference, here are the old colors that were used: (BGR)
	// title: 0xFF0000
	// company: 0x007030
	int ImageIndex = -1;

	GameListItem& rISOFile = *m_ISOFiles[_Index];

	// Insert a first row with nothing in it, that will be used as the Index
	long ItemIndex = InsertItem(_Index, wxEmptyString);

	// Insert the platform's image in the first (visible) column
	SetItemColumnImage(_Index, COLUMN_PLATFORM, m_PlatformImageIndex[rISOFile.GetPlatform()]);

	if (rISOFile.GetImage().IsOk())
		ImageIndex = m_imageListSmall->Add(rISOFile.GetImage());

	// Set the game's banner in the second column
	SetItemColumnImage(_Index, COLUMN_BANNER, ImageIndex);

	int SelectedLanguage = SConfig::GetInstance().m_LocalCoreStartupParameter.SelectedLanguage;
	
	// Is this sane?
	switch (rISOFile.GetCountry())
	{
	case DiscIO::IVolume::COUNTRY_TAIWAN:
	case DiscIO::IVolume::COUNTRY_JAPAN:
		SelectedLanguage = -1;
		break;
		
	case DiscIO::IVolume::COUNTRY_USA:
		SelectedLanguage = 0;
		break;
		
	default:
		break;
	}
	
	std::string const name = rISOFile.GetName(SelectedLanguage);
	SetItem(_Index, COLUMN_TITLE, StrToWxStr(name), -1);

	// We show the company string on Gamecube only
	// On Wii we show the description instead as the company string is empty
	std::string const notes = (rISOFile.GetPlatform() == GameListItem::GAMECUBE_DISC) ?
		rISOFile.GetCompany() : rISOFile.GetDescription(SelectedLanguage);
	SetItem(_Index, COLUMN_NOTES, StrToWxStr(notes), -1);

	// Emulation state
	SetItemColumnImage(_Index, COLUMN_EMULATION_STATE, m_EmuStateImageIndex[rISOFile.GetEmuState()]);

	// Country
	SetItemColumnImage(_Index, COLUMN_COUNTRY, m_FlagImageIndex[rISOFile.GetCountry()]);

	// File size
	SetItem(_Index, COLUMN_SIZE, NiceSizeFormat(rISOFile.GetFileSize()), -1);

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
		wxColour color = (i & 1) ?
			blend50(wxSystemSettings::GetColour(wxSYS_COLOUR_3DLIGHT),
					wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW)) :
			wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
		CGameListCtrl::SetItemBackgroundColour(i, color);
	}
}

void CGameListCtrl::ScanForISOs()
{
	ClearIsoFiles();

	CFileSearch::XStringVector Directories(SConfig::GetInstance().m_ISOFolder);

	if (SConfig::GetInstance().m_RecursiveISOFolder)
	{
		for (u32 i = 0; i < Directories.size(); i++)
		{
			File::FSTEntry FST_Temp;
			File::ScanDirectoryTree(Directories[i], FST_Temp);
			for (u32 j = 0; j < FST_Temp.children.size(); j++)
			{
				if (FST_Temp.children[j].isDirectory)
				{
					bool duplicate = false;
					for (u32 k = 0; k < Directories.size(); k++)
					{
						if (strcmp(Directories[k].c_str(),
									FST_Temp.children[j].physicalName.c_str()) == 0)
						{
							duplicate = true;
							break;
						}
					}
					if (!duplicate)
						Directories.push_back(
								FST_Temp.children[j].physicalName.c_str());
				}
			}
		}
	}

	CFileSearch::XStringVector Extensions;

	if (SConfig::GetInstance().m_ListGC)
		Extensions.push_back("*.gcm");
	if (SConfig::GetInstance().m_ListWii || SConfig::GetInstance().m_ListGC)
	{
		Extensions.push_back("*.iso");
		Extensions.push_back("*.ciso");
		Extensions.push_back("*.gcz");
		Extensions.push_back("*.wbfs");
	}
	if (SConfig::GetInstance().m_ListWad)
		Extensions.push_back("*.wad");

	CFileSearch FileSearch(Extensions, Directories);
	const CFileSearch::XStringVector& rFilenames = FileSearch.GetFileNames();

	if (rFilenames.size() > 0)
	{
		wxProgressDialog dialog(
			_("Scanning for ISOs"),
			_("Scanning..."),
			(int)rFilenames.size() - 1,
			this,
			wxPD_APP_MODAL |
			wxPD_AUTO_HIDE |
			wxPD_CAN_ABORT |
			wxPD_ELAPSED_TIME | wxPD_ESTIMATED_TIME | wxPD_REMAINING_TIME |
			wxPD_SMOOTH // - makes updates as small as possible (down to 1px)
			);

		for (u32 i = 0; i < rFilenames.size(); i++)
		{
			std::string FileName;
			SplitPath(rFilenames[i], NULL, &FileName, NULL);

			// Update with the progress (i) and the message
			dialog.Update(i, wxString::Format(_("Scanning %s"),
				StrToWxStr(FileName)));
			if (dialog.WasCancelled())
				break;

			std::auto_ptr<GameListItem> iso_file(new GameListItem(rFilenames[i]));
			const GameListItem& ISOFile = *iso_file;

			if (ISOFile.IsValid())
			{
				bool list = true;

				switch(ISOFile.GetPlatform())
				{
					case GameListItem::WII_DISC:
						if (!SConfig::GetInstance().m_ListWii)
							list = false;
						break;
					case GameListItem::WII_WAD:
						if (!SConfig::GetInstance().m_ListWad)
							list = false;
						break;
					default:
						if (!SConfig::GetInstance().m_ListGC)
							list = false;
						break;
				}

				switch(ISOFile.GetCountry())
				{
					case DiscIO::IVolume::COUNTRY_TAIWAN:
						if (!SConfig::GetInstance().m_ListTaiwan)
							list = false;
					case DiscIO::IVolume::COUNTRY_KOREA:
						if (!SConfig::GetInstance().m_ListKorea)
							list = false;
						break;
					case DiscIO::IVolume::COUNTRY_JAPAN:
						if (!SConfig::GetInstance().m_ListJap)
							list = false;
						break;
					case DiscIO::IVolume::COUNTRY_USA:
						if (!SConfig::GetInstance().m_ListUsa)
							list = false;
						break;
					case DiscIO::IVolume::COUNTRY_FRANCE:
						if (!SConfig::GetInstance().m_ListFrance)
							list = false;
						break;
					case DiscIO::IVolume::COUNTRY_ITALY:
						if (!SConfig::GetInstance().m_ListItaly)
							list = false;
						break;
					default:
						if (!SConfig::GetInstance().m_ListPal)
							list = false;
						break;
				}

				if (list)
					m_ISOFiles.push_back(iso_file.release());
			}
		}
	}

	if (SConfig::GetInstance().m_ListDrives)
	{
		const std::vector<std::string> drives = cdio_get_devices();

		for (std::vector<std::string>::const_iterator iter = drives.begin(); iter != drives.end(); ++iter)
		{
			#ifdef __APPLE__
			std::auto_ptr<GameListItem> gli(new GameListItem(*iter));
			#else
			std::unique_ptr<GameListItem> gli(new GameListItem(*iter));
			#endif

			if (gli->IsValid())
				m_ISOFiles.push_back(gli.release());
		}
	}

	std::sort(m_ISOFiles.begin(), m_ISOFiles.end());
}

void CGameListCtrl::OnColBeginDrag(wxListEvent& event)
{
	if (event.GetColumn() != COLUMN_TITLE && event.GetColumn() != COLUMN_NOTES)
		event.Veto();
}

const GameListItem *CGameListCtrl::GetISO(size_t index) const
{
	if (index < m_ISOFiles.size())
		return m_ISOFiles[index];
	else
		return NULL;
}

CGameListCtrl *caller;
#if wxCHECK_VERSION(2, 9, 0)
int wxCALLBACK wxListCompare(wxIntPtr item1, wxIntPtr item2, wxIntPtr sortData)
#else // 2.8.x
int wxCALLBACK wxListCompare(long item1, long item2, long sortData)
#endif
{
	// return 1 if item1 > item2
	// return -1 if item1 < item2
	// return 0 for identity
	const GameListItem *iso1 = caller->GetISO(item1);
	const GameListItem *iso2 = caller->GetISO(item2);

	return CompareGameListItems(iso1, iso2, sortData);
}

void CGameListCtrl::OnColumnClick(wxListEvent& event)
{
	if(event.GetColumn() != COLUMN_BANNER)
	{
		int current_column = event.GetColumn();
		if (sorted)
		{
			if (last_column == current_column)
			{
				last_sort = -last_sort;
			}
			else
			{
				SConfig::GetInstance().m_ListSort2 = last_sort;
				last_column = current_column;
				last_sort = current_column;
			}
			SConfig::GetInstance().m_ListSort = last_sort;
		}
		else
		{
			last_sort = current_column;
			last_column = current_column;
		}
		caller = this;
		SortItems(wxListCompare, last_sort);
	}

	SetBackgroundColor();

	event.Skip();
}

// This is used by keyboard gamelist search
void CGameListCtrl::OnKeyPress(wxListEvent& event)
{
	static int lastKey = 0, sLoop = 0;
	int Loop = 0;

	for (int i = 0; i < (int)m_ISOFiles.size(); i++)
	{
		// Easy way to get game string
		wxListItem bleh;
		bleh.SetId(i);
		bleh.SetColumn(COLUMN_TITLE);
		bleh.SetMask(wxLIST_MASK_TEXT);
		GetItem(bleh);

		wxString text = bleh.GetText();

#ifdef __WXGTK__
		if (text.MakeLower()[0] == event.GetKeyCode())
#else
		if (text.MakeUpper()[0] == event.GetKeyCode())
#endif
		{
			if (lastKey == event.GetKeyCode() && Loop < sLoop)
			{
				Loop++;
				if (i+1 == (int)m_ISOFiles.size())
					i = -1;
				continue;
			}
			else if (lastKey != event.GetKeyCode())
			{
				sLoop = 0;
			}

			lastKey = event.GetKeyCode();
			sLoop++;

			UnselectAll();
			SetItemState(i, wxLIST_STATE_SELECTED|wxLIST_STATE_FOCUSED,
					wxLIST_STATE_SELECTED|wxLIST_STATE_FOCUSED);
			EnsureVisible(i);
			break;
		}

		// If we get past the last game in the list,
		// we'll have to go back to the first one.
		if (i+1 == (int)m_ISOFiles.size() && sLoop > 0 && Loop > 0)
			i = -1;
	}

	event.Skip();
}

// This shows a little tooltip with the current Game's emulation state
void CGameListCtrl::OnMouseMotion(wxMouseEvent& event)
{
	int flags;
	long subitem = 0;
	const long item = HitTest(event.GetPosition(), flags, &subitem);
	static int lastItem = -1;

	if (GetColumnCount() <= 1)
		return;

	if (item != wxNOT_FOUND)
	{
		wxRect Rect;
#ifdef __WXMSW__
		if (subitem == COLUMN_EMULATION_STATE)
#else
		// The subitem parameter of HitTest is only implemented for wxMSW.  On
		// all other platforms it will always be -1.  Check the x position
		// instead.
		GetItemRect(item, Rect);
		if (Rect.GetX() + Rect.GetWidth() - GetColumnWidth(COLUMN_EMULATION_STATE) < event.GetX())
#endif
		{
			if (toolTip || lastItem == item || this != FindFocus())
			{
				if (lastItem != item) lastItem = -1;
				event.Skip();
				return;
			}

			// Emulation status
			static const char* const emuState[] = { "Broken", "Intro", "In-Game", "Playable", "Perfect" };

			const GameListItem& rISO = *m_ISOFiles[GetItemData(item)];

			const int emu_state = rISO.GetEmuState();
			const std::string& issues = rISO.GetIssues();

			// Show a tooltip containing the EmuState and the state description
			if (emu_state > 0 && emu_state < 6)
			{
				char temp[2048];
				sprintf(temp, "^ %s%s%s", emuState[emu_state - 1],
						issues.size() > 0 ? " :\n" : "", issues.c_str());
				toolTip = new wxEmuStateTip(this, StrToWxStr(temp), &toolTip);
			}
			else
			{
				toolTip = new wxEmuStateTip(this, _("Not Set"), &toolTip);
			}

			// Get item Coords
			GetItemRect(item, Rect);
			int mx = Rect.GetWidth();
			int my = Rect.GetY();
#ifndef __WXMSW__
			// For some reason the y position does not account for the header
			// row, so subtract the y position of the first visible item.
			GetItemRect(GetTopItem(), Rect);
			my -= Rect.GetY();
#endif
			// Convert to screen coordinates
			ClientToScreen(&mx, &my);
			toolTip->SetBoundingRect(wxRect(mx - GetColumnWidth(COLUMN_EMULATION_STATE),
						my, GetColumnWidth(COLUMN_EMULATION_STATE), Rect.GetHeight()));
			toolTip->SetPosition(wxPoint(mx - GetColumnWidth(COLUMN_EMULATION_STATE),
						my - 5 + Rect.GetHeight()));
			lastItem = item;
		}
	}
	if (!toolTip)
		lastItem = -1;

	event.Skip();
}

void CGameListCtrl::OnLeftClick(wxMouseEvent& event)
{
	// Focus the clicked item.
	int flags;
	long item = HitTest(event.GetPosition(), flags);
	if ((item != wxNOT_FOUND) && (GetSelectedItemCount() == 0) &&
			(!event.ControlDown()) && (!event.ShiftDown()))
	{
		SetItemState(item, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED );
		SetItemState(item, wxLIST_STATE_FOCUSED, wxLIST_STATE_FOCUSED);
		wxGetApp().GetCFrame()->UpdateGUI();
	}

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
			wxMenu* popupMenu = new wxMenu;
			popupMenu->Append(IDM_PROPERTIES, _("&Properties"));
			popupMenu->Append(IDM_GAMEWIKI, _("&Wiki"));
			popupMenu->AppendSeparator();

			if (selected_iso->GetPlatform() != GameListItem::GAMECUBE_DISC)
			{
				popupMenu->Append(IDM_OPENSAVEFOLDER, _("Open Wii &save folder"));
				popupMenu->Append(IDM_EXPORTSAVE, _("Export Wii save (Experimental)"));
			}
			popupMenu->Append(IDM_OPENCONTAININGFOLDER, _("Open &containing folder"));
			popupMenu->AppendCheckItem(IDM_SETDEFAULTGCM, _("Set as &default ISO"));

			// First we have to decide a starting value when we append it
			if(selected_iso->GetFileName() == SConfig::GetInstance().
				m_LocalCoreStartupParameter.m_strDefaultGCM)
				popupMenu->FindItem(IDM_SETDEFAULTGCM)->Check();

			popupMenu->AppendSeparator();
			popupMenu->Append(IDM_DELETEGCM, _("&Delete ISO..."));

			if (selected_iso->GetPlatform() != GameListItem::WII_WAD)
			{
				if (selected_iso->IsCompressed())
					popupMenu->Append(IDM_COMPRESSGCM, _("Decompress ISO..."));
				else if (selected_iso->GetFileName().substr(selected_iso->GetFileName().find_last_of(".")) != ".ciso" 
						 && selected_iso->GetFileName().substr(selected_iso->GetFileName().find_last_of(".")) != ".wbfs")
					popupMenu->Append(IDM_COMPRESSGCM, _("Compress ISO..."));
			}
			else
			{
				popupMenu->Append(IDM_LIST_INSTALLWAD, _("Install to Wii Menu"));
			}

			PopupMenu(popupMenu);
		}
	}
	else if (GetSelectedItemCount() > 1)
	{
		wxMenu* popupMenu = new wxMenu;
		popupMenu->Append(IDM_DELETEGCM, _("&Delete selected ISOs..."));
		popupMenu->AppendSeparator();
		popupMenu->Append(IDM_MULTICOMPRESSGCM, _("Compress selected ISOs..."));
		popupMenu->Append(IDM_MULTIDECOMPRESSGCM, _("Decompress selected ISOs..."));
		PopupMenu(popupMenu);
	}
}

const GameListItem * CGameListCtrl::GetSelectedISO()
{
	if (m_ISOFiles.size() == 0)
	{
		return NULL;
	}
	else if (GetSelectedItemCount() == 0)
	{
		return NULL;
	}
	else
	{
		long item = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		if (item == wxNOT_FOUND)
		{
			return NULL;
		}
		else
		{
			// Here is a little workaround for multiselections:
			// when > 1 item is selected, return info on the first one
			// and deselect it so the next time GetSelectedISO() is called,
			// the next item's info is returned
			if (GetSelectedItemCount() > 1)
				SetItemState(item, 0, wxLIST_STATE_SELECTED);

			return m_ISOFiles[GetItemData(item)];
		}
	}
}

void CGameListCtrl::OnOpenContainingFolder(wxCommandEvent& WXUNUSED (event))
{
	const GameListItem *iso = GetSelectedISO();
	if (!iso)
		return;

	wxFileName path = wxFileName::FileName(StrToWxStr(iso->GetFileName()));
	path.MakeAbsolute();
	WxUtils::Explore(path.GetPath().char_str());
}

void CGameListCtrl::OnOpenSaveFolder(wxCommandEvent& WXUNUSED (event))
{
	const GameListItem *iso = GetSelectedISO();
	if (!iso)
		return;
	std::string path = iso->GetWiiFSPath();
	if (!path.empty())
		WxUtils::Explore(path.c_str());
}

void CGameListCtrl::OnExportSave(wxCommandEvent& WXUNUSED (event))
{
	const GameListItem *iso =  GetSelectedISO();
	if (!iso)
		return;
	u64 title;
	DiscIO::IVolume *Iso = DiscIO::CreateVolumeFromFilename(iso->GetFileName());
	if (Iso)
	{
		if (Iso->GetTitleID((u8*)&title))
		{
			title = Common::swap64(title);
			CWiiSaveCrypted* exportSave = new CWiiSaveCrypted("", title);
			delete exportSave;
		}
		delete Iso;
	}
}

// Save this file as the default file
void CGameListCtrl::OnSetDefaultGCM(wxCommandEvent& event)
{
	const GameListItem *iso = GetSelectedISO();
	if (!iso) return;

	if (event.IsChecked())
	{
		// Write the new default value and save it the ini file
		SConfig::GetInstance().m_LocalCoreStartupParameter.m_strDefaultGCM =
			iso->GetFileName();
		SConfig::GetInstance().SaveSettings();
	}
	else
	{
		// Otherwise blank the value and save it
		SConfig::GetInstance().m_LocalCoreStartupParameter.m_strDefaultGCM = "";
		SConfig::GetInstance().SaveSettings();
	}
}

void CGameListCtrl::OnDeleteGCM(wxCommandEvent& WXUNUSED (event))
{
	if (GetSelectedItemCount() == 1)
	{
		const GameListItem *iso = GetSelectedISO();
		if (!iso)
			return;
		if (wxMessageBox(_("Are you sure you want to delete this file?  It will be gone forever!"),
					wxMessageBoxCaptionStr, wxYES_NO | wxICON_EXCLAMATION) == wxYES)
		{
			File::Delete(iso->GetFileName());
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
				File::Delete(iso->GetFileName());
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

void CGameListCtrl::OnWiki(wxCommandEvent& WXUNUSED (event))
{
	const GameListItem *iso = GetSelectedISO();
	if (!iso)
		return;

	std::string wikiUrl = "http://wiki.dolphin-emu.org/dolphin-redirect.php?gameid=[GAME_ID]";
	wikiUrl = ReplaceAll(wikiUrl, "[GAME_ID]", UriEncode(iso->GetUniqueID()));
	if (UriEncode(iso->GetName(0)).length() < 100)
		wikiUrl = ReplaceAll(wikiUrl, "[GAME_NAME]", UriEncode(iso->GetName(0)));
	else
		wikiUrl = ReplaceAll(wikiUrl, "[GAME_NAME]", "");

	WxUtils::Launch(wikiUrl.c_str());
}

void CGameListCtrl::MultiCompressCB(const char* text, float percent, void* arg)
{
	percent = (((float)m_currentItem) + percent) / (float)m_numberItem;
	wxString textString(StrToWxStr(StringFromFormat("%s (%i/%i) - %s",
				m_currentFilename.c_str(), (int)m_currentItem+1,
				(int)m_numberItem, text)));

	((wxProgressDialog*)arg)->Update((int)(percent*1000), textString);
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

	wxDirDialog browseDialog(this, _("Browse for output directory"), dirHome,
			wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
	if (browseDialog.ShowModal() != wxID_OK)
		return;

	bool all_good = true;

	{
	wxProgressDialog progressDialog(
		_compress ? _("Compressing ISO") : _("Decompressing ISO"),
		_("Working..."),
		1000,
		this,
		wxPD_APP_MODAL |
		wxPD_ELAPSED_TIME | wxPD_ESTIMATED_TIME | wxPD_REMAINING_TIME |
		wxPD_SMOOTH
		);

	m_currentItem = 0;
	m_numberItem = GetSelectedItemCount();
	for (u32 i=0; i < m_numberItem; i++)
	{
		const GameListItem *iso = GetSelectedISO();

			if (!iso->IsCompressed() && _compress)
			{
				std::string FileName, FileExt;
				SplitPath(iso->GetFileName(), NULL, &FileName, &FileExt);
				m_currentFilename = FileName;
				FileName.append(".gcz");

				std::string OutputFileName;
				BuildCompleteFilename(OutputFileName,
						WxStrToStr(browseDialog.GetPath()),
						FileName);

				if (wxFileExists(StrToWxStr(OutputFileName)) &&
						wxMessageBox(
							wxString::Format(_("The file %s already exists.\nDo you wish to replace it?"),
								StrToWxStr(OutputFileName)), 
							_("Confirm File Overwrite"),
							wxYES_NO) == wxNO)
					continue;

				all_good &= DiscIO::CompressFileToBlob(iso->GetFileName().c_str(),
						OutputFileName.c_str(),
						(iso->GetPlatform() == GameListItem::WII_DISC) ? 1 : 0,
						16384, &MultiCompressCB, &progressDialog);
			}
			else if (iso->IsCompressed() && !_compress)
			{
				std::string FileName, FileExt;
				SplitPath(iso->GetFileName(), NULL, &FileName, &FileExt);
				m_currentFilename = FileName;
				if (iso->GetPlatform() == GameListItem::WII_DISC)
					FileName.append(".iso");
				else
					FileName.append(".gcm");

				std::string OutputFileName;
				BuildCompleteFilename(OutputFileName,
						WxStrToStr(browseDialog.GetPath()),
						FileName);

				if (wxFileExists(StrToWxStr(OutputFileName)) &&
						wxMessageBox(
							wxString::Format(_("The file %s already exists.\nDo you wish to replace it?"),
								StrToWxStr(OutputFileName)), 
							_("Confirm File Overwrite"),
							wxYES_NO) == wxNO)
					continue;

				all_good &= DiscIO::DecompressBlobToFile(iso->GetFileName().c_str(),
						OutputFileName.c_str(), &MultiCompressCB, &progressDialog);
			}
			m_currentItem++;
	}
	}

	if (!all_good)
		wxMessageBox(_("Dolphin was unable to complete the requested action."));

	Update();
}

void CGameListCtrl::CompressCB(const char* text, float percent, void* arg)
{
	((wxProgressDialog*)arg)->
		Update((int)(percent*1000), StrToWxStr(text));
}

void CGameListCtrl::OnCompressGCM(wxCommandEvent& WXUNUSED (event))
{
	const GameListItem *iso = GetSelectedISO();
	if (!iso)
		return;

	wxString path;

	std::string FileName, FilePath, FileExtension;
	SplitPath(iso->GetFileName(), &FilePath, &FileName, &FileExtension);

	do
	{
		if (iso->IsCompressed())
		{
			wxString FileType;
			if (iso->GetPlatform() == GameListItem::WII_DISC)
				FileType = _("All Wii ISO files (iso)") + wxString(wxT("|*.iso"));
			else
				FileType = _("All Gamecube GCM files (gcm)") + wxString(wxT("|*.gcm"));

			path = wxFileSelector(
					_("Save decompressed GCM/ISO"),
					StrToWxStr(FilePath),
					StrToWxStr(FileName) + FileType.After('*'),
					wxEmptyString,
					FileType + wxT("|") + wxGetTranslation(wxALL_FILES),
					wxFD_SAVE,
					this);
		}
		else
		{
			path = wxFileSelector(
					_("Save compressed GCM/ISO"),
					StrToWxStr(FilePath),
					StrToWxStr(FileName) + _T(".gcz"),
					wxEmptyString,
					_("All compressed GC/Wii ISO files (gcz)") + 
						wxString::Format(wxT("|*.gcz|%s"), wxGetTranslation(wxALL_FILES)),
					wxFD_SAVE,
					this);
		}
		if (!path)
			return;
	} while (wxFileExists(path) &&
			wxMessageBox(
				wxString::Format(_("The file %s already exists.\nDo you wish to replace it?"), path.c_str()), 
				_("Confirm File Overwrite"),
				wxYES_NO) == wxNO);

	bool all_good = false;

	{
	wxProgressDialog dialog(
		iso->IsCompressed() ? _("Decompressing ISO") : _("Compressing ISO"),
		_("Working..."),
		1000,
		this,
		wxPD_APP_MODAL |
		wxPD_ELAPSED_TIME | wxPD_ESTIMATED_TIME | wxPD_REMAINING_TIME |
		wxPD_SMOOTH
		);


	if (iso->IsCompressed())
		all_good = DiscIO::DecompressBlobToFile(iso->GetFileName().c_str(),
				path.char_str(), &CompressCB, &dialog);
	else
		all_good = DiscIO::CompressFileToBlob(iso->GetFileName().c_str(),
				path.char_str(),
				(iso->GetPlatform() == GameListItem::WII_DISC) ? 1 : 0,
				16384, &CompressCB, &dialog);
	}

	if (!all_good)
		wxMessageBox(_("Dolphin was unable to complete the requested action."));

	Update();
}

void CGameListCtrl::OnSize(wxSizeEvent& event)
{
	if (lastpos == event.GetSize())
		return;

	lastpos = event.GetSize();
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
			+ GetColumnWidth(COLUMN_PLATFORM));

		// We hide the Notes column if the window is too small
		if (resizable > 400)
		{
			SetColumnWidth(COLUMN_TITLE, resizable / 2);
			SetColumnWidth(COLUMN_NOTES, resizable / 2);
		}
		else
		{
			SetColumnWidth(COLUMN_TITLE, resizable);
			SetColumnWidth(COLUMN_NOTES, 0);
		}
	}
}

void CGameListCtrl::UnselectAll()
{
	for (int i=0; i<GetItemCount(); i++)
	{
		SetItemState(i, 0, wxLIST_STATE_SELECTED);
	}
}

void CGameListCtrl::OnDropFiles(wxDropFilesEvent& event)
{
	if (event.GetNumberOfFiles() != 1)
		return;
	if (File::IsDirectory(WxStrToStr(event.GetFiles()[0])))
		return;

	wxFileName file = event.GetFiles()[0];

	if (file.GetExt() == "dtm")
	{
		if (Core::IsRunning())
			return;

		if (!Movie::IsReadOnly())
		{
			// let's make the read-only flag consistent at the start of a movie.
			Movie::SetReadOnly(true);
			main_frame->GetMenuBar()->FindItem(IDM_RECORDREADONLY)->Check(true);
		}

		if (Movie::PlayInput(file.GetFullPath().c_str()))
			main_frame->BootGame(std::string(""));
	}
	else if (!Core::IsRunning())
	{
		main_frame->BootGame(WxStrToStr(file.GetFullPath()));
	}
	else
	{
		DVDInterface::ChangeDisc(WxStrToStr(file.GetFullPath()).c_str());
	}
}
