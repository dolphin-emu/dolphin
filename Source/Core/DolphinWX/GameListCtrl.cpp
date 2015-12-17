// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cinttypes>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <wx/bitmap.h>
#include <wx/buffer.h>
#include <wx/colour.h>
#include <wx/dirdlg.h>
#include <wx/filedlg.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/gdicmn.h>
#include <wx/imaglist.h>
#include <wx/listctrl.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/progdlg.h>
#include <wx/settings.h>
#include <wx/tipwin.h>
#include <wx/wxcrt.h>

#include "Common/CDUtils.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/MathUtil.h"
#include "Common/StringUtil.h"
#include "Common/SysConf.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
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

struct CompressionProgress final
{
public:
	CompressionProgress(int items_done_, int items_total_, const std::string& current_filename_, wxProgressDialog* dialog_)
		: items_done(items_done_), items_total(items_total_), current_filename(current_filename_), dialog(dialog_)
	{ }

	int items_done;
	int items_total;
	std::string current_filename;
	wxProgressDialog* dialog;
};

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

	switch (sortData)
	{
		case CGameListCtrl::COLUMN_TITLE:
			if (!strcasecmp(iso1->GetName().c_str(), iso2->GetName().c_str()))
			{
				if (iso1->GetUniqueID() != iso2->GetUniqueID())
					return t * (iso1->GetUniqueID() > iso2->GetUniqueID() ? 1 : -1);
				if (iso1->GetRevision() != iso2->GetRevision())
					return t * (iso1->GetRevision() > iso2->GetRevision() ? 1 : -1);
				if (iso1->GetDiscNumber() != iso2->GetDiscNumber())
					return t * (iso1->GetDiscNumber() > iso2->GetDiscNumber() ? 1 : -1);

				wxString iso1_filename = wxFileNameFromPath(iso1->GetFileName());
				wxString iso2_filename = wxFileNameFromPath(iso2->GetFileName());

				if (iso1_filename != iso2_filename)
					return t * wxStricmp(iso1_filename, iso2_filename);
			}
			return strcasecmp(iso1->GetName().c_str(), iso2->GetName().c_str()) * t;
		case CGameListCtrl::COLUMN_MAKER:
			return strcasecmp(iso1->GetCompany().c_str(), iso2->GetCompany().c_str()) * t;
		case CGameListCtrl::COLUMN_FILENAME:
			return wxStricmp(wxFileNameFromPath(iso1->GetFileName()),
			                 wxFileNameFromPath(iso2->GetFileName())) * t;
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

CGameListCtrl::CGameListCtrl(wxWindow* parent, const wxWindowID id, const
		wxPoint& pos, const wxSize& size, long style)
	: wxListCtrl(parent, id, pos, size, style), toolTip(nullptr)
{
	Bind(wxEVT_SIZE, &CGameListCtrl::OnSize, this);
	Bind(wxEVT_RIGHT_DOWN, &CGameListCtrl::OnRightClick, this);
	Bind(wxEVT_LEFT_DOWN, &CGameListCtrl::OnLeftClick, this);
	Bind(wxEVT_MOTION, &CGameListCtrl::OnMouseMotion, this);
	Bind(wxEVT_LIST_KEY_DOWN, &CGameListCtrl::OnKeyPress, this);
	Bind(wxEVT_LIST_COL_BEGIN_DRAG, &CGameListCtrl::OnColBeginDrag, this);
	Bind(wxEVT_LIST_COL_CLICK, &CGameListCtrl::OnColumnClick, this);

	Bind(wxEVT_MENU, &CGameListCtrl::OnProperties, this, IDM_PROPERTIES);
	Bind(wxEVT_MENU, &CGameListCtrl::OnWiki, this, IDM_GAME_WIKI);
	Bind(wxEVT_MENU, &CGameListCtrl::OnOpenContainingFolder, this, IDM_OPEN_CONTAINING_FOLDER);
	Bind(wxEVT_MENU, &CGameListCtrl::OnOpenSaveFolder, this, IDM_OPEN_SAVE_FOLDER);
	Bind(wxEVT_MENU, &CGameListCtrl::OnExportSave, this, IDM_EXPORT_SAVE);
	Bind(wxEVT_MENU, &CGameListCtrl::OnSetDefaultISO, this, IDM_SET_DEFAULT_ISO);
	Bind(wxEVT_MENU, &CGameListCtrl::OnCompressISO, this, IDM_COMPRESS_ISO);
	Bind(wxEVT_MENU, &CGameListCtrl::OnMultiCompressISO, this, IDM_MULTI_COMPRESS_ISO);
	Bind(wxEVT_MENU, &CGameListCtrl::OnMultiDecompressISO, this, IDM_MULTI_DECOMPRESS_ISO);
	Bind(wxEVT_MENU, &CGameListCtrl::OnDeleteISO, this, IDM_DELETE_ISO);
	Bind(wxEVT_MENU, &CGameListCtrl::OnChangeDisc, this, IDM_LIST_CHANGE_DISC);
}

CGameListCtrl::~CGameListCtrl()
{
	if (m_imageListSmall)
		delete m_imageListSmall;

	ClearIsoFiles();
}

void CGameListCtrl::InitBitmaps()
{
	wxSize size(96, 32);
	m_imageListSmall = new wxImageList(96, 32);
	SetImageList(m_imageListSmall, wxIMAGE_LIST_SMALL);

	m_FlagImageIndex.resize(DiscIO::IVolume::NUMBER_OF_COUNTRIES);
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_JAPAN]       = m_imageListSmall->Add(WxUtils::LoadResourceBitmap("Flag_Japan", size));
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_EUROPE]      = m_imageListSmall->Add(WxUtils::LoadResourceBitmap("Flag_Europe", size));
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_USA]         = m_imageListSmall->Add(WxUtils::LoadResourceBitmap("Flag_USA", size));
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_AUSTRALIA]   = m_imageListSmall->Add(WxUtils::LoadResourceBitmap("Flag_Australia", size));
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_FRANCE]      = m_imageListSmall->Add(WxUtils::LoadResourceBitmap("Flag_France", size));
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_GERMANY]     = m_imageListSmall->Add(WxUtils::LoadResourceBitmap("Flag_Germany", size));
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_ITALY]       = m_imageListSmall->Add(WxUtils::LoadResourceBitmap("Flag_Italy", size));
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_KOREA]       = m_imageListSmall->Add(WxUtils::LoadResourceBitmap("Flag_Korea", size));
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_NETHERLANDS] = m_imageListSmall->Add(WxUtils::LoadResourceBitmap("Flag_Netherlands", size));
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_RUSSIA]      = m_imageListSmall->Add(WxUtils::LoadResourceBitmap("Flag_Russia", size));
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_SPAIN]       = m_imageListSmall->Add(WxUtils::LoadResourceBitmap("Flag_Spain", size));
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_TAIWAN]      = m_imageListSmall->Add(WxUtils::LoadResourceBitmap("Flag_Taiwan", size));
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_WORLD]       = m_imageListSmall->Add(WxUtils::LoadResourceBitmap("Flag_International", size));
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_UNKNOWN]     = m_imageListSmall->Add(WxUtils::LoadResourceBitmap("Flag_Unknown", size));

	m_PlatformImageIndex.resize(4);
	m_PlatformImageIndex[DiscIO::IVolume::GAMECUBE_DISC]   = m_imageListSmall->Add(WxUtils::LoadResourceBitmap("Platform_Gamecube", size));
	m_PlatformImageIndex[DiscIO::IVolume::WII_DISC]        = m_imageListSmall->Add(WxUtils::LoadResourceBitmap("Platform_Wii", size));
	m_PlatformImageIndex[DiscIO::IVolume::WII_WAD]         = m_imageListSmall->Add(WxUtils::LoadResourceBitmap("Platform_Wad", size));
	m_PlatformImageIndex[DiscIO::IVolume::ELF_DOL]         = m_imageListSmall->Add(WxUtils::LoadResourceBitmap("Platform_File", size));

	m_EmuStateImageIndex.resize(6);
	m_EmuStateImageIndex[0] = m_imageListSmall->Add(WxUtils::LoadResourceBitmap("rating0", size));
	m_EmuStateImageIndex[1] = m_imageListSmall->Add(WxUtils::LoadResourceBitmap("rating1", size));
	m_EmuStateImageIndex[2] = m_imageListSmall->Add(WxUtils::LoadResourceBitmap("rating2", size));
	m_EmuStateImageIndex[3] = m_imageListSmall->Add(WxUtils::LoadResourceBitmap("rating3", size));
	m_EmuStateImageIndex[4] = m_imageListSmall->Add(WxUtils::LoadResourceBitmap("rating4", size));
	m_EmuStateImageIndex[5] = m_imageListSmall->Add(WxUtils::LoadResourceBitmap("rating5", size));
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

		InsertColumn(COLUMN_MAKER, _("Maker"));
		InsertColumn(COLUMN_FILENAME, _("File"));
		InsertColumn(COLUMN_ID, _("ID"));
		InsertColumn(COLUMN_COUNTRY, "");
		InsertColumn(COLUMN_SIZE, _("Size"));
		InsertColumn(COLUMN_EMULATION_STATE, _("State"));

#ifdef __WXMSW__
		const int platform_padding = 0;
#else
		const int platform_padding = 8;
#endif

		const int platform_icon_padding = 1;

		// set initial sizes for columns
		SetColumnWidth(COLUMN_DUMMY, 0);
		SetColumnWidth(COLUMN_PLATFORM, SConfig::GetInstance().m_showSystemColumn ? 32 + platform_icon_padding + platform_padding : 0);
		SetColumnWidth(COLUMN_BANNER, SConfig::GetInstance().m_showBannerColumn ? 96 + platform_padding : 0);
		SetColumnWidth(COLUMN_TITLE, 175 + platform_padding);
		SetColumnWidth(COLUMN_MAKER, SConfig::GetInstance().m_showMakerColumn ? 150 + platform_padding : 0);
		SetColumnWidth(COLUMN_FILENAME, SConfig::GetInstance().m_showFileNameColumn ? 100 + platform_padding : 0);
		SetColumnWidth(COLUMN_ID, SConfig::GetInstance().m_showIDColumn ? 75 + platform_padding : 0);
		SetColumnWidth(COLUMN_COUNTRY, SConfig::GetInstance().m_showRegionColumn ? 32 + platform_padding : 0);
		SetColumnWidth(COLUMN_EMULATION_STATE, SConfig::GetInstance().m_showStateColumn ? 48 + platform_padding : 0);

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
		// first message instead
		if (IsHidingItems())
		{
			errorString = _("Dolphin is currently set to hide all games. Double-click here to show all games...");
		}
		else
		{
			errorString = _("Dolphin could not find any GameCube/Wii ISOs or WADs. Double-click here to browse for files...");
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
	const u64 unit_size = (1ull << (unit * 10));

	// mul 1000 for 3 decimal places, add 5 to round up, div 10 for 2 decimal places
	std::string value = std::to_string((_size * 1000 / unit_size + 5) / 10);
	// Insert decimal point.
	value.insert(value.size() - 2, ".");
	return StrToWxStr(StringFromFormat("%s %s", value.c_str(), unit_symbols[unit]));
}

// Update the column content of the item at _Index
void CGameListCtrl::UpdateItemAtColumn(long _Index, int column)
{
	GameListItem& rISOFile = *m_ISOFiles[_Index];

	switch(column)
	{
		case COLUMN_PLATFORM:
		{
			SetItemColumnImage(_Index, COLUMN_PLATFORM,
			                   m_PlatformImageIndex[rISOFile.GetPlatform()]);
			break;
		}
		case COLUMN_BANNER:
		{
			int ImageIndex = -1;

			if (rISOFile.GetBitmap().IsOk())
				ImageIndex = m_imageListSmall->Add(rISOFile.GetBitmap());

			SetItemColumnImage(_Index, COLUMN_BANNER, ImageIndex);
			break;
		}
		case COLUMN_TITLE:
		{
			wxString name = StrToWxStr(rISOFile.GetName());
			int disc_number = rISOFile.GetDiscNumber() + 1;

			if (disc_number > 1 &&
			    name.Lower().find(wxString::Format("disc %i", disc_number)) == std::string::npos &&
			    name.Lower().find(wxString::Format("disc%i", disc_number)) == std::string::npos)
			{
				name = wxString::Format(_("%s (Disc %i)"), name.c_str(), disc_number);
			}

			SetItem(_Index, COLUMN_TITLE, name, -1);
			break;
		}
		case COLUMN_MAKER:
			SetItem(_Index, COLUMN_MAKER, StrToWxStr(rISOFile.GetCompany()), -1);
			break;
		case COLUMN_FILENAME:
			SetItem(_Index, COLUMN_FILENAME,
			        wxFileNameFromPath(rISOFile.GetFileName()), -1);
			break;
		case COLUMN_EMULATION_STATE:
			SetItemColumnImage(_Index, COLUMN_EMULATION_STATE,
			                   m_EmuStateImageIndex[rISOFile.GetEmuState()]);
			break;
		case COLUMN_COUNTRY:
			SetItemColumnImage(_Index, COLUMN_COUNTRY,
			                   m_FlagImageIndex[rISOFile.GetCountry()]);
			break;
		case COLUMN_SIZE:
			SetItem(_Index, COLUMN_SIZE, NiceSizeFormat(rISOFile.GetFileSize()), -1);
			break;
		case COLUMN_ID:
			SetItem(_Index, COLUMN_ID, rISOFile.GetUniqueID(), -1);
			break;
	}
}

void CGameListCtrl::InsertItemInReportView(long _Index)
{
	// When using wxListCtrl, there is no hope of per-column text colors.
	// But for reference, here are the old colors that were used: (BGR)
	// title: 0xFF0000
	// company: 0x007030

	// Insert a first row with nothing in it, that will be used as the Index
	long ItemIndex = InsertItem(_Index, wxEmptyString);

	// Iterate over all columns and fill them with content if they are visible
	for (int i = 1; i < NUMBER_OF_COLUMN; i++)
	{
		if (GetColumnWidth(i) != 0)
			UpdateItemAtColumn(_Index, i);
	}

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

	// Load custom game titles from titles.txt
	// http://www.gametdb.com/Wii/Downloads
	std::unordered_map<std::string, std::string> custom_title_map;
	std::ifstream titlestxt;
	OpenFStream(titlestxt, File::GetUserPath(D_LOAD_IDX) + "titles.txt", std::ios::in);

	if (!titlestxt.is_open())
		OpenFStream(titlestxt, File::GetUserPath(D_LOAD_IDX) + "wiitdb.txt", std::ios::in);

	if (titlestxt.is_open())
	{
		std::string line;
		while (!titlestxt.eof() && std::getline(titlestxt, line))
		{
			const size_t equals_index = line.find('=');
			if (equals_index != std::string::npos)
				custom_title_map.emplace(StripSpaces(line.substr(0, equals_index)),
			                             StripSpaces(line.substr(equals_index + 1)));
		}
		titlestxt.close();
	}

	std::vector<std::string> Extensions;

	if (SConfig::GetInstance().m_ListGC)
		Extensions.push_back(".gcm");
	if (SConfig::GetInstance().m_ListWii || SConfig::GetInstance().m_ListGC)
	{
		Extensions.push_back(".iso");
		Extensions.push_back(".ciso");
		Extensions.push_back(".gcz");
		Extensions.push_back(".wbfs");
	}
	if (SConfig::GetInstance().m_ListWad)
		Extensions.push_back(".wad");
	if (SConfig::GetInstance().m_ListElfDol)
	{
		Extensions.push_back(".dol");
		Extensions.push_back(".elf");
	}

	auto rFilenames = DoFileSearch(Extensions, SConfig::GetInstance().m_ISOFolder, SConfig::GetInstance().m_RecursiveISOFolder);

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

			auto iso_file = std::make_unique<GameListItem>(rFilenames[i], custom_title_map);

			if (iso_file->IsValid())
			{
				bool list = true;

				switch(iso_file->GetPlatform())
				{
					case DiscIO::IVolume::WII_DISC:
						if (!SConfig::GetInstance().m_ListWii)
							list = false;
						break;
					case DiscIO::IVolume::WII_WAD:
						if (!SConfig::GetInstance().m_ListWad)
							list = false;
						break;
					case DiscIO::IVolume::ELF_DOL:
						if (!SConfig::GetInstance().m_ListElfDol)
							list = false;
						break;
					default:
						if (!SConfig::GetInstance().m_ListGC)
							list = false;
						break;
				}

				switch(iso_file->GetCountry())
				{
					case DiscIO::IVolume::COUNTRY_AUSTRALIA:
						if (!SConfig::GetInstance().m_ListAustralia)
							list = false;
						break;
					case DiscIO::IVolume::COUNTRY_EUROPE:
						if (!SConfig::GetInstance().m_ListPal)
							list = false;
						break;
					case DiscIO::IVolume::COUNTRY_FRANCE:
						if (!SConfig::GetInstance().m_ListFrance)
							list = false;
						break;
					case DiscIO::IVolume::COUNTRY_GERMANY:
						if (!SConfig::GetInstance().m_ListGermany)
							list = false;
						break;
					case DiscIO::IVolume::COUNTRY_ITALY:
						if (!SConfig::GetInstance().m_ListItaly)
							list = false;
						break;
					case DiscIO::IVolume::COUNTRY_JAPAN:
						if (!SConfig::GetInstance().m_ListJap)
							list = false;
						break;
					case DiscIO::IVolume::COUNTRY_KOREA:
						if (!SConfig::GetInstance().m_ListKorea)
							list = false;
						break;
					case DiscIO::IVolume::COUNTRY_NETHERLANDS:
						if (!SConfig::GetInstance().m_ListNetherlands)
							list = false;
						break;
					case DiscIO::IVolume::COUNTRY_RUSSIA:
						if (!SConfig::GetInstance().m_ListRussia)
							list = false;
						break;
					case DiscIO::IVolume::COUNTRY_SPAIN:
						if (!SConfig::GetInstance().m_ListSpain)
							list = false;
						break;
					case DiscIO::IVolume::COUNTRY_TAIWAN:
						if (!SConfig::GetInstance().m_ListTaiwan)
							list = false;
						break;
					case DiscIO::IVolume::COUNTRY_USA:
						if (!SConfig::GetInstance().m_ListUsa)
							list = false;
						break;
					case DiscIO::IVolume::COUNTRY_WORLD:
						if (!SConfig::GetInstance().m_ListWorld)
							list = false;
						break;
					case DiscIO::IVolume::COUNTRY_UNKNOWN:
					default:
						if (!SConfig::GetInstance().m_ListUnknown)
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
			auto gli = std::make_unique<GameListItem>(drive, custom_title_map);

			if (gli->IsValid())
				m_ISOFiles.push_back(gli.release());
		}
	}

	std::sort(m_ISOFiles.begin(), m_ISOFiles.end());
}

void CGameListCtrl::OnColBeginDrag(wxListEvent& event)
{
	const int column_id = event.GetColumn();

	if (column_id != COLUMN_TITLE && column_id != COLUMN_MAKER && column_id != COLUMN_FILENAME)
		event.Veto();
}

const GameListItem* CGameListCtrl::GetISO(size_t index) const
{
	if (index < m_ISOFiles.size())
		return m_ISOFiles[index];
	else
		return nullptr;
}

static CGameListCtrl* caller;
static int wxCALLBACK wxListCompare(wxIntPtr item1, wxIntPtr item2, wxIntPtr sortData)
{
	// return 1 if item1 > item2
	// return -1 if item1 < item2
	// return 0 for identity
	const GameListItem* iso1 = caller->GetISO(item1);
	const GameListItem* iso2 = caller->GetISO(item2);

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

		if (text.MakeUpper()[0] == event.GetKeyCode())
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
		const GameListItem* selected_iso = GetSelectedISO();
		if (selected_iso)
		{
			wxMenu popupMenu;
			DiscIO::IVolume::EPlatform platform = selected_iso->GetPlatform();

			if (platform != DiscIO::IVolume::ELF_DOL)
			{
				popupMenu.Append(IDM_PROPERTIES, _("&Properties"));
				popupMenu.Append(IDM_GAME_WIKI, _("&Wiki"));
				popupMenu.AppendSeparator();
			}
			if (platform == DiscIO::IVolume::WII_DISC || platform == DiscIO::IVolume::WII_WAD)
			{
				popupMenu.Append(IDM_OPEN_SAVE_FOLDER, _("Open Wii &save folder"));
				popupMenu.Append(IDM_EXPORT_SAVE, _("Export Wii save (Experimental)"));
			}
			popupMenu.Append(IDM_OPEN_CONTAINING_FOLDER, _("Open &containing folder"));

			if (platform != DiscIO::IVolume::ELF_DOL)
				popupMenu.AppendCheckItem(IDM_SET_DEFAULT_ISO, _("Set as &default ISO"));

			// First we have to decide a starting value when we append it
			if (platform == SConfig::GetInstance().m_strDefaultISO)
				popupMenu.FindItem(IDM_SET_DEFAULT_ISO)->Check();

			popupMenu.AppendSeparator();
			popupMenu.Append(IDM_DELETE_ISO, _("&Delete File..."));

			if (platform == DiscIO::IVolume::GAMECUBE_DISC || platform == DiscIO::IVolume::WII_DISC)
			{
				if (selected_iso->GetBlobType() == DiscIO::BlobType::GCZ)
					popupMenu.Append(IDM_COMPRESS_ISO, _("Decompress ISO..."));
				else if (selected_iso->GetBlobType() == DiscIO::BlobType::PLAIN)
					popupMenu.Append(IDM_COMPRESS_ISO, _("Compress ISO..."));

				wxMenuItem* changeDiscItem = popupMenu.Append(IDM_LIST_CHANGE_DISC, _("Change &Disc"));
				changeDiscItem->Enable(Core::IsRunning());
			}

			if (platform == DiscIO::IVolume::WII_WAD)
				popupMenu.Append(IDM_LIST_INSTALL_WAD, _("Install to Wii Menu"));

			PopupMenu(&popupMenu);
		}
	}
	else if (GetSelectedItemCount() > 1)
	{
		wxMenu popupMenu;
		popupMenu.Append(IDM_DELETE_ISO, _("&Delete selected ISOs..."));
		popupMenu.AppendSeparator();
		popupMenu.Append(IDM_MULTI_COMPRESS_ISO, _("Compress selected ISOs..."));
		popupMenu.Append(IDM_MULTI_DECOMPRESS_ISO, _("Decompress selected ISOs..."));
		PopupMenu(&popupMenu);
	}
}

const GameListItem* CGameListCtrl::GetSelectedISO() const
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
			return nullptr;
		return m_ISOFiles[GetItemData(item)];
	}
}

std::vector<const GameListItem*> CGameListCtrl::GetAllSelectedISOs() const
{
	std::vector<const GameListItem*> result;
	long item = -1;
	while (true)
	{
		item = GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		if (item == wxNOT_FOUND)
			return result;
		result.push_back(m_ISOFiles[GetItemData(item)]);
	}
}

bool CGameListCtrl::IsHidingItems()
{
	return !(SConfig::GetInstance().m_ListGC &&
	         SConfig::GetInstance().m_ListWii &&
	         SConfig::GetInstance().m_ListWad &&
	         SConfig::GetInstance().m_ListElfDol &&
	         SConfig::GetInstance().m_ListJap &&
	         SConfig::GetInstance().m_ListUsa &&
	         SConfig::GetInstance().m_ListPal &&
	         SConfig::GetInstance().m_ListAustralia &&
	         SConfig::GetInstance().m_ListFrance &&
	         SConfig::GetInstance().m_ListGermany &&
	         SConfig::GetInstance().m_ListItaly &&
	         SConfig::GetInstance().m_ListKorea &&
	         SConfig::GetInstance().m_ListNetherlands &&
	         SConfig::GetInstance().m_ListRussia &&
	         SConfig::GetInstance().m_ListSpain &&
	         SConfig::GetInstance().m_ListTaiwan &&
	         SConfig::GetInstance().m_ListWorld &&
	         SConfig::GetInstance().m_ListUnknown);
}

void CGameListCtrl::OnOpenContainingFolder(wxCommandEvent& WXUNUSED (event))
{
	const GameListItem* iso = GetSelectedISO();
	if (!iso)
		return;

	wxFileName path = wxFileName::FileName(StrToWxStr(iso->GetFileName()));
	path.MakeAbsolute();
	WxUtils::Explore(WxStrToStr(path.GetPath()));
}

void CGameListCtrl::OnOpenSaveFolder(wxCommandEvent& WXUNUSED (event))
{
	const GameListItem* iso = GetSelectedISO();
	if (!iso)
		return;
	std::string path = iso->GetWiiFSPath();
	if (!path.empty())
		WxUtils::Explore(path);
}

void CGameListCtrl::OnExportSave(wxCommandEvent& WXUNUSED (event))
{
	const GameListItem* iso = GetSelectedISO();
	if (!iso)
		return;

	u64 title_id;
	std::unique_ptr<DiscIO::IVolume> volume(DiscIO::CreateVolumeFromFilename(iso->GetFileName()));
	if (volume && volume->GetTitleID(&title_id))
	{
		CWiiSaveCrypted::ExportWiiSave(title_id);
	}
}

// Save this file as the default file
void CGameListCtrl::OnSetDefaultISO(wxCommandEvent& event)
{
	const GameListItem* iso = GetSelectedISO();
	if (!iso) return;

	if (event.IsChecked())
	{
		// Write the new default value and save it the ini file
		SConfig::GetInstance().m_strDefaultISO =
			iso->GetFileName();
		SConfig::GetInstance().SaveSettings();
	}
	else
	{
		// Otherwise blank the value and save it
		SConfig::GetInstance().m_strDefaultISO = "";
		SConfig::GetInstance().SaveSettings();
	}
}

void CGameListCtrl::OnDeleteISO(wxCommandEvent& WXUNUSED (event))
{
	const wxString message = GetSelectedItemCount() == 1 ?
			_("Are you sure you want to delete this file? It will be gone forever!") :
			_("Are you sure you want to delete these files? They will be gone forever!");

	if (wxMessageBox(message, _("Warning"), wxYES_NO | wxICON_EXCLAMATION) == wxYES)
	{
		for (const GameListItem* iso : GetAllSelectedISOs())
			File::Delete(iso->GetFileName());
		Update();
	}
}

void CGameListCtrl::OnProperties(wxCommandEvent& WXUNUSED (event))
{
	const GameListItem* iso = GetSelectedISO();
	if (!iso)
		return;

	CISOProperties* ISOProperties = new CISOProperties(*iso, this);
	ISOProperties->Show();
}

void CGameListCtrl::OnWiki(wxCommandEvent& WXUNUSED (event))
{
	const GameListItem* iso = GetSelectedISO();
	if (!iso)
		return;

	std::string wikiUrl = "https://wiki.dolphin-emu.org/dolphin-redirect.php?gameid=" + iso->GetUniqueID();
	WxUtils::Launch(wikiUrl);
}

bool CGameListCtrl::MultiCompressCB(const std::string& text, float percent, void* arg)
{
	CompressionProgress* progress = static_cast<CompressionProgress*>(arg);

	float total_percent = ((float)progress->items_done + percent) / (float)progress->items_total;
	wxString text_string(StrToWxStr(StringFromFormat("%s (%i/%i) - %s",
				progress->current_filename.c_str(), progress->items_done + 1,
				progress->items_total, text.c_str())));

	return progress->dialog->Update(total_percent * progress->dialog->GetRange(), text_string);
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
	const std::vector<const GameListItem*> selected_items = GetAllSelectedISOs();

	// If any Wii discs are going to be compressed, show the Wii compression warning once
	if (_compress)
	{
		for (const GameListItem* iso : selected_items)
		{
			if (!iso->IsCompressed() && iso->GetPlatform() == DiscIO::IVolume::WII_DISC)
			{
				if (WiiCompressWarning())
					break;
				else
					return;
			}
		}
	}

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
			1000, // Arbitrary number that's larger than the dialog's width in pixels
			this,
			wxPD_APP_MODAL |
			wxPD_CAN_ABORT |
			wxPD_ELAPSED_TIME | wxPD_ESTIMATED_TIME | wxPD_REMAINING_TIME |
			wxPD_SMOOTH
			);

		CompressionProgress progress(0, GetSelectedItemCount(), "", &progressDialog);

		for (const GameListItem* iso : selected_items)
		{
			if (iso->GetPlatform() != DiscIO::IVolume::GAMECUBE_DISC && iso->GetPlatform() != DiscIO::IVolume::WII_DISC)
				continue;
			if (iso->GetBlobType() != DiscIO::BlobType::PLAIN && iso->GetBlobType() != DiscIO::BlobType::GCZ)
				continue;

			if (!iso->IsCompressed() && _compress)
			{
				std::string FileName;
				SplitPath(iso->GetFileName(), nullptr, &FileName, nullptr);
				progress.current_filename = FileName;
				FileName.append(".gcz");

				std::string OutputFileName;
				BuildCompleteFilename(OutputFileName,
						WxStrToStr(browseDialog.GetPath()),
						FileName);

				if (File::Exists(OutputFileName) &&
						wxMessageBox(
							wxString::Format(_("The file %s already exists.\nDo you wish to replace it?"),
							StrToWxStr(OutputFileName)),
							_("Confirm File Overwrite"),
							wxYES_NO) == wxNO)
					continue;

				all_good &= DiscIO::CompressFileToBlob(iso->GetFileName(),
						OutputFileName,
						(iso->GetPlatform() == DiscIO::IVolume::WII_DISC) ? 1 : 0,
						16384, &MultiCompressCB, &progress);
			}
			else if (iso->IsCompressed() && !_compress)
			{
				std::string FileName;
				SplitPath(iso->GetFileName(), nullptr, &FileName, nullptr);
				progress.current_filename = FileName;
				if (iso->GetPlatform() == DiscIO::IVolume::WII_DISC)
					FileName.append(".iso");
				else
					FileName.append(".gcm");

				std::string OutputFileName;
				BuildCompleteFilename(OutputFileName,
						WxStrToStr(browseDialog.GetPath()),
						FileName);

				if (File::Exists(OutputFileName) &&
						wxMessageBox(
							wxString::Format(_("The file %s already exists.\nDo you wish to replace it?"),
							StrToWxStr(OutputFileName)),
							_("Confirm File Overwrite"),
							wxYES_NO) == wxNO)
					continue;

				all_good &= DiscIO::DecompressBlobToFile(iso->GetFileName().c_str(),
						OutputFileName.c_str(), &MultiCompressCB, &progress);
			}

			progress.items_done++;
		}
	}

	if (!all_good)
		WxUtils::ShowErrorDialog(_("Dolphin was unable to complete the requested action."));

	Update();
}

bool CGameListCtrl::CompressCB(const std::string& text, float percent, void* arg)
{
	return ((wxProgressDialog*)arg)->
		Update((int)(percent * 1000), StrToWxStr(text));
}

void CGameListCtrl::OnCompressISO(wxCommandEvent& WXUNUSED (event))
{
	const GameListItem* iso = GetSelectedISO();
	if (!iso)
		return;

	bool is_compressed = iso->GetBlobType() == DiscIO::BlobType::GCZ;
	wxString path;

	std::string FileName, FilePath, FileExtension;
	SplitPath(iso->GetFileName(), &FilePath, &FileName, &FileExtension);

	do
	{
		if (is_compressed)
		{
			wxString FileType;
			if (iso->GetPlatform() == DiscIO::IVolume::WII_DISC)
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
			if (iso->GetPlatform() == DiscIO::IVolume::WII_DISC && !WiiCompressWarning())
				return;

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
		is_compressed ? _("Decompressing ISO") : _("Compressing ISO"),
		_("Working..."),
		1000,
		this,
		wxPD_APP_MODAL |
		wxPD_CAN_ABORT |
		wxPD_ELAPSED_TIME | wxPD_ESTIMATED_TIME | wxPD_REMAINING_TIME |
		wxPD_SMOOTH
		);


	if (is_compressed)
		all_good = DiscIO::DecompressBlobToFile(iso->GetFileName(),
				WxStrToStr(path), &CompressCB, &dialog);
	else
		all_good = DiscIO::CompressFileToBlob(iso->GetFileName(),
				WxStrToStr(path),
				(iso->GetPlatform() == DiscIO::IVolume::WII_DISC) ? 1 : 0,
				16384, &CompressCB, &dialog);
	}

	if (!all_good)
		WxUtils::ShowErrorDialog(_("Dolphin was unable to complete the requested action."));

	Update();
}

void CGameListCtrl::OnChangeDisc(wxCommandEvent& WXUNUSED(event))
{
	const GameListItem* iso = GetSelectedISO();
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

		// We hide the Maker column if the window is too small
		// Use ShowColumn() instead of SetColumnWidth because
		// the maker column may have been autohidden and the
		// therefore the content needs to be restored.
		if (resizable > 425)
		{
			if (SConfig::GetInstance().m_showMakerColumn &&
			    SConfig::GetInstance().m_showFileNameColumn)
			{
				SetColumnWidth(COLUMN_TITLE, resizable / 3);
				ShowColumn(COLUMN_MAKER, resizable / 3);
				SetColumnWidth(COLUMN_FILENAME, resizable / 3);
			}
			else if (SConfig::GetInstance().m_showMakerColumn)
			{
				SetColumnWidth(COLUMN_TITLE, resizable / 2);
				ShowColumn(COLUMN_MAKER, resizable / 2);
			}
			else if (SConfig::GetInstance().m_showFileNameColumn)
			{
				SetColumnWidth(COLUMN_TITLE, resizable / 2);
				SetColumnWidth(COLUMN_FILENAME, resizable / 2);
			}
			else
			{
				SetColumnWidth(COLUMN_TITLE, resizable);
			}
		}
		else
		{
			if (SConfig::GetInstance().m_showFileNameColumn)
			{
				SetColumnWidth(COLUMN_TITLE, resizable / 2);
				SetColumnWidth(COLUMN_FILENAME, resizable / 2);
			}
			else
			{
				SetColumnWidth(COLUMN_TITLE, resizable);
			}
			HideColumn(COLUMN_MAKER);
		}
	}
}

// Fills a previously hidden column with items. Acts
// as a SetColumnWidth if width is nonzero.
void CGameListCtrl::ShowColumn(int column, int width)
{
	// Fill the column with items if it was hidden
	if (GetColumnWidth(column) == 0)
	{
		for (int i = 0; i < GetItemCount(); i++)
		{
			UpdateItemAtColumn(i, column);
		}
	}
	SetColumnWidth(column, width);
}

// Hide the passed column from the gamelist.
// It is not enough to set the width to zero because this leads to
// graphical glitches where the content of the hidden column is
// squeezed into the next column. Therefore we need to clear the
// items, too.
void CGameListCtrl::HideColumn(int column)
{
	// Do nothing if the column is already hidden
	if (GetColumnWidth(column) == 0)
		return;

	// Remove the items from the column
	for (int i = 0; i < GetItemCount(); i++)
	{
		SetItem(i, column, "", -1);
	}
	SetColumnWidth(column, 0);
}

void CGameListCtrl::UnselectAll()
{
	for (int i = 0; i < GetItemCount(); i++)
	{
		SetItemState(i, 0, wxLIST_STATE_SELECTED);
	}
}
bool CGameListCtrl::WiiCompressWarning()
{
	return wxMessageBox(
			_("Compressing a Wii disc image will irreversibly change the compressed copy by removing padding data. Your disc image will still work. Continue?"),
			_("Warning"),
			wxYES_NO) == wxYES;
}
