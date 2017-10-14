// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <string>
#include <wx/dialog.h>
#include <wx/listctrl.h>

#include "Common/CommonTypes.h"
#include "Common/IniFile.h"

class GCMemcard;
class wxButton;
class wxFileDirPickerEvent;
class wxFilePickerCtrl;
class wxStaticText;

class CMemcardManager final : public wxDialog
{
public:
  CMemcardManager(wxWindow* parent);
  ~CMemcardManager();

private:
  DECLARE_EVENT_TABLE()

  std::array<int, 2> page;
  int itemsPerPage;
  int maxPages;
  std::array<std::string, 2> DefaultMemcard;
  std::string DefaultIOPath;
  IniFile MemcardManagerIni;
  IniFile::Section* iniMemcardSection;

  std::array<wxButton*, 2> m_CopyFrom;
  std::array<wxButton*, 2> m_SaveImport;
  std::array<wxButton*, 2> m_SaveExport;
  std::array<wxButton*, 2> m_Delete;
  std::array<wxButton*, 2> m_NextPage;
  std::array<wxButton*, 2> m_PrevPage;
  wxButton* m_ConvertToGci;
  std::array<wxFilePickerCtrl*, 2> m_MemcardPath;
  std::array<wxStaticText*, 2> t_Status;

  enum
  {
    ID_COPYFROM_A = 1000,  // Do not rearrange these items,
    ID_COPYFROM_B,         // ID_..._B must be 1 more than ID_..._A
    ID_FIXCHECKSUM_A,
    ID_FIXCHECKSUM_B,
    ID_DELETE_A,
    ID_DELETE_B,
    ID_SAVEEXPORT_A,
    ID_SAVEEXPORT_B,
    ID_SAVEIMPORT_A,
    ID_SAVEIMPORT_B,
    ID_EXPORTALL_A,
    ID_EXPORTALL_B,
    ID_CONVERTTOGCI,
    ID_NEXTPAGE_A,
    ID_NEXTPAGE_B,
    ID_PREVPAGE_A,
    ID_PREVPAGE_B,
    ID_MEMCARDLIST_A,
    ID_MEMCARDLIST_B,
    ID_MEMCARDPATH_A,
    ID_MEMCARDPATH_B,
    ID_USEPAGES,
    ID_DUMMY_VALUE_  // don't remove this value unless you have other enum values
  };

  enum
  {
    COLUMN_BANNER = 0,
    COLUMN_TITLE,
    COLUMN_COMMENT,
    COLUMN_ICON,
    COLUMN_BLOCKS,
    COLUMN_FIRSTBLOCK,
    COLUMN_GAMECODE,
    COLUMN_MAKERCODE,
    COLUMN_FILENAME,
    COLUMN_BIFLAGS,
    COLUMN_MODTIME,
    COLUMN_IMAGEADD,
    COLUMN_ICONFMT,
    COLUMN_ANIMSPEED,
    COLUMN_PERMISSIONS,
    COLUMN_COPYCOUNTER,
    COLUMN_COMMENTSADDRESS,
    NUMBER_OF_COLUMN
  };

  std::array<std::unique_ptr<GCMemcard>, 2> memoryCard;

  void CreateGUIControls();
  void CopyDeleteClick(wxCommandEvent& event);
  bool ReloadMemcard(const std::string& fileName, int card);
  void OnMenuChange(wxCommandEvent& event);
  void OnPageChange(wxCommandEvent& event);
  void OnPathChange(wxFileDirPickerEvent& event);
  void ChangePath(int id);
  bool CopyDeleteSwitch(u32 error, int slot);
  bool LoadSettings();
  bool SaveSettings();

  struct ManagerSettings
  {
    bool twoCardsLoaded;
    bool usePages;
    std::array<bool, NUMBER_OF_COLUMN + 1> column;
  } mcmSettings;

  class CMemcardListCtrl : public wxListCtrl
  {
  public:
    CMemcardListCtrl(wxWindow* parent, const wxWindowID id, const wxPoint& pos, const wxSize& size,
                     long style, ManagerSettings& manager_settings)
        : wxListCtrl(parent, id, pos, size, style), mgr_settings(manager_settings)
    {
      Bind(wxEVT_RIGHT_DOWN, &CMemcardListCtrl::OnRightClick, this);
    }
    ~CMemcardListCtrl() { Unbind(wxEVT_RIGHT_DOWN, &CMemcardListCtrl::OnRightClick, this); }
    ManagerSettings& mgr_settings;
    bool prevPage;
    bool nextPage;

  private:
    void OnRightClick(wxMouseEvent& event);
  };

  wxSize m_image_list_size;
  std::array<CMemcardListCtrl*, 2> m_MemcardList;
};
