// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <sstream>
#include <string>
#include <vector>
#include <wx/button.h>
#include <wx/checklst.h>
#include <wx/listbox.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"
#include "Core/Core.h"
#include "Core/GeckoCode.h"
#include "Core/GeckoCodeConfig.h"
#include "DolphinWX/Cheats/GeckoCodeDiag.h"
#include "DolphinWX/WxUtils.h"

wxDEFINE_EVENT(DOLPHIN_EVT_GECKOCODE_TOGGLED, wxCommandEvent);

namespace Gecko
{
static const char str_name[] = wxTRANSLATE("Name:");
static const char str_notes[] = wxTRANSLATE("Notes:");
static const char str_creator[] = wxTRANSLATE("Creator:");

CodeConfigPanel::CodeConfigPanel(wxWindow* const parent) : wxPanel(parent)
{
  m_listbox_gcodes = new wxCheckListBox(this, wxID_ANY);
  m_listbox_gcodes->Bind(wxEVT_LISTBOX, &CodeConfigPanel::UpdateInfoBox, this);
  m_listbox_gcodes->Bind(wxEVT_CHECKLISTBOX, &CodeConfigPanel::ToggleCode, this);

  m_infobox.label_name = new wxStaticText(this, wxID_ANY, wxGetTranslation(str_name));
  m_infobox.label_creator = new wxStaticText(this, wxID_ANY, wxGetTranslation(str_creator));
  m_infobox.label_notes = new wxStaticText(this, wxID_ANY, wxGetTranslation(str_notes));
  m_infobox.textctrl_notes = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
                                            wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);
  m_infobox.listbox_codes =
      new wxListBox(this, wxID_ANY, wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1, 48)));

  // TODO: buttons to add/edit codes

  // sizers
  const int space5 = FromDIP(5);
  wxBoxSizer* const sizer_infobox = new wxBoxSizer(wxVERTICAL);
  sizer_infobox->Add(m_infobox.label_name);
  sizer_infobox->Add(m_infobox.label_creator, 0, wxTOP, space5);
  sizer_infobox->Add(m_infobox.label_notes, 0, wxTOP, space5);
  sizer_infobox->Add(m_infobox.textctrl_notes, 0, wxEXPAND | wxTOP, space5);
  sizer_infobox->Add(m_infobox.listbox_codes, 1, wxEXPAND | wxTOP, space5);

  // button sizer
  wxBoxSizer* const sizer_buttons = new wxBoxSizer(wxHORIZONTAL);
  btn_download = new wxButton(this, wxID_ANY, _("Download Codes (WiiRD Database)"));
  btn_download->Disable();
  btn_download->Bind(wxEVT_BUTTON, &CodeConfigPanel::DownloadCodes, this);
  sizer_buttons->AddStretchSpacer(1);
  sizer_buttons->Add(WxUtils::GiveMinSizeDIP(btn_download, wxSize(128, -1)), 1, wxEXPAND);

  wxBoxSizer* const sizer_main = new wxBoxSizer(wxVERTICAL);
  sizer_main->AddSpacer(space5);
  sizer_main->Add(m_listbox_gcodes, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);
  sizer_main->AddSpacer(space5);
  sizer_main->Add(sizer_infobox, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  sizer_main->AddSpacer(space5);
  sizer_main->Add(sizer_buttons, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  sizer_main->AddSpacer(space5);

  SetSizerAndFit(sizer_main);
}

void CodeConfigPanel::UpdateCodeList(bool checkRunning)
{
  // disable the button if it doesn't have an effect
  btn_download->Enable((!checkRunning || Core::IsRunning()) && !m_gameid.empty());

  m_listbox_gcodes->Clear();
  // add the codes to the listbox
  for (const GeckoCode& code : m_gcodes)
  {
    m_listbox_gcodes->Append(m_listbox_gcodes->EscapeMnemonics(StrToWxStr(code.name)));
    if (code.enabled)
    {
      m_listbox_gcodes->Check(m_listbox_gcodes->GetCount() - 1, true);
    }
  }

  wxCommandEvent evt;
  UpdateInfoBox(evt);
}

void CodeConfigPanel::LoadCodes(const IniFile& globalIni, const IniFile& localIni,
                                const std::string& gameid, bool checkRunning)
{
  m_gameid = gameid;

  m_gcodes.clear();
  if (!checkRunning || Core::IsRunning())
    m_gcodes = Gecko::LoadCodes(globalIni, localIni);

  UpdateCodeList(checkRunning);
}

void CodeConfigPanel::ToggleCode(wxCommandEvent& evt)
{
  const int sel = evt.GetInt();  // this right?
  if (sel > -1)
  {
    m_gcodes[sel].enabled = m_listbox_gcodes->IsChecked(sel);

    wxCommandEvent toggle_event(DOLPHIN_EVT_GECKOCODE_TOGGLED, GetId());
    toggle_event.SetClientData(&m_gcodes[sel]);
    GetEventHandler()->ProcessEvent(toggle_event);
  }
}

void CodeConfigPanel::UpdateInfoBox(wxCommandEvent&)
{
  m_infobox.listbox_codes->Clear();
  const int sel = m_listbox_gcodes->GetSelection();

  if (sel > -1)
  {
    m_infobox.label_name->SetLabel(wxGetTranslation(str_name) + StrToWxStr(m_gcodes[sel].name));

    // notes textctrl
    m_infobox.textctrl_notes->Clear();
    for (const std::string& note : m_gcodes[sel].notes)
    {
      m_infobox.textctrl_notes->AppendText(StrToWxStr(note));
    }
    m_infobox.textctrl_notes->ScrollLines(-99);  // silly

    m_infobox.label_creator->SetLabel(wxGetTranslation(str_creator) +
                                      StrToWxStr(m_gcodes[sel].creator));

    // add codes to info listbox
    for (const GeckoCode::Code& code : m_gcodes[sel].codes)
    {
      m_infobox.listbox_codes->Append(wxString::Format("%08X %08X", code.address, code.data));
    }
  }
  else
  {
    m_infobox.label_name->SetLabel(wxGetTranslation(str_name));
    m_infobox.textctrl_notes->Clear();
    m_infobox.label_creator->SetLabel(wxGetTranslation(str_creator));
  }
}

void CodeConfigPanel::DownloadCodes(wxCommandEvent&)
{
  if (m_gameid.empty())
    return;

  bool succeeded;
  std::vector<GeckoCode> gcodes = Gecko::DownloadCodes(m_gameid, &succeeded);
  if (!succeeded)
  {
    WxUtils::ShowErrorDialog(_("Failed to download codes."));
    return;
  }

  if (!gcodes.size())
  {
    wxMessageBox(_("File contained no codes."));
    return;
  }

  unsigned long added_count = 0;

  // append the codes to the code list
  for (const GeckoCode& code : gcodes)
  {
    // only add codes which do not already exist
    auto existing_gcodes_iter = m_gcodes.begin();
    auto existing_gcodes_end = m_gcodes.end();
    for (;; ++existing_gcodes_iter)
    {
      if (existing_gcodes_end == existing_gcodes_iter)
      {
        m_gcodes.push_back(code);
        ++added_count;
        break;
      }

      // code exists
      if (*existing_gcodes_iter == code)
        break;
    }
  }

  wxMessageBox(wxString::Format(_("Downloaded %lu codes. (added %lu)"),
                                (unsigned long)gcodes.size(), added_count));

  // refresh the list
  UpdateCodeList();
}
}
