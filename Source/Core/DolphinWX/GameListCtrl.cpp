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
#include "Core/Boot/Boot.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/DVDInterface.h"
#include "Core/HW/WiiSaveCrypted.h"
#include "Core/Movie.h"
#include "DiscIO/Blob.h"
#include "DiscIO/Enums.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeCreator.h"
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
    }
    return strcasecmp(iso1->GetName().c_str(), iso2->GetName().c_str()) * t;
  case CGameListCtrl::COLUMN_MAKER:
    return strcasecmp(iso1->GetCompany().c_str(), iso2->GetCompany().c_str()) * t;
  case CGameListCtrl::COLUMN_FILENAME:
    return wxStricmp(wxFileNameFromPath(iso1->GetFileName()),
                     wxFileNameFromPath(iso2->GetFileName())) *
           t;
  case CGameListCtrl::COLUMN_ID:
    return strcasecmp(iso1->GetGameID().c_str(), iso2->GetGameID().c_str()) * t;
  case CGameListCtrl::COLUMN_COUNTRY:
    if (iso1->GetCountry() > iso2->GetCountry())
      return 1 * t;
    if (iso1->GetCountry() < iso2->GetCountry())
      return -1 * t;
    return 0;
  case CGameListCtrl::COLUMN_SIZE:
    if (iso1->GetFileSize() > iso2->GetFileSize())
      return 1 * t;
    if (iso1->GetFileSize() < iso2->GetFileSize())
      return -1 * t;
    return 0;
  case CGameListCtrl::COLUMN_PLATFORM:
    if (iso1->GetPlatform() > iso2->GetPlatform())
      return 1 * t;
    if (iso1->GetPlatform() < iso2->GetPlatform())
      return -1 * t;
    return 0;

  case CGameListCtrl::COLUMN_EMULATION_STATE:
  {
    const int nState1 = iso1->GetEmuState(), nState2 = iso2->GetEmuState();

    if (nState1 > nState2)
      return 1 * t;
    if (nState1 < nState2)
      return -1 * t;
    else
      return 0;
  }
  break;
  }

  return 0;
}

static std::unordered_map<std::string, std::string> LoadCustomTitles()
{
  // Load custom game titles from titles.txt
  // http://www.gametdb.com/Wii/Downloads
  const std::string& load_directory = File::GetUserPath(D_LOAD_IDX);

  std::ifstream titlestxt;
  OpenFStream(titlestxt, load_directory + "titles.txt", std::ios::in);

  if (!titlestxt.is_open())
    OpenFStream(titlestxt, load_directory + "wiitdb.txt", std::ios::in);

  if (!titlestxt.is_open())
    return {};

  std::unordered_map<std::string, std::string> custom_titles;

  std::string line;
  while (!titlestxt.eof() && std::getline(titlestxt, line))
  {
    const size_t equals_index = line.find('=');
    if (equals_index != std::string::npos)
    {
      custom_titles.emplace(StripSpaces(line.substr(0, equals_index)),
                            StripSpaces(line.substr(equals_index + 1)));
    }
  }

  return custom_titles;
}

static std::vector<std::string> GetFileSearchExtensions()
{
  std::vector<std::string> extensions;

  if (SConfig::GetInstance().m_ListGC)
  {
    extensions.push_back(".gcm");
    extensions.push_back(".tgc");
  }

  if (SConfig::GetInstance().m_ListWii || SConfig::GetInstance().m_ListGC)
  {
    extensions.push_back(".iso");
    extensions.push_back(".ciso");
    extensions.push_back(".gcz");
    extensions.push_back(".wbfs");
  }

  if (SConfig::GetInstance().m_ListWad)
    extensions.push_back(".wad");

  if (SConfig::GetInstance().m_ListElfDol)
  {
    extensions.push_back(".dol");
    extensions.push_back(".elf");
  }

  return extensions;
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

wxDEFINE_EVENT(DOLPHIN_EVT_RELOAD_GAMELIST, wxCommandEvent);

CGameListCtrl::CGameListCtrl(wxWindow* parent, const wxWindowID id, const wxPoint& pos,
                             const wxSize& size, long style)
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
  Bind(wxEVT_MENU, &CGameListCtrl::OnNetPlayHost, this, IDM_START_NETPLAY);

  Bind(DOLPHIN_EVT_RELOAD_GAMELIST, &CGameListCtrl::OnReloadGameList, this);

  wxTheApp->Bind(DOLPHIN_EVT_LOCAL_INI_CHANGED, &CGameListCtrl::OnLocalIniModified, this);
}

CGameListCtrl::~CGameListCtrl()
{
}

template <typename T>
static void InitBitmap(wxImageList* img_list, std::vector<int>* vector, wxWindow* context,
                       const wxSize& usable_size, T index, const std::string& name)
{
  wxSize size = img_list->GetSize();
  (*vector)[static_cast<size_t>(index)] = img_list->Add(WxUtils::LoadScaledResourceBitmap(
      name, context, size, usable_size, WxUtils::LSI_SCALE | WxUtils::LSI_ALIGN_VCENTER));
}

void CGameListCtrl::InitBitmaps()
{
  const wxSize size = FromDIP(wxSize(96, 32));
  const wxSize flag_bmp_size = FromDIP(wxSize(32, 32));
  const wxSize platform_bmp_size = flag_bmp_size;
  const wxSize rating_bmp_size = FromDIP(wxSize(48, 32));
  wxImageList* img_list = new wxImageList(size.GetWidth(), size.GetHeight());
  AssignImageList(img_list, wxIMAGE_LIST_SMALL);

  m_FlagImageIndex.resize(static_cast<size_t>(DiscIO::Country::NUMBER_OF_COUNTRIES));
  InitBitmap(img_list, &m_FlagImageIndex, this, flag_bmp_size, DiscIO::Country::COUNTRY_JAPAN,
             "Flag_Japan");
  InitBitmap(img_list, &m_FlagImageIndex, this, flag_bmp_size, DiscIO::Country::COUNTRY_EUROPE,
             "Flag_Europe");
  InitBitmap(img_list, &m_FlagImageIndex, this, flag_bmp_size, DiscIO::Country::COUNTRY_USA,
             "Flag_USA");
  InitBitmap(img_list, &m_FlagImageIndex, this, flag_bmp_size, DiscIO::Country::COUNTRY_AUSTRALIA,
             "Flag_Australia");
  InitBitmap(img_list, &m_FlagImageIndex, this, flag_bmp_size, DiscIO::Country::COUNTRY_FRANCE,
             "Flag_France");
  InitBitmap(img_list, &m_FlagImageIndex, this, flag_bmp_size, DiscIO::Country::COUNTRY_GERMANY,
             "Flag_Germany");
  InitBitmap(img_list, &m_FlagImageIndex, this, flag_bmp_size, DiscIO::Country::COUNTRY_ITALY,
             "Flag_Italy");
  InitBitmap(img_list, &m_FlagImageIndex, this, flag_bmp_size, DiscIO::Country::COUNTRY_KOREA,
             "Flag_Korea");
  InitBitmap(img_list, &m_FlagImageIndex, this, flag_bmp_size, DiscIO::Country::COUNTRY_NETHERLANDS,
             "Flag_Netherlands");
  InitBitmap(img_list, &m_FlagImageIndex, this, flag_bmp_size, DiscIO::Country::COUNTRY_RUSSIA,
             "Flag_Russia");
  InitBitmap(img_list, &m_FlagImageIndex, this, flag_bmp_size, DiscIO::Country::COUNTRY_SPAIN,
             "Flag_Spain");
  InitBitmap(img_list, &m_FlagImageIndex, this, flag_bmp_size, DiscIO::Country::COUNTRY_TAIWAN,
             "Flag_Taiwan");
  InitBitmap(img_list, &m_FlagImageIndex, this, flag_bmp_size, DiscIO::Country::COUNTRY_WORLD,
             "Flag_International");
  InitBitmap(img_list, &m_FlagImageIndex, this, flag_bmp_size, DiscIO::Country::COUNTRY_UNKNOWN,
             "Flag_Unknown");

  m_PlatformImageIndex.resize(static_cast<size_t>(DiscIO::Platform::NUMBER_OF_PLATFORMS));
  InitBitmap(img_list, &m_PlatformImageIndex, this, platform_bmp_size,
             DiscIO::Platform::GAMECUBE_DISC, "Platform_Gamecube");
  InitBitmap(img_list, &m_PlatformImageIndex, this, platform_bmp_size, DiscIO::Platform::WII_DISC,
             "Platform_Wii");
  InitBitmap(img_list, &m_PlatformImageIndex, this, platform_bmp_size, DiscIO::Platform::WII_WAD,
             "Platform_Wad");
  InitBitmap(img_list, &m_PlatformImageIndex, this, platform_bmp_size, DiscIO::Platform::ELF_DOL,
             "Platform_File");

  m_EmuStateImageIndex.resize(6);
  InitBitmap(img_list, &m_EmuStateImageIndex, this, rating_bmp_size, 0, "rating0");
  InitBitmap(img_list, &m_EmuStateImageIndex, this, rating_bmp_size, 1, "rating1");
  InitBitmap(img_list, &m_EmuStateImageIndex, this, rating_bmp_size, 2, "rating2");
  InitBitmap(img_list, &m_EmuStateImageIndex, this, rating_bmp_size, 3, "rating3");
  InitBitmap(img_list, &m_EmuStateImageIndex, this, rating_bmp_size, 4, "rating4");
  InitBitmap(img_list, &m_EmuStateImageIndex, this, rating_bmp_size, 5, "rating5");

  m_utility_game_banners.resize(1);
  InitBitmap(img_list, &m_utility_game_banners, this, size, 0, "nobanner");
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
    std::vector<std::string>::iterator itResult =
        std::find(SConfig::GetInstance().m_ISOFolder.begin(),
                  SConfig::GetInstance().m_ISOFolder.end(), sPath);

    if (itResult == SConfig::GetInstance().m_ISOFolder.end())
    {
      SConfig::GetInstance().m_ISOFolder.push_back(sPath);
      SConfig::GetInstance().SaveSettings();
    }

    ReloadList();
  }
}

void CGameListCtrl::ReloadList()
{
  int scrollPos = wxWindow::GetScrollPos(wxVERTICAL);
  // Don't let the user refresh it while a game is running
  if (Core::GetState() != Core::State::Uninitialized)
    return;

  ScanForISOs();

  Freeze();
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
    SetColumnWidth(COLUMN_PLATFORM, SConfig::GetInstance().m_showSystemColumn ?
                                        FromDIP(32 + platform_icon_padding + platform_padding) :
                                        0);
    SetColumnWidth(COLUMN_BANNER,
                   SConfig::GetInstance().m_showBannerColumn ? FromDIP(96 + platform_padding) : 0);
    SetColumnWidth(COLUMN_TITLE, FromDIP(175 + platform_padding));
    SetColumnWidth(COLUMN_MAKER,
                   SConfig::GetInstance().m_showMakerColumn ? FromDIP(150 + platform_padding) : 0);
    SetColumnWidth(COLUMN_FILENAME, SConfig::GetInstance().m_showFileNameColumn ?
                                        FromDIP(100 + platform_padding) :
                                        0);
    SetColumnWidth(COLUMN_ID,
                   SConfig::GetInstance().m_showIDColumn ? FromDIP(75 + platform_padding) : 0);
    SetColumnWidth(COLUMN_COUNTRY,
                   SConfig::GetInstance().m_showRegionColumn ? FromDIP(32 + platform_padding) : 0);
    SetColumnWidth(COLUMN_EMULATION_STATE,
                   SConfig::GetInstance().m_showStateColumn ? FromDIP(48 + platform_padding) : 0);

    // add all items
    for (int i = 0; i < (int)m_ISOFiles.size(); i++)
      InsertItemInReportView(i);

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
    InsertColumn(0, "");
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

// Update the column content of the item at _Index
void CGameListCtrl::UpdateItemAtColumn(long _Index, int column)
{
  GameListItem& rISOFile = *m_ISOFiles[GetItemData(_Index)];

  switch (column)
  {
  case COLUMN_PLATFORM:
  {
    SetItemColumnImage(_Index, COLUMN_PLATFORM,
                       m_PlatformImageIndex[static_cast<size_t>(rISOFile.GetPlatform())]);
    break;
  }
  case COLUMN_BANNER:
  {
    int ImageIndex = m_utility_game_banners[0];  // nobanner

    if (rISOFile.GetBannerImage().IsOk())
    {
      wxImageList* img_list = GetImageList(wxIMAGE_LIST_SMALL);
      ImageIndex = img_list->Add(
          WxUtils::ScaleImageToBitmap(rISOFile.GetBannerImage(), this, img_list->GetSize()));
    }

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
    SetItem(_Index, COLUMN_FILENAME, wxFileNameFromPath(StrToWxStr(rISOFile.GetFileName())), -1);
    break;
  case COLUMN_EMULATION_STATE:
    SetItemColumnImage(_Index, COLUMN_EMULATION_STATE,
                       m_EmuStateImageIndex[rISOFile.GetEmuState()]);
    break;
  case COLUMN_COUNTRY:
    SetItemColumnImage(_Index, COLUMN_COUNTRY,
                       m_FlagImageIndex[static_cast<size_t>(rISOFile.GetCountry())]);
    break;
  case COLUMN_SIZE:
    SetItem(_Index, COLUMN_SIZE, NiceSizeFormat(rISOFile.GetFileSize()), -1);
    break;
  case COLUMN_ID:
    SetItem(_Index, COLUMN_ID, rISOFile.GetGameID(), -1);
    break;
  }
}

void CGameListCtrl::InsertItemInReportView(long index)
{
  // When using wxListCtrl, there is no hope of per-column text colors.
  // But for reference, here are the old colors that were used: (BGR)
  // title: 0xFF0000
  // company: 0x007030

  // Insert a first column with nothing in it, that will be used as the Index
  long item_index;
  {
    wxListItem li;
    li.SetId(index);
    li.SetData(index);
    li.SetMask(wxLIST_MASK_DATA);
    item_index = InsertItem(li);
  }

  // Iterate over all columns and fill them with content if they are visible
  for (int i = 1; i < NUMBER_OF_COLUMN; i++)
  {
    if (GetColumnWidth(i) != 0)
      UpdateItemAtColumn(item_index, i);
  }

  // List colors
  SetColors();
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

void CGameListCtrl::SetColors()
{
  for (long i = 0; i < GetItemCount(); i++)
  {
    wxColour color = (i & 1) ? blend50(wxSystemSettings::GetColour(wxSYS_COLOUR_3DLIGHT),
                                       wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW)) :
                               wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
    CGameListCtrl::SetItemBackgroundColour(i, color);
    SetItemTextColour(i, ContrastText(color));
  }
}

void CGameListCtrl::ScanForISOs()
{
  m_ISOFiles.clear();

  const auto custom_titles = LoadCustomTitles();
  auto rFilenames = DoFileSearch(GetFileSearchExtensions(), SConfig::GetInstance().m_ISOFolder,
                                 SConfig::GetInstance().m_RecursiveISOFolder);

  if (rFilenames.size() > 0)
  {
    wxProgressDialog dialog(
        _("Scanning for ISOs"), _("Scanning..."), (int)rFilenames.size() - 1, this,
        wxPD_APP_MODAL | wxPD_AUTO_HIDE | wxPD_CAN_ABORT | wxPD_ELAPSED_TIME | wxPD_ESTIMATED_TIME |
            wxPD_REMAINING_TIME | wxPD_SMOOTH  // - makes updates as small as possible (down to 1px)
        );

    for (u32 i = 0; i < rFilenames.size(); i++)
    {
      std::string FileName;
      SplitPath(rFilenames[i], nullptr, &FileName, nullptr);

      // Update with the progress (i) and the message
      dialog.Update(i, wxString::Format(_("Scanning %s"), StrToWxStr(FileName)));
      if (dialog.WasCancelled())
        break;

      auto iso_file = std::make_unique<GameListItem>(rFilenames[i], custom_titles);

      if (iso_file->IsValid() && ShouldDisplayGameListItem(*iso_file))
      {
        m_ISOFiles.push_back(std::move(iso_file));
      }
    }
  }

  if (SConfig::GetInstance().m_ListDrives)
  {
    const std::vector<std::string> drives = cdio_get_devices();

    for (const auto& drive : drives)
    {
      auto gli = std::make_unique<GameListItem>(drive, custom_titles);

      if (gli->IsValid())
        m_ISOFiles.push_back(std::move(gli));
    }
  }

  std::sort(m_ISOFiles.begin(), m_ISOFiles.end());
}

void CGameListCtrl::OnReloadGameList(wxCommandEvent& WXUNUSED(event))
{
  ReloadList();
}

void CGameListCtrl::OnLocalIniModified(wxCommandEvent& ev)
{
  ev.Skip();
  std::string game_id = WxStrToStr(ev.GetString());
  // NOTE: The same game may occur multiple times if there are multiple
  //   physical copies in the search paths.
  for (std::size_t i = 0; i < m_ISOFiles.size(); ++i)
  {
    if (m_ISOFiles[i]->GetGameID() != game_id)
      continue;
    m_ISOFiles[i]->ReloadINI();

    // The indexes in m_ISOFiles and the list do not line up.
    // We need to find the corresponding item in the list (if it exists)
    long item_id = 0;
    for (; item_id < GetItemCount(); ++item_id)
    {
      if (i == static_cast<std::size_t>(GetItemData(item_id)))
        break;
    }
    // If the item is not currently being displayed then we're done.
    if (item_id == GetItemCount())
      continue;

    // Update all the columns
    for (int j = 1; j < NUMBER_OF_COLUMN; ++j)
    {
      // NOTE: Banner is not modified by the INI and updating it will
      //  duplicate it in memory which is not wanted.
      if (j != COLUMN_BANNER && GetColumnWidth(j) != 0)
        UpdateItemAtColumn(item_id, j);
    }
  }
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
    return m_ISOFiles[index].get();

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

  SetColors();

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
        if (i + 1 == (int)m_ISOFiles.size())
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
    if (i + 1 == (int)m_ISOFiles.size() && sLoop > 0 && Loop > 0)
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
        if (lastItem != item)
          lastItem = -1;
        event.Skip();
        return;
      }

      // Emulation status
      static const char* const emuState[] = {"Broken", "Intro", "In-Game", "Playable", "Perfect"};

      const GameListItem* iso = m_ISOFiles[GetItemData(item)].get();

      const int emu_state = iso->GetEmuState();
      const std::string& issues = iso->GetIssues();

      // Show a tooltip containing the EmuState and the state description
      if (emu_state > 0 && emu_state < 6)
      {
        char temp[2048];
        sprintf(temp, "^ %s%s%s", emuState[emu_state - 1], issues.size() > 0 ? " :\n" : "",
                issues.c_str());
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
#if !defined(__WXMSW__) && !defined(__WXOSX__)
      // For some reason the y position does not account for the header
      // row, so subtract the y position of the first visible item.
      GetItemRect(GetTopItem(), Rect);
      my -= Rect.GetY();
#endif
      // Convert to screen coordinates
      ClientToScreen(&mx, &my);
      toolTip->SetBoundingRect(wxRect(mx - GetColumnWidth(COLUMN_EMULATION_STATE), my,
                                      GetColumnWidth(COLUMN_EMULATION_STATE), Rect.GetHeight()));
      toolTip->SetPosition(
          wxPoint(mx - GetColumnWidth(COLUMN_EMULATION_STATE), my - 5 + Rect.GetHeight()));
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
  if ((item != wxNOT_FOUND) && (GetSelectedItemCount() == 0) && (!event.ControlDown()) &&
      (!event.ShiftDown()))
  {
    SetItemState(item, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
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
        popupMenu.Append(IDM_OPEN_SAVE_FOLDER, _("Open Wii &save folder"));
        popupMenu.Append(IDM_EXPORT_SAVE, _("Export Wii save (Experimental)"));
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

      if (platform == DiscIO::Platform::WII_WAD)
        popupMenu.Append(IDM_LIST_INSTALL_WAD, _("Install to Wii Menu"));

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

const GameListItem* CGameListCtrl::GetSelectedISO() const
{
  if (m_ISOFiles.empty())
    return nullptr;

  if (GetSelectedItemCount() == 0)
    return nullptr;

  long item = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
  if (item == wxNOT_FOUND)
    return nullptr;

  return m_ISOFiles[GetItemData(item)].get();
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
    result.push_back(m_ISOFiles[GetItemData(item)].get());
  }
}

bool CGameListCtrl::IsHidingItems()
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

void CGameListCtrl::OnOpenContainingFolder(wxCommandEvent& WXUNUSED(event))
{
  const GameListItem* iso = GetSelectedISO();
  if (!iso)
    return;

  wxFileName path = wxFileName::FileName(StrToWxStr(iso->GetFileName()));
  path.MakeAbsolute();
  WxUtils::Explore(WxStrToStr(path.GetPath()));
}

void CGameListCtrl::OnOpenSaveFolder(wxCommandEvent& WXUNUSED(event))
{
  const GameListItem* iso = GetSelectedISO();
  if (!iso)
    return;
  std::string path = iso->GetWiiFSPath();
  if (!path.empty())
    WxUtils::Explore(path);
}

void CGameListCtrl::OnExportSave(wxCommandEvent& WXUNUSED(event))
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

void CGameListCtrl::OnDeleteISO(wxCommandEvent& WXUNUSED(event))
{
  const wxString message =
      GetSelectedItemCount() == 1 ?
          _("Are you sure you want to delete this file? It will be gone forever!") :
          _("Are you sure you want to delete these files? They will be gone forever!");

  if (wxMessageBox(message, _("Warning"), wxYES_NO | wxICON_EXCLAMATION) == wxYES)
  {
    for (const GameListItem* iso : GetAllSelectedISOs())
      File::Delete(iso->GetFileName());
    ReloadList();
  }
}

void CGameListCtrl::OnProperties(wxCommandEvent& WXUNUSED(event))
{
  const GameListItem* iso = GetSelectedISO();
  if (!iso)
    return;

  CISOProperties* ISOProperties = new CISOProperties(*iso, this);
  ISOProperties->Show();
}

void CGameListCtrl::OnWiki(wxCommandEvent& WXUNUSED(event))
{
  const GameListItem* iso = GetSelectedISO();
  if (!iso)
    return;

  std::string wikiUrl =
      "https://wiki.dolphin-emu.org/dolphin-redirect.php?gameid=" + iso->GetGameID();
  WxUtils::Launch(wikiUrl);
}

void CGameListCtrl::OnNetPlayHost(wxCommandEvent& WXUNUSED(event))
{
  const GameListItem* iso = GetSelectedISO();
  if (!iso)
    return;

  IniFile ini_file;
  const std::string dolphin_ini = File::GetUserPath(F_DOLPHINCONFIG_IDX);
  ini_file.Load(dolphin_ini);
  IniFile::Section& netplay_section = *ini_file.GetOrCreateSection("NetPlay");

  NetPlayHostConfig config;
  config.FromIniConfig(netplay_section);
  config.game_name = iso->GetUniqueIdentifier();
  config.game_list_ctrl = this;
  config.SetDialogInfo(netplay_section, m_parent);

  netplay_section.Set("SelectedHostGame", config.game_name);
  ini_file.Save(dolphin_ini);

  NetPlayLauncher::Host(config);
}

bool CGameListCtrl::MultiCompressCB(const std::string& text, float percent, void* arg)
{
  CompressionProgress* progress = static_cast<CompressionProgress*>(arg);

  float total_percent = ((float)progress->items_done + percent) / (float)progress->items_total;
  wxString text_string(
      StrToWxStr(StringFromFormat("%s (%i/%i) - %s", progress->current_filename.c_str(),
                                  progress->items_done + 1, progress->items_total, text.c_str())));

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

  ReloadList();
}

bool CGameListCtrl::CompressCB(const std::string& text, float percent, void* arg)
{
  return ((wxProgressDialog*)arg)->Update((int)(percent * 1000), StrToWxStr(text));
}

void CGameListCtrl::OnCompressISO(wxCommandEvent& WXUNUSED(event))
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

  ReloadList();
}

void CGameListCtrl::OnChangeDisc(wxCommandEvent& WXUNUSED(event))
{
  const GameListItem* iso = GetSelectedISO();
  if (!iso || !Core::IsRunning())
    return;
  DVDInterface::ChangeDiscAsHost(WxStrToStr(iso->GetFileName()));
}

void CGameListCtrl::OnSize(wxSizeEvent& event)
{
  event.Skip();
  if (lastpos == event.GetSize())
    return;

  lastpos = event.GetSize();
  AutomaticColumnWidth();
}

void CGameListCtrl::AutomaticColumnWidth()
{
  wxRect rc(GetClientRect());

  Freeze();
  if (GetColumnCount() == 1)
  {
    SetColumnWidth(0, rc.GetWidth());
  }
  else if (GetColumnCount() > 0)
  {
    int resizable =
        rc.GetWidth() - (GetColumnWidth(COLUMN_PLATFORM) + GetColumnWidth(COLUMN_BANNER) +
                         GetColumnWidth(COLUMN_ID) + GetColumnWidth(COLUMN_COUNTRY) +
                         GetColumnWidth(COLUMN_SIZE) + GetColumnWidth(COLUMN_EMULATION_STATE));

    if (SConfig::GetInstance().m_showMakerColumn && SConfig::GetInstance().m_showFileNameColumn)
    {
      SetColumnWidth(COLUMN_TITLE, resizable / 3);
      SetColumnWidth(COLUMN_MAKER, resizable / 3);
      SetColumnWidth(COLUMN_FILENAME, resizable / 3);
    }
    else if (SConfig::GetInstance().m_showMakerColumn)
    {
      SetColumnWidth(COLUMN_TITLE, resizable / 2);
      SetColumnWidth(COLUMN_MAKER, resizable / 2);
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
  Thaw();
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
  return wxMessageBox(_("Compressing a Wii disc image will irreversibly change the compressed copy "
                        "by removing padding data. Your disc image will still work. Continue?"),
                      _("Warning"), wxYES_NO) == wxYES;
}

#ifdef __WXMSW__
// Windows draws vertical rules between columns when using UXTheme (e.g. Aero, Win10)
// This function paints over those lines which removes them.
// [The repaint background idea is ripped off from Eclipse SWT which does the same thing]
bool CGameListCtrl::MSWOnNotify(int id, WXLPARAM lparam, WXLPARAM* result)
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
