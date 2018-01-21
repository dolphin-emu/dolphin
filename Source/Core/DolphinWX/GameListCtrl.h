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
#include "DolphinWX/ISOFile.h"

class wxEmuStateTip : public wxTipWindow
{
public:
  wxEmuStateTip(wxWindow* parent, const wxString& text, wxEmuStateTip** windowPtr)
      : wxTipWindow(parent, text, 70, (wxTipWindow**)windowPtr)
  {
    Bind(wxEVT_KEY_DOWN, &wxEmuStateTip::OnKeyDown, this);
  }

  // wxTipWindow doesn't correctly handle KeyEvents and crashes... we must overload that.
  void OnKeyDown(wxKeyEvent& event)
  {
    event.StopPropagation();
    Close();
  }
};

wxDECLARE_EVENT(DOLPHIN_EVT_REFRESH_GAMELIST, wxCommandEvent);
wxDECLARE_EVENT(DOLPHIN_EVT_RESCAN_GAMELIST, wxCommandEvent);

class GameListCtrl : public wxListCtrl
{
public:
  GameListCtrl(bool disable_scanning, wxWindow* parent, const wxWindowID id, const wxPoint& pos,
               const wxSize& size, long style);
  ~GameListCtrl();

  void BrowseForDirectory();
  const GameListItem* GetISO(size_t index) const;
  const GameListItem* GetSelectedISO() const;

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
    COLUMN_EMULATION_STATE,
    COLUMN_VR_STATE,
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
  void DoState(PointerWrap* p, u32 size = 0);
  bool SyncCacheFile(bool write);
  std::vector<const GameListItem*> GetAllSelectedISOs() const;

  // events
  void OnRefreshGameList(wxCommandEvent& event);
  void OnRescanGameList(wxCommandEvent& event);
  void OnLeftClick(wxMouseEvent& event);
  void OnRightClick(wxMouseEvent& event);
  void OnMouseMotion(wxMouseEvent& event);
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
    std::vector<int> emu_state;
  } m_image_indexes;

  // Actual backing GameListItems are maintained in a background thread and cached to file
  std::list<std::shared_ptr<GameListItem>> m_cached_files;
  // Locks the list, not the contents
  std::mutex m_cache_mutex;
  Core::TitleDatabase m_title_database;
  std::mutex m_title_database_mutex;
  std::thread m_scan_thread;
  Common::Event m_scan_trigger;
  Common::Flag m_scan_exiting;
  // UI thread's view into the cache
  std::vector<std::shared_ptr<GameListItem>> m_shown_files;

  int m_last_column;
  int m_last_sort;
  wxSize m_lastpos;
  wxEmuStateTip* m_tooltip;

  std::vector<ColumnInfo> m_columns;
};
