// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/ISOProperties/ISOProperties.h"

#include <cinttypes>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <type_traits>
#include <vector>
#include <wx/app.h>
#include <wx/artprov.h>
#include <wx/bitmap.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/checklst.h>
#include <wx/choice.h>
#include <wx/dialog.h>
#include <wx/filedlg.h>
#include <wx/gbsizer.h>
#include <wx/image.h>
#include <wx/menu.h>
#include <wx/mimetype.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/spinbutt.h>
#include <wx/spinctrl.h>
#include <wx/statbmp.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/thread.h>
#include <wx/utils.h>

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/StringUtil.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/GeckoCodeConfig.h"
#include "Core/HideObjectEngine.h"
#include "Core/PatchEngine.h"
#include "DiscIO/Blob.h"
#include "DiscIO/Enums.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeCreator.h"
#include "DolphinWX/Cheats/ActionReplayCodesPanel.h"
#include "DolphinWX/Cheats/GeckoCodeDiag.h"
#include "DolphinWX/Config/ConfigMain.h"
#include "DolphinWX/DolphinSlider.h"
#include "DolphinWX/Frame.h"
#include "DolphinWX/Globals.h"
#include "DolphinWX/HideObjectAddEdit.h"
#include "DolphinWX/ISOFile.h"
#include "DolphinWX/ISOProperties/FilesystemPanel.h"
#include "DolphinWX/ISOProperties/InfoPanel.h"
#include "DolphinWX/Main.h"
#include "DolphinWX/PatchAddEdit.h"
#include "DolphinWX/VideoConfigDiag.h"
#include "DolphinWX/WxUtils.h"
#include "VideoCommon/VR.h"

std::vector<HideObjectEngine::HideObject> HideObjectCodes;

// A warning message displayed on the ARCodes and GeckoCodes pages when cheats are
// disabled globally to explain why turning cheats on does not work.
// Also displays a different warning when the game is currently running to explain
// that toggling codes has no effect while the game is already running.
class CheatWarningMessage final : public wxPanel
{
public:
  CheatWarningMessage(wxWindow* parent, std::string game_id)
      : wxPanel(parent), m_game_id(std::move(game_id))
  {
    SetExtraStyle(GetExtraStyle() | wxWS_EX_BLOCK_EVENTS);
    CreateGUI();
    wxTheApp->Bind(wxEVT_IDLE, &CheatWarningMessage::OnAppIdle, this);
    Hide();
  }

  void UpdateState()
  {
    // If cheats are disabled then show the notification about that.
    // If cheats are enabled and the game is currently running then display that warning.
    State new_state = State::Hidden;
    if (!SConfig::GetInstance().bEnableCheats)
      new_state = State::DisabledCheats;
    else if (Core::IsRunning() && SConfig::GetInstance().GetGameID() == m_game_id)
      new_state = State::GameRunning;
    ApplyState(new_state);
  }

private:
  enum class State
  {
    Inactive,
    Hidden,
    DisabledCheats,
    GameRunning
  };

  void CreateGUI()
  {
    int space10 = FromDIP(10);
    int space15 = FromDIP(15);

    wxStaticBitmap* icon =
        new wxStaticBitmap(this, wxID_ANY, wxArtProvider::GetMessageBoxIcon(wxICON_WARNING));
    m_message = new wxStaticText(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                                 wxST_NO_AUTORESIZE);
    m_btn_configure = new wxButton(this, wxID_ANY, _("Configure Dolphin"));

    m_btn_configure->Bind(wxEVT_BUTTON, &CheatWarningMessage::OnConfigureClicked, this);

    wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(icon, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, space15);
    sizer->Add(m_message, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, space15);
    sizer->Add(m_btn_configure, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, space10);
    sizer->AddSpacer(space10);

    SetSizer(sizer);
  }

  void OnConfigureClicked(wxCommandEvent&)
  {
    main_frame->OpenGeneralConfiguration(CConfigMain::ID_GENERALPAGE);
    UpdateState();
  }

  void OnAppIdle(wxIdleEvent& ev)
  {
    ev.Skip();
    // Only respond to setting changes if we've been triggered once already.
    if (m_state != State::Inactive)
      UpdateState();
  }

  void ApplyState(State new_state)
  {
    // The purpose of this function is to prevent unnecessary UI updates which cause flickering.
    if (new_state == m_state || (m_state == State::Inactive && new_state == State::Hidden))
      return;

    bool visible = true;
    switch (new_state)
    {
    case State::Inactive:
    case State::Hidden:
      visible = false;
      break;

    case State::DisabledCheats:
      m_btn_configure->Show();
      m_message->SetLabelText(_("Dolphin's cheat system is currently disabled."));
      break;

    case State::GameRunning:
      m_btn_configure->Hide();
      m_message->SetLabelText(
          _("Changing cheats will only take effect when the game is restarted."));
      break;
    }
    m_state = new_state;
    Show(visible);
    GetParent()->Layout();
    if (visible)
    {
      m_message->Wrap(m_message->GetSize().GetWidth());
      m_message->InvalidateBestSize();
      GetParent()->Layout();
    }
  }

  std::string m_game_id;
  wxStaticText* m_message = nullptr;
  wxButton* m_btn_configure = nullptr;
  State m_state = State::Inactive;
};

wxDEFINE_EVENT(DOLPHIN_EVT_CHANGE_ISO_PROPERTIES_TITLE, wxCommandEvent);

BEGIN_EVENT_TABLE(CISOProperties, wxDialog)
EVT_CLOSE(CISOProperties::OnClose)
EVT_BUTTON(wxID_OK, CISOProperties::OnCloseClick)
EVT_BUTTON(ID_EDITCONFIG, CISOProperties::OnEditConfig)
EVT_BUTTON(ID_SHOWDEFAULTCONFIG, CISOProperties::OnShowDefaultConfig)
EVT_CHOICE(ID_EMUSTATE, CISOProperties::OnEmustateChanged)
EVT_CHECKLISTBOX(ID_HIDEOBJECTS_LIST, CISOProperties::CheckboxSelectionChanged)
EVT_BUTTON(ID_EDITHIDEOBJECT, CISOProperties::HideObjectButtonClicked)
EVT_BUTTON(ID_ADDHideObject, CISOProperties::HideObjectButtonClicked)
EVT_BUTTON(ID_REMOVEHIDEOBJECT, CISOProperties::HideObjectButtonClicked)
EVT_LISTBOX(ID_PATCHES_LIST, CISOProperties::PatchListSelectionChanged)
EVT_BUTTON(ID_EDITPATCH, CISOProperties::PatchButtonClicked)
EVT_BUTTON(ID_ADDPATCH, CISOProperties::PatchButtonClicked)
EVT_BUTTON(ID_REMOVEPATCH, CISOProperties::PatchButtonClicked)
END_EVENT_TABLE()

CISOProperties::CISOProperties(const GameListItem& game_list_item, wxWindow* parent, wxWindowID id,
                               const wxString& title, const wxPoint& position, const wxSize& size,
                               long style)
    : wxDialog(parent, id, title, position, size, style), OpenGameListItem(game_list_item)
{
  Bind(DOLPHIN_EVT_CHANGE_ISO_PROPERTIES_TITLE, &CISOProperties::OnChangeTitle, this);

  // Load ISO data
  m_open_iso = DiscIO::CreateVolumeFromFilename(OpenGameListItem.GetFileName());

  game_id = m_open_iso->GetGameID();

  // Load game INIs
  GameIniFileLocal = File::GetUserPath(D_GAMESETTINGS_IDX) + game_id + ".ini";
  GameIniDefault = SConfig::LoadDefaultGameIni(game_id, m_open_iso->GetRevision());
  GameIniLocal = SConfig::LoadLocalGameIni(game_id, m_open_iso->GetRevision());

  // Setup GUI
  CreateGUIControls();
  LoadGameConfig();

  wxTheApp->Bind(DOLPHIN_EVT_LOCAL_INI_CHANGED, &CISOProperties::OnLocalIniModified, this);
}

CISOProperties::~CISOProperties()
{
}

long CISOProperties::GetElementStyle(const char* section, const char* key)
{
  // Disable 3rd state if default gameini overrides the setting
  if (GameIniDefault.Exists(section, key))
    return 0;

  return wxCHK_3STATE | wxCHK_ALLOW_3RD_STATE_FOR_USER;
}

void CISOProperties::CreateGUIControls()
{
  const int space5 = FromDIP(5);

  wxButton* const EditConfig = new wxButton(this, ID_EDITCONFIG, _("Edit Config"));
  EditConfig->SetToolTip(_("This will let you manually edit the INI config file."));

  wxButton* const EditConfigDefault = new wxButton(this, ID_SHOWDEFAULTCONFIG, _("Show Defaults"));
  EditConfigDefault->SetToolTip(
      _("Opens the default (read-only) configuration for this game in an external text editor."));

  // Notebook
  wxNotebook* const m_Notebook = new wxNotebook(this, ID_NOTEBOOK);
  wxPanel* const m_GameConfig = new wxPanel(m_Notebook, ID_GAMECONFIG);
  m_Notebook->AddPage(m_GameConfig, _("GameConfig"));
  wxPanel* const m_VR = new wxPanel(m_Notebook, ID_VR);
  m_Notebook->AddPage(m_VR, _("VR"));
  wxPanel* const m_HideObjectPage = new wxPanel(m_Notebook, ID_HIDEOBJECT_PAGE);
  m_Notebook->AddPage(m_HideObjectPage, _("Hide Objects"));
  wxPanel* const m_PatchPage = new wxPanel(m_Notebook, ID_PATCH_PAGE);
  m_Notebook->AddPage(m_PatchPage, _("Patches"));
  wxPanel* const m_CheatPage = new wxPanel(m_Notebook, ID_ARCODE_PAGE);
  m_Notebook->AddPage(m_CheatPage, _("AR Codes"));
  wxPanel* const gecko_cheat_page = new wxPanel(m_Notebook);
  m_Notebook->AddPage(gecko_cheat_page, _("Gecko Codes"));
  m_Notebook->AddPage(new InfoPanel(m_Notebook, ID_INFORMATION, OpenGameListItem, m_open_iso),
                      _("Info"));

  // VR
  wxBoxSizer* const sbVR = new wxBoxSizer(wxVERTICAL);
  m_VR->SetSizer(sbVR);
  // wxStaticBoxSizer * const sbVR = new wxStaticBoxSizer(wxVERTICAL, m_VR, _("Game-Specific VR
  // Settings"));
  // sVRPage->Add(sbVR, 0, wxEXPAND | wxALL, 5);
  // wxStaticText* const OverrideTextVR = new wxStaticText(m_VR, wxID_ANY, _("These settings
  // override core Dolphin settings.\nUndetermined means the game uses Dolphin's setting."));
  // sbVR->Add(OverrideTextVR, 0, wxEXPAND | wxALL, 5);
  wxStaticBoxSizer* const sb3D = new wxStaticBoxSizer(wxVERTICAL, m_VR, _("3D World"));
  sbVR->Add(sb3D, 0, wxEXPAND | wxALL, 5);
  // Disable3D = new wxCheckBox(m_VR, ID_DISABLE_3D, _("Disable 3D"), wxDefaultPosition,
  // wxDefaultSize, GetElementStyle("VR", "Disable3D"));
  HudFullscreen = new wxCheckBox(m_VR, ID_HUD_FULLSCREEN, _("HUD Fullscreen"), wxDefaultPosition,
                                 wxDefaultSize, GetElementStyle("VR", "HudFullscreen"));
  HudOnTop = new wxCheckBox(m_VR, ID_HUD_ON_TOP, _("HUD On Top"), wxDefaultPosition, wxDefaultSize,
                            GetElementStyle("VR", "HudOnTop"));
  // sb3D->Add(Disable3D, 0, wxLEFT, 5);
  sb3D->Add(HudOnTop, 0, wxLEFT, 5);
  sb3D->Add(HudFullscreen, 0, wxLEFT, 5);
  wxGridBagSizer* s3DGrid = new wxGridBagSizer();
  sb3D->Add(s3DGrid, 0, wxEXPAND);

  s3DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("Units Per Metre:")), wxGBPosition(0, 0),
               wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
  UnitsPerMetre =
      new wxSpinCtrlDouble(m_VR, ID_UNITS_PER_METRE, "", wxDefaultPosition, wxDefaultSize,
                           wxSP_ARROW_KEYS, 0.0000001, 10000000, DEFAULT_VR_UNITS_PER_METRE, 0.5);
  s3DGrid->Add(UnitsPerMetre, wxGBPosition(0, 1), wxDefaultSpan, wxALL, 5);
  s3DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("game units")), wxGBPosition(0, 2), wxDefaultSpan,
               wxALIGN_CENTER_VERTICAL | wxALL, 5);

  s3DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("HUD Distance:")), wxGBPosition(1, 0),
               wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
  HudDistance = new wxSpinCtrlDouble(m_VR, ID_HUD_DISTANCE, "", wxDefaultPosition, wxDefaultSize,
                                     wxSP_ARROW_KEYS, 0.01, 10000, DEFAULT_VR_HUD_DISTANCE, 0.1);
  s3DGrid->Add(HudDistance, wxGBPosition(1, 1), wxDefaultSpan, wxALL, 5);
  s3DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("metres")), wxGBPosition(1, 2), wxDefaultSpan,
               wxALIGN_CENTER_VERTICAL | wxALL, 5);

  s3DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("HUD Thickness:")), wxGBPosition(2, 0),
               wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
  HudThickness = new wxSpinCtrlDouble(m_VR, ID_HUD_THICKNESS, "", wxDefaultPosition, wxDefaultSize,
                                      wxSP_ARROW_KEYS, 0, 10000, DEFAULT_VR_HUD_THICKNESS, 0.1);
  s3DGrid->Add(HudThickness, wxGBPosition(2, 1), wxDefaultSpan, wxALL, 5);
  s3DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("metres")), wxGBPosition(2, 2), wxDefaultSpan,
               wxALIGN_CENTER_VERTICAL | wxALL, 5);

  s3DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("HUD 3D Closer:")), wxGBPosition(3, 0),
               wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
  Hud3DCloser = new wxSpinCtrlDouble(m_VR, ID_HUD_3D_CLOSER, "", wxDefaultPosition, wxDefaultSize,
                                     wxSP_ARROW_KEYS, 0, 1, DEFAULT_VR_HUD_3D_CLOSER, 0.1);
  s3DGrid->Add(Hud3DCloser, wxGBPosition(3, 1), wxDefaultSpan, wxALL, 5);
  s3DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("0=Back, 1=Front")), wxGBPosition(3, 2),
               wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);

  s3DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("Camera Forward:")), wxGBPosition(4, 0),
               wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
  CameraForward =
      new wxSpinCtrlDouble(m_VR, ID_CAMERA_FORWARD, "", wxDefaultPosition, wxDefaultSize,
                           wxSP_ARROW_KEYS, -10000, 10000, DEFAULT_VR_CAMERA_FORWARD, 0.1);
  s3DGrid->Add(CameraForward, wxGBPosition(4, 1), wxDefaultSpan, wxALL, 5);
  s3DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("metres")), wxGBPosition(4, 2), wxDefaultSpan,
               wxALIGN_CENTER_VERTICAL | wxALL, 5);

  s3DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("Camera Pitch:")), wxGBPosition(5, 0),
               wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
  CameraPitch = new wxSpinCtrlDouble(m_VR, ID_CAMERA_PITCH, "", wxDefaultPosition, wxDefaultSize,
                                     wxSP_ARROW_KEYS, -180, 360, DEFAULT_VR_CAMERA_PITCH, 1);
  s3DGrid->Add(CameraPitch, wxGBPosition(5, 1), wxDefaultSpan, wxALL, 5);
  s3DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("Degrees Up")), wxGBPosition(5, 2), wxDefaultSpan,
               wxALIGN_CENTER_VERTICAL | wxALL, 5);

  s3DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("Aim Distance:")), wxGBPosition(6, 0),
               wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
  AimDistance = new wxSpinCtrlDouble(m_VR, ID_AIM_DISTANCE, "", wxDefaultPosition, wxDefaultSize,
                                     wxSP_ARROW_KEYS, 0.01, 10000, DEFAULT_VR_AIM_DISTANCE, 0.1);
  s3DGrid->Add(AimDistance, wxGBPosition(6, 1), wxDefaultSpan, wxALL, 5);
  s3DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("metres")), wxGBPosition(6, 2), wxDefaultSpan,
               wxALIGN_CENTER_VERTICAL | wxALL, 5);

  s3DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("Min HFOV:")), wxGBPosition(7, 0), wxDefaultSpan,
               wxALIGN_CENTER_VERTICAL | wxALL, 5);
  MinFOV = new wxSpinCtrlDouble(m_VR, ID_MIN_FOV, "", wxDefaultPosition, wxDefaultSize,
                                wxSP_ARROW_KEYS, 0, 179, DEFAULT_VR_MIN_FOV, 1);
  s3DGrid->Add(MinFOV, wxGBPosition(7, 1), wxDefaultSpan, wxALL, 5);
  s3DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("degrees")), wxGBPosition(7, 2), wxDefaultSpan,
               wxALIGN_CENTER_VERTICAL | wxALL, 5);

  wxStaticBoxSizer* const sb2D = new wxStaticBoxSizer(wxVERTICAL, m_VR, _("2D Screens"));
  sbVR->Add(sb2D, 0, wxEXPAND | wxALL, 5);
  wxGridBagSizer* s2DGrid = new wxGridBagSizer();
  sb2D->Add(s2DGrid, 0, wxEXPAND);

  s2DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("Screen Height:")), wxGBPosition(0, 0),
               wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
  ScreenHeight = new wxSpinCtrlDouble(m_VR, ID_SCREEN_HEIGHT, "", wxDefaultPosition, wxDefaultSize,
                                      wxSP_ARROW_KEYS, 0.01, 10000, DEFAULT_VR_SCREEN_HEIGHT, 0.1);
  s2DGrid->Add(ScreenHeight, wxGBPosition(0, 1), wxDefaultSpan, wxALL, 5);
  s2DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("metres")), wxGBPosition(0, 2), wxDefaultSpan,
               wxALIGN_CENTER_VERTICAL | wxALL, 5);

  s2DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("Screen Distance:")), wxGBPosition(1, 0),
               wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
  ScreenDistance =
      new wxSpinCtrlDouble(m_VR, ID_SCREEN_DISTANCE, "", wxDefaultPosition, wxDefaultSize,
                           wxSP_ARROW_KEYS, 0.01, 10000, DEFAULT_VR_SCREEN_DISTANCE, 0.1);
  s2DGrid->Add(ScreenDistance, wxGBPosition(1, 1), wxDefaultSpan, wxALL, 5);
  s2DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("metres")), wxGBPosition(1, 2), wxDefaultSpan,
               wxALIGN_CENTER_VERTICAL | wxALL, 5);

  s2DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("Screen Thickness:")), wxGBPosition(2, 0),
               wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
  ScreenThickness =
      new wxSpinCtrlDouble(m_VR, ID_SCREEN_THICKNESS, "", wxDefaultPosition, wxDefaultSize,
                           wxSP_ARROW_KEYS, 0, 10000, DEFAULT_VR_SCREEN_THICKNESS, 0.1);
  s2DGrid->Add(ScreenThickness, wxGBPosition(2, 1), wxDefaultSpan, wxALL, 5);
  s2DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("metres")), wxGBPosition(2, 2), wxDefaultSpan,
               wxALIGN_CENTER_VERTICAL | wxALL, 5);

  s2DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("Screen Up:")), wxGBPosition(3, 0), wxDefaultSpan,
               wxALIGN_CENTER_VERTICAL | wxALL, 5);
  ScreenUp = new wxSpinCtrlDouble(m_VR, ID_SCREEN_UP, "", wxDefaultPosition, wxDefaultSize,
                                  wxSP_ARROW_KEYS, -10000, 10000, DEFAULT_VR_SCREEN_UP, 0.1);
  s2DGrid->Add(ScreenUp, wxGBPosition(3, 1), wxDefaultSpan, wxALL, 5);
  s2DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("metres")), wxGBPosition(3, 2), wxDefaultSpan,
               wxALIGN_CENTER_VERTICAL | wxALL, 5);

  s2DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("Screen Right:")), wxGBPosition(4, 0),
               wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
  ScreenRight = new wxSpinCtrlDouble(m_VR, ID_SCREEN_RIGHT, "", wxDefaultPosition, wxDefaultSize,
                                     wxSP_ARROW_KEYS, -10000, 10000, DEFAULT_VR_SCREEN_RIGHT, 0.1);
  s2DGrid->Add(ScreenRight, wxGBPosition(4, 1), wxDefaultSpan, wxALL, 5);
  s2DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("metres")), wxGBPosition(4, 2), wxDefaultSpan,
               wxALIGN_CENTER_VERTICAL | wxALL, 5);

  s2DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("Screen Pitch:")), wxGBPosition(5, 0),
               wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
  ScreenPitch = new wxSpinCtrlDouble(m_VR, ID_SCREEN_PITCH, "", wxDefaultPosition, wxDefaultSize,
                                     wxSP_ARROW_KEYS, -180, 360, DEFAULT_VR_SCREEN_PITCH, 1);
  s2DGrid->Add(ScreenPitch, wxGBPosition(5, 1), wxDefaultSpan, wxALL, 5);
  s2DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("degrees")), wxGBPosition(5, 2), wxDefaultSpan,
               wxALIGN_CENTER_VERTICAL | wxALL, 5);

  wxGridBagSizer* sVRGrid = new wxGridBagSizer();
  sbVR->Add(sVRGrid, 0, wxEXPAND);

  arrayStringFor_VRState.Add(_("Not Set"));
  arrayStringFor_VRState.Add(_("Unplayable"));
  arrayStringFor_VRState.Add(_("Bad"));
  arrayStringFor_VRState.Add(_("Playable"));
  arrayStringFor_VRState.Add(_("Good"));
  arrayStringFor_VRState.Add(_("Perfect"));
  VRState =
      new wxChoice(m_VR, ID_EMUSTATE, wxDefaultPosition, wxDefaultSize, arrayStringFor_VRState);
  sVRGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("VR state:")), wxGBPosition(0, 0), wxDefaultSpan,
               wxALIGN_CENTER_VERTICAL | wxALL, 5);
  sVRGrid->Add(VRState, wxGBPosition(0, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
  VRIssues = new wxTextCtrl(m_VR, ID_EMU_ISSUES, wxEmptyString);
  sbVR->Add(VRIssues, 0, wxEXPAND);

  // GameConfig editing - Overrides and emulation state
  wxStaticText* const OverrideText = new wxStaticText(
      m_GameConfig, wxID_ANY, _("These settings override core Dolphin settings.\nUndetermined "
                                "means the game uses Dolphin's setting."));

  // Core
  CPUThread = new wxCheckBox(m_GameConfig, ID_USEDUALCORE, _("Enable Dual Core"), wxDefaultPosition,
                             wxDefaultSize, GetElementStyle("Core", "CPUThread"));
  MMU = new wxCheckBox(m_GameConfig, ID_MMU, _("Enable MMU"), wxDefaultPosition, wxDefaultSize,
                       GetElementStyle("Core", "MMU"));
  MMU->SetToolTip(_(
      "Enables the Memory Management Unit, needed for some games. (ON = Compatible, OFF = Fast)"));
  DCBZOFF = new wxCheckBox(m_GameConfig, ID_DCBZOFF, _("Skip DCBZ clearing"), wxDefaultPosition,
                           wxDefaultSize, GetElementStyle("Core", "DCBZ"));
  DCBZOFF->SetToolTip(_("Bypass the clearing of the data cache by the DCBZ instruction. Usually "
                        "leave this option disabled."));
  FPRF = new wxCheckBox(m_GameConfig, ID_FPRF, _("Enable FPRF"), wxDefaultPosition, wxDefaultSize,
                        GetElementStyle("Core", "FPRF"));
  FPRF->SetToolTip(_("Enables Floating Point Result Flag calculation, needed for a few games. (ON "
                     "= Compatible, OFF = Fast)"));
  SyncGPU = new wxCheckBox(m_GameConfig, ID_SYNCGPU, _("Synchronize GPU thread"), wxDefaultPosition,
                           wxDefaultSize, GetElementStyle("Core", "SyncGPU"));
  SyncGPU->SetToolTip(_("Synchronizes the GPU and CPU threads to help prevent random freezes in "
                        "Dual Core mode. (ON = Compatible, OFF = Fast)"));
  FastDiscSpeed =
      new wxCheckBox(m_GameConfig, ID_DISCSPEED, _("Speed up Disc Transfer Rate"),
                     wxDefaultPosition, wxDefaultSize, GetElementStyle("Core", "FastDiscSpeed"));
  FastDiscSpeed->SetToolTip(_("Enable fast disc access. This can cause crashes and other problems "
                              "in some games. (ON = Fast, OFF = Compatible)"));
  DSPHLE = new wxCheckBox(m_GameConfig, ID_AUDIO_DSP_HLE, _("DSP HLE emulation (fast)"),
                          wxDefaultPosition, wxDefaultSize, GetElementStyle("Core", "DSPHLE"));

  wxBoxSizer* const sAudioSlowDown = new wxBoxSizer(wxHORIZONTAL);
  wxStaticText* const AudioSlowDownText =
      new wxStaticText(m_GameConfig, wxID_ANY, _("Frame Rate Hack Audio Synchronization: "));
  AudioSlowDown = new wxSpinCtrlDouble(m_GameConfig, ID_AUDIOSLOWDOWN, "", wxDefaultPosition,
                                       wxDefaultSize, wxSP_ARROW_KEYS, 0.01, 10, 1, 0.01);
  AudioSlowDown->SetToolTip(
      _("Leave at 1 unless using a frame rate hack. Input a multiplier equivilent to the amount "
        "the frame rate has been altered. e.g. If a 30fps game is hacked to 75fps, input 2.5"));
  sAudioSlowDown->Add(AudioSlowDownText);
  sAudioSlowDown->Add(AudioSlowDown);

  wxBoxSizer* const sGPUDeterminism = new wxBoxSizer(wxHORIZONTAL);
  wxStaticText* const GPUDeterminismText =
      new wxStaticText(m_GameConfig, wxID_ANY, _("Deterministic dual core: "));
  arrayStringFor_GPUDeterminism.Add(_("Not Set"));
  arrayStringFor_GPUDeterminism.Add(_("auto"));
  arrayStringFor_GPUDeterminism.Add(_("none"));
  arrayStringFor_GPUDeterminism.Add(_("fake-completion"));
  GPUDeterminism = new wxChoice(m_GameConfig, ID_GPUDETERMINISM, wxDefaultPosition, wxDefaultSize,
                                arrayStringFor_GPUDeterminism);
  sGPUDeterminism->Add(GPUDeterminismText, 0, wxALIGN_CENTER_VERTICAL);
  sGPUDeterminism->Add(GPUDeterminism, 0, wxALIGN_CENTER_VERTICAL);

  // Wii Console
  EnableWideScreen =
      new wxCheckBox(m_GameConfig, ID_ENABLEWIDESCREEN, _("Enable WideScreen"), wxDefaultPosition,
                     wxDefaultSize, GetElementStyle("Wii", "Widescreen"));

  // Stereoscopy
  wxBoxSizer* const sDepthPercentage = new wxBoxSizer(wxHORIZONTAL);
  wxStaticText* const DepthPercentageText =
      new wxStaticText(m_GameConfig, wxID_ANY, _("Depth Percentage: "));
  DepthPercentage = new DolphinSlider(m_GameConfig, ID_DEPTHPERCENTAGE, 100, 0, 200);
  DepthPercentage->SetToolTip(
      _("This value is multiplied with the depth set in the graphics configuration."));
  sDepthPercentage->Add(DepthPercentageText);
  sDepthPercentage->Add(DepthPercentage);

  wxBoxSizer* const sConvergence = new wxBoxSizer(wxHORIZONTAL);
  wxStaticText* const ConvergenceText =
      new wxStaticText(m_GameConfig, wxID_ANY, _("Convergence: "));
  Convergence = new wxSpinCtrl(m_GameConfig, ID_CONVERGENCE);
  Convergence->SetRange(0, INT32_MAX);
  Convergence->SetToolTip(
      _("This value is added to the convergence value set in the graphics configuration."));
  sConvergence->Add(ConvergenceText);
  sConvergence->Add(Convergence);

  MonoDepth =
      new wxCheckBox(m_GameConfig, ID_MONODEPTH, _("Monoscopic Shadows"), wxDefaultPosition,
                     wxDefaultSize, GetElementStyle("Video_Stereoscopy", "StereoEFBMonoDepth"));
  MonoDepth->SetToolTip(_("Use a single depth buffer for both eyes. Needed for a few games."));

  wxBoxSizer* const sEmuState = new wxBoxSizer(wxHORIZONTAL);
  wxStaticText* const EmuStateText =
      new wxStaticText(m_GameConfig, wxID_ANY, _("Emulation State: "));
  arrayStringFor_EmuState.Add(_("Not Set"));
  arrayStringFor_EmuState.Add(_("Broken"));
  arrayStringFor_EmuState.Add(_("Intro"));
  arrayStringFor_EmuState.Add(_("In Game"));
  arrayStringFor_EmuState.Add(_("Playable"));
  arrayStringFor_EmuState.Add(_("Perfect"));
  EmuState = new wxChoice(m_GameConfig, ID_EMUSTATE, wxDefaultPosition, wxDefaultSize,
                          arrayStringFor_EmuState);
  EmuIssues = new wxTextCtrl(m_GameConfig, ID_EMU_ISSUES, wxEmptyString);
  sEmuState->Add(EmuStateText, 0, wxALIGN_CENTER_VERTICAL);
  sEmuState->Add(EmuState, 0, wxALIGN_CENTER_VERTICAL);
  sEmuState->Add(EmuIssues, 1, wxEXPAND);

  wxStaticBoxSizer* const sbCoreOverrides =
      new wxStaticBoxSizer(wxVERTICAL, m_GameConfig, _("Core"));
  sbCoreOverrides->Add(CPUThread, 0, wxLEFT | wxRIGHT, space5);
  sbCoreOverrides->Add(MMU, 0, wxLEFT | wxRIGHT, space5);
  sbCoreOverrides->Add(DCBZOFF, 0, wxLEFT | wxRIGHT, space5);
  sbCoreOverrides->Add(FPRF, 0, wxLEFT | wxRIGHT, space5);
  sbCoreOverrides->Add(SyncGPU, 0, wxLEFT | wxRIGHT, space5);
  sbCoreOverrides->Add(FastDiscSpeed, 0, wxLEFT | wxRIGHT, space5);
  sbCoreOverrides->Add(DSPHLE, 0, wxLEFT | wxRIGHT, space5);
  sbCoreOverrides->AddSpacer(space5);
  sbCoreOverrides->Add(sGPUDeterminism, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  sbCoreOverrides->AddSpacer(space5);
  sbCoreOverrides->Add(sAudioSlowDown, 0, wxEXPAND | wxALL, 5);

  wxStaticBoxSizer* const sbWiiOverrides =
      new wxStaticBoxSizer(wxVERTICAL, m_GameConfig, _("Wii Console"));
  if (m_open_iso->GetVolumeType() == DiscIO::Platform::GAMECUBE_DISC)
  {
    sbWiiOverrides->ShowItems(false);
    EnableWideScreen->Hide();
  }
  sbWiiOverrides->Add(EnableWideScreen, 0, wxLEFT, space5);

  wxStaticBoxSizer* const sbStereoOverrides =
      new wxStaticBoxSizer(wxVERTICAL, m_GameConfig, _("Stereoscopy"));
  sbStereoOverrides->Add(sDepthPercentage);
  sbStereoOverrides->Add(sConvergence);
  sbStereoOverrides->Add(MonoDepth);

  wxStaticBoxSizer* const sbGameConfig =
      new wxStaticBoxSizer(wxVERTICAL, m_GameConfig, _("Game-Specific Settings"));
  sbGameConfig->AddSpacer(space5);
  sbGameConfig->Add(OverrideText, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  sbGameConfig->AddSpacer(space5);
  sbGameConfig->Add(sbCoreOverrides, 0, wxEXPAND);
  sbGameConfig->Add(sbWiiOverrides, 0, wxEXPAND);
  sbGameConfig->Add(sbStereoOverrides, 0, wxEXPAND);

  wxBoxSizer* const sConfigPage = new wxBoxSizer(wxVERTICAL);
  sConfigPage->AddSpacer(space5);
  sConfigPage->Add(sbGameConfig, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  sConfigPage->AddSpacer(space5);
  sConfigPage->Add(sEmuState, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  sConfigPage->AddSpacer(space5);
  m_GameConfig->SetSizer(sConfigPage);

  // Hide Object Range
  wxBoxSizer* const sHideObjects = new wxBoxSizer(wxVERTICAL);

  wxStaticBoxSizer* const sbHideObjectRange =
      new wxStaticBoxSizer(wxVERTICAL, m_HideObjectPage, _("Hide Object Range"));
  sHideObjects->Add(sbHideObjectRange, 0, wxEXPAND | wxALL, 5);
  wxGridBagSizer* sbHideObjectRangeGrid = new wxGridBagSizer();
  sbHideObjectRange->Add(sbHideObjectRangeGrid, 0, wxEXPAND);

  sbHideObjectRangeGrid->Add(new wxStaticText(m_HideObjectPage, wxID_ANY, _("From:")),
                             wxGBPosition(0, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
  sbHideObjectRangeGrid->Add(new wxStaticText(m_HideObjectPage, wxID_ANY, _("To:")),
                             wxGBPosition(0, 2), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);

  U32Setting* HideObjectsStart = new U32Setting(
      m_HideObjectPage, _("Hide Start"), SConfig::GetInstance().skip_objects_start, 0, 100000);
  U32Setting* HideObjectsEnd = new U32Setting(m_HideObjectPage, _("Hide End"),
                                              SConfig::GetInstance().skip_objects_end, 0, 100000);
  sbHideObjectRangeGrid->Add(HideObjectsStart, wxGBPosition(0, 1), wxDefaultSpan, wxALL, 5);
  sbHideObjectRangeGrid->Add(HideObjectsEnd, wxGBPosition(0, 3), wxDefaultSpan, wxALL, 5);

#ifdef DEBUG_OBJECTS

  sbHideObjectRangeGrid->Add(new wxStaticText(m_HideObjectPage, wxID_ANY, _("From:")),
                             wxGBPosition(1, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
  sbHideObjectRangeGrid->Add(new wxStaticText(m_HideObjectPage, wxID_ANY, _("To:")),
                             wxGBPosition(1, 2), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);

  U32Setting* HideObjectsStartTwo =
      new U32Setting(m_HideObjectPage, _("Hide Start Two"),
                     SConfig::GetInstance().skip_objects_start_two, 0, 100000);
  U32Setting* HideObjectsEndTwo = new U32Setting(
      m_HideObjectPage, _("Hide End Two"), SConfig::GetInstance().skip_objects_end_two, 0, 100000);
  sbHideObjectRangeGrid->Add(HideObjectsStartTwo, wxGBPosition(1, 1), wxDefaultSpan, wxALL, 5);
  sbHideObjectRangeGrid->Add(HideObjectsEndTwo, wxGBPosition(1, 3), wxDefaultSpan, wxALL, 5);

#endif
  // Hide Object Code
  HideObjects = new wxCheckListBox(m_HideObjectPage, ID_HIDEOBJECTS_LIST, wxDefaultPosition,
                                   wxDefaultSize, 0, nullptr, wxLB_HSCROLL);
  wxBoxSizer* const sHideObjectsButtons = new wxBoxSizer(wxHORIZONTAL);
  EditHideObject = new wxButton(m_HideObjectPage, ID_EDITHIDEOBJECT, _("Edit..."));
  wxButton* const AddHideObject = new wxButton(m_HideObjectPage, ID_ADDHideObject, _("Add..."));
  RemoveHideObject = new wxButton(m_HideObjectPage, ID_REMOVEHIDEOBJECT, _("Remove"));
  EditHideObject->Enable(false);
  RemoveHideObject->Enable(false);

  wxBoxSizer* sHideObjectPage = new wxBoxSizer(wxVERTICAL);
  sHideObjects->Add(HideObjects, 1, wxEXPAND | wxALL);
  sHideObjectsButtons->Add(EditHideObject, 0, wxEXPAND | wxALL);
  sHideObjectsButtons->AddStretchSpacer();
  sHideObjectsButtons->Add(AddHideObject, 0, wxEXPAND | wxALL);
  sHideObjectsButtons->Add(RemoveHideObject, 0, wxEXPAND | wxALL);
  sHideObjects->Add(sHideObjectsButtons, 0, wxEXPAND | wxALL);
  sHideObjectPage->Add(sHideObjects, 1, wxEXPAND | wxALL, 5);
  m_HideObjectPage->SetSizer(sHideObjectPage);

  // Patches
  wxBoxSizer* const sPatches = new wxBoxSizer(wxVERTICAL);
  Patches = new wxCheckListBox(m_PatchPage, ID_PATCHES_LIST, wxDefaultPosition, wxDefaultSize, 0,
                               nullptr, wxLB_HSCROLL);
  wxBoxSizer* const sPatchButtons = new wxBoxSizer(wxHORIZONTAL);
  EditPatch = new wxButton(m_PatchPage, ID_EDITPATCH, _("Edit..."));
  wxButton* const AddPatch = new wxButton(m_PatchPage, ID_ADDPATCH, _("Add..."));
  RemovePatch = new wxButton(m_PatchPage, ID_REMOVEPATCH, _("Remove"));
  EditPatch->Disable();
  RemovePatch->Disable();

  wxBoxSizer* sPatchPage = new wxBoxSizer(wxVERTICAL);
  sPatches->Add(Patches, 1, wxEXPAND);
  sPatchButtons->Add(EditPatch, 0, wxEXPAND);
  sPatchButtons->AddStretchSpacer();
  sPatchButtons->Add(AddPatch, 0, wxEXPAND);
  sPatchButtons->Add(RemovePatch, 0, wxEXPAND);
  sPatches->Add(sPatchButtons, 0, wxEXPAND);
  sPatchPage->AddSpacer(space5);
  sPatchPage->Add(sPatches, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);
  sPatchPage->AddSpacer(space5);
  m_PatchPage->SetSizer(sPatchPage);

  // Action Replay Cheats
  m_ar_code_panel =
      new ActionReplayCodesPanel(m_CheatPage, ActionReplayCodesPanel::STYLE_MODIFY_BUTTONS);
  m_cheats_disabled_ar = new CheatWarningMessage(m_CheatPage, game_id);

  m_ar_code_panel->Bind(DOLPHIN_EVT_ARCODE_TOGGLED, &CISOProperties::OnCheatCodeToggled, this);

  wxBoxSizer* const sCheatPage = new wxBoxSizer(wxVERTICAL);
  sCheatPage->Add(m_cheats_disabled_ar, 0, wxEXPAND | wxTOP, space5);
  sCheatPage->Add(m_ar_code_panel, 1, wxEXPAND | wxALL, space5);
  m_CheatPage->SetSizer(sCheatPage);

  // Gecko Cheats
  m_geckocode_panel = new Gecko::CodeConfigPanel(gecko_cheat_page);
  m_cheats_disabled_gecko = new CheatWarningMessage(gecko_cheat_page, game_id);

  m_geckocode_panel->Bind(DOLPHIN_EVT_GECKOCODE_TOGGLED, &CISOProperties::OnCheatCodeToggled, this);

  wxBoxSizer* gecko_layout = new wxBoxSizer(wxVERTICAL);
  gecko_layout->Add(m_cheats_disabled_gecko, 0, wxEXPAND | wxTOP, space5);
  gecko_layout->Add(m_geckocode_panel, 1, wxEXPAND);
  gecko_cheat_page->SetSizer(gecko_layout);

  if (m_open_iso->GetVolumeType() != DiscIO::Platform::WII_WAD)
  {
    m_Notebook->AddPage(new FilesystemPanel(m_Notebook, ID_FILESYSTEM, m_open_iso),
                        _("Filesystem"));
  }

  wxStdDialogButtonSizer* sButtons = CreateStdDialogButtonSizer(wxOK | wxNO_DEFAULT);
  sButtons->Prepend(EditConfigDefault);
  sButtons->Prepend(EditConfig);
  sButtons->GetAffirmativeButton()->SetLabel(_("Close"));

  // If there is no default gameini, disable the button.
  bool game_ini_exists = false;
  for (const std::string& ini_filename :
       SConfig::GetGameIniFilenames(game_id, m_open_iso->GetRevision()))
  {
    if (File::Exists(File::GetSysDirectory() + GAMESETTINGS_DIR DIR_SEP + ini_filename))
    {
      game_ini_exists = true;
      break;
    }
  }
  if (!game_ini_exists)
    EditConfigDefault->Disable();

  // Add notebook and buttons to the dialog
  wxBoxSizer* sMain = new wxBoxSizer(wxVERTICAL);
  sMain->AddSpacer(space5);
  sMain->Add(m_Notebook, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);
  sMain->AddSpacer(space5);
  sMain->Add(sButtons, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  sMain->AddSpacer(space5);
  sMain->SetMinSize(FromDIP(wxSize(500, -1)));

  SetLayoutAdaptationMode(wxDIALOG_ADAPTATION_MODE_ENABLED);
  SetLayoutAdaptationLevel(wxDIALOG_ADAPTATION_STANDARD_SIZER);
  SetSizerAndFit(sMain);
  Center();
  SetFocus();
}

void CISOProperties::OnClose(wxCloseEvent& WXUNUSED(event))
{
  if (!SaveGameConfig())
    WxUtils::ShowErrorDialog(wxString::Format(_("Could not save %s."), GameIniFileLocal.c_str()));
  Destroy();
}

void CISOProperties::OnCloseClick(wxCommandEvent& WXUNUSED(event))
{
  Close();
}

void CISOProperties::OnEmustateChanged(wxCommandEvent& event)
{
  EmuIssues->Enable(event.GetSelection() != 0);
}

void CISOProperties::SetCheckboxValueFromGameini(const char* section, const char* key,
                                                 wxCheckBox* checkbox)
{
  // Prefer local gameini value over default gameini value.
  bool value;
  if (GameIniLocal.GetOrCreateSection(section)->Get(key, &value))
    checkbox->Set3StateValue((wxCheckBoxState)value);
  else if (GameIniDefault.GetOrCreateSection(section)->Get(key, &value))
    checkbox->Set3StateValue((wxCheckBoxState)value);
  else
    checkbox->Set3StateValue(wxCHK_UNDETERMINED);
}

void CISOProperties::LoadGameConfig()
{
  SetCheckboxValueFromGameini("Core", "CPUThread", CPUThread);
  SetCheckboxValueFromGameini("Core", "MMU", MMU);
  SetCheckboxValueFromGameini("Core", "DCBZ", DCBZOFF);
  SetCheckboxValueFromGameini("Core", "FPRF", FPRF);
  SetCheckboxValueFromGameini("Core", "SyncGPU", SyncGPU);
  SetCheckboxValueFromGameini("Core", "FastDiscSpeed", FastDiscSpeed);
  SetCheckboxValueFromGameini("Core", "DSPHLE", DSPHLE);
  SetCheckboxValueFromGameini("Wii", "Widescreen", EnableWideScreen);
  SetCheckboxValueFromGameini("Video_Stereoscopy", "StereoEFBMonoDepth", MonoDepth);

  IniFile::Section* default_video = GameIniDefault.GetOrCreateSection("Video");

  int iTemp;
  default_video->Get("ProjectionHack", &iTemp);
  default_video->Get("PH_SZNear", &m_PHack_Data.PHackSZNear);
  if (GameIniLocal.GetIfExists("Video", "PH_SZNear", &iTemp))
    m_PHack_Data.PHackSZNear = !!iTemp;
  default_video->Get("PH_SZFar", &m_PHack_Data.PHackSZFar);
  if (GameIniLocal.GetIfExists("Video", "PH_SZFar", &iTemp))
    m_PHack_Data.PHackSZFar = !!iTemp;

  std::string sTemp;
  default_video->Get("PH_ZNear", &m_PHack_Data.PHZNear);
  if (GameIniLocal.GetIfExists("Video", "PH_ZNear", &sTemp))
    m_PHack_Data.PHZNear = sTemp;
  default_video->Get("PH_ZFar", &m_PHack_Data.PHZFar);
  if (GameIniLocal.GetIfExists("Video", "PH_ZFar", &sTemp))
    m_PHack_Data.PHZFar = sTemp;

  IniFile::Section* default_emustate = GameIniDefault.GetOrCreateSection("EmuState");
  default_emustate->Get("EmulationStateId", &iTemp, 0 /*Not Set*/);
  EmuState->SetSelection(iTemp);
  if (GameIniLocal.GetIfExists("EmuState", "EmulationStateId", &iTemp))
    EmuState->SetSelection(iTemp);

  default_emustate->Get("EmulationIssues", &sTemp);
  if (!sTemp.empty())
    EmuIssues->SetValue(StrToWxStr(sTemp));
  if (GameIniLocal.GetIfExists("EmuState", "EmulationIssues", &sTemp))
    EmuIssues->SetValue(StrToWxStr(sTemp));

  EmuIssues->Enable(EmuState->GetSelection() != 0);

  sTemp = "";
  if (!GameIniLocal.GetIfExists("Core", "GPUDeterminismMode", &sTemp))
    GameIniDefault.GetIfExists("Core", "GPUDeterminismMode", &sTemp);

  if (sTemp == "")
    GPUDeterminism->SetSelection(0);
  else if (sTemp == "auto")
    GPUDeterminism->SetSelection(1);
  else if (sTemp == "none")
    GPUDeterminism->SetSelection(2);
  else if (sTemp == "fake-completion")
    GPUDeterminism->SetSelection(3);

  float fTemp;

  fTemp = 1;
  GameIniDefault.GetIfExists("Core", "AudioSlowDown", &fTemp);
  GameIniLocal.GetIfExists("Core", "AudioSlowDown", &fTemp);
  AudioSlowDown->SetValue(fTemp);

  IniFile::Section* default_stereoscopy = GameIniDefault.GetOrCreateSection("Video_Stereoscopy");
  default_stereoscopy->Get("StereoDepthPercentage", &iTemp, 100);
  GameIniLocal.GetIfExists("Video_Stereoscopy", "StereoDepthPercentage", &iTemp);
  DepthPercentage->SetValue(iTemp);
  default_stereoscopy->Get("StereoConvergence", &iTemp, 0);
  GameIniLocal.GetIfExists("Video_Stereoscopy", "StereoConvergence", &iTemp);
  Convergence->SetValue(iTemp);
  // SetCheckboxValueFromGameini("VR", "Disable3D", Disable3D);
  SetCheckboxValueFromGameini("VR", "HudFullscreen", HudFullscreen);
  SetCheckboxValueFromGameini("VR", "HudOnTop", HudOnTop);

  fTemp = DEFAULT_VR_UNITS_PER_METRE;
  if (GameIniDefault.GetIfExists("VR", "UnitsPerMetre", &fTemp))
    UnitsPerMetre->SetValue(fTemp);
  if (GameIniLocal.GetIfExists("VR", "UnitsPerMetre", &fTemp))
    UnitsPerMetre->SetValue(fTemp);

  fTemp = DEFAULT_VR_HUD_DISTANCE;
  if (GameIniDefault.GetIfExists("VR", "HudDistance", &fTemp))
    HudDistance->SetValue(fTemp);
  if (GameIniLocal.GetIfExists("VR", "HudDistance", &fTemp))
    HudDistance->SetValue(fTemp);

  fTemp = DEFAULT_VR_HUD_THICKNESS;
  if (GameIniDefault.GetIfExists("VR", "HudThickness", &fTemp))
    HudThickness->SetValue(fTemp);
  if (GameIniLocal.GetIfExists("VR", "HudThickness", &fTemp))
    HudThickness->SetValue(fTemp);

  fTemp = DEFAULT_VR_HUD_3D_CLOSER;
  if (GameIniDefault.GetIfExists("VR", "Hud3DCloser", &fTemp))
    Hud3DCloser->SetValue(fTemp);
  if (GameIniLocal.GetIfExists("VR", "Hud3DCloser", &fTemp))
    Hud3DCloser->SetValue(fTemp);

  fTemp = DEFAULT_VR_CAMERA_FORWARD;
  if (GameIniDefault.GetIfExists("VR", "CameraForward", &fTemp))
    CameraForward->SetValue(fTemp);
  if (GameIniLocal.GetIfExists("VR", "CameraForward", &fTemp))
    CameraForward->SetValue(fTemp);

  fTemp = DEFAULT_VR_CAMERA_PITCH;
  if (GameIniDefault.GetIfExists("VR", "CameraPitch", &fTemp))
    CameraPitch->SetValue(fTemp);
  if (GameIniLocal.GetIfExists("VR", "CameraPitch", &fTemp))
    CameraPitch->SetValue(fTemp);

  fTemp = DEFAULT_VR_AIM_DISTANCE;
  if (GameIniDefault.GetIfExists("VR", "AimDistance", &fTemp))
    AimDistance->SetValue(fTemp);
  if (GameIniLocal.GetIfExists("VR", "AimDistance", &fTemp))
    AimDistance->SetValue(fTemp);

  fTemp = DEFAULT_VR_MIN_FOV;
  if (GameIniDefault.GetIfExists("VR", "MinFOV", &fTemp))
    MinFOV->SetValue(fTemp);
  if (GameIniLocal.GetIfExists("VR", "MinFOV", &fTemp))
    MinFOV->SetValue(fTemp);

  fTemp = DEFAULT_VR_SCREEN_HEIGHT;
  if (GameIniDefault.GetIfExists("VR", "ScreenHeight", &fTemp))
    ScreenHeight->SetValue(fTemp);
  if (GameIniLocal.GetIfExists("VR", "ScreenHeight", &fTemp))
    ScreenHeight->SetValue(fTemp);

  fTemp = DEFAULT_VR_SCREEN_DISTANCE;
  if (GameIniDefault.GetIfExists("VR", "ScreenDistance", &fTemp))
    ScreenDistance->SetValue(fTemp);
  if (GameIniLocal.GetIfExists("VR", "ScreenDistance", &fTemp))
    ScreenDistance->SetValue(fTemp);

  fTemp = DEFAULT_VR_SCREEN_THICKNESS;
  if (GameIniDefault.GetIfExists("VR", "ScreenThickness", &fTemp))
    ScreenThickness->SetValue(fTemp);
  if (GameIniLocal.GetIfExists("VR", "ScreenThickness", &fTemp))
    ScreenThickness->SetValue(fTemp);

  fTemp = DEFAULT_VR_SCREEN_UP;
  if (GameIniDefault.GetIfExists("VR", "ScreenUp", &fTemp))
    ScreenUp->SetValue(fTemp);
  if (GameIniLocal.GetIfExists("VR", "ScreenUp", &fTemp))
    ScreenUp->SetValue(fTemp);

  fTemp = DEFAULT_VR_SCREEN_RIGHT;
  if (GameIniDefault.GetIfExists("VR", "ScreenRight", &fTemp))
    ScreenRight->SetValue(fTemp);
  if (GameIniLocal.GetIfExists("VR", "ScreenRight", &fTemp))
    ScreenRight->SetValue(fTemp);

  fTemp = DEFAULT_VR_SCREEN_PITCH;
  if (GameIniDefault.GetIfExists("VR", "ScreenPitch", &fTemp))
    ScreenPitch->SetValue(fTemp);
  if (GameIniLocal.GetIfExists("VR", "ScreenPitch", &fTemp))
    ScreenPitch->SetValue(fTemp);

  iTemp = 0;
  GameIniDefault.GetIfExists("VR", "VRStateId", &iTemp);
  VRState->SetSelection(iTemp);
  if (GameIniLocal.GetIfExists("VR", "VRStateId", &iTemp))
    VRState->SetSelection(iTemp);

  sTemp = "";
  GameIniDefault.GetIfExists("VR", "VRIssues", &sTemp);
  if (!sTemp.empty())
    VRIssues->SetValue(StrToWxStr(sTemp));
  if (GameIniLocal.GetIfExists("VR", "VRIssues", &sTemp))
    VRIssues->SetValue(StrToWxStr(sTemp));

  HideObjectList_Load();
  PatchList_Load();
  m_ar_code_panel->LoadCodes(GameIniDefault, GameIniLocal);
  m_geckocode_panel->LoadCodes(GameIniDefault, GameIniLocal, m_open_iso->GetGameID());
}

void CISOProperties::SaveGameIniValueFrom3StateCheckbox(const char* section, const char* key,
                                                        wxCheckBox* checkbox)
{
  // Delete any existing entries from the local gameini if checkbox is undetermined.
  // Otherwise, write the current value to the local gameini if the value differs from the default
  // gameini values.
  // Delete any existing entry from the local gameini if the value does not differ from the default
  // gameini value.
  bool checkbox_val = (checkbox->Get3StateValue() == wxCHK_CHECKED);

  if (checkbox->Get3StateValue() == wxCHK_UNDETERMINED)
    GameIniLocal.DeleteKey(section, key);
  else if (!GameIniDefault.Exists(section, key))
    GameIniLocal.GetOrCreateSection(section)->Set(key, checkbox_val);
  else
  {
    bool default_value;
    GameIniDefault.GetOrCreateSection(section)->Get(key, &default_value);
    if (default_value != checkbox_val)
      GameIniLocal.GetOrCreateSection(section)->Set(key, checkbox_val);
    else
      GameIniLocal.DeleteKey(section, key);
  }
}

bool CISOProperties::SaveGameConfig()
{
  SaveGameIniValueFrom3StateCheckbox("Core", "CPUThread", CPUThread);
  SaveGameIniValueFrom3StateCheckbox("Core", "MMU", MMU);
  SaveGameIniValueFrom3StateCheckbox("Core", "DCBZ", DCBZOFF);
  SaveGameIniValueFrom3StateCheckbox("Core", "FPRF", FPRF);
  SaveGameIniValueFrom3StateCheckbox("Core", "SyncGPU", SyncGPU);
  SaveGameIniValueFrom3StateCheckbox("Core", "FastDiscSpeed", FastDiscSpeed);
  SaveGameIniValueFrom3StateCheckbox("Core", "DSPHLE", DSPHLE);
  SaveGameIniValueFrom3StateCheckbox("Wii", "Widescreen", EnableWideScreen);
  SaveGameIniValueFrom3StateCheckbox("Video_Stereoscopy", "StereoEFBMonoDepth", MonoDepth);

#define SAVE_IF_NOT_DEFAULT(section, key, val, def)                                                \
  do                                                                                               \
  {                                                                                                \
    if (GameIniDefault.Exists((section), (key)))                                                   \
    {                                                                                              \
      std::remove_reference<decltype((val))>::type tmp__;                                          \
      GameIniDefault.GetOrCreateSection((section))->Get((key), &tmp__);                            \
      if ((val) != tmp__)                                                                          \
        GameIniLocal.GetOrCreateSection((section))->Set((key), (val));                             \
      else                                                                                         \
        GameIniLocal.DeleteKey((section), (key));                                                  \
    }                                                                                              \
    else if ((val) != (def))                                                                       \
      GameIniLocal.GetOrCreateSection((section))->Set((key), (val));                               \
    else                                                                                           \
      GameIniLocal.DeleteKey((section), (key));                                                    \
  } while (0)

  SAVE_IF_NOT_DEFAULT("Video", "PH_SZNear", (m_PHack_Data.PHackSZNear ? 1 : 0), 0);
  SAVE_IF_NOT_DEFAULT("Video", "PH_SZFar", (m_PHack_Data.PHackSZFar ? 1 : 0), 0);
  SAVE_IF_NOT_DEFAULT("Video", "PH_ZNear", m_PHack_Data.PHZNear, "");
  SAVE_IF_NOT_DEFAULT("Video", "PH_ZFar", m_PHack_Data.PHZFar, "");
  SAVE_IF_NOT_DEFAULT("EmuState", "EmulationStateId", EmuState->GetSelection(), 0);

  std::string emu_issues = EmuIssues->GetValue().ToStdString();
  SAVE_IF_NOT_DEFAULT("EmuState", "EmulationIssues", emu_issues, "");

  std::string tmp;
  if (GPUDeterminism->GetSelection() == 0)
    tmp = "Not Set";
  else if (GPUDeterminism->GetSelection() == 1)
    tmp = "auto";
  else if (GPUDeterminism->GetSelection() == 2)
    tmp = "none";
  else if (GPUDeterminism->GetSelection() == 3)
    tmp = "fake-completion";

  SAVE_IF_NOT_DEFAULT("Core", "GPUDeterminismMode", tmp, "Not Set");
  SAVE_IF_NOT_DEFAULT("Core", "AudioSlowDown", AudioSlowDown->GetValue(), 1.00f);

  int depth = DepthPercentage->GetValue() > 0 ? DepthPercentage->GetValue() : 100;
  SAVE_IF_NOT_DEFAULT("Video_Stereoscopy", "StereoDepthPercentage", depth, 100);
  SAVE_IF_NOT_DEFAULT("Video_Stereoscopy", "StereoConvergence", Convergence->GetValue(), 0);
  // SaveGameIniValueFrom3StateCheckbox("VR", "Disable3D", Disable3D);
  SaveGameIniValueFrom3StateCheckbox("VR", "HudFullscreen", HudFullscreen);
  SaveGameIniValueFrom3StateCheckbox("VR", "HudOnTop", HudOnTop);
  SAVE_IF_NOT_DEFAULT("VR", "UnitsPerMetre", (float)UnitsPerMetre->GetValue(),
                      DEFAULT_VR_UNITS_PER_METRE);
  SAVE_IF_NOT_DEFAULT("VR", "HudDistance", (float)HudDistance->GetValue(), DEFAULT_VR_HUD_DISTANCE);
  SAVE_IF_NOT_DEFAULT("VR", "HudThickness", (float)HudThickness->GetValue(),
                      DEFAULT_VR_HUD_THICKNESS);
  SAVE_IF_NOT_DEFAULT("VR", "Hud3DCloser", (float)Hud3DCloser->GetValue(),
                      DEFAULT_VR_HUD_3D_CLOSER);
  SAVE_IF_NOT_DEFAULT("VR", "CameraForward", (float)CameraForward->GetValue(),
                      DEFAULT_VR_CAMERA_FORWARD);
  SAVE_IF_NOT_DEFAULT("VR", "CameraPitch", (float)CameraPitch->GetValue(), DEFAULT_VR_CAMERA_PITCH);
  SAVE_IF_NOT_DEFAULT("VR", "AimDistance", (float)AimDistance->GetValue(), DEFAULT_VR_AIM_DISTANCE);
  SAVE_IF_NOT_DEFAULT("VR", "MinFOV", (float)MinFOV->GetValue(), DEFAULT_VR_MIN_FOV);
  SAVE_IF_NOT_DEFAULT("VR", "ScreenHeight", (float)ScreenHeight->GetValue(),
                      DEFAULT_VR_SCREEN_HEIGHT);
  SAVE_IF_NOT_DEFAULT("VR", "ScreenDistance", (float)ScreenDistance->GetValue(),
                      DEFAULT_VR_SCREEN_DISTANCE);
  SAVE_IF_NOT_DEFAULT("VR", "ScreenThickness", (float)ScreenThickness->GetValue(),
                      DEFAULT_VR_SCREEN_THICKNESS);
  SAVE_IF_NOT_DEFAULT("VR", "ScreenUp", (float)ScreenUp->GetValue(), DEFAULT_VR_SCREEN_UP);
  SAVE_IF_NOT_DEFAULT("VR", "ScreenRight", (float)ScreenRight->GetValue(), DEFAULT_VR_SCREEN_RIGHT);
  SAVE_IF_NOT_DEFAULT("VR", "ScreenPitch", (float)ScreenPitch->GetValue(), DEFAULT_VR_SCREEN_PITCH);
  SAVE_IF_NOT_DEFAULT("VR", "VRStateId", VRState->GetSelection(), 0);
  emu_issues = VRIssues->GetValue().ToStdString();
  SAVE_IF_NOT_DEFAULT("VR", "VRIssues", emu_issues, "");

  PatchList_Save();
  m_ar_code_panel->SaveCodes(&GameIniLocal);
  Gecko::SaveCodes(GameIniLocal, m_geckocode_panel->GetCodes());

  bool success = GameIniLocal.Save(GameIniFileLocal);

  // If the resulting file is empty, delete it. Kind of a hack, but meh.
  if (success && File::GetSize(GameIniFileLocal) == 0)
    File::Delete(GameIniFileLocal);

  if (success)
    GenerateLocalIniModified();

  return success;
}

void CISOProperties::LaunchExternalEditor(const std::string& filename, bool wait_until_closed)
{
#ifdef __APPLE__
  // GetOpenCommand does not work for wxCocoa
  const char* OpenCommandConst[] = {"open", "-a", "TextEdit", filename.c_str(), NULL};
  char** OpenCommand = const_cast<char**>(OpenCommandConst);
#else
  wxFileType* filetype = wxTheMimeTypesManager->GetFileTypeFromExtension("ini");
  if (filetype == nullptr)  // From extension failed, trying with MIME type now
  {
    filetype = wxTheMimeTypesManager->GetFileTypeFromMimeType("text/plain");
    if (filetype == nullptr)  // MIME type failed, aborting mission
    {
      WxUtils::ShowErrorDialog(_("Filetype 'ini' is unknown! Will not open!"));
      return;
    }
  }

  wxString OpenCommand = filetype->GetOpenCommand(StrToWxStr(filename));
  if (OpenCommand.IsEmpty())
  {
    WxUtils::ShowErrorDialog(_("Couldn't find open command for extension 'ini'!"));
    return;
  }
#endif

  long result;

  if (wait_until_closed)
    result = wxExecute(OpenCommand, wxEXEC_SYNC);
  else
    result = wxExecute(OpenCommand);

  if (result == -1)
  {
    WxUtils::ShowErrorDialog(_("wxExecute returned -1 on application run!"));
    return;
  }
}

void CISOProperties::GenerateLocalIniModified()
{
  wxCommandEvent event_update(DOLPHIN_EVT_LOCAL_INI_CHANGED);
  event_update.SetString(StrToWxStr(game_id));
  event_update.SetInt(OpenGameListItem.GetRevision());
  wxTheApp->ProcessEvent(event_update);
}

void CISOProperties::OnLocalIniModified(wxCommandEvent& ev)
{
  ev.Skip();
  if (WxStrToStr(ev.GetString()) != game_id)
    return;

  GameIniLocal.Load(GameIniFileLocal);
  LoadGameConfig();
}

void CISOProperties::OnEditConfig(wxCommandEvent& WXUNUSED(event))
{
  SaveGameConfig();
  // Create blank file to prevent editor from prompting to create it.
  if (!File::Exists(GameIniFileLocal))
  {
    std::fstream blankFile(GameIniFileLocal, std::ios::out);
    blankFile.close();
  }
  LaunchExternalEditor(GameIniFileLocal, true);
  GenerateLocalIniModified();
}

void CISOProperties::OnCheatCodeToggled(wxCommandEvent&)
{
  m_cheats_disabled_ar->UpdateState();
  m_cheats_disabled_gecko->UpdateState();
}

void CISOProperties::OnChangeTitle(wxCommandEvent& event)
{
  SetTitle(event.GetString());
}

// Opens all pre-defined INIs for the game. If there are multiple ones,
// they will all be opened, but there is usually only one
void CISOProperties::OnShowDefaultConfig(wxCommandEvent& WXUNUSED(event))
{
  for (const std::string& filename :
       SConfig::GetGameIniFilenames(game_id, m_open_iso->GetRevision()))
  {
    std::string path = File::GetSysDirectory() + GAMESETTINGS_DIR DIR_SEP + filename;
    if (File::Exists(path))
      LaunchExternalEditor(path, false);
  }
}

void CISOProperties::PatchListSelectionChanged(wxCommandEvent& event)
{
  switch (event.GetId())
  {
  case ID_HIDEOBJECTS_LIST:
    if (HideObjects->GetSelection() == wxNOT_FOUND ||
        DefaultHideObjects.find(
            HideObjects->GetString(HideObjects->GetSelection()).ToStdString()) !=
            DefaultHideObjects.end())
    {
      EditHideObject->Disable();
      RemoveHideObject->Disable();
    }
    else
    {
      EditHideObject->Enable();
      RemoveHideObject->Enable();
    }
    break;
  case ID_PATCHES_LIST:
    if (Patches->GetSelection() == wxNOT_FOUND ||
        DefaultPatches.find(Patches->GetString(Patches->GetSelection()).ToStdString()) !=
            DefaultPatches.end())
    {
      EditPatch->Disable();
      RemovePatch->Disable();
    }
    else
    {
      EditPatch->Enable();
      RemovePatch->Enable();
    }
    break;
  case ID_CHEATS_LIST:
/*    if (Cheats->GetSelection() == wxNOT_FOUND ||
        DefaultCheats.find(
            Cheats->RemoveMnemonics(Cheats->GetString(Cheats->GetSelection())).ToStdString()) !=
            DefaultCheats.end())
    {
      EditCheat->Disable();
      RemoveCheat->Disable();
    }
    else
    {
      EditCheat->Enable();
      RemoveCheat->Enable();
    } */
    break;
  }
}

void CISOProperties::CheckboxSelectionChanged(wxCommandEvent& event)
{
  HideObjectList_Save();
  HideObjectList_Load();
}

void CISOProperties::HideObjectList_Load()
{
  HideObjectCodes.clear();
  HideObjects->Clear();

  HideObjectEngine::LoadHideObjectSection("HideObjectCodes", HideObjectCodes, GameIniDefault,
                                          GameIniLocal);

  u32 index = 0;
  for (HideObjectEngine::HideObject& p : HideObjectCodes)
  {
    HideObjects->Append(StrToWxStr(p.name));
    HideObjects->Check(index, p.active);
    if (!p.user_defined)
      DefaultHideObjects.insert(p.name);
    ++index;
  }

  HideObjectEngine::ApplyHideObjects(HideObjectCodes);
}

void CISOProperties::HideObjectList_Save()
{
  std::vector<std::string> lines;
  std::vector<std::string> enabledLines;
  u32 index = 0;
  for (HideObjectEngine::HideObject& p : HideObjectCodes)
  {
    if (HideObjects->IsChecked(index))
      enabledLines.push_back("$" + p.name);

    // Do not save default removed objects.
    if (DefaultHideObjects.find(p.name) == DefaultHideObjects.end())
    {
      lines.push_back("$" + p.name);
      for (const HideObjectEngine::HideObjectEntry& entry : p.entries)
      {
        std::string temp = StringFromFormat(
            "%s:0x%08X%08X:0x%08X%08X", HideObjectEngine::HideObjectTypeStrings[entry.type].c_str(),
            (entry.value_upper & 0xffffffff00000000) >> 32, (entry.value_upper & 0xffffffff),
            (entry.value_lower & 0xffffffff00000000) >> 32, (entry.value_lower & 0xffffffff));
        lines.push_back(temp);
      }
    }
    ++index;
  }
  GameIniLocal.SetLines("HideObjectCodes_Enabled", enabledLines);
  GameIniLocal.SetLines("HideObjectCodes", lines);
}

void CISOProperties::HideObjectButtonClicked(wxCommandEvent& event)
{
  int selection = HideObjects->GetSelection();

  switch (event.GetId())
  {
  case ID_EDITHIDEOBJECT:
  {
    CHideObjectAddEdit dlg(selection, this);
    dlg.ShowModal();
  }
  break;
  case ID_ADDHideObject:
  {
    CHideObjectAddEdit dlg(-1, this, 1, _("Add Hide Object Code"));
    if (dlg.ShowModal() == wxID_OK)
    {
      HideObjects->Append(StrToWxStr(HideObjectCodes.back().name));
      HideObjects->Check((unsigned int)(HideObjectCodes.size() - 1), HideObjectCodes.back().active);
    }
  }
  break;
  case ID_REMOVEHIDEOBJECT:
    HideObjectCodes.erase(HideObjectCodes.begin() + HideObjects->GetSelection());
    HideObjects->Delete(HideObjects->GetSelection());
    break;
  }

  HideObjectList_Save();
  HideObjects->Clear();
  HideObjectList_Load();

  HideObjectEngine::ApplyHideObjects(HideObjectCodes);

  EditHideObject->Disable();
  RemoveHideObject->Disable();
}

void CISOProperties::PatchList_Load()
{
  onFrame.clear();
  Patches->Clear();

  PatchEngine::LoadPatchSection("OnFrame", onFrame, GameIniDefault, GameIniLocal);

  u32 index = 0;
  for (PatchEngine::Patch& p : onFrame)
  {
    Patches->Append(StrToWxStr(p.name));
    Patches->Check(index, p.active);
    if (!p.user_defined)
      DefaultPatches.insert(p.name);
    ++index;
  }
}

void CISOProperties::PatchList_Save()
{
  std::vector<std::string> lines;
  std::vector<std::string> enabledLines;
  u32 index = 0;
  for (PatchEngine::Patch& p : onFrame)
  {
    if (Patches->IsChecked(index))
      enabledLines.push_back("$" + p.name);

    // Do not save default patches.
    if (DefaultPatches.find(p.name) == DefaultPatches.end())
    {
      lines.push_back("$" + p.name);
      for (const PatchEngine::PatchEntry& entry : p.entries)
      {
        std::string temp = StringFromFormat("0x%08X:%s:0x%08X", entry.address,
                                            PatchEngine::PatchTypeStrings[entry.type], entry.value);
        lines.push_back(temp);
      }
    }
    ++index;
  }
  GameIniLocal.SetLines("OnFrame_Enabled", enabledLines);
  GameIniLocal.SetLines("OnFrame", lines);
}

void CISOProperties::PatchButtonClicked(wxCommandEvent& event)
{
  int selection = Patches->GetSelection();

  switch (event.GetId())
  {
  case ID_EDITPATCH:
  {
    CPatchAddEdit dlg(selection, &onFrame, this);
    dlg.ShowModal();
    Raise();
  }
  break;
  case ID_ADDPATCH:
  {
    CPatchAddEdit dlg(-1, &onFrame, this, 1, _("Add Patch"));
    int res = dlg.ShowModal();
    Raise();
    if (res == wxID_OK)
    {
      Patches->Append(StrToWxStr(onFrame.back().name));
      Patches->Check((unsigned int)(onFrame.size() - 1), onFrame.back().active);
    }
  }
  break;
  case ID_REMOVEPATCH:
    onFrame.erase(onFrame.begin() + Patches->GetSelection());
    Patches->Delete(Patches->GetSelection());
    break;
  }

  PatchList_Save();
  Patches->Clear();
  PatchList_Load();

  EditPatch->Disable();
  RemovePatch->Disable();
}
