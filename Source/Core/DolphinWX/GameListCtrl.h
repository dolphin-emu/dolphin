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

wxDECLARE_EVENT(DOLPHIN_EVT_RELOAD_GAMELIST, wxCommandEvent);

class CGameListCtrl : public wxListCtrl
{
public:
  CGameListCtrl(wxWindow* parent, const wxWindowID id, const wxPoint& pos, const wxSize& size,
                long style);
  ~CGameListCtrl();

  void BrowseForDirectory();
  const GameListItem* GetISO(size_t index) const;
  const GameListItem* GetSelectedISO() const;
  std::vector<const GameListItem*> GetAllSelectedISOs() const;

  static bool IsHidingItems();

  enum
  {
    COLUMN_DUMMY = 0,
    COLUMN_PLATFORM,
    COLUMN_BANNER,
    COLUMN_TITLE,
    COLUMN_MAKER,
    COLUMN_FILENAME,
    COLUMN_ID,
    COLUMN_COUNTRY,
    COLUMN_SIZE,
    COLUMN_EMULATION_STATE,
    NUMBER_OF_COLUMN
  };

#ifdef __WXMSW__
  bool MSWOnNotify(int id, WXLPARAM lparam, WXLPARAM* result) override;
#endif

private:
  void ReloadList();

  void ClearIsoFiles() { m_ISOFiles.clear(); }
  void InitBitmaps();
  void UpdateItemAtColumn(long _Index, int column);
  void InsertItemInReportView(long _Index);
  void SetColors();
  void ScanForISOs();

  // events
  void OnReloadGameList(wxCommandEvent& event);
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

  std::vector<int> m_FlagImageIndex;
  std::vector<int> m_PlatformImageIndex;
  std::vector<int> m_EmuStateImageIndex;
  std::vector<int> m_utility_game_banners;
  std::vector<std::unique_ptr<GameListItem>> m_ISOFiles;

  int last_column;
  int last_sort;
  wxSize lastpos;
  wxEmuStateTip* toolTip;
};
