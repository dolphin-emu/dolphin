// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <wx/app.h>
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

#ifdef __WXMSW__
#include <CommCtrl.h>
#include <wx/msw/dc.h>
#endif

#include "Common/CDUtils.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/MathUtil.h"
#include "Common/StringUtil.h"
#include "Common/SysConf.h"
#include "Common/Thread.h"
#include "Core/Boot/Boot.h"
#include "Core/Config/NetplaySettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/WiiSaveCrypted.h"
#include "Core/Movie.h"
#include "Core/TitleDatabase.h"
#include "DiscIO/Blob.h"
#include "DiscIO/DirectoryBlob.h"
#include "DiscIO/Enums.h"
#include "DiscIO/Volume.h"
#include "DolphinWX/Frame.h"
#include "DolphinWX/GameListCtrl.h"
#include "DolphinWX/Globals.h"
#include "DolphinWX/ISOFile.h"
#include "DolphinWX/ISOProperties/ISOProperties.h"
#include "DolphinWX/Main.h"
#include "DolphinWX/NetPlay/NetPlayLauncher.h"
#include "DolphinWX/WxUtils.h"

struct CompressionProgress final
{
public:
  CompressionProgress(int items_done_, int items_total_, const std::string& current_filename_,
                      wxProgressDialog* dialog_)
      : items_done(items_done_), items_total(items_total_), current_filename(current_filename_),
        dialog(dialog_)
  {
  }

  int items_done;
  int items_total;
  std::string current_filename;
  wxProgressDialog* dialog;
};

static constexpr u32 CACHE_REVISION = 3;  // Last changed in PR 5573

static bool sorted = false;

static int CompareGameListItems(const GameListItem* iso1, const GameListItem* iso2,
                                long sortData = GameListCtrl::COLUMN_TITLE)
{
  int t = 1;

  if (sortData < 0)
  {
    t = -1;
    sortData = -sortData;
  }

  switch (sortData)
  {
  case GameListCtrl::COLUMN_MAKER:
  {
    int maker_cmp = strcasecmp(iso1->GetCompany().c_str(), iso2->GetCompany().c_str()) * t;
    if (maker_cmp != 0)
      return maker_cmp;
    break;
  }
  case GameListCtrl::COLUMN_FILENAME:
    return wxStricmp(wxFileNameFromPath(iso1->GetFileName()),
                     wxFileNameFromPath(iso2->GetFileName())) *
           t;
  case GameListCtrl::COLUMN_ID:
  {
    int id_cmp = strcasecmp(iso1->GetGameID().c_str(), iso2->GetGameID().c_str()) * t;
    if (id_cmp != 0)
      return id_cmp;
    break;
  }
  case GameListCtrl::COLUMN_COUNTRY:
    if (iso1->GetCountry() > iso2->GetCountry())
      return 1 * t;
    if (iso1->GetCountry() < iso2->GetCountry())
      return -1 * t;
    break;
  case GameListCtrl::COLUMN_SIZE:
    if (iso1->GetFileSize() > iso2->GetFileSize())
      return 1 * t;
    if (iso1->GetFileSize() < iso2->GetFileSize())
      return -1 * t;
    break;
  case GameListCtrl::COLUMN_PLATFORM:
    if (iso1->GetPlatform() > iso2->GetPlatform())
      return 1 * t;
    if (iso1->GetPlatform() < iso2->GetPlatform())
      return -1 * t;
    break;

  case GameListCtrl::COLUMN_EMULATION_STATE:
  {
    const int nState1 = iso1->GetEmuState(), nState2 = iso2->GetEmuState();

    if (nState1 > nState2)
      return 1 * t;
    if (nState1 < nState2)
      return -1 * t;
    break;
  }
  }

  if (sortData != GameListCtrl::COLUMN_TITLE)
    t = 1;

  int name_cmp = strcasecmp(iso1->GetName().c_str(), iso2->GetName().c_str()) * t;
  if (name_cmp != 0)
    return name_cmp;

  if (iso1->GetGameID() != iso2->GetGameID())
    return t * (iso1->GetGameID() > iso2->GetGameID() ? 1 : -1);
  if (iso1->GetRevision() != iso2->GetRevision())
    return t * (iso1->GetRevision() > iso2->GetRevision() ? 1 : -1);
  if (iso1->GetDiscNumber() != iso2->GetDiscNumber())
    return t * (iso1->GetDiscNumber() > iso2->GetDiscNumber() ? 1 : -1);

  wxString iso1_filename = wxFileNameFromPath(iso1->GetFileName());
  wxString iso2_filename = wxFileNameFromPath(iso2->GetFileName());

  if (iso1_filename != iso2_filename)
    return t * wxStricmp(iso1_filename, iso2_filename);

  return 0;
}

static bool ShouldDisplayGameListItem(const GameListItem& item)
{
  const bool show_platform = [&item] {
    switch (item.GetPlatform())
    {
    case DiscIO::Platform::GAMECUBE_DISC:
      return SConfig::GetInstance().m_ListGC;
    case DiscIO::Platform::WII_DISC:
      return SConfig::GetInstance().m_ListWii;
    case DiscIO::Platform::WII_WAD:
      return SConfig::GetInstance().m_ListWad;
    case DiscIO::Platform::ELF_DOL:
      return SConfig::GetInstance().m_ListElfDol;
    default:
      return false;
    }
  }();

  if (!show_platform)
    return false;

  switch (item.GetCountry())
  {
  case DiscIO::Country::COUNTRY_AUSTRALIA:
    return SConfig::GetInstance().m_ListAustralia;
  case DiscIO::Country::COUNTRY_EUROPE:
    return SConfig::GetInstance().m_ListPal;
  case DiscIO::Country::COUNTRY_FRANCE:
    return SConfig::GetInstance().m_ListFrance;
  case DiscIO::Country::COUNTRY_GERMANY:
    return SConfig::GetInstance().m_ListGermany;
  case DiscIO::Country::COUNTRY_ITALY:
    return SConfig::GetInstance().m_ListItaly;
  case DiscIO::Country::COUNTRY_JAPAN:
    return SConfig::GetInstance().m_ListJap;
  case DiscIO::Country::COUNTRY_KOREA:
    return SConfig::GetInstance().m_ListKorea;
  case DiscIO::Country::COUNTRY_NETHERLANDS:
    return SConfig::GetInstance().m_ListNetherlands;
  case DiscIO::Country::COUNTRY_RUSSIA:
    return SConfig::GetInstance().m_ListRussia;
  case DiscIO::Country::COUNTRY_SPAIN:
    return SConfig::GetInstance().m_ListSpain;
  case DiscIO::Country::COUNTRY_TAIWAN:
    return SConfig::GetInstance().m_ListTaiwan;
  case DiscIO::Country::COUNTRY_USA:
    return SConfig::GetInstance().m_ListUsa;
  case DiscIO::Country::COUNTRY_WORLD:
    return SConfig::GetInstance().m_ListWorld;
  case DiscIO::Country::COUNTRY_UNKNOWN:
  default:
    return SConfig::GetInstance().m_ListUnknown;
  }
}

wxDEFINE_EVENT(DOLPHIN_EVT_REFRESH_GAMELIST, wxCommandEvent);
wxDEFINE_EVENT(DOLPHIN_EVT_RESCAN_GAMELIST, wxCommandEvent);

struct GameListCtrl::ColumnInfo
{
  const int id;
  const int default_width;
  const bool resizable;
  bool& visible;
};

GameListCtrl::GameListCtrl(bool disable_scanning, wxWindow* parent, const wxWindowID id,
                           const wxPoint& pos, const wxSize& size, long style)
    : wxListCtrl(parent, id, pos, size, style), m_tooltip(nullptr),
      m_columns({// {COLUMN, {default_width (without platform padding), resizability, visibility}}
                 {COLUMN_PLATFORM, 32 + 1 /* icon padding */, false,
                  SConfig::GetInstance().m_showSystemColumn},
                 {COLUMN_BANNER, 96, false, SConfig::GetInstance().m_showBannerColumn},
                 {COLUMN_TITLE, 175, true, SConfig::GetInstance().m_showTitleColumn},
                 {COLUMN_MAKER, 150, true, SConfig::GetInstance().m_showMakerColumn},
                 {COLUMN_FILENAME, 100, true, SConfig::GetInstance().m_showFileNameColumn},
                 {COLUMN_ID, 75, false, SConfig::GetInstance().m_showIDColumn},
                 {COLUMN_COUNTRY, 32, false, SConfig::GetInstance().m_showRegionColumn},
                 {COLUMN_EMULATION_STATE, 48, false, SConfig::GetInstance().m_showStateColumn},
                 {COLUMN_SIZE, wxLIST_AUTOSIZE, false, SConfig::GetInstance().m_showSizeColumn}})
{
  Bind(wxEVT_SIZE, &GameListCtrl::OnSize, this);
  Bind(wxEVT_RIGHT_DOWN, &GameListCtrl::OnRightClick, this);
  Bind(wxEVT_LEFT_DOWN, &GameListCtrl::OnLeftClick, this);
  Bind(wxEVT_MOTION, &GameListCtrl::OnMouseMotion, this);
  Bind(wxEVT_LIST_KEY_DOWN, &GameListCtrl::OnKeyPress, this);
  Bind(wxEVT_LIST_COL_BEGIN_DRAG, &GameListCtrl::OnColBeginDrag, this);
  Bind(wxEVT_LIST_COL_CLICK, &GameListCtrl::OnColumnClick, this);

  Bind(wxEVT_MENU, &GameListCtrl::OnProperties, this, IDM_PROPERTIES);
  Bind(wxEVT_MENU, &GameListCtrl::OnWiki, this, IDM_GAME_WIKI);
  Bind(wxEVT_MENU, &GameListCtrl::OnOpenContainingFolder, this, IDM_OPEN_CONTAINING_FOLDER);
  Bind(wxEVT_MENU, &GameListCtrl::OnOpenSaveFolder, this, IDM_OPEN_SAVE_FOLDER);
  Bind(wxEVT_MENU, &GameListCtrl::OnExportSave, this, IDM_EXPORT_SAVE);
  Bind(wxEVT_MENU, &GameListCtrl::OnSetDefaultISO, this, IDM_SET_DEFAULT_ISO);
  Bind(wxEVT_MENU, &GameListCtrl::OnCompressISO, this, IDM_COMPRESS_ISO);
  Bind(wxEVT_MENU, &GameListCtrl::OnMultiCompressISO, this, IDM_MULTI_COMPRESS_ISO);
  Bind(wxEVT_MENU, &GameListCtrl::OnMultiDecompressISO, this, IDM_MULTI_DECOMPRESS_ISO);
  Bind(wxEVT_MENU, &GameListCtrl::OnDeleteISO, this, IDM_DELETE_ISO);
  Bind(wxEVT_MENU, &GameListCtrl::OnChangeDisc, this, IDM_LIST_CHANGE_DISC);
  Bind(wxEVT_MENU, &GameListCtrl::OnNetPlayHost, this, IDM_START_NETPLAY);

  Bind(DOLPHIN_EVT_REFRESH_GAMELIST, &GameListCtrl::OnRefreshGameList, this);
  Bind(DOLPHIN_EVT_RESCAN_GAMELIST, &GameListCtrl::OnRescanGameList, this);

  wxTheApp->Bind(DOLPHIN_EVT_LOCAL_INI_CHANGED, &GameListCtrl::OnLocalIniModified, this);

  if (!disable_scanning)
  {
    m_scan_thread = std::thread([&] {
      Common::SetCurrentThreadName("gamelist scanner");

      if (SyncCacheFile(false))
        QueueEvent(new wxCommandEvent(DOLPHIN_EVT_REFRESH_GAMELIST));

      // Always do an initial scan to catch new files and perform the more expensive per-file
      // checks. TODO Make this safely cancellable if it becomes too slow?
      RescanList();

      m_scan_trigger.Wait();
      while (!m_scan_exiting.IsSet())
      {
        RescanList();
        m_scan_trigger.Wait();
      }
    });
  }
}

GameListCtrl::~GameListCtrl()
{
  if (m_scan_thread.joinable())
  {
    m_scan_exiting.Set();
    m_scan_trigger.Set();
    m_scan_thread.join();
  }
}

template <typename T>
static void InitBitmap(wxImageList* img_list, std::vector<int>* vector, wxWindow* context,
                       const wxSize& usable_size, T index, const std::string& name,
                       bool themed = false)
{
  wxSize size = img_list->GetSize();
  auto bitmap_fnc = themed ? WxUtils::LoadScaledThemeBitmap : WxUtils::LoadScaledResourceBitmap;
  (*vector)[static_cast<size_t>(index)] = img_list->Add(
      bitmap_fnc(name, context, size, usable_size, WxUtils::LSI_SCALE | WxUtils::LSI_ALIGN_VCENTER,
                 wxTransparentColour));
}

void GameListCtrl::InitBitmaps()
{
  const wxSize size = FromDIP(wxSize(96, 32));
  const wxSize flag_bmp_size = FromDIP(wxSize(32, 32));
  const wxSize platform_bmp_size = flag_bmp_size;
  const wxSize rating_bmp_size = FromDIP(wxSize(48, 32));
  wxImageList* img_list = new wxImageList(size.GetWidth(), size.GetHeight());
  AssignImageList(img_list, wxIMAGE_LIST_SMALL);

  auto& flag_indexes = m_image_indexes.flag;
  flag_indexes.resize(static_cast<size_t>(DiscIO::Country::NUMBER_OF_COUNTRIES));
  InitBitmap(img_list, &flag_indexes, this, flag_bmp_size, DiscIO::Country::COUNTRY_JAPAN,
             "Flag_Japan");
  InitBitmap(img_list, &flag_indexes, this, flag_bmp_size, DiscIO::Country::COUNTRY_EUROPE,
             "Flag_Europe");
  InitBitmap(img_list, &flag_indexes, this, flag_bmp_size, DiscIO::Country::COUNTRY_USA,
             "Flag_USA");
  InitBitmap(img_list, &flag_indexes, this, flag_bmp_size, DiscIO::Country::COUNTRY_AUSTRALIA,
             "Flag_Australia");
  InitBitmap(img_list, &flag_indexes, this, flag_bmp_size, DiscIO::Country::COUNTRY_FRANCE,
             "Flag_France");
  InitBitmap(img_list, &flag_indexes, this, flag_bmp_size, DiscIO::Country::COUNTRY_GERMANY,
             "Flag_Germany");
  InitBitmap(img_list, &flag_indexes, this, flag_bmp_size, DiscIO::Country::COUNTRY_ITALY,
             "Flag_Italy");
  InitBitmap(img_list, &flag_indexes, this, flag_bmp_size, DiscIO::Country::COUNTRY_KOREA,
             "Flag_Korea");
  InitBitmap(img_list, &flag_indexes, this, flag_bmp_size, DiscIO::Country::COUNTRY_NETHERLANDS,
             "Flag_Netherlands");
  InitBitmap(img_list, &flag_indexes, this, flag_bmp_size, DiscIO::Country::COUNTRY_RUSSIA,
             "Flag_Russia");
  InitBitmap(img_list, &flag_indexes, this, flag_bmp_size, DiscIO::Country::COUNTRY_SPAIN,
             "Flag_Spain");
  InitBitmap(img_list, &flag_indexes, this, flag_bmp_size, DiscIO::Country::COUNTRY_TAIWAN,
             "Flag_Taiwan");
  InitBitmap(img_list, &flag_indexes, this, flag_bmp_size, DiscIO::Country::COUNTRY_WORLD,
             "Flag_International");
  InitBitmap(img_list, &flag_indexes, this, flag_bmp_size, DiscIO::Country::COUNTRY_UNKNOWN,
             "Flag_Unknown");

  auto& platform_indexes = m_image_indexes.platform;
  platform_indexes.resize(static_cast<size_t>(DiscIO::Platform::NUMBER_OF_PLATFORMS));
  InitBitmap(img_list, &platform_indexes, this, platform_bmp_size, DiscIO::Platform::GAMECUBE_DISC,
             "Platform_Gamecube");
  InitBitmap(img_list, &platform_indexes, this, platform_bmp_size, DiscIO::Platform::WII_DISC,
             "Platform_Wii");
  InitBitmap(img_list, &platform_indexes, this, platform_bmp_size, DiscIO::Platform::WII_WAD,
             "Platform_Wad");
  InitBitmap(img_list, &platform_indexes, this, platform_bmp_size, DiscIO::Platform::ELF_DOL,
             "Platform_File");

  auto& emu_state_indexes = m_image_indexes.emu_state;
  emu_state_indexes.resize(6);
  InitBitmap(img_list, &emu_state_indexes, this, rating_bmp_size, 0, "rating0", true);
  InitBitmap(img_list, &emu_state_indexes, this, rating_bmp_size, 1, "rating1", true);
  InitBitmap(img_list, &emu_state_indexes, this, rating_bmp_size, 2, "rating2", true);
  InitBitmap(img_list, &emu_state_indexes, this, rating_bmp_size, 3, "rating3", true);
  InitBitmap(img_list, &emu_state_indexes, this, rating_bmp_size, 4, "rating4", true);
  InitBitmap(img_list, &emu_state_indexes, this, rating_bmp_size, 5, "rating5", true);

  auto& utility_banner_indexes = m_image_indexes.utility_banner;
  utility_banner_indexes.resize(1);
  InitBitmap(img_list, &utility_banner_indexes, this, size, 0, "nobanner");
}

void GameListCtrl::BrowseForDirectory()
{
  wxString dirHome;
  wxGetHomeDir(&dirHome);

  // browse
  wxDirDialog dialog(this, _("Browse for a directory to add"), dirHome,
                     wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);

  if (dialog.ShowModal() == wxID_OK)
  {
    std::string sPath(WxStrToStr(dialog.GetPath()));
    std::vector<std::string>::iterator itResult =
        std::find(SConfig::GetInstance().m_ISOFolder.begin(),
                  SConfig::GetInstance().m_ISOFolder.end(), sPath);

    if (itResult == SConfig::GetInstance().m_ISOFolder.end())
    {
      SConfig::GetInstance().m_ISOFolder.push_back(sPath);
      SConfig::GetInstance().SaveSettings();
      m_scan_trigger.Set();
    }
  }
}

void GameListCtrl::RefreshList()
{
  int scrollPos = wxWindow::GetScrollPos(wxVERTICAL);
  // Don't let the user refresh it while a game is running
  if (Core::GetState() != Core::State::Uninitialized)
    return;

  m_shown_files.clear();
  {
    std::unique_lock<std::mutex> lk(m_cache_mutex);
    for (auto& item : m_cached_files)
    {
      if (ShouldDisplayGameListItem(*item))
        m_shown_files.push_back(item);
    }
  }

  // Drives are not cached. Not sure if this is required, but better to err on the
  // side of caution if cross-platform issues could come into play.
  if (SConfig::GetInstance().m_ListDrives)
  {
    std::unique_lock<std::mutex> lk(m_title_database_mutex);
    for (const auto& drive : cdio_get_devices())
    {
      auto file = std::make_shared<GameListItem>(drive);
      if (file->IsValid())
      {
        if (file->EmuStateChanged())
          file->EmuStateCommit();
        if (file->CustomNameChanged(m_title_database))
          file->CustomNameCommit();
        m_shown_files.push_back(file);
      }
    }
  }

  Freeze();
  ClearAll();

  if (!m_shown_files.empty())
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
    // set initial sizes for columns
    SetColumnWidth(COLUMN_DUMMY, 0);
    for (const auto& c : m_columns)
    {
      SetColumnWidth(c.id, c.visible ? FromDIP(c.default_width + platform_padding) : 0);
    }

    // add all items
    for (int i = 0; i < (int)m_shown_files.size(); i++)
      InsertItemInReportView(i);
    SetColors();

    // Sort items by Title
    if (!sorted)
      m_last_column = 0;
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
    // Remove existing image list and replace it with the smallest possible one.
    // The list needs an image list because it reserves screen pixels for the
    // image even if we aren't going to use one. It uses the dimensions of the
    // last non-null list so assigning nullptr doesn't work.
    AssignImageList(new wxImageList(1, 1), wxIMAGE_LIST_SMALL);

    wxString errorString;
    // We just check for one hide setting to be enabled, as we may only
    // have GC games for example, and hide them, so we should show the
    // first message instead
    if (IsHidingItems())
    {
      errorString =
          _("Dolphin is currently set to hide all games. Double-click here to show all games...");
    }
    else
    {
      errorString = _("Dolphin could not find any GameCube/Wii ISOs or WADs. Double-click here to "
                      "set a games directory...");
    }
    InsertColumn(COLUMN_DUMMY, "");
    long index = InsertItem(0, errorString);
    SetItemFont(index, *wxITALIC_FONT);
    SetColumnWidth(0, wxLIST_AUTOSIZE);
  }
  if (GetSelectedISO() == nullptr)
    main_frame->UpdateGUI();
  // Thaw before calling AutomaticColumnWidth so that GetClientSize will
  // correctly account for the width of scrollbars if they appear.
  Thaw();

  AutomaticColumnWidth();
  ScrollLines(scrollPos);
  SetFocus();
}

static wxString NiceSizeFormat(u64 size)
{
  // Return a pretty filesize string from byte count.
  // e.g. 1134278 -> "1.08 MiB"

  const char* const unit_symbols[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB"};

  // Find largest power of 2 less than size.
  // div 10 to get largest named unit less than size
  // 10 == log2(1024) (number of B in a KiB, KiB in a MiB, etc)
  // Max value is 63 / 10 = 6
  const int unit = IntLog2(std::max<u64>(size, 1)) / 10;

  // Don't need exact values, only 5 most significant digits
  double unit_size = std::pow(2, unit * 10);
  return wxString::Format("%.2f %s", size / unit_size, unit_symbols[unit]);
}

// Update the column content of the item at index
void GameListCtrl::UpdateItemAtColumn(long index, int column)
{
  const auto& iso_file = *GetISO(GetItemData(index));

  switch (column)
  {
  case COLUMN_PLATFORM:
  {
    SetItemColumnImage(index, COLUMN_PLATFORM,
                       m_image_indexes.platform[static_cast<size_t>(iso_file.GetPlatform())]);
    break;
  }
  case COLUMN_BANNER:
  {
    int image_index = m_image_indexes.utility_banner[0];  // nobanner

    if (iso_file.GetBannerImage().IsOk())
    {
      wxImageList* img_list = GetImageList(wxIMAGE_LIST_SMALL);
      image_index = img_list->Add(
          WxUtils::ScaleImageToBitmap(iso_file.GetBannerImage(), this, img_list->GetSize()));
    }

    SetItemColumnImage(index, COLUMN_BANNER, image_index);
    break;
  }
  case COLUMN_TITLE:
  {
    wxString name = StrToWxStr(iso_file.GetName());
    int disc_number = iso_file.GetDiscNumber() + 1;

    if (disc_number > 1 &&
        name.Lower().find(wxString::Format("disc %i", disc_number)) == std::string::npos &&
        name.Lower().find(wxString::Format("disc%i", disc_number)) == std::string::npos)
    {
      name = wxString::Format(_("%s (Disc %i)"), name.c_str(), disc_number);
    }

    SetItem(index, COLUMN_TITLE, name, -1);
    break;
  }
  case COLUMN_MAKER:
    SetItem(index, COLUMN_MAKER, StrToWxStr(iso_file.GetCompany()), -1);
    break;
  case COLUMN_FILENAME:
    SetItem(index, COLUMN_FILENAME, wxFileNameFromPath(StrToWxStr(iso_file.GetFileName())), -1);
    break;
  case COLUMN_EMULATION_STATE:
    SetItemColumnImage(index, COLUMN_EMULATION_STATE,
                       m_image_indexes.emu_state[iso_file.GetEmuState()]);
    break;
  case COLUMN_COUNTRY:
    SetItemColumnImage(index, COLUMN_COUNTRY,
                       m_image_indexes.flag[static_cast<size_t>(iso_file.GetCountry())]);
    break;
  case COLUMN_SIZE:
    SetItem(index, COLUMN_SIZE, NiceSizeFormat(iso_file.GetFileSize()), -1);
    break;
  case COLUMN_ID:
    SetItem(index, COLUMN_ID, iso_file.GetGameID(), -1);
    break;
  }
}

void GameListCtrl::InsertItemInReportView(long index)
{
  // When using wxListCtrl, there is no hope of per-column text colors.
  // But for reference, here are the old colors that were used: (BGR)
  // title: 0xFF0000
  // company: 0x007030

  // Insert a first column (COLUMN_DUMMY) with nothing in it to use as the Index
  long item_index;
  {
    wxListItem li;
    li.SetId(index);
    li.SetData(index);
    li.SetMask(wxLIST_MASK_DATA);
    item_index = InsertItem(li);
  }

  // Iterate over all columns and fill them with content if they are visible
  for (int i = FIRST_COLUMN_WITH_CONTENT; i < NUMBER_OF_COLUMN; i++)
  {
    if (GetColumnWidth(i) != 0)
      UpdateItemAtColumn(item_index, i);
  }
}

static wxColour blend50(const wxColour& c1, const wxColour& c2)
{
  unsigned char r, g, b, a;
  r = c1.Red() / 2 + c2.Red() / 2;
  g = c1.Green() / 2 + c2.Green() / 2;
  b = c1.Blue() / 2 + c2.Blue() / 2;
  a = c1.Alpha() / 2 + c2.Alpha() / 2;
  return a << 24 | b << 16 | g << 8 | r;
}

static wxColour ContrastText(const wxColour& bgc)
{
  // Luminance threshold to determine whether to use black text on light background
  static constexpr int LUM_THRESHOLD = 186;
  int lum = 0.299 * bgc.Red() + 0.587 * bgc.Green() + 0.114 * bgc.Blue();
  return (lum > LUM_THRESHOLD) ? *wxBLACK : *wxWHITE;
}

void GameListCtrl::SetColors()
{
  for (long i = 0; i < GetItemCount(); i++)
  {
    wxColour color = (i & 1) ? blend50(wxSystemSettings::GetColour(wxSYS_COLOUR_3DLIGHT),
                                       wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW)) :
                               wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
    SetItemBackgroundColour(i, color);
    SetItemTextColour(i, ContrastText(color));
  }
}

void GameListCtrl::DoState(PointerWrap* p, u32 size)
{
  struct
  {
    u32 Revision;
    u32 ExpectedSize;
  } header = {CACHE_REVISION, size};
  p->Do(header);
  if (p->GetMode() == PointerWrap::MODE_READ)
  {
    if (header.Revision != CACHE_REVISION || header.ExpectedSize != size)
    {
      p->SetMode(PointerWrap::MODE_MEASURE);
      return;
    }
  }
  p->DoEachElement(m_cached_files, [](PointerWrap& state, std::shared_ptr<GameListItem>& elem) {
    if (state.GetMode() == PointerWrap::MODE_READ)
    {
      elem = std::make_shared<GameListItem>();
    }
    elem->DoState(state);
  });
}

bool GameListCtrl::SyncCacheFile(bool write)
{
  std::string filename(File::GetUserPath(D_CACHE_IDX) + "wx_gamelist.cache");
  const char* open_mode = write ? "wb" : "rb";
  File::IOFile f(filename, open_mode);
  if (!f)
    return false;
  bool success = false;
  if (write)
  {
    // Measure the size of the buffer.
    u8* ptr = nullptr;
    PointerWrap p(&ptr, PointerWrap::MODE_MEASURE);
    DoState(&p);
    const size_t buffer_size = reinterpret_cast<size_t>(ptr);

    // Then actually do the write.
    std::vector<u8> buffer(buffer_size);
    ptr = &buffer[0];
    p.SetMode(PointerWrap::MODE_WRITE);
    DoState(&p, buffer_size);
    if (f.WriteBytes(buffer.data(), buffer.size()))
      success = true;
  }
  else
  {
    std::vector<u8> buffer(f.GetSize());
    if (buffer.size() && f.ReadBytes(buffer.data(), buffer.size()))
    {
      u8* ptr = buffer.data();
      PointerWrap p(&ptr, PointerWrap::MODE_READ);
      DoState(&p, buffer.size());
      if (p.GetMode() == PointerWrap::MODE_READ)
        success = true;
    }
  }
  if (!success)
  {
    // If some file operation failed, try to delete the probably-corrupted cache
    f.Close();
    File::Delete(filename);
  }
  return success;
}

void GameListCtrl::RescanList()
{
  auto post_status = [&](const wxString& status) {
    auto event = new wxCommandEvent(wxEVT_HOST_COMMAND, IDM_UPDATE_STATUS_BAR);
    event->SetInt(0);
    event->SetString(status);
    QueueEvent(event);
  };

  post_status(_("Scanning..."));

  const std::vector<std::string> search_extensions = {".gcm",  ".tgc", ".iso", ".ciso", ".gcz",
                                                      ".wbfs", ".wad", ".dol", ".elf"};
  // TODO This could process paths iteratively as they are found
  auto search_results = Common::DoFileSearch(SConfig::GetInstance().m_ISOFolder, search_extensions,
                                             SConfig::GetInstance().m_RecursiveISOFolder);

  // TODO Prevent DoFileSearch from looking inside /files/ directories of DirectoryBlobs at all?
  // TODO Make DoFileSearch support filter predicates so we don't have remove things afterwards?
  search_results.erase(
      std::remove_if(search_results.begin(), search_results.end(), DiscIO::ShouldHideFromGameList),
      search_results.end());

  std::vector<std::string> cached_paths;
  for (const auto& file : m_cached_files)
    cached_paths.emplace_back(file->GetFileName());
  std::sort(cached_paths.begin(), cached_paths.end());

  std::list<std::string> removed_paths;
  std::set_difference(cached_paths.cbegin(), cached_paths.cend(), search_results.cbegin(),
                      search_results.cend(), std::back_inserter(removed_paths));

  std::vector<std::string> new_paths;
  std::set_difference(search_results.cbegin(), search_results.cend(), cached_paths.cbegin(),
                      cached_paths.cend(), std::back_inserter(new_paths));

  // Reload the TitleDatabase
  {
    std::unique_lock<std::mutex> lk(m_title_database_mutex);
    m_title_database = {};
  }

  // For now, only scan new_paths. This could cause false negatives (file actively being written),
  // but otherwise should be fine.
  bool cache_changed = false;
  {
    std::unique_lock<std::mutex> lk(m_cache_mutex);
    for (const auto& path : removed_paths)
    {
      auto it = std::find_if(m_cached_files.cbegin(), m_cached_files.cend(),
                             [&path](const std::shared_ptr<GameListItem>& file) {
                               return file->GetFileName() == path;
                             });
      if (it != m_cached_files.cend())
      {
        cache_changed = true;
        m_cached_files.erase(it);
      }
    }
    for (const auto& path : new_paths)
    {
      auto file = std::make_shared<GameListItem>(path);
      if (file->IsValid())
      {
        cache_changed = true;
        m_cached_files.push_back(std::move(file));
      }
    }
  }
  // The common case is that just a file has been added/removed, so trigger a refresh ASAP with the
  // assumption that other properties of files will not change at the same time (which will be fine
  // and just causes a double refresh).
  if (cache_changed)
    QueueEvent(new wxCommandEvent(DOLPHIN_EVT_REFRESH_GAMELIST));

  // If any cached files need updates, apply the updates to a copy and delete the original - this
  // makes the UI thread's use of cached files safe. Note however, it is assumed that RefreshList
  // will not iterate m_cached_files while the scan thread is modifying the list itself.
  bool refresh_needed = false;
  {
    std::unique_lock<std::mutex> lk(m_cache_mutex);
    for (auto& file : m_cached_files)
    {
      bool emu_state_changed = file->EmuStateChanged();
      bool banner_changed = file->BannerChanged();
      bool custom_title_changed = file->CustomNameChanged(m_title_database);
      if (emu_state_changed || banner_changed || custom_title_changed)
      {
        cache_changed = refresh_needed = true;
        auto copy = std::make_shared<GameListItem>(*file);
        if (emu_state_changed)
          copy->EmuStateCommit();
        if (banner_changed)
          copy->BannerCommit();
        if (custom_title_changed)
          copy->CustomNameCommit();
        file = std::move(copy);
      }
    }
  }
  // Only post UI event to update the displayed list if something actually changed
  if (refresh_needed)
    QueueEvent(new wxCommandEvent(DOLPHIN_EVT_REFRESH_GAMELIST));

  post_status("");

  if (cache_changed)
    SyncCacheFile(true);
}

void GameListCtrl::OnRefreshGameList(wxCommandEvent& WXUNUSED(event))
{
  RefreshList();
}

void GameListCtrl::OnRescanGameList(wxCommandEvent& event)
{
  if (event.GetInt())
  {
    // Knock out the cache on a purge event
    std::unique_lock<std::mutex> lk(m_cache_mutex);
    m_cached_files.clear();
  }
  m_scan_trigger.Set();
}

void GameListCtrl::OnLocalIniModified(wxCommandEvent& ev)
{
  ev.Skip();
  // We need show any changes to the ini which could impact our columns. Currently only the
  // EmuState/Issues settings can do that. We also need to persist the changes to the cache - so
  // just trigger a rescan which will sync the cache and then display the new values.
  m_scan_trigger.Set();
}

void GameListCtrl::OnColBeginDrag(wxListEvent& event)
{
  const int column_id = event.GetColumn();

  if (column_id != COLUMN_TITLE && column_id != COLUMN_MAKER && column_id != COLUMN_FILENAME)
    event.Veto();
}

const GameListItem* GameListCtrl::GetISO(size_t index) const
{
  if (index < m_shown_files.size())
    return m_shown_files[index].get();

  return nullptr;
}

static GameListCtrl* caller;
static int wxCALLBACK wxListCompare(wxIntPtr item1, wxIntPtr item2, wxIntPtr sortData)
{
  // return 1 if item1 > item2
  // return -1 if item1 < item2
  // return 0 for identity
  const GameListItem* iso1 = caller->GetISO(item1);
  const GameListItem* iso2 = caller->GetISO(item2);

  if (iso1 == iso2)
    return 0;

  return CompareGameListItems(iso1, iso2, sortData);
}

void GameListCtrl::OnColumnClick(wxListEvent& event)
{
  if (event.GetColumn() != COLUMN_BANNER)
  {
    int current_column = event.GetColumn();
    if (sorted)
    {
      if (m_last_column == current_column)
      {
        m_last_sort = -m_last_sort;
      }
      else
      {
        SConfig::GetInstance().m_ListSort2 = m_last_sort;
        m_last_column = current_column;
        m_last_sort = current_column;
      }
      SConfig::GetInstance().m_ListSort = m_last_sort;
    }
    else
    {
      m_last_sort = current_column;
      m_last_column = current_column;
    }
    caller = this;
    SortItems(wxListCompare, m_last_sort);
  }

  SetColors();

  event.Skip();
}

// This is used by keyboard gamelist search
void GameListCtrl::OnKeyPress(wxListEvent& event)
{
  static int lastKey = 0, sLoop = 0;
  int Loop = 0;

  for (int i = 0; i < (int)m_shown_files.size(); i++)
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
        if (i + 1 == (int)m_shown_files.size())
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
      SetItemState(i, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
                   wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
      EnsureVisible(i);
      break;
    }

    // If we get past the last game in the list,
    // we'll have to go back to the first one.
    if (i + 1 == (int)m_shown_files.size() && sLoop > 0 && Loop > 0)
      i = -1;
  }

  event.Skip();
}

// This shows a little tooltip with the current Game's emulation state
void GameListCtrl::OnMouseMotion(wxMouseEvent& event)
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
      if (m_tooltip || lastItem == item || this != FindFocus())
      {
        if (lastItem != item)
          lastItem = -1;
        event.Skip();
        return;
      }

      // Emulation status
      static const char* const emuState[] = {"Broken", "Intro", "In-Game", "Playable", "Perfect"};

      const GameListItem* iso = GetISO(GetItemData(item));

      const int emu_state = iso->GetEmuState();
      const std::string& issues = iso->GetIssues();

      // Show a tooltip containing the EmuState and the state description
      if (emu_state > 0 && emu_state < 6)
      {
        char temp[2048];
        sprintf(temp, "^ %s%s%s", emuState[emu_state - 1], issues.size() > 0 ? " :\n" : "",
                issues.c_str());
        m_tooltip = new wxEmuStateTip(this, StrToWxStr(temp), &m_tooltip);
      }
      else
      {
        m_tooltip = new wxEmuStateTip(this, _("Not Set"), &m_tooltip);
      }

      // Get item Coords
      GetItemRect(item, Rect);
      int mx = Rect.GetWidth();
      int my = Rect.GetY();
#if !defined(__WXMSW__) && !defined(__WXOSX__)
      // For some reason the y position does not account for the header
      // row, so subtract the y position of the first visible item.
      GetItemRect(GetTopItem(), Rect);
      my -= Rect.GetY();
#endif
      // Convert to screen coordinates
      ClientToScreen(&mx, &my);
      m_tooltip->SetBoundingRect(wxRect(mx - GetColumnWidth(COLUMN_EMULATION_STATE), my,
                                        GetColumnWidth(COLUMN_EMULATION_STATE), Rect.GetHeight()));
      m_tooltip->SetPosition(
          wxPoint(mx - GetColumnWidth(COLUMN_EMULATION_STATE), my - 5 + Rect.GetHeight()));
      lastItem = item;
    }
  }
  if (!m_tooltip)
    lastItem = -1;

  event.Skip();
}

void GameListCtrl::OnLeftClick(wxMouseEvent& event)
{
  // Focus the clicked item.
  int flags;
  long item = HitTest(event.GetPosition(), flags);
  if ((item != wxNOT_FOUND) && (GetSelectedItemCount() == 0) && (!event.ControlDown()) &&
      (!event.ShiftDown()))
  {
    SetItemState(item, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
    SetItemState(item, wxLIST_STATE_FOCUSED, wxLIST_STATE_FOCUSED);
    wxGetApp().GetCFrame()->UpdateGUI();
  }

  event.Skip();
}

static bool IsWADInstalled(const GameListItem& wad)
{
  const std::string content_dir =
      Common::GetTitleContentPath(wad.GetTitleID(), Common::FromWhichRoot::FROM_CONFIGURED_ROOT);

  if (!File::IsDirectory(content_dir))
    return false;

  // Since this isn't IOS and we only need a simple way to figure out if a title is installed,
  // we make the (reasonable) assumption that having more than just the TMD in the content
  // directory means that the title is installed.
  const auto entries = File::ScanDirectoryTree(content_dir, false);
  return std::any_of(entries.children.begin(), entries.children.end(),
                     [](const auto& file) { return file.virtualName != "title.tmd"; });
}

void GameListCtrl::OnRightClick(wxMouseEvent& event)
{
  // Focus the clicked item.
  int flags;
  long item = HitTest(event.GetPosition(), flags);
  if (item != wxNOT_FOUND)
  {
    if (GetItemState(item, wxLIST_STATE_SELECTED) != wxLIST_STATE_SELECTED)
    {
      UnselectAll();
      SetItemState(item, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
    }
    SetItemState(item, wxLIST_STATE_FOCUSED, wxLIST_STATE_FOCUSED);
  }
  if (GetSelectedItemCount() == 1)
  {
    const GameListItem* selected_iso = GetSelectedISO();
    if (selected_iso)
    {
      wxMenu popupMenu;
      DiscIO::Platform platform = selected_iso->GetPlatform();

      if (platform != DiscIO::Platform::ELF_DOL)
      {
        popupMenu.Append(IDM_PROPERTIES, _("&Properties"));
        popupMenu.Append(IDM_GAME_WIKI, _("&Wiki"));
        popupMenu.AppendSeparator();
      }
      if (platform == DiscIO::Platform::WII_DISC || platform == DiscIO::Platform::WII_WAD)
      {
        auto* const open_save_folder_item =
            popupMenu.Append(IDM_OPEN_SAVE_FOLDER, _("Open Wii &save folder"));
        auto* const export_save_item =
            popupMenu.Append(IDM_EXPORT_SAVE, _("Export Wii save (Experimental)"));

        // We should not allow the user to mess with the save folder or export saves while
        // emulation is running, because this could result in the exported save being in
        // an inconsistent state; the emulated software can do *anything* to its data directory,
        // and we definitely do not want the user to touch anything in there if it's running.
        for (auto* menu_item : {open_save_folder_item, export_save_item})
        {
          menu_item->Enable((!Core::IsRunning() || !SConfig::GetInstance().bWii) &&
                            File::IsDirectory(selected_iso->GetWiiFSPath()));
        }
      }
      popupMenu.Append(IDM_OPEN_CONTAINING_FOLDER, _("Open &containing folder"));

      if (platform != DiscIO::Platform::ELF_DOL)
        popupMenu.AppendCheckItem(IDM_SET_DEFAULT_ISO, _("Set as &default ISO"));

      // First we have to decide a starting value when we append it
      if (selected_iso->GetFileName() == SConfig::GetInstance().m_strDefaultISO)
        popupMenu.FindItem(IDM_SET_DEFAULT_ISO)->Check();

      popupMenu.AppendSeparator();
      popupMenu.Append(IDM_DELETE_ISO, _("&Delete File..."));

      if (platform == DiscIO::Platform::GAMECUBE_DISC || platform == DiscIO::Platform::WII_DISC)
      {
        if (selected_iso->GetBlobType() == DiscIO::BlobType::GCZ)
          popupMenu.Append(IDM_COMPRESS_ISO, _("Decompress ISO..."));
        else if (selected_iso->GetBlobType() == DiscIO::BlobType::PLAIN)
          popupMenu.Append(IDM_COMPRESS_ISO, _("Compress ISO..."));

        wxMenuItem* changeDiscItem = popupMenu.Append(IDM_LIST_CHANGE_DISC, _("Change &Disc"));
        changeDiscItem->Enable(Core::IsRunning());
      }

      if (platform == DiscIO::Platform::WII_DISC)
      {
        auto* const perform_update_item =
            popupMenu.Append(IDM_LIST_PERFORM_DISC_UPDATE, _("Perform System Update"));
        perform_update_item->Enable(!Core::IsRunning() || !SConfig::GetInstance().bWii);
      }

      if (platform == DiscIO::Platform::WII_WAD)
      {
        auto* const install_wad_item =
            popupMenu.Append(IDM_LIST_INSTALL_WAD, _("Install to the NAND"));
        auto* const uninstall_wad_item =
            popupMenu.Append(IDM_LIST_UNINSTALL_WAD, _("Uninstall from the NAND"));
        // These should not be allowed while emulation is running for safety reasons.
        for (auto* menu_item : {install_wad_item, uninstall_wad_item})
          menu_item->Enable(!Core::IsRunning() || !SConfig::GetInstance().bWii);

        if (!IsWADInstalled(*selected_iso))
          uninstall_wad_item->Enable(false);
      }

      popupMenu.Append(IDM_START_NETPLAY, _("Host with Netplay"));

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

const GameListItem* GameListCtrl::GetSelectedISO() const
{
  if (m_shown_files.empty())
    return nullptr;

  if (GetSelectedItemCount() == 0)
    return nullptr;

  long item = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
  if (item == wxNOT_FOUND)
    return nullptr;

  return GetISO(GetItemData(item));
}

std::vector<const GameListItem*> GameListCtrl::GetAllSelectedISOs() const
{
  std::vector<const GameListItem*> result;
  long item = -1;
  while (true)
  {
    item = GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (item == wxNOT_FOUND)
      return result;
    result.push_back(GetISO(GetItemData(item)));
  }
}

bool GameListCtrl::IsHidingItems()
{
  return !(SConfig::GetInstance().m_ListGC && SConfig::GetInstance().m_ListWii &&
           SConfig::GetInstance().m_ListWad && SConfig::GetInstance().m_ListElfDol &&
           SConfig::GetInstance().m_ListJap && SConfig::GetInstance().m_ListUsa &&
           SConfig::GetInstance().m_ListPal && SConfig::GetInstance().m_ListAustralia &&
           SConfig::GetInstance().m_ListFrance && SConfig::GetInstance().m_ListGermany &&
           SConfig::GetInstance().m_ListItaly && SConfig::GetInstance().m_ListKorea &&
           SConfig::GetInstance().m_ListNetherlands && SConfig::GetInstance().m_ListRussia &&
           SConfig::GetInstance().m_ListSpain && SConfig::GetInstance().m_ListTaiwan &&
           SConfig::GetInstance().m_ListWorld && SConfig::GetInstance().m_ListUnknown);
}

void GameListCtrl::OnOpenContainingFolder(wxCommandEvent& WXUNUSED(event))
{
  const GameListItem* iso = GetSelectedISO();
  if (!iso)
    return;

  wxFileName path = wxFileName::FileName(StrToWxStr(iso->GetFileName()));
  path.MakeAbsolute();
  WxUtils::Explore(WxStrToStr(path.GetPath()));
}

void GameListCtrl::OnOpenSaveFolder(wxCommandEvent& WXUNUSED(event))
{
  const GameListItem* iso = GetSelectedISO();
  if (!iso)
    return;
  std::string path = iso->GetWiiFSPath();
  if (!path.empty())
    WxUtils::Explore(path);
}

void GameListCtrl::OnExportSave(wxCommandEvent& WXUNUSED(event))
{
  const GameListItem* iso = GetSelectedISO();
  if (iso)
    CWiiSaveCrypted::ExportWiiSave(iso->GetTitleID());
}

// Save this file as the default file
void GameListCtrl::OnSetDefaultISO(wxCommandEvent& event)
{
  const GameListItem* iso = GetSelectedISO();
  if (!iso)
    return;

  if (event.IsChecked())
  {
    // Write the new default value and save it the ini file
    SConfig::GetInstance().m_strDefaultISO = iso->GetFileName();
    SConfig::GetInstance().SaveSettings();
  }
  else
  {
    // Otherwise blank the value and save it
    SConfig::GetInstance().m_strDefaultISO = "";
    SConfig::GetInstance().SaveSettings();
  }
}

void GameListCtrl::OnDeleteISO(wxCommandEvent& WXUNUSED(event))
{
  const wxString message =
      GetSelectedItemCount() == 1 ?
          _("Are you sure you want to delete this file? It will be gone forever!") :
          _("Are you sure you want to delete these files? They will be gone forever!");

  if (wxMessageBox(message, _("Warning"), wxYES_NO | wxICON_EXCLAMATION) == wxYES)
  {
    for (const GameListItem* iso : GetAllSelectedISOs())
      File::Delete(iso->GetFileName());
    m_scan_trigger.Set();
  }
}

void GameListCtrl::OnProperties(wxCommandEvent& WXUNUSED(event))
{
  const GameListItem* iso = GetSelectedISO();
  if (!iso)
    return;

  CISOProperties* ISOProperties = new CISOProperties(*iso, this);
  ISOProperties->Show();
}

void GameListCtrl::OnWiki(wxCommandEvent& WXUNUSED(event))
{
  const GameListItem* iso = GetSelectedISO();
  if (!iso)
    return;

  std::string wikiUrl =
      "https://wiki.dolphin-emu.org/dolphin-redirect.php?gameid=" + iso->GetGameID();
  WxUtils::Launch(wikiUrl);
}

void GameListCtrl::OnNetPlayHost(wxCommandEvent& WXUNUSED(event))
{
  const GameListItem* iso = GetSelectedISO();
  if (!iso)
    return;

  NetPlayHostConfig config;
  config.FromConfig();
  config.game_name = iso->GetUniqueIdentifier();
  config.game_list_ctrl = this;
  config.SetDialogInfo(m_parent);

  Config::SetBaseOrCurrent(Config::NETPLAY_SELECTED_HOST_GAME, config.game_name);
  NetPlayLauncher::Host(config);
}

bool GameListCtrl::MultiCompressCB(const std::string& text, float percent, void* arg)
{
  CompressionProgress* progress = static_cast<CompressionProgress*>(arg);

  float total_percent = ((float)progress->items_done + percent) / (float)progress->items_total;
  wxString text_string(
      StrToWxStr(StringFromFormat("%s (%i/%i) - %s", progress->current_filename.c_str(),
                                  progress->items_done + 1, progress->items_total, text.c_str())));

  return progress->dialog->Update(total_percent * progress->dialog->GetRange(), text_string);
}

void GameListCtrl::OnMultiCompressISO(wxCommandEvent& /*event*/)
{
  CompressSelection(true);
}

void GameListCtrl::OnMultiDecompressISO(wxCommandEvent& /*event*/)
{
  CompressSelection(false);
}

void GameListCtrl::CompressSelection(bool _compress)
{
  std::vector<const GameListItem*> items_to_compress;
  bool wii_compression_warning_accepted = false;
  for (const GameListItem* iso : GetAllSelectedISOs())
  {
    // Don't include items that we can't do anything with
    if (iso->GetPlatform() != DiscIO::Platform::GAMECUBE_DISC &&
        iso->GetPlatform() != DiscIO::Platform::WII_DISC)
      continue;
    if (iso->GetBlobType() != DiscIO::BlobType::PLAIN &&
        iso->GetBlobType() != DiscIO::BlobType::GCZ)
      continue;

    items_to_compress.push_back(iso);

    // Show the Wii compression warning if it's relevant and it hasn't been shown already
    if (!wii_compression_warning_accepted && _compress &&
        iso->GetBlobType() != DiscIO::BlobType::GCZ &&
        iso->GetPlatform() == DiscIO::Platform::WII_DISC)
    {
      if (WiiCompressWarning())
        wii_compression_warning_accepted = true;
      else
        return;
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
        _compress ? _("Compressing ISO") : _("Decompressing ISO"), _("Working..."),
        1000,  // Arbitrary number that's larger than the dialog's width in pixels
        this, wxPD_APP_MODAL | wxPD_CAN_ABORT | wxPD_ELAPSED_TIME | wxPD_ESTIMATED_TIME |
                  wxPD_REMAINING_TIME | wxPD_SMOOTH);

    CompressionProgress progress(0, items_to_compress.size(), "", &progressDialog);

    for (const GameListItem* iso : items_to_compress)
    {
      if (iso->GetBlobType() != DiscIO::BlobType::GCZ && _compress)
      {
        std::string FileName;
        SplitPath(iso->GetFileName(), nullptr, &FileName, nullptr);
        progress.current_filename = FileName;
        FileName.append(".gcz");

        std::string OutputFileName;
        BuildCompleteFilename(OutputFileName, WxStrToStr(browseDialog.GetPath()), FileName);

        if (File::Exists(OutputFileName) &&
            wxMessageBox(
                wxString::Format(_("The file %s already exists.\nDo you wish to replace it?"),
                                 StrToWxStr(OutputFileName)),
                _("Confirm File Overwrite"), wxYES_NO) == wxNO)
          continue;

        all_good &=
            DiscIO::CompressFileToBlob(iso->GetFileName(), OutputFileName,
                                       (iso->GetPlatform() == DiscIO::Platform::WII_DISC) ? 1 : 0,
                                       16384, &MultiCompressCB, &progress);
      }
      else if (iso->GetBlobType() == DiscIO::BlobType::GCZ && !_compress)
      {
        std::string FileName;
        SplitPath(iso->GetFileName(), nullptr, &FileName, nullptr);
        progress.current_filename = FileName;
        if (iso->GetPlatform() == DiscIO::Platform::WII_DISC)
          FileName.append(".iso");
        else
          FileName.append(".gcm");

        std::string OutputFileName;
        BuildCompleteFilename(OutputFileName, WxStrToStr(browseDialog.GetPath()), FileName);

        if (File::Exists(OutputFileName) &&
            wxMessageBox(
                wxString::Format(_("The file %s already exists.\nDo you wish to replace it?"),
                                 StrToWxStr(OutputFileName)),
                _("Confirm File Overwrite"), wxYES_NO) == wxNO)
          continue;

        all_good &= DiscIO::DecompressBlobToFile(iso->GetFileName().c_str(), OutputFileName.c_str(),
                                                 &MultiCompressCB, &progress);
      }

      progress.items_done++;
    }
  }

  if (!all_good)
    WxUtils::ShowErrorDialog(_("Dolphin was unable to complete the requested action."));

  m_scan_trigger.Set();
}

bool GameListCtrl::CompressCB(const std::string& text, float percent, void* arg)
{
  return ((wxProgressDialog*)arg)->Update((int)(percent * 1000), StrToWxStr(text));
}

void GameListCtrl::OnCompressISO(wxCommandEvent& WXUNUSED(event))
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
      if (iso->GetPlatform() == DiscIO::Platform::WII_DISC)
        FileType = _("All Wii ISO files (iso)") + "|*.iso";
      else
        FileType = _("All GameCube GCM files (gcm)") + "|*.gcm";

      path = wxFileSelector(_("Save decompressed GCM/ISO"), StrToWxStr(FilePath),
                            StrToWxStr(FileName) + FileType.After('*'), wxEmptyString,
                            FileType + "|" + wxGetTranslation(wxALL_FILES), wxFD_SAVE, this);
    }
    else
    {
      if (iso->GetPlatform() == DiscIO::Platform::WII_DISC && !WiiCompressWarning())
        return;

      path = wxFileSelector(_("Save compressed GCM/ISO"), StrToWxStr(FilePath),
                            StrToWxStr(FileName) + ".gcz", wxEmptyString,
                            _("All compressed GC/Wii ISO files (gcz)") +
                                wxString::Format("|*.gcz|%s", wxGetTranslation(wxALL_FILES)),
                            wxFD_SAVE, this);
    }
    if (!path)
      return;
  } while (
      wxFileExists(path) &&
      wxMessageBox(wxString::Format(_("The file %s already exists.\nDo you wish to replace it?"),
                                    path.c_str()),
                   _("Confirm File Overwrite"), wxYES_NO) == wxNO);

  bool all_good = false;

  {
    wxProgressDialog dialog(is_compressed ? _("Decompressing ISO") : _("Compressing ISO"),
                            _("Working..."), 1000, this,
                            wxPD_APP_MODAL | wxPD_CAN_ABORT | wxPD_ELAPSED_TIME |
                                wxPD_ESTIMATED_TIME | wxPD_REMAINING_TIME | wxPD_SMOOTH);

    if (is_compressed)
      all_good =
          DiscIO::DecompressBlobToFile(iso->GetFileName(), WxStrToStr(path), &CompressCB, &dialog);
    else
      all_good = DiscIO::CompressFileToBlob(
          iso->GetFileName(), WxStrToStr(path),
          (iso->GetPlatform() == DiscIO::Platform::WII_DISC) ? 1 : 0, 16384, &CompressCB, &dialog);
  }

  if (!all_good)
    WxUtils::ShowErrorDialog(_("Dolphin was unable to complete the requested action."));

  m_scan_trigger.Set();
}

void GameListCtrl::OnChangeDisc(wxCommandEvent& WXUNUSED(event))
{
  const GameListItem* iso = GetSelectedISO();
  if (!iso || !Core::IsRunning())
    return;
  DVDInterface::ChangeDiscAsHost(WxStrToStr(iso->GetFileName()));
}

void GameListCtrl::OnSize(wxSizeEvent& event)
{
  event.Skip();
  if (m_lastpos == event.GetSize())
    return;

  m_lastpos = event.GetSize();
  AutomaticColumnWidth();
}

void GameListCtrl::AutomaticColumnWidth()
{
  wxRect rc(GetClientRect());

  Freeze();
  if (GetColumnCount() == 1)
  {
    SetColumnWidth(0, rc.GetWidth());
  }
  else if (GetColumnCount() > 0)
  {
    int remaining_width = rc.GetWidth();
    std::vector<int> visible_columns;

    for (const auto& c : m_columns)
    {
      if (c.visible)
      {
        if (c.resizable)
          visible_columns.push_back(c.id);
        else
          remaining_width -= GetColumnWidth(c.id);
      }
    }

    if (visible_columns.empty())
      visible_columns.push_back(COLUMN_DUMMY);

    for (const int column : visible_columns)
      SetColumnWidth(column, static_cast<int>(remaining_width / visible_columns.size()));
  }
  Thaw();
}

void GameListCtrl::UnselectAll()
{
  for (int i = 0; i < GetItemCount(); i++)
  {
    SetItemState(i, 0, wxLIST_STATE_SELECTED);
  }
}
bool GameListCtrl::WiiCompressWarning()
{
  return wxMessageBox(_("Compressing a Wii disc image will irreversibly change the compressed copy "
                        "by removing padding data. Your disc image will still work. Continue?"),
                      _("Warning"), wxYES_NO) == wxYES;
}

#ifdef __WXMSW__
// Windows draws vertical rules between columns when using UXTheme (e.g. Aero, Win10)
// This function paints over those lines which removes them.
// [The repaint background idea is ripped off from Eclipse SWT which does the same thing]
bool GameListCtrl::MSWOnNotify(int id, WXLPARAM lparam, WXLPARAM* result)
{
  NMLVCUSTOMDRAW* nmlv = reinterpret_cast<NMLVCUSTOMDRAW*>(lparam);
  // Intercept the NM_CUSTOMDRAW[CDDS_PREPAINT]
  // This event occurs after the background has been painted before the content of the list
  // is painted. We can repaint the background to eliminate the column lines here.
  if (nmlv->nmcd.hdr.hwndFrom == GetHWND() && nmlv->nmcd.hdr.code == NM_CUSTOMDRAW &&
      nmlv->nmcd.dwDrawStage == CDDS_PREPAINT)
  {
    // The column separators have already been painted, paint over them.
    wxDCTemp dc(nmlv->nmcd.hdc);
    dc.SetBrush(GetBackgroundColour());
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.DrawRectangle(nmlv->nmcd.rc.left, nmlv->nmcd.rc.top,
                     nmlv->nmcd.rc.right - nmlv->nmcd.rc.left,
                     nmlv->nmcd.rc.bottom - nmlv->nmcd.rc.top);
  }

  // Defer to wxWidgets for normal processing.
  return wxListCtrl::MSWOnNotify(id, lparam, result);
}
#endif
