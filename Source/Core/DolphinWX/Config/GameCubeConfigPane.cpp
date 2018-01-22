// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/Config/GameCubeConfigPane.h"

#include <cassert>
#include <string>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/gbsizer.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include "Common/Common.h"
#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/EXI/EXI.h"
#include "Core/HW/GCMemcard/GCMemcard.h"
#include "Core/HW/GCPad.h"
#include "Core/NetPlayProto.h"
#include "DolphinWX/Config/ConfigMain.h"
#include "DolphinWX/Input/MicButtonConfigDiag.h"
#include "DolphinWX/WxEventUtils.h"
#include "DolphinWX/WxUtils.h"

#define DEV_NONE_STR _trans("<Nothing>")
#define DEV_DUMMY_STR _trans("Dummy")

#define EXIDEV_MEMCARD_STR _trans("Memory Card")
#define EXIDEV_MEMDIR_STR _trans("GCI Folder")
#define EXIDEV_MIC_STR _trans("Microphone")
#define EXIDEV_BBA_STR _trans("Broadband Adapter")
#define EXIDEV_AGP_STR _trans("Advance Game Port")
#define EXIDEV_GECKO_STR _trans("USB Gecko")

GameCubeConfigPane::GameCubeConfigPane(wxWindow* parent, wxWindowID id) : wxPanel(parent, id)
{
  InitializeGUI();
  LoadGUIValues();
  BindEvents();
}

void GameCubeConfigPane::InitializeGUI()
{
  m_ipl_language_strings.Add(_("English"));
  m_ipl_language_strings.Add(_("German"));
  m_ipl_language_strings.Add(_("French"));
  m_ipl_language_strings.Add(_("Spanish"));
  m_ipl_language_strings.Add(_("Italian"));
  m_ipl_language_strings.Add(_("Dutch"));

  m_system_lang_choice =
      new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_ipl_language_strings);
  m_system_lang_choice->SetToolTip(_("Sets the GameCube system language."));

  m_override_lang_checkbox = new wxCheckBox(this, wxID_ANY, _("Override Language on NTSC Games"));
  m_override_lang_checkbox->SetToolTip(_(
      "Lets the system language be set to values that games were not designed for. This can allow "
      "the use of extra translations for a few games, but can also lead to text display issues."));

  m_skip_ipl_checkbox = new wxCheckBox(this, wxID_ANY, _("Skip Main Menu"));

  if (!File::Exists(File::GetUserPath(D_GCUSER_IDX) + DIR_SEP + USA_DIR + DIR_SEP GC_IPL) &&
      !File::Exists(File::GetSysDirectory() + GC_SYS_DIR + DIR_SEP + USA_DIR + DIR_SEP GC_IPL) &&
      !File::Exists(File::GetUserPath(D_GCUSER_IDX) + DIR_SEP + JAP_DIR + DIR_SEP GC_IPL) &&
      !File::Exists(File::GetSysDirectory() + GC_SYS_DIR + DIR_SEP + JAP_DIR + DIR_SEP GC_IPL) &&
      !File::Exists(File::GetUserPath(D_GCUSER_IDX) + DIR_SEP + EUR_DIR + DIR_SEP GC_IPL) &&
      !File::Exists(File::GetSysDirectory() + GC_SYS_DIR + DIR_SEP + EUR_DIR + DIR_SEP GC_IPL))
  {
    m_skip_ipl_checkbox->Disable();
    m_skip_ipl_checkbox->SetToolTip(_("Put Main Menu roms in User/GC/{region}."));
  }

  // Device settings
  // EXI Devices
  wxStaticText* GCEXIDeviceText[3] = {
      new wxStaticText(this, wxID_ANY, _("Slot A")), new wxStaticText(this, wxID_ANY, _("Slot B")),
      new wxStaticText(this, wxID_ANY, "SP1"),
  };

  m_exi_devices[0] = new wxChoice(this, wxID_ANY);
  m_exi_devices[1] = new wxChoice(this, wxID_ANY);
  m_exi_devices[2] = new wxChoice(this, wxID_ANY);
  m_exi_devices[2]->SetToolTip(
      _("Serial Port 1 - This is the port which devices such as the net adapter use."));

  m_memcard_path[0] =
      new wxButton(this, wxID_ANY, "...", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
  m_memcard_path[1] =
      new wxButton(this, wxID_ANY, "...", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);

  const int space5 = FromDIP(5);
  const int space10 = FromDIP(10);

  // Populate the GameCube page
  wxGridBagSizer* const sGamecubeIPLSettings = new wxGridBagSizer(space5, space5);
  sGamecubeIPLSettings->Add(m_skip_ipl_checkbox, wxGBPosition(0, 0), wxGBSpan(1, 2));
  sGamecubeIPLSettings->Add(new wxStaticText(this, wxID_ANY, _("System Language:")),
                            wxGBPosition(1, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
  sGamecubeIPLSettings->Add(m_system_lang_choice, wxGBPosition(1, 1), wxDefaultSpan,
                            wxALIGN_CENTER_VERTICAL);
  sGamecubeIPLSettings->Add(m_override_lang_checkbox, wxGBPosition(2, 0), wxGBSpan(1, 2));

  wxStaticBoxSizer* const sbGamecubeIPLSettings =
      new wxStaticBoxSizer(wxVERTICAL, this, _("IPL Settings"));
  sbGamecubeIPLSettings->AddSpacer(space5);
  sbGamecubeIPLSettings->Add(sGamecubeIPLSettings, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  sbGamecubeIPLSettings->AddSpacer(space5);

  wxStaticBoxSizer* const sbGamecubeDeviceSettings =
      new wxStaticBoxSizer(wxVERTICAL, this, _("Device Settings"));
  wxGridBagSizer* const gamecube_EXIDev_sizer = new wxGridBagSizer(space10, space10);
  for (int i = 0; i < 3; ++i)
  {
    gamecube_EXIDev_sizer->Add(GCEXIDeviceText[i], wxGBPosition(i, 0), wxDefaultSpan,
                               wxALIGN_CENTER_VERTICAL);
    gamecube_EXIDev_sizer->Add(m_exi_devices[i], wxGBPosition(i, 1), wxGBSpan(1, (i < 2) ? 1 : 2),
                               wxALIGN_CENTER_VERTICAL);

    if (i < 2)
      gamecube_EXIDev_sizer->Add(m_memcard_path[i], wxGBPosition(i, 2), wxDefaultSpan,
                                 wxALIGN_CENTER_VERTICAL);
  }
  sbGamecubeDeviceSettings->AddSpacer(space5);
  sbGamecubeDeviceSettings->Add(gamecube_EXIDev_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  sbGamecubeDeviceSettings->AddSpacer(space5);

  wxBoxSizer* const main_sizer = new wxBoxSizer(wxVERTICAL);
  main_sizer->AddSpacer(space5);
  main_sizer->Add(sbGamecubeIPLSettings, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  main_sizer->AddSpacer(space5);
  main_sizer->Add(sbGamecubeDeviceSettings, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  main_sizer->AddSpacer(space5);

  SetSizer(main_sizer);
}

void GameCubeConfigPane::LoadGUIValues()
{
  const SConfig& startup_params = SConfig::GetInstance();

  m_system_lang_choice->SetSelection(startup_params.SelectedLanguage);
  m_skip_ipl_checkbox->SetValue(startup_params.bHLE_BS2);
  m_override_lang_checkbox->SetValue(startup_params.bOverrideGCLanguage);

  wxArrayString slot_devices;
  slot_devices.Add(_(DEV_NONE_STR));
  slot_devices.Add(_(DEV_DUMMY_STR));
  slot_devices.Add(_(EXIDEV_MEMCARD_STR));
  slot_devices.Add(_(EXIDEV_MEMDIR_STR));
  slot_devices.Add(_(EXIDEV_GECKO_STR));
  slot_devices.Add(_(EXIDEV_AGP_STR));
  slot_devices.Add(_(EXIDEV_MIC_STR));

  wxArrayString sp1_devices;
  sp1_devices.Add(_(DEV_NONE_STR));
  sp1_devices.Add(_(DEV_DUMMY_STR));
  sp1_devices.Add(_(EXIDEV_BBA_STR));

  for (int i = 0; i < 3; ++i)
  {
    bool isMemcard = false;
    bool isMic = false;

    // Add strings to the wxChoice list, the third wxChoice is the SP1 slot
    if (i == 2)
      m_exi_devices[i]->Append(sp1_devices);
    else
      m_exi_devices[i]->Append(slot_devices);

    switch (SConfig::GetInstance().m_EXIDevice[i])
    {
    case ExpansionInterface::EXIDEVICE_NONE:
      m_exi_devices[i]->SetStringSelection(slot_devices[0]);
      break;
    case ExpansionInterface::EXIDEVICE_MEMORYCARD:
      isMemcard = m_exi_devices[i]->SetStringSelection(slot_devices[2]);
      break;
    case ExpansionInterface::EXIDEVICE_MEMORYCARDFOLDER:
      m_exi_devices[i]->SetStringSelection(slot_devices[3]);
      break;
    case ExpansionInterface::EXIDEVICE_GECKO:
      m_exi_devices[i]->SetStringSelection(slot_devices[4]);
      break;
    case ExpansionInterface::EXIDEVICE_AGP:
      isMemcard = m_exi_devices[i]->SetStringSelection(slot_devices[5]);
      break;
    case ExpansionInterface::EXIDEVICE_MIC:
      isMic = m_exi_devices[i]->SetStringSelection(slot_devices[6]);
      break;
    case ExpansionInterface::EXIDEVICE_ETH:
      m_exi_devices[i]->SetStringSelection(sp1_devices[2]);
      break;
    case ExpansionInterface::EXIDEVICE_DUMMY:
    default:
      m_exi_devices[i]->SetStringSelection(slot_devices[1]);
      break;
    }

    if (!isMemcard && !isMic && i < 2)
      m_memcard_path[i]->Disable();
  }
}

void GameCubeConfigPane::BindEvents()
{
  m_system_lang_choice->Bind(wxEVT_CHOICE, &GameCubeConfigPane::OnSystemLanguageChange, this);
  m_system_lang_choice->Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreNotRunning);

  m_override_lang_checkbox->Bind(wxEVT_CHECKBOX,
                                 &GameCubeConfigPane::OnOverrideLanguageCheckBoxChanged, this);
  m_override_lang_checkbox->Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreNotRunning);

  m_skip_ipl_checkbox->Bind(wxEVT_CHECKBOX, &GameCubeConfigPane::OnSkipIPLCheckBoxChanged, this);
  m_skip_ipl_checkbox->Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreNotRunning);

  m_exi_devices[0]->Bind(wxEVT_CHOICE, &GameCubeConfigPane::OnSlotAChanged, this);
  m_exi_devices[0]->Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfNetplayNotRunning);
  m_exi_devices[1]->Bind(wxEVT_CHOICE, &GameCubeConfigPane::OnSlotBChanged, this);
  m_exi_devices[1]->Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfNetplayNotRunning);
  m_exi_devices[2]->Bind(wxEVT_CHOICE, &GameCubeConfigPane::OnSP1Changed, this);
  m_exi_devices[2]->Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfNetplayNotRunning);

  m_memcard_path[0]->Bind(wxEVT_BUTTON, &GameCubeConfigPane::OnSlotAButtonClick, this);
  m_memcard_path[1]->Bind(wxEVT_BUTTON, &GameCubeConfigPane::OnSlotBButtonClick, this);
}

void GameCubeConfigPane::OnSystemLanguageChange(wxCommandEvent& event)
{
  SConfig::GetInstance().SelectedLanguage = m_system_lang_choice->GetSelection();

  AddPendingEvent(wxCommandEvent(wxDOLPHIN_CFG_REFRESH_LIST));
}

void GameCubeConfigPane::OnOverrideLanguageCheckBoxChanged(wxCommandEvent& event)
{
  SConfig::GetInstance().bOverrideGCLanguage = m_override_lang_checkbox->IsChecked();

  AddPendingEvent(wxCommandEvent(wxDOLPHIN_CFG_REFRESH_LIST));
}

void GameCubeConfigPane::OnSkipIPLCheckBoxChanged(wxCommandEvent& event)
{
  SConfig::GetInstance().bHLE_BS2 = m_skip_ipl_checkbox->IsChecked();
}

void GameCubeConfigPane::OnSlotAChanged(wxCommandEvent& event)
{
  ChooseEXIDevice(event.GetString(), 0);
}

void GameCubeConfigPane::OnSlotBChanged(wxCommandEvent& event)
{
  ChooseEXIDevice(event.GetString(), 1);
}

void GameCubeConfigPane::OnSP1Changed(wxCommandEvent& event)
{
  ChooseEXIDevice(event.GetString(), 2);
}

void GameCubeConfigPane::HandleEXISlotChange(int slot, const wxString& title)
{
  assert(slot >= 0 && slot <= 1);

  if (!m_exi_devices[slot]->GetStringSelection().compare(_(EXIDEV_MIC_STR)))
  {
    InputConfig* const pad_plugin = Pad::GetConfig();
    MicButtonConfigDialog dialog(this, *pad_plugin, title, slot);
    dialog.ShowModal();
  }
  else
  {
    ChooseSlotPath(slot == 0, SConfig::GetInstance().m_EXIDevice[slot]);
  }
}

void GameCubeConfigPane::OnSlotAButtonClick(wxCommandEvent& event)
{
  HandleEXISlotChange(0, wxString(_("GameCube Microphone Slot A")));
}

void GameCubeConfigPane::OnSlotBButtonClick(wxCommandEvent& event)
{
  HandleEXISlotChange(1, wxString(_("GameCube Microphone Slot B")));
}

void GameCubeConfigPane::ChooseEXIDevice(const wxString& deviceName, int deviceNum)
{
  ExpansionInterface::TEXIDevices tempType;

  if (!deviceName.compare(_(EXIDEV_MEMCARD_STR)))
    tempType = ExpansionInterface::EXIDEVICE_MEMORYCARD;
  else if (!deviceName.compare(_(EXIDEV_MEMDIR_STR)))
    tempType = ExpansionInterface::EXIDEVICE_MEMORYCARDFOLDER;
  else if (!deviceName.compare(_(EXIDEV_MIC_STR)))
    tempType = ExpansionInterface::EXIDEVICE_MIC;
  else if (!deviceName.compare(_(EXIDEV_BBA_STR)))
    tempType = ExpansionInterface::EXIDEVICE_ETH;
  else if (!deviceName.compare(_(EXIDEV_AGP_STR)))
    tempType = ExpansionInterface::EXIDEVICE_AGP;
  else if (!deviceName.compare(_(EXIDEV_GECKO_STR)))
    tempType = ExpansionInterface::EXIDEVICE_GECKO;
  else if (!deviceName.compare(_(DEV_NONE_STR)))
    tempType = ExpansionInterface::EXIDEVICE_NONE;
  else
    tempType = ExpansionInterface::EXIDEVICE_DUMMY;

  // Gray out the memcard path button if we're not on a memcard or AGP
  if (tempType == ExpansionInterface::EXIDEVICE_MEMORYCARD ||
      tempType == ExpansionInterface::EXIDEVICE_AGP ||
      tempType == ExpansionInterface::EXIDEVICE_MIC)
  {
    m_memcard_path[deviceNum]->Enable();
  }
  else if (deviceNum == 0 || deviceNum == 1)
  {
    m_memcard_path[deviceNum]->Disable();
  }

  SConfig::GetInstance().m_EXIDevice[deviceNum] = tempType;

  if (Core::IsRunning())
  {
    // Change plugged device! :D
    ExpansionInterface::ChangeDevice(
        (deviceNum == 1) ? 1 : 0,   // SlotB is on channel 1, slotA and SP1 are on 0
        tempType,                   // The device enum to change to
        (deviceNum == 2) ? 2 : 0);  // SP1 is device 2, slots are device 0
  }
}

void GameCubeConfigPane::ChooseSlotPath(bool is_slot_a, ExpansionInterface::TEXIDevices device_type)
{
  bool memcard = (device_type == ExpansionInterface::EXIDEVICE_MEMORYCARD);
  std::string path;
  std::string cardname;
  std::string ext;
  std::string pathA = SConfig::GetInstance().m_strMemoryCardA;
  std::string pathB = SConfig::GetInstance().m_strMemoryCardB;
  if (!memcard)
  {
    pathA = SConfig::GetInstance().m_strGbaCartA;
    pathB = SConfig::GetInstance().m_strGbaCartB;
  }
  SplitPath(is_slot_a ? pathA : pathB, &path, &cardname, &ext);
  std::string filename = WxStrToStr(wxFileSelector(
      _("Choose a file to open"), StrToWxStr(path), StrToWxStr(cardname), StrToWxStr(ext),
      memcard ? _("GameCube Memory Cards (*.raw,*.gcp)") + "|*.raw;*.gcp" :
                _("Game Boy Advance Carts (*.gba)") + "|*.gba"));

  if (!filename.empty())
  {
    if (memcard && File::Exists(filename))
    {
      GCMemcard memorycard(filename);
      if (!memorycard.IsValid())
      {
        WxUtils::ShowErrorDialog(wxString::Format(_("Cannot use that file as a memory card.\n%s\n"
                                                    "is not a valid GameCube memory card file"),
                                                  filename.c_str()));
        return;
      }
    }

    wxFileName newFilename(filename);
    newFilename.MakeAbsolute();
    filename = newFilename.GetFullPath();

#ifdef _WIN32
    // If the Memory Card file is within the Exe dir, we can assume that the user wants it to be
    // stored relative
    // to the executable, so it stays set correctly when the probably portable Exe dir is moved.
    // TODO: Replace this with a cleaner, non-wx solution once std::filesystem is standard
    std::string exeDir = File::GetExeDirectory() + '\\';
    if (wxString(filename).Lower().StartsWith(wxString(exeDir).Lower()))
      filename.erase(0, exeDir.size());

    std::replace(filename.begin(), filename.end(), '\\', '/');
#endif

    // also check that the path isn't used for the other memcard...
    wxFileName otherFilename(is_slot_a ? pathB : pathA);
    otherFilename.MakeAbsolute();
    if (newFilename.GetFullPath().compare(otherFilename.GetFullPath()) != 0)
    {
      if (memcard)
      {
        if (is_slot_a)
          SConfig::GetInstance().m_strMemoryCardA = filename;
        else
          SConfig::GetInstance().m_strMemoryCardB = filename;
      }
      else
      {
        if (is_slot_a)
          SConfig::GetInstance().m_strGbaCartA = filename;
        else
          SConfig::GetInstance().m_strGbaCartB = filename;
      }

      if (Core::IsRunning())
      {
        // Change memcard to the new file
        ExpansionInterface::ChangeDevice(is_slot_a ? 0 : 1,  // SlotA: channel 0, SlotB channel 1
                                         device_type,
                                         0);  // SP1 is device 2, slots are device 0
      }
    }
    else
    {
      WxUtils::ShowErrorDialog(_("Are you trying to use the same file in both slots?"));
    }
  }
}
