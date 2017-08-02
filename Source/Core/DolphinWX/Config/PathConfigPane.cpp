// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/Config/PathConfigPane.h"

#include <string>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/dirdlg.h>
#include <wx/filepicker.h>
#include <wx/gbsizer.h>
#include <wx/listbox.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "DiscIO/NANDContentLoader.h"
#include "DolphinWX/Config/ConfigMain.h"
#include "DolphinWX/Frame.h"
#include "DolphinWX/WxEventUtils.h"
#include "DolphinWX/WxUtils.h"

PathConfigPane::PathConfigPane(wxWindow* panel, wxWindowID id) : wxPanel(panel, id)
{
  InitializeGUI();
  LoadGUIValues();
  BindEvents();
}

void PathConfigPane::InitializeGUI()
{
  m_iso_paths_listbox = new wxListBox(this, wxID_ANY);
  m_recursive_iso_paths_checkbox = new wxCheckBox(this, wxID_ANY, _("Search Subfolders"));
  m_add_iso_path_button = new wxButton(this, wxID_ANY, _("Add..."));
  m_remove_iso_path_button = new wxButton(this, wxID_ANY, _("Remove"));
  m_remove_iso_path_button->Disable();

  m_default_iso_filepicker = new wxFilePickerCtrl(
      this, wxID_ANY, wxEmptyString, _("Choose a default ISO:"),
      _("All GC/Wii files (elf, dol, gcm, iso, tgc, wbfs, ciso, gcz, wad)") +
          wxString::Format("|*.elf;*.dol;*.gcm;*.iso;*.tgc;*.wbfs;*.ciso;*.gcz;*.wad|%s",
                           wxGetTranslation(wxALL_FILES)),
      wxDefaultPosition, wxDefaultSize, wxFLP_USE_TEXTCTRL | wxFLP_OPEN | wxFLP_SMALL);
  m_nand_root_dirpicker =
      new wxDirPickerCtrl(this, wxID_ANY, wxEmptyString, _("Choose a NAND root directory:"),
                          wxDefaultPosition, wxDefaultSize, wxDIRP_USE_TEXTCTRL | wxDIRP_SMALL);
  m_dump_path_dirpicker =
      new wxDirPickerCtrl(this, wxID_ANY, wxEmptyString, _("Choose a dump directory:"),
                          wxDefaultPosition, wxDefaultSize, wxDIRP_USE_TEXTCTRL | wxDIRP_SMALL);
  m_wii_sdcard_filepicker = new wxFilePickerCtrl(
      this, wxID_ANY, wxEmptyString, _("Choose an SD Card file:"), wxFileSelectorDefaultWildcardStr,
      wxDefaultPosition, wxDefaultSize, wxDIRP_USE_TEXTCTRL | wxDIRP_SMALL);

  const int space5 = FromDIP(5);

  wxBoxSizer* const iso_button_sizer = new wxBoxSizer(wxHORIZONTAL);
  iso_button_sizer->Add(m_recursive_iso_paths_checkbox, 0, wxALIGN_CENTER_VERTICAL);
  iso_button_sizer->AddStretchSpacer();
  iso_button_sizer->Add(m_add_iso_path_button, 0, wxALIGN_CENTER_VERTICAL);
  iso_button_sizer->Add(m_remove_iso_path_button, 0, wxALIGN_CENTER_VERTICAL);

  wxStaticBoxSizer* const iso_listbox_sizer =
      new wxStaticBoxSizer(wxVERTICAL, this, _("ISO Directories"));
  iso_listbox_sizer->Add(m_iso_paths_listbox, 1, wxEXPAND);
  iso_listbox_sizer->AddSpacer(space5);
  iso_listbox_sizer->Add(iso_button_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  iso_listbox_sizer->AddSpacer(space5);

  wxGridBagSizer* const picker_sizer = new wxGridBagSizer(space5, space5);
  picker_sizer->Add(new wxStaticText(this, wxID_ANY, _("Default ISO:")), wxGBPosition(0, 0),
                    wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
  picker_sizer->Add(m_default_iso_filepicker, wxGBPosition(0, 1), wxDefaultSpan, wxEXPAND);
  picker_sizer->Add(new wxStaticText(this, wxID_ANY, _("Wii NAND Root:")), wxGBPosition(1, 0),
                    wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
  picker_sizer->Add(m_nand_root_dirpicker, wxGBPosition(1, 1), wxDefaultSpan, wxEXPAND);
  picker_sizer->Add(new wxStaticText(this, wxID_ANY, _("Dump Path:")), wxGBPosition(2, 0),
                    wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
  picker_sizer->Add(m_dump_path_dirpicker, wxGBPosition(2, 1), wxDefaultSpan, wxEXPAND);
  picker_sizer->Add(new wxStaticText(this, wxID_ANY, _("SD Card Path:")), wxGBPosition(3, 0),
                    wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
  picker_sizer->Add(m_wii_sdcard_filepicker, wxGBPosition(3, 1), wxDefaultSpan, wxEXPAND);
  picker_sizer->AddGrowableCol(1);

  // Populate the Paths page
  wxBoxSizer* const main_sizer = new wxBoxSizer(wxVERTICAL);
  main_sizer->AddSpacer(space5);
  main_sizer->Add(iso_listbox_sizer, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);
  main_sizer->AddSpacer(space5);
  main_sizer->Add(picker_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  main_sizer->AddSpacer(space5);

  SetSizer(main_sizer);
}

void PathConfigPane::LoadGUIValues()
{
  const SConfig& startup_params = SConfig::GetInstance();

  m_recursive_iso_paths_checkbox->SetValue(SConfig::GetInstance().m_RecursiveISOFolder);
  m_default_iso_filepicker->SetPath(StrToWxStr(startup_params.m_strDefaultISO));
  m_nand_root_dirpicker->SetPath(StrToWxStr(SConfig::GetInstance().m_NANDPath));
  m_dump_path_dirpicker->SetPath(StrToWxStr(SConfig::GetInstance().m_DumpPath));
  m_wii_sdcard_filepicker->SetPath(StrToWxStr(SConfig::GetInstance().m_strWiiSDCardPath));

  // Update selected ISO paths
  for (const std::string& folder : SConfig::GetInstance().m_ISOFolder)
    m_iso_paths_listbox->Append(StrToWxStr(folder));
}

void PathConfigPane::BindEvents()
{
  m_iso_paths_listbox->Bind(wxEVT_LISTBOX, &PathConfigPane::OnISOPathSelectionChanged, this);
  m_recursive_iso_paths_checkbox->Bind(wxEVT_CHECKBOX,
                                       &PathConfigPane::OnRecursiveISOCheckBoxChanged, this);
  m_add_iso_path_button->Bind(wxEVT_BUTTON, &PathConfigPane::OnAddISOPath, this);
  m_remove_iso_path_button->Bind(wxEVT_BUTTON, &PathConfigPane::OnRemoveISOPath, this);
  m_default_iso_filepicker->Bind(wxEVT_FILEPICKER_CHANGED, &PathConfigPane::OnDefaultISOChanged,
                                 this);
  m_nand_root_dirpicker->Bind(wxEVT_DIRPICKER_CHANGED, &PathConfigPane::OnNANDRootChanged, this);
  m_dump_path_dirpicker->Bind(wxEVT_DIRPICKER_CHANGED, &PathConfigPane::OnDumpPathChanged, this);
  m_wii_sdcard_filepicker->Bind(wxEVT_FILEPICKER_CHANGED, &PathConfigPane::OnSdCardPathChanged,
                                this);

  Bind(wxEVT_UPDATE_UI, &PathConfigPane::OnEnableIfCoreNotRunning, this);
}

void PathConfigPane::OnEnableIfCoreNotRunning(wxUpdateUIEvent& event)
{
  // Prevent the Remove button from being enabled via wxUpdateUIEvent
  if (event.GetId() != m_remove_iso_path_button->GetId())
    WxEventUtils::OnEnableIfCoreNotRunning(event);
}

void PathConfigPane::OnISOPathSelectionChanged(wxCommandEvent& event)
{
  m_remove_iso_path_button->Enable(m_iso_paths_listbox->GetSelection() != wxNOT_FOUND);
}

void PathConfigPane::OnRecursiveISOCheckBoxChanged(wxCommandEvent& event)
{
  SConfig::GetInstance().m_RecursiveISOFolder = m_recursive_iso_paths_checkbox->IsChecked();

  AddPendingEvent(wxCommandEvent(wxDOLPHIN_CFG_RESCAN_LIST));
}

void PathConfigPane::OnAddISOPath(wxCommandEvent& event)
{
  wxDirDialog dialog(this, _("Choose a directory to add"), wxGetHomeDir(),
                     wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);

  if (dialog.ShowModal() == wxID_OK)
  {
    if (m_iso_paths_listbox->FindString(dialog.GetPath()) != wxNOT_FOUND)
    {
      WxUtils::ShowErrorDialog(_("The chosen directory is already in the list."));
    }
    else
    {
      AddPendingEvent(wxCommandEvent(wxDOLPHIN_CFG_RESCAN_LIST));
      m_iso_paths_listbox->Append(dialog.GetPath());
    }
  }

  SaveISOPathChanges();
}

void PathConfigPane::OnRemoveISOPath(wxCommandEvent& event)
{
  AddPendingEvent(wxCommandEvent(wxDOLPHIN_CFG_RESCAN_LIST));
  m_iso_paths_listbox->Delete(m_iso_paths_listbox->GetSelection());

// This seems to not be activated on Windows when it should be. wxw bug?
#ifdef _WIN32
  wxCommandEvent dummy_event{};
  OnISOPathSelectionChanged(dummy_event);
#endif

  SaveISOPathChanges();
}

void PathConfigPane::OnDefaultISOChanged(wxCommandEvent& event)
{
  SConfig::GetInstance().m_strDefaultISO = WxStrToStr(m_default_iso_filepicker->GetPath());
}

void PathConfigPane::OnSdCardPathChanged(wxCommandEvent& event)
{
  std::string sd_card_path = WxStrToStr(m_wii_sdcard_filepicker->GetPath());
  SConfig::GetInstance().m_strWiiSDCardPath = sd_card_path;
  File::SetUserPath(F_WIISDCARD_IDX, sd_card_path);
}

void PathConfigPane::OnNANDRootChanged(wxCommandEvent& event)
{
  std::string nand_path = SConfig::GetInstance().m_NANDPath =
      WxStrToStr(m_nand_root_dirpicker->GetPath());

  File::SetUserPath(D_WIIROOT_IDX, nand_path);
  m_nand_root_dirpicker->SetPath(StrToWxStr(nand_path));

  DiscIO::NANDContentManager::Access().ClearCache();

  wxCommandEvent update_event{DOLPHIN_EVT_UPDATE_LOAD_WII_MENU_ITEM, GetId()};
  update_event.SetEventObject(this);
  AddPendingEvent(update_event);
}

void PathConfigPane::OnDumpPathChanged(wxCommandEvent& event)
{
  std::string dump_path = SConfig::GetInstance().m_DumpPath =
      WxStrToStr(m_dump_path_dirpicker->GetPath());

  m_dump_path_dirpicker->SetPath(StrToWxStr(dump_path));
}

void PathConfigPane::SaveISOPathChanges()
{
  SConfig::GetInstance().m_ISOFolder.clear();

  for (unsigned int i = 0; i < m_iso_paths_listbox->GetCount(); i++)
    SConfig::GetInstance().m_ISOFolder.push_back(WxStrToStr(m_iso_paths_listbox->GetStrings()[i]));
}
