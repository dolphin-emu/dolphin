// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>
#include <vector>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/combobox.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "Common/FileUtil.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/Core.h"
#include "DolphinWX/SoftwareVideoConfigDialog.h"
#include "DolphinWX/VideoConfigDiag.h"
#include "DolphinWX/WxUtils.h"

SoftwareVideoConfigDialog::SoftwareVideoConfigDialog(wxWindow* parent, const std::string& title)
    : wxDialog(parent, wxID_ANY,
               wxString(wxString::Format(_("Dolphin %s Graphics Configuration"), title)))
{
  wxNotebook* const notebook = new wxNotebook(this, wxID_ANY);

  const int space5 = FromDIP(5);

  // -- GENERAL --
  {
    wxPanel* const page_general = new wxPanel(notebook);
    notebook->AddPage(page_general, _("General"));
    wxBoxSizer* const szr_general = new wxBoxSizer(wxVERTICAL);

    // - rendering
    {
      wxStaticBoxSizer* const group_rendering =
          new wxStaticBoxSizer(wxVERTICAL, page_general, _("Rendering"));
      szr_general->AddSpacer(space5);
      szr_general->Add(group_rendering, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
      wxGridSizer* const szr_rendering = new wxGridSizer(2, space5, space5);
      group_rendering->Add(szr_rendering, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
      group_rendering->AddSpacer(space5);

      // backend
      wxStaticText* const label_backend = new wxStaticText(page_general, wxID_ANY, _("Backend:"));
      wxChoice* const choice_backend = new wxChoice(page_general, wxID_ANY);

      for (const auto& backend : g_available_video_backends)
      {
        choice_backend->AppendString(StrToWxStr(backend->GetDisplayName()));
      }

      // TODO: How to get the translated plugin name?
      choice_backend->SetStringSelection(StrToWxStr(g_video_backend->GetName()));
      choice_backend->Bind(wxEVT_CHOICE, &SoftwareVideoConfigDialog::Event_Backend, this);

      szr_rendering->Add(label_backend, 0, wxALIGN_CENTER_VERTICAL);
      szr_rendering->Add(choice_backend, 0, wxALIGN_CENTER_VERTICAL);

      if (Core::GetState() != Core::State::Uninitialized)
      {
        label_backend->Disable();
        choice_backend->Disable();
      }

      // xfb
      szr_rendering->Add(
          new SettingCheckBox(page_general, _("Bypass XFB"), "", Config::GFX_USE_XFB, true));
    }

    // - info
    {
      wxStaticBoxSizer* const group_info =
          new wxStaticBoxSizer(wxVERTICAL, page_general, _("Overlay Information"));
      szr_general->AddSpacer(space5);
      szr_general->Add(group_info, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
      wxGridSizer* const szr_info = new wxGridSizer(2, space5, space5);
      group_info->Add(szr_info, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
      group_info->AddSpacer(space5);

      szr_info->Add(new SettingCheckBox(page_general, _("Various Statistics"), "",
                                        Config::GFX_OVERLAY_STATS));
    }

    // - utility
    {
      wxStaticBoxSizer* const group_utility =
          new wxStaticBoxSizer(wxVERTICAL, page_general, _("Utility"));
      szr_general->AddSpacer(space5);
      szr_general->Add(group_utility, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
      wxGridSizer* const szr_utility = new wxGridSizer(2, space5, space5);
      group_utility->Add(szr_utility, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
      group_utility->AddSpacer(space5);

      szr_utility->Add(
          new SettingCheckBox(page_general, _("Dump Textures"), "", Config::GFX_DUMP_TEXTURES));
      szr_utility->Add(
          new SettingCheckBox(page_general, _("Dump Objects"), "", Config::GFX_SW_DUMP_OBJECTS));

      // - debug only
      wxStaticBoxSizer* const group_debug_only_utility =
          new wxStaticBoxSizer(wxHORIZONTAL, page_general, _("Debug Only"));
      group_utility->Add(group_debug_only_utility, 0, wxEXPAND);
      group_utility->AddSpacer(space5);
      wxGridSizer* const szr_debug_only_utility = new wxGridSizer(2, space5, space5);
      group_debug_only_utility->AddSpacer(space5);
      group_debug_only_utility->Add(szr_debug_only_utility, 0, wxEXPAND | wxBOTTOM, space5);
      group_debug_only_utility->AddSpacer(space5);

      szr_debug_only_utility->Add(new SettingCheckBox(page_general, _("Dump TEV Stages"), "",
                                                      Config::GFX_SW_DUMP_TEV_STAGES));
      szr_debug_only_utility->Add(new SettingCheckBox(page_general, _("Dump Texture Fetches"), "",
                                                      Config::GFX_SW_DUMP_TEV_TEX_FETCHES));
    }

    // - misc
    {
      wxStaticBoxSizer* const group_misc =
          new wxStaticBoxSizer(wxVERTICAL, page_general, _("Drawn Object Range"));
      szr_general->AddSpacer(space5);
      szr_general->Add(group_misc, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
      wxFlexGridSizer* const szr_misc = new wxFlexGridSizer(2, space5, space5);
      group_misc->Add(szr_misc, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
      group_misc->AddSpacer(space5);

      szr_misc->Add(
          new I32Setting(page_general, _("Start"), Config::GFX_SW_DRAW_START, 0, 100000));
      szr_misc->Add(new I32Setting(page_general, _("End"), Config::GFX_SW_DRAW_END, 0, 100000));
    }

    szr_general->AddSpacer(space5);
    page_general->SetSizerAndFit(szr_general);
  }

  wxStdDialogButtonSizer* const btn_sizer = CreateStdDialogButtonSizer(wxOK | wxNO_DEFAULT);
  btn_sizer->GetAffirmativeButton()->SetLabel(_("Close"));

  wxBoxSizer* const szr_main = new wxBoxSizer(wxVERTICAL);
  szr_main->AddSpacer(space5);
  szr_main->Add(notebook, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);
  szr_main->AddSpacer(space5);
  szr_main->Add(btn_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  szr_main->AddSpacer(space5);

  SetLayoutAdaptationMode(wxDIALOG_ADAPTATION_MODE_ENABLED);
  SetLayoutAdaptationLevel(wxDIALOG_ADAPTATION_STANDARD_SIZER);
  SetSizerAndFit(szr_main);
  Center();
  SetFocus();
}

SoftwareVideoConfigDialog::~SoftwareVideoConfigDialog()
{
  Config::Save();
}
