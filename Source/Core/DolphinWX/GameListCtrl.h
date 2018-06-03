// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <wx/listctrl.h>
#include <wx/tipwin.h>

#include "Common/ChunkFile.h"
#include "Common/Event.h"
#include "Common/Flag.h"
#include "UICommon/GameFileCache.h"

namespace UICommon
{
class GameFile;
}

wxDECLARE_EVENT(DOLPHIN_EVT_REFRESH_GAMELIST, wxCommandEvent);
wxDECLARE_EVENT(DOLPHIN_EVT_RESCAN_GAMELIST, wxCommandEvent);

class GameListCtrl : public wxListCtrl
{
public:
  GameListCtrl(bool disable_scanning, wxWindow* parent, const wxWindowID id, const wxPoint& pos,
               const wxSize& size, long style);
  ~GameListCtrl();

  void BrowseForDirectory();
  const UICommon::GameFile* GetISO(size_t index) const;
  const std::string& GetShownName(size_t index) const;
  const UICommon::GameFile* GetSelectedISO() const;

  static bool IsHidingItems();

  enum
  {
    COLUMN_DUMMY = 0,
    FIRST_COLUMN_WITH_CONTENT,
    COLUMN_PLATFORM = FIRST_COLUMN_WITH_CONTENT,
    COLUMN_BANNER,
    COLUMN_TITLE,
    COLUMN_MAKER,
    COLUMN_FILENAME,
    COLUMN_ID,
    COLUMN_COUNTRY,
    COLUMN_SIZE,
    NUMBER_OF_COLUMN
  };

#ifdef __WXMSW__
  bool MSWOnNotify(int id, WXLPARAM lparam, WXLPARAM* result) override;
#endif

private:
  struct ColumnInfo;

  void InitBitmaps();
  void UpdateItemAtColumn(long index, int column);
  void InsertItemInReportView(long index);
  void SetColors();
  void RefreshList();
  void RescanList();
  std::vector<const UICommon::GameFile*> GetAllSelectedISOs() const;

  // events
  void OnRefreshGameList(wxCommandEvent& event);
  void OnRescanGameList(wxCommandEvent& event);
  void OnLeftClick(wxMouseEvent& event);
  void OnRightClick(wxMouseEvent& event);
  void OnColumnClick(wxListEvent& event);
  void OnColBeginDrag(wxListEvent& event);
  void OnKeyPress(wxListEvent& event);
  void OnSize(wxSizeEvent& event);
  void OnProperties(wxCommandEvent& event);
  void OnWiki(wxCommandEvent& event);
  void OnNetPlayHost(wxCommandEvent& event);
  void OnOpenContainingFolder(wxCommandEvent& event);
  void OnOpenSaveFolder(wxCommandEvent& event);
  void OnExportSave(wxCommandEvent& event);
  void OnSetDefaultISO(wxCommandEvent& event);
  void OnDeleteISO(wxCommandEvent& event);
  void OnCompressISO(wxCommandEvent& event);
  void OnMultiCompressISO(wxCommandEvent& event);
  void OnMultiDecompressISO(wxCommandEvent& event);
  void OnChangeDisc(wxCommandEvent& event);
  void OnLocalIniModified(wxCommandEvent& event);

  void CompressSelection(bool _compress);
  void AutomaticColumnWidth();
  void UnselectAll();

  static bool CompressCB(const std::string& text, float percent, void* arg);
  static bool MultiCompressCB(const std::string& text, float percent, void* arg);
  static bool WiiCompressWarning();

  struct
  {
    std::vector<int> flag;
    std::vector<int> platform;
    std::vector<int> utility_banner;
  } m_image_indexes;

  // Actual backing GameFiles are maintained in a background thread and cached to file
  UICommon::GameFileCache m_cache;
  // Locks the cache object, not the shared_ptr<GameFile>s obtained from it
  std::mutex m_cache_mutex;
  std::thread m_scan_thread;
  Common::Event m_scan_trigger;
  Common::Flag m_scan_exiting;
  // UI thread's view into the cache
  std::vector<std::shared_ptr<const UICommon::GameFile>> m_shown_files;
  std::vector<std::string> m_shown_names;

  int m_last_column;
  int m_last_sort;
  wxSize m_lastpos;

  std::vector<ColumnInfo> m_columns;
};
