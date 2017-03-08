// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <wx/choice.h>
#include <wx/gbsizer.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include "DolphinWX/NetPlay/PadMapDialog.h"
#include "DolphinWX/WxUtils.h"

#include "Core/NetPlayClient.h"
#include "Core/NetPlayProto.h"
#include "Core/NetPlayServer.h"

PadMapDialog::PadMapDialog(wxWindow* parent, NetPlayServer* server, NetPlayClient* client)
    : wxDialog(parent, wxID_ANY, _("Controller Ports")), m_pad_mapping(server->GetPadMapping()),
      m_wii_mapping(server->GetWiimoteMapping()), m_player_list(client->GetPlayers())
{
  const int space5 = FromDIP(5);
  const int space10 = FromDIP(10);

  wxGridBagSizer* pad_sizer = new wxGridBagSizer(space5, space10);

  wxArrayString player_names;
  player_names.Add(_("None"));
  for (const auto& player : m_player_list)
    player_names.Add(StrToWxStr(player->name));

  auto build_choice = [&](unsigned int base_idx, unsigned int idx, const PadMappingArray& mapping,
                          const wxString& port_name) {
    pad_sizer->Add(new wxStaticText(this, wxID_ANY, wxString::Format("%s %d", port_name, idx + 1)),
                   wxGBPosition(0, base_idx + idx), wxDefaultSpan, wxALIGN_CENTER);

    m_map_cbox[base_idx + idx] =
        new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, player_names);
    m_map_cbox[base_idx + idx]->Select(0);
    m_map_cbox[base_idx + idx]->Bind(wxEVT_CHOICE, &PadMapDialog::OnAdjust, this);
    if (mapping[idx] != -1)
    {
      for (unsigned int j = 0; j < m_player_list.size(); j++)
      {
        if (mapping[idx] == m_player_list[j]->pid)
        {
          m_map_cbox[base_idx + idx]->Select(j + 1);
          break;
        }
      }
    }
    // Combo boxes break on Windows when wxEXPAND-ed vertically but you can't control the
    // direction of expansion in a grid sizer. Solution is to wrap in a box sizer.
    wxBoxSizer* wrapper = new wxBoxSizer(wxHORIZONTAL);
    wrapper->Add(m_map_cbox[base_idx + idx], 1, wxALIGN_CENTER_VERTICAL);
    pad_sizer->Add(wrapper, wxGBPosition(1, base_idx + idx), wxDefaultSpan, wxEXPAND);
  };

  for (unsigned int i = 0; i < 4; ++i)
  {
    // This looks a little weird but it's fine because we're using a grid bag sizer;
    // we can add columns in any order.
    build_choice(0, i, m_pad_mapping, _("GC Port"));
    build_choice(4, i, m_wii_mapping, _("Wii Remote"));
  }

  wxBoxSizer* const main_szr = new wxBoxSizer(wxVERTICAL);
  main_szr->AddSpacer(space10);
  main_szr->Add(pad_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space10);
  main_szr->AddSpacer(space10);
  main_szr->Add(CreateButtonSizer(wxOK), 0, wxEXPAND | wxLEFT | wxRIGHT, space10);
  main_szr->AddSpacer(space5);
  SetSizerAndFit(main_szr);
  SetFocus();
}

PadMappingArray PadMapDialog::GetModifiedPadMappings() const
{
  return m_pad_mapping;
}

PadMappingArray PadMapDialog::GetModifiedWiimoteMappings() const
{
  return m_wii_mapping;
}

void PadMapDialog::OnAdjust(wxCommandEvent& WXUNUSED(event))
{
  for (unsigned int i = 0; i < 4; i++)
  {
    int player_idx = m_map_cbox[i]->GetSelection();
    if (player_idx > 0)
      m_pad_mapping[i] = m_player_list[player_idx - 1]->pid;
    else
      m_pad_mapping[i] = -1;

    player_idx = m_map_cbox[i + 4]->GetSelection();
    if (player_idx > 0)
      m_wii_mapping[i] = m_player_list[player_idx - 1]->pid;
    else
      m_wii_mapping[i] = -1;
  }
}
