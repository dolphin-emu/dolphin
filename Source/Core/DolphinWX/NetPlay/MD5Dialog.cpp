// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <wx/button.h>
#include <wx/event.h>
#include <wx/gauge.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/stattext.h>

#include "Common/StringUtil.h"
#include "DolphinWX/NetPlay/MD5Dialog.h"
#include "DolphinWX/NetPlay/NetWindow.h"

MD5Dialog::MD5Dialog(wxWindow* parent, NetPlayServer* server, std::vector<const Player*> players,
                     const std::string& game)
    : wxDialog(parent, wxID_ANY, _("MD5 Checksum")), m_netplay_server(server)
{
  const int space5 = FromDIP(5);

  wxBoxSizer* const main_sizer = new wxBoxSizer(wxVERTICAL);

  main_sizer->AddSpacer(space5);
  main_sizer->Add(new wxStaticText(this, wxID_ANY,
                                   wxString::Format(_("Computing MD5 Checksum for:\n%s"), game),
                                   wxDefaultPosition, wxDefaultSize,
                                   wxALIGN_CENTRE_HORIZONTAL | wxST_NO_AUTORESIZE),
                  0, wxEXPAND | wxLEFT | wxRIGHT, space5);

  for (const Player* player : players)
  {
    wxStaticBoxSizer* const player_szr = new wxStaticBoxSizer(
        wxVERTICAL, this, player->name + " (p" + std::to_string(player->pid) + ")");

    wxGauge* gauge = new wxGauge(player_szr->GetStaticBox(), wxID_ANY, 100);
    m_progress_bars[player->pid] = gauge;

    m_result_labels[player->pid] =
        new wxStaticText(player_szr->GetStaticBox(), wxID_ANY, _("Computing..."));

    player_szr->AddSpacer(space5);
    player_szr->Add(gauge, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
    player_szr->AddSpacer(space5);
    player_szr->Add(m_result_labels[player->pid], 0, wxALIGN_CENTER_HORIZONTAL | wxLEFT | wxRIGHT,
                    space5);
    player_szr->AddSpacer(space5);
    player_szr->SetMinSize(FromDIP(wxSize(250, -1)));

    main_sizer->AddSpacer(space5);
    main_sizer->Add(player_szr, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  }

  m_final_result_label =
      new wxStaticText(this, wxID_ANY,
                       " ",  // so it takes space
                       wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL);

  main_sizer->AddSpacer(space5);
  main_sizer->Add(m_final_result_label, 1, wxLEFT | wxRIGHT, space5);
  main_sizer->AddSpacer(space5);
  main_sizer->Add(CreateStdDialogButtonSizer(wxCLOSE), 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  main_sizer->AddSpacer(space5);
  SetSizerAndFit(main_sizer);

  Bind(wxEVT_BUTTON, &MD5Dialog::OnCloseBtnPressed, this, wxID_CLOSE);
  Bind(wxEVT_CLOSE_WINDOW, &MD5Dialog::OnClose, this);
  SetFocus();
  Center();
}

void MD5Dialog::SetProgress(int pid, int progress)
{
  if (m_progress_bars[pid] == nullptr)
    return;

  m_progress_bars[pid]->SetValue(progress);
  m_result_labels[pid]->SetLabel(_("Computing: ") + std::to_string(progress) + "%");
  Layout();
  Update();
}

void MD5Dialog::SetResult(int pid, const std::string& result)
{
  if (m_result_labels[pid] == nullptr)
    return;

  m_result_labels[pid]->SetLabel(result);
  m_hashes.push_back(result);

  if (m_hashes.size() > 1)
  {
    wxString label = AllHashesMatch() ? _("The hashes match!") : _("The hashes do not match!");
    m_final_result_label->SetLabel(label);
  }
  Layout();
}

bool MD5Dialog::AllHashesMatch() const
{
  return std::adjacent_find(m_hashes.begin(), m_hashes.end(), std::not_equal_to<>()) ==
         m_hashes.end();
}

void MD5Dialog::OnClose(wxCloseEvent&)
{
  m_netplay_server->AbortMD5();
}

void MD5Dialog::OnCloseBtnPressed(wxCommandEvent&)
{
  Close();
}
