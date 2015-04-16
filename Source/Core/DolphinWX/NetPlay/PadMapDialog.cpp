// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <wx/choice.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include "Core/NetPlayClient.h"
#include "Core/NetPlayProto.h"
#include "DolphinWX/NetPlay/PadMapDialog.h"

PadMapDialog::PadMapDialog(wxWindow* parent, PadMapping map[], PadMapping wiimotemap[], std::vector<const Player*>& player_list)
	: wxDialog(parent, wxID_ANY, _("Configure Pads"))
	, m_mapping(map)
	, m_wiimapping(wiimotemap)
	, m_player_list(player_list)
{
	wxBoxSizer* const h_szr = new wxBoxSizer(wxHORIZONTAL);
	h_szr->AddSpacer(10);

	wxArrayString player_names;
	player_names.Add(_("None"));
	for (auto& player : m_player_list)
		player_names.Add(player->name);

	for (unsigned int i = 0; i < 4; ++i)
	{
		wxBoxSizer* const v_szr = new wxBoxSizer(wxVERTICAL);
		v_szr->Add(new wxStaticText(this, wxID_ANY, (wxString(_("Pad ")) + (wxChar)('0' + i))),
			1, wxALIGN_CENTER_HORIZONTAL);

		m_map_cbox[i] = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, player_names);
		m_map_cbox[i]->Bind(wxEVT_CHOICE, &PadMapDialog::OnAdjust, this);
		if (m_mapping[i] == -1)
		{
			m_map_cbox[i]->Select(0);
		}
		else
		{
			for (unsigned int j = 0; j < m_player_list.size(); j++)
			{
				if (m_mapping[i] == m_player_list[j]->pid)
					m_map_cbox[i]->Select(j + 1);
			}
		}

		v_szr->Add(m_map_cbox[i], 1);

		h_szr->Add(v_szr, 1, wxTOP | wxEXPAND, 20);
		h_szr->AddSpacer(10);
	}

	for (unsigned int i = 0; i < 4; ++i)
	{
		wxBoxSizer* const v_szr = new wxBoxSizer(wxVERTICAL);
		v_szr->Add(new wxStaticText(this, wxID_ANY, (wxString(_("Wiimote ")) + (wxChar)('0' + i))),
			1, wxALIGN_CENTER_HORIZONTAL);

		m_map_cbox[i + 4] = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, player_names);
		m_map_cbox[i + 4]->Bind(wxEVT_CHOICE, &PadMapDialog::OnAdjust, this);
		if (m_wiimapping[i] == -1)
		{
			m_map_cbox[i + 4]->Select(0);
		}
		else
		{
			for (unsigned int j = 0; j < m_player_list.size(); j++)
			{
				if (m_wiimapping[i] == m_player_list[j]->pid)
					m_map_cbox[i + 4]->Select(j + 1);
			}
		}

		v_szr->Add(m_map_cbox[i + 4], 1);

		h_szr->Add(v_szr, 1, wxTOP | wxEXPAND, 20);
		h_szr->AddSpacer(10);
	}

	wxBoxSizer* const main_szr = new wxBoxSizer(wxVERTICAL);
	main_szr->Add(h_szr);
	main_szr->AddSpacer(5);
	main_szr->Add(CreateButtonSizer(wxOK), 0, wxEXPAND | wxLEFT | wxRIGHT, 20);
	main_szr->AddSpacer(5);
	SetSizerAndFit(main_szr);
	SetFocus();
}

void PadMapDialog::OnAdjust(wxCommandEvent& WXUNUSED(event))
{
	for (unsigned int i = 0; i < 4; i++)
	{
		int player_idx = m_map_cbox[i]->GetSelection();
		if (player_idx > 0)
			m_mapping[i] = m_player_list[player_idx - 1]->pid;
		else
			m_mapping[i] = -1;

		player_idx = m_map_cbox[i + 4]->GetSelection();
		if (player_idx > 0)
			m_wiimapping[i] = m_player_list[player_idx - 1]->pid;
		else
			m_wiimapping[i] = -1;
	}
}
