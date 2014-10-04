// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <algorithm>
#include <cinttypes>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <wx/app.h>
#include <wx/bitmap.h>
#include <wx/buffer.h>
#include <wx/chartype.h>
#include <wx/colour.h>
#include <wx/defs.h>
#include <wx/dirdlg.h>
#include <wx/event.h>
#include <wx/filedlg.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/gdicmn.h>
#include <wx/imaglist.h>
#include <wx/listbase.h>
#include <wx/listctrl.h>
#include <wx/menu.h>
#include <wx/menuitem.h>
#include <wx/msgdlg.h>
#include <wx/progdlg.h>
#include <wx/settings.h>
#include <wx/string.h>
#include <wx/tipwin.h>
#include <wx/translation.h>
#include <wx/unichar.h>
#include <wx/utils.h>
#include <wx/version.h>
#include <wx/window.h>
#include <wx/windowid.h>

#include "Common/CDUtils.h"
#include "Common/CommonTypes.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/MathUtil.h"
#include "Common/StdMakeUnique.h"
#include "Common/StringUtil.h"
#include "Common/SysConf.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreParameter.h"
#include "Core/Movie.h"
#include "Core/Boot/Boot.h"
#include "Core/HW/DVDInterface.h"
#include "Core/HW/WiiSaveCrypted.h"
#include "DiscIO/Blob.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeCreator.h"
#include "DolphinWX/Frame.h"
#include "DolphinWX/GameListCtrl.h"
#include "DolphinWX/Globals.h"
#include "DolphinWX/ISOFile.h"
#include "DolphinWX/ISOProperties.h"
#include "DolphinWX/Main.h"
#include "DolphinWX/WxUtils.h"
#include "DolphinWX/resources/Flag_Europe.xpm"
#include "DolphinWX/resources/Flag_France.xpm"
#include "DolphinWX/resources/Flag_Germany.xpm"
#include "DolphinWX/resources/Flag_Italy.xpm"
#include "DolphinWX/resources/Flag_Japan.xpm"
#include "DolphinWX/resources/Flag_Korea.xpm"
#include "DolphinWX/resources/Flag_SDK.xpm"
#include "DolphinWX/resources/Flag_Taiwan.xpm"
#include "DolphinWX/resources/Flag_Unknown.xpm"
#include "DolphinWX/resources/Flag_USA.xpm"
#include "DolphinWX/resources/Platform_Gamecube.xpm"
#include "DolphinWX/resources/Platform_Wad.xpm"
#include "DolphinWX/resources/Platform_Wii.xpm"
#include "DolphinWX/resources/rating_gamelist.h"

size_t CGameListCtrl::m_currentItem = 0;
size_t CGameListCtrl::m_numberItem = 0;
std::string CGameListCtrl::m_currentFilename;
static bool sorted = false;

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


	// index only matters for WADS and PAL GC games, but invalid indicies for the others
	// will return the (only) language in the list
	if (iso1->GetPlatform() == GameListItem::WII_WAD)
	{
		indexOne = SConfig::GetInstance().m_SYSCONF->GetData<u8>("IPL.LNG");
	}
	else // GC
	{
		indexOne = SConfig::GetInstance().m_LocalCoreStartupParameter.SelectedLanguage;
	}

	if (iso2->GetPlatform() == GameListItem::WII_WAD)
	{
		indexOther = SConfig::GetInstance().m_SYSCONF->GetData<u8>("IPL.LNG");
	}
	else // GC
	{
		indexOther = SConfig::GetInstance().m_LocalCoreStartupParameter.SelectedLanguage;
	}

	switch (sortData)
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
		case CGameListCtrl::COLUMN_ID:
			 return strcasecmp(iso1->GetUniqueID().c_str(), iso2->GetUniqueID().c_str()) * t;
		case CGameListCtrl::COLUMN_COUNTRY:
			if (iso1->GetCountry() > iso2->GetCountry())
				return  1 * t;
			if (iso1->GetCountry() < iso2->GetCountry())
				return -1 * t;
			return 0;
		case CGameListCtrl::COLUMN_SIZE:
			if (iso1->GetFileSize() > iso2->GetFileSize())
				return  1 * t;
			if (iso1->GetFileSize() < iso2->GetFileSize())
				return -1 * t;
			return 0;
		case CGameListCtrl::COLUMN_PLATFORM:
			if (iso1->GetPlatform() > iso2->GetPlatform())
				return  1 * t;
			if (iso1->GetPlatform() < iso2->GetPlatform())
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
	EVT_MENU(IDM_SETDEFAULTISO, CGameListCtrl::OnSetDefaultISO)
	EVT_MENU(IDM_COMPRESSISO, CGameListCtrl::OnCompressISO)
	EVT_MENU(IDM_MULTICOMPRESSISO, CGameListCtrl::OnMultiCompressISO)
	EVT_MENU(IDM_MULTIDECOMPRESSISO, CGameListCtrl::OnMultiDecompressISO)
	EVT_MENU(IDM_DELETEISO, CGameListCtrl::OnDeleteISO)
	EVT_MENU(IDM_LIST_CHANGEDISC, CGameListCtrl::OnChangeDisc)
END_EVENT_TABLE()

CGameListCtrl::CGameListCtrl(wxWindow* parent, const wxWindowID id, const
		wxPoint& pos, const wxSize& size, long style)
	: wxListCtrl(parent, id, pos, size, style), toolTip(nullptr)
{
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
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_EUROPE]  = m_imageListSmall->Add(wxBitmap(Flag_Europe_xpm));
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_GERMANY] = m_imageListSmall->Add(wxBitmap(Flag_Germany_xpm));
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_FRANCE]  = m_imageListSmall->Add(wxBitmap(Flag_France_xpm));
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_USA]     = m_imageListSmall->Add(wxBitmap(Flag_USA_xpm));
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_JAPAN]   = m_imageListSmall->Add(wxBitmap(Flag_Japan_xpm));
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_KOREA]   = m_imageListSmall->Add(wxBitmap(Flag_Korea_xpm));
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_ITALY]   = m_imageListSmall->Add(wxBitmap(Flag_Italy_xpm));
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_TAIWAN]  = m_imageListSmall->Add(wxBitmap(Flag_Taiwan_xpm));
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_SDK]     = m_imageListSmall->Add(wxBitmap(Flag_SDK_xpm));
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_UNKNOWN] = m_imageListSmall->Add(wxBitmap(Flag_Unknown_xpm));

	m_PlatformImageIndex.resize(3);
	m_PlatformImageIndex[0] = m_imageListSmall->Add(wxBitmap(Platform_Gamecube_xpm));
	m_PlatformImageIndex[1] = m_imageListSmall->Add(wxBitmap(Platform_Wii_xpm));
	m_PlatformImageIndex[2] = m_imageListSmall->Add(wxBitmap(Platform_Wad_xpm));

	m_EmuStateImageIndex.resize(6);
	m_EmuStateImageIndex[0] = m_imageListSmall->Add(wxBitmap(rating_0));
	m_EmuStateImageIndex[1] = m_imageListSmall->Add(wxBitmap(rating_1));
	m_EmuStateImageIndex[2] = m_imageListSmall->Add(wxBitmap(rating_2));
	m_EmuStateImageIndex[3] = m_imageListSmall->Add(wxBitmap(rating_3));
	m_EmuStateImageIndex[4] = m_imageListSmall->Add(wxBitmap(rating_4));
	m_EmuStateImageIndex[5] = m_imageListSmall->Add(wxBitmap(rating_5));
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
		m_imageListSmall = nullptr;
	}

	Hide();

	ScanForISOs();

	ClearAll();

	if (m_ISOFiles.size() != 0)
	{
		// Don't load bitmaps unless there are games to list
		InitBitmaps();

		// add columns
		InsertColumn(COLUMN_DUMMY, "");
		InsertColumn(COLUMN_PLATFORM, "");
		InsertColumn(COLUMN_BANNER, _("Banner"));
		InsertColumn(COLUMN_TITLE, _("Title"));

		// Instead of showing the notes + the company, which is unknown with
		// wii titles We show in the same column : company for GC games and
		// description for wii/wad games
		InsertColumn(COLUMN_NOTES, _("Notes"));
		InsertColumn(COLUMN_ID, _("ID"));
		InsertColumn(COLUMN_COUNTRY, "");
		InsertColumn(COLUMN_SIZE, _("Size"));
		InsertColumn(COLUMN_EMULATION_STATE, _("State"));

#ifdef __WXMSW__
		const int platform_padding = 0;
#else
		const int platform_padding = 8;
#endif

		// set initial sizes for columns
		SetColumnWidth(COLUMN_DUMMY,0);
		SetColumnWidth(COLUMN_PLATFORM, SConfig::GetInstance().m_showSystemColumn ? 35 + platform_padding : 0);
		SetColumnWidth(COLUMN_BANNER, SConfig::GetInstance().m_showBannerColumn ? 96 + platform_padding : 0);
		SetColumnWidth(COLUMN_TITLE, 175 + platform_padding);
		SetColumnWidth(COLUMN_NOTES, SConfig::GetInstance().m_showNotesColumn ? 150 + platform_padding : 0);
		SetColumnWidth(COLUMN_ID, SConfig::GetInstance().m_showIDColumn ? 75 + platform_padding : 0);
		SetColumnWidth(COLUMN_COUNTRY, SConfig::GetInstance().m_showRegionColumn ? 32 + platform_padding : 0);
		SetColumnWidth(COLUMN_EMULATION_STATE, SConfig::GetInstance().m_showStateColumn ? 50 + platform_padding : 0);

		// add all items
		for (int i = 0; i < (int)m_ISOFiles.size(); i++)
		{
			InsertItemInReportView(i);
			if (SConfig::GetInstance().m_ColorCompressed && m_ISOFiles[i]->IsCompressed())
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

		SetColumnWidth(COLUMN_SIZE, SConfig::GetInstance().m_showSizeColumn ? wxLIST_AUTOSIZE : 0);
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
			errorString = _("Dolphin could not find any GameCube/Wii ISOs or WADs.  Double-click here to browse for files...");
		}
		else
		{
			errorString = _("Dolphin is currently set to hide all games.  Double-click here to show all games...");
		}
		InsertColumn(0, "");
		long index = InsertItem(0, errorString);
		SetItemFont(index, *wxITALIC_FONT);
		SetColumnWidth(0, wxLIST_AUTOSIZE);
	}
	if (GetSelectedISO() == nullptr)
		main_frame->UpdateGUI();
	Show();

	AutomaticColumnWidth();
	ScrollLines(scrollPos);
	SetFocus();
}

static wxString NiceSizeFormat(u64 _size)
{
	// Return a pretty filesize string from byte count.
	// e.g. 1134278 -> "1.08 MiB"

	const char* const unit_symbols[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB"};

	// Find largest power of 2 less than _size.
	// div 10 to get largest named unit less than _size
	// 10 == log2(1024) (number of B in a KiB, KiB in a MiB, etc)
	const u64 unit = IntLog2(std::max<u64>(_size, 1)) / 10;
	const u64 unit_size = (1 << (unit * 10));

	// mul 1000 for 3 decimal places, add 5 to round up, div 10 for 2 decimal places
	std::string value = std::to_string((_size * 1000 / unit_size + 5) / 10);
	// Insert decimal point.
	value.insert(value.size() - 2, ".");
	return StrToWxStr(StringFromFormat("%s %s", value.c_str(), unit_symbols[unit]));
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

	if (rISOFile.GetBitmap().IsOk())
		ImageIndex = m_imageListSmall->Add(rISOFile.GetBitmap());

	// Set the game's banner in the second column
	SetItemColumnImage(_Index, COLUMN_BANNER, ImageIndex);

	int SelectedLanguage = SConfig::GetInstance().m_LocalCoreStartupParameter.SelectedLanguage;

	// Is this sane?
	if  (rISOFile.GetPlatform() == GameListItem::WII_WAD)
	{
		SelectedLanguage = SConfig::GetInstance().m_SYSCONF->GetData<u8>("IPL.LNG");
	}

	std::string const name = rISOFile.GetName(SelectedLanguage);
	SetItem(_Index, COLUMN_TITLE, StrToWxStr(name), -1);

	// We show the company string on GameCube only
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

	// Game ID
	SetItem(_Index, COLUMN_ID, rISOFile.GetUniqueID(), -1);

	// Background color
	SetBackgroundColor();

	// Item data
	SetItemData(_Index, ItemIndex);
}

static wxColour blend50(const wxColour& c1, const wxColour& c2)
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
	for (long i = 0; i < GetItemCount(); i++)
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
			for (auto& Entry : FST_Temp.children)
			{
				if (Entry.isDirectory)
				{
					bool duplicate = false;
					for (auto& Directory : Directories)
					{
						if (Directory == Entry.physicalName)
						{
							duplicate = true;
							break;
						}
					}
					if (!duplicate)
						Directories.push_back(Entry.physicalName);
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
			SplitPath(rFilenames[i], nullptr, &FileName, nullptr);

			// Update with the progress (i) and the message
			dialog.Update(i, wxString::Format(_("Scanning %s"),
				StrToWxStr(FileName)));
			if (dialog.WasCancelled())
				break;

			auto iso_file = std::make_unique<GameListItem>(rFilenames[i]);

			if (iso_file->IsValid())
			{
				bool list = true;

				switch(iso_file->GetPlatform())
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

				switch(iso_file->GetCountry())
				{
					case DiscIO::IVolume::COUNTRY_TAIWAN:
						if (!SConfig::GetInstance().m_ListTaiwan)
							list = false;
						break;
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

		for (const auto& drive : drives)
		{
			auto gli = std::make_unique<GameListItem>(drive);

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
		return nullptr;
}

static CGameListCtrl *caller;
static int wxCALLBACK wxListCompare(wxIntPtr item1, wxIntPtr item2, wxIntPtr sortData)
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
	if (event.GetColumn() != COLUMN_BANNER)
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
			popupMenu->AppendCheckItem(IDM_SETDEFAULTISO, _("Set as &default ISO"));

			// First we have to decide a starting value when we append it
			if (selected_iso->GetFileName() == SConfig::GetInstance().
				m_LocalCoreStartupParameter.m_strDefaultGCM)
				popupMenu->FindItem(IDM_SETDEFAULTISO)->Check();

			popupMenu->AppendSeparator();
			popupMenu->Append(IDM_DELETEISO, _("&Delete ISO..."));

			if (selected_iso->GetPlatform() != GameListItem::WII_WAD)
			{
				if (selected_iso->IsCompressed())
					popupMenu->Append(IDM_COMPRESSISO, _("Decompress ISO..."));
				else if (selected_iso->GetFileName().substr(selected_iso->GetFileName().find_last_of(".")) != ".ciso" &&
				         selected_iso->GetFileName().substr(selected_iso->GetFileName().find_last_of(".")) != ".wbfs")
					popupMenu->Append(IDM_COMPRESSISO, _("Compress ISO..."));
			}
			else
			{
				popupMenu->Append(IDM_LIST_INSTALLWAD, _("Install to Wii Menu"));
			}
			if (selected_iso->GetPlatform() == GameListItem::GAMECUBE_DISC ||
				selected_iso->GetPlatform() == GameListItem::WII_DISC)
			{
				wxMenuItem* changeDiscItem = popupMenu->Append(IDM_LIST_CHANGEDISC, _("Change &Disc"));
				changeDiscItem->Enable(Core::IsRunning());
			}

			PopupMenu(popupMenu);
		}
	}
	else if (GetSelectedItemCount() > 1)
	{
		wxMenu* popupMenu = new wxMenu;
		popupMenu->Append(IDM_DELETEISO, _("&Delete selected ISOs..."));
		popupMenu->AppendSeparator();
		popupMenu->Append(IDM_MULTICOMPRESSISO, _("Compress selected ISOs..."));
		popupMenu->Append(IDM_MULTIDECOMPRESSISO, _("Decompress selected ISOs..."));
		PopupMenu(popupMenu);
	}
}

const GameListItem * CGameListCtrl::GetSelectedISO()
{
	if (m_ISOFiles.size() == 0)
	{
		return nullptr;
	}
	else if (GetSelectedItemCount() == 0)
	{
		return nullptr;
	}
	else
	{
		long item = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		if (item == wxNOT_FOUND)
		{
			return nullptr;
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
	WxUtils::Explore(WxStrToStr(path.GetPath()));
}

void CGameListCtrl::OnOpenSaveFolder(wxCommandEvent& WXUNUSED (event))
{
	const GameListItem *iso = GetSelectedISO();
	if (!iso)
		return;
	std::string path = iso->GetWiiFSPath();
	if (!path.empty())
		WxUtils::Explore(path);
}

void CGameListCtrl::OnExportSave(wxCommandEvent& WXUNUSED (event))
{
	const GameListItem *iso =  GetSelectedISO();
	if (!iso)
		return;

	u64 title;
	std::unique_ptr<DiscIO::IVolume> volume(DiscIO::CreateVolumeFromFilename(iso->GetFileName()));
	if (volume && volume->GetTitleID((u8*)&title))
	{
		title = Common::swap64(title);
		CWiiSaveCrypted::ExportWiiSave(title);
	}
}

// Save this file as the default file
void CGameListCtrl::OnSetDefaultISO(wxCommandEvent& event)
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

void CGameListCtrl::OnDeleteISO(wxCommandEvent& WXUNUSED (event))
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
	if (ISOProperties.ShowModal() == wxID_OK)
		Update();
}

void CGameListCtrl::OnWiki(wxCommandEvent& WXUNUSED (event))
{
	const GameListItem *iso = GetSelectedISO();
	if (!iso)
		return;

	std::string wikiUrl = "http://wiki.dolphin-emu.org/dolphin-redirect.php?gameid=" + iso->GetUniqueID();
	WxUtils::Launch(wikiUrl);
}

void CGameListCtrl::MultiCompressCB(const std::string& text, float percent, void* arg)
{
	percent = (((float)m_currentItem) + percent) / (float)m_numberItem;
	wxString textString(StrToWxStr(StringFromFormat("%s (%i/%i) - %s",
				m_currentFilename.c_str(), (int)m_currentItem+1,
				(int)m_numberItem, text.c_str())));

	((wxProgressDialog*)arg)->Update((int)(percent*1000), textString);
}

void CGameListCtrl::OnMultiCompressISO(wxCommandEvent& /*event*/)
{
	CompressSelection(true);
}

void CGameListCtrl::OnMultiDecompressISO(wxCommandEvent& /*event*/)
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
		if (iso->GetPlatform() == GameListItem::WII_WAD || iso->GetFileName().rfind(".wbfs") != std::string::npos)
			continue;

			if (!iso->IsCompressed() && _compress)
			{
				std::string FileName, FileExt;
				SplitPath(iso->GetFileName(), nullptr, &FileName, &FileExt);
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

				all_good &= DiscIO::CompressFileToBlob(iso->GetFileName(),
						OutputFileName,
						(iso->GetPlatform() == GameListItem::WII_DISC) ? 1 : 0,
						16384, &MultiCompressCB, &progressDialog);
			}
			else if (iso->IsCompressed() && !_compress)
			{
				std::string FileName, FileExt;
				SplitPath(iso->GetFileName(), nullptr, &FileName, &FileExt);
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
		WxUtils::ShowErrorDialog(_("Dolphin was unable to complete the requested action."));

	Update();
}

void CGameListCtrl::CompressCB(const std::string& text, float percent, void* arg)
{
	((wxProgressDialog*)arg)->
		Update((int)(percent*1000), StrToWxStr(text));
}

void CGameListCtrl::OnCompressISO(wxCommandEvent& WXUNUSED (event))
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
				FileType = _("All Wii ISO files (iso)") + "|*.iso";
			else
				FileType = _("All GameCube GCM files (gcm)") + "|*.gcm";

			path = wxFileSelector(
					_("Save decompressed GCM/ISO"),
					StrToWxStr(FilePath),
					StrToWxStr(FileName) + FileType.After('*'),
					wxEmptyString,
					FileType + "|" + wxGetTranslation(wxALL_FILES),
					wxFD_SAVE,
					this);
		}
		else
		{
			path = wxFileSelector(
					_("Save compressed GCM/ISO"),
					StrToWxStr(FilePath),
					StrToWxStr(FileName) + ".gcz",
					wxEmptyString,
					_("All compressed GC/Wii ISO files (gcz)") +
						wxString::Format("|*.gcz|%s", wxGetTranslation(wxALL_FILES)),
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
		all_good = DiscIO::DecompressBlobToFile(iso->GetFileName(),
				WxStrToStr(path), &CompressCB, &dialog);
	else
		all_good = DiscIO::CompressFileToBlob(iso->GetFileName(),
				WxStrToStr(path),
				(iso->GetPlatform() == GameListItem::WII_DISC) ? 1 : 0,
				16384, &CompressCB, &dialog);
	}

	if (!all_good)
		WxUtils::ShowErrorDialog(_("Dolphin was unable to complete the requested action."));

	Update();
}

void CGameListCtrl::OnChangeDisc(wxCommandEvent& WXUNUSED(event))
{
	const GameListItem *iso = GetSelectedISO();
	if (!iso || !Core::IsRunning())
		return;
	DVDInterface::ChangeDisc(WxStrToStr(iso->GetFileName()));
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
	else if (GetColumnCount() > 0)
	{

		int resizable = rc.GetWidth() - (
			GetColumnWidth(COLUMN_PLATFORM)
			+ GetColumnWidth(COLUMN_BANNER)
			+ GetColumnWidth(COLUMN_ID)
			+ GetColumnWidth(COLUMN_COUNTRY)
			+ GetColumnWidth(COLUMN_SIZE)
			+ GetColumnWidth(COLUMN_EMULATION_STATE));

		// We hide the Notes column if the window is too small
		if (resizable > 400)
		{
			if (SConfig::GetInstance().m_showNotesColumn)
			{
				SetColumnWidth(COLUMN_TITLE, resizable / 2);
				SetColumnWidth(COLUMN_NOTES, resizable / 2);
			}
			else
			{
				SetColumnWidth(COLUMN_TITLE, resizable);
			}
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
