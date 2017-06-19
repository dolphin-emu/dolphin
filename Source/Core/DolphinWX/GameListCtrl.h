// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include <wx/listctrl.h>
#include <wx/tipwin.h>

#include "DolphinWX/ISOFile.h"

wxDECLARE_EVENT(DOLPHIN_EVT_RELOAD_GAMELIST, wxCommandEvent);

class GameListCtrl : public wxListCtrl
{
public:
  GameListCtrl(wxWindow* parent, const wxWindowID id, const wxPoint& pos, const wxSize& size,
                long style);
  ~GameListCtrl() = default;

  void BrowseForDirectory();
  const GameListItem* GetISO(size_t index) const;
  const GameListItem* GetSelectedISO() const;
  std::vector<const GameListItem*> GetAllSelectedISOs() const;

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

  void ReloadList();

  void InitBitmaps();
  void UpdateItemAtColumn(long index, int column);
  void InsertItemInReportView(long index);
  void SetColors();
  void ScanForISOs();

  // events
  void OnReloadGameList(wxCommandEvent& event);
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

  std::vector<std::unique_ptr<GameListItem>> m_ISOFiles;
  struct {
    std::vector<int> flag;
    std::vector<int> platform;
    std::vector<int> utility_banner;
  } m_image_indexes;

  int m_last_column;
  int m_last_sort;
  wxSize m_lastpos;

  std::vector<ColumnInfo> m_columns;
};
