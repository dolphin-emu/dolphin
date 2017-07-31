// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <wx/dialog.h>
#include <wx/treebase.h>

#include "Common/IniFile.h"
#include "DolphinWX/ISOFile.h"
#include "DolphinWX/PatchAddEdit.h"

class ActionReplayCodesPanel;
class CheatWarningMessage;
class DolphinSlider;
class wxButton;
class wxCheckBox;
class wxCheckListBox;
class wxChoice;
class wxSpinCtrl;
class wxStaticBitmap;
class wxTextCtrl;

namespace DiscIO
{
class Volume;
}

namespace Gecko
{
class CodeConfigPanel;
}

struct PHackData
{
  bool PHackSZNear;
  bool PHackSZFar;
  std::string PHZNear;
  std::string PHZFar;
};

wxDECLARE_EVENT(DOLPHIN_EVT_CHANGE_ISO_PROPERTIES_TITLE, wxCommandEvent);

class CISOProperties : public wxDialog
{
public:
  CISOProperties(const GameListItem& game_list_item, wxWindow* parent, wxWindowID id = wxID_ANY,
                 const wxString& title = _("Properties"), const wxPoint& pos = wxDefaultPosition,
                 const wxSize& size = wxDefaultSize,
                 long style = wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
  virtual ~CISOProperties();

private:
  DECLARE_EVENT_TABLE()

  std::unique_ptr<DiscIO::Volume> m_open_iso;

  std::vector<PatchEngine::Patch> onFrame;
  PHackData m_PHack_Data;

  // Core
  wxCheckBox *CPUThread, *MMU, *DCBZOFF, *FPRF;
  wxCheckBox *SyncGPU, *FastDiscSpeed, *DSPHLE;

  wxArrayString arrayStringFor_GPUDeterminism;
  wxChoice* GPUDeterminism;
  // Wii
  wxCheckBox* EnableWideScreen;

  // Stereoscopy
  DolphinSlider* DepthPercentage;
  wxSpinCtrl* Convergence;
  wxCheckBox* MonoDepth;

  wxArrayString arrayStringFor_EmuState;
  wxChoice* EmuState;
  wxTextCtrl* EmuIssues;

  wxCheckListBox* Patches;
  wxButton* EditPatch;
  wxButton* RemovePatch;

  ActionReplayCodesPanel* m_ar_code_panel;
  Gecko::CodeConfigPanel* m_geckocode_panel;

  CheatWarningMessage* m_cheats_disabled_ar;
  CheatWarningMessage* m_cheats_disabled_gecko;

  enum
  {
    ID_NOTEBOOK = 1000,
    ID_GAMECONFIG,
    ID_PATCH_PAGE,
    ID_ARCODE_PAGE,
    ID_SPEEDHACK_PAGE,
    ID_INFORMATION,
    ID_FILESYSTEM,

    ID_USEDUALCORE,
    ID_MMU,
    ID_DCBZOFF,
    ID_FPRF,
    ID_SYNCGPU,
    ID_DISCSPEED,
    ID_AUDIO_DSP_HLE,
    ID_USE_BBOX,
    ID_ENABLEPROGRESSIVESCAN,
    ID_ENABLEWIDESCREEN,
    ID_EDITCONFIG,
    ID_SHOWDEFAULTCONFIG,
    ID_EMUSTATE,
    ID_EMU_ISSUES,
    ID_PATCHES_LIST,
    ID_EDITPATCH,
    ID_ADDPATCH,
    ID_REMOVEPATCH,
    ID_GPUDETERMINISM,
    ID_DEPTHPERCENTAGE,
    ID_CONVERGENCE,
    ID_MONODEPTH,
  };

  void LaunchExternalEditor(const std::string& filename, bool wait_until_closed);

  void CreateGUIControls();
  void OnClose(wxCloseEvent& event);
  void OnCloseClick(wxCommandEvent& event);
  void OnEditConfig(wxCommandEvent& event);
  void OnShowDefaultConfig(wxCommandEvent& event);
  void PatchListSelectionChanged(wxCommandEvent& event);
  void PatchButtonClicked(wxCommandEvent& event);
  void OnEmustateChanged(wxCommandEvent& event);
  void OnCheatCodeToggled(wxCommandEvent& event);
  void OnChangeTitle(wxCommandEvent& event);

  const GameListItem OpenGameListItem;

  IniFile GameIniDefault;
  IniFile GameIniLocal;
  std::string GameIniFileLocal;
  std::string game_id;

  std::set<std::string> DefaultPatches;

  void LoadGameConfig();
  bool SaveGameConfig();
  void OnLocalIniModified(wxCommandEvent& ev);
  void GenerateLocalIniModified();
  void PatchList_Load();
  void PatchList_Save();

  long GetElementStyle(const char* section, const char* key);
  void SetCheckboxValueFromGameini(const char* section, const char* key, wxCheckBox* checkbox);
  void SaveGameIniValueFrom3StateCheckbox(const char* section, const char* key,
                                          wxCheckBox* checkbox);
};
