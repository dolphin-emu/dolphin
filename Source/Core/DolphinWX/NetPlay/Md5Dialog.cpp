// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <wx/gauge.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/panel.h>
#include <wx/event.h>
#include <wx/button.h>

#include "DolphinWX/NetPlay/Md5Dialog.h"
#include "DolphinWX/NetPlay/NetWindow.h"
#include "Common/StringUtil.h"

Md5Dialog::Md5Dialog(
	wxWindow* parent,
	NetPlayServer* server,
	std::vector<const Player*> players,
	const std::string game
)
	: wxFrame(parent, wxID_ANY, _("MD5 Checksum"))
	, m_parent(parent)
	, m_netplay_server(server)
{
	this->Bind(wxEVT_CLOSE_WINDOW, &Md5Dialog::OnClose, this);
	wxBoxSizer* const szr = new wxBoxSizer(wxVERTICAL);

	szr->Add(
		new wxStaticText(
			this,
			wxID_ANY,
			_("Computing MD5 Checksum for:") + "\n" + game,
			wxDefaultPosition,
			wxDefaultSize,
			wxALIGN_CENTRE_HORIZONTAL
		),
		0,
		wxALIGN_CENTER_HORIZONTAL | wxALL,
		5
	);

	for (const Player* player : players)
	{
		wxStaticBoxSizer* const player_szr = new wxStaticBoxSizer(
			wxVERTICAL,
			this,
			player->name + " (p" + std::to_string(player->pid) + ")"
		);

		wxGauge* gauge = new wxGauge(this, wxID_ANY, 100);
		m_progress_bars[player->pid] = gauge;
		player_szr->Add(gauge, 0, wxEXPAND |  wxALL, 5);

		m_result_labels[player->pid] = new wxStaticText(
			this,
			wxID_ANY,
			_("Computing..."),
			wxDefaultPosition,
			wxSize(250, 20),
			wxALIGN_CENTRE_HORIZONTAL
		);

		m_result_labels[player->pid]->SetSize(250, 15);
		player_szr->Add(m_result_labels[player->pid], 0, wxALL, 5);

		szr->Add(player_szr, 0, wxEXPAND |  wxALL, 5);
	}

	m_final_result_label = new wxStaticText(
		this,
		wxID_ANY,
		" ", // so it takes space
		wxDefaultPosition,
		wxDefaultSize,
		wxALIGN_CENTRE_HORIZONTAL
	);
	szr->Add(m_final_result_label, 1, wxALL, 5);

	wxButton* close_btn = new wxButton(this, wxID_ANY, _("Close"));
	close_btn->Bind(wxEVT_BUTTON, &Md5Dialog::OnCloseBtnPressed, this);
	szr->Add(close_btn, 0, wxEXPAND | wxALL, 5);

	SetSizerAndFit(szr);
	SetFocus();
	Center();
}

void Md5Dialog::SetProgress(int pid, int progress)
{
	if (m_progress_bars[pid] == nullptr)
		return;

	m_progress_bars[pid]->SetValue(progress);
	m_result_labels[pid]->SetLabel(_("Computing: ") + std::to_string(progress) + "%");
	Update();
}

void Md5Dialog::SetResult(int pid, const std::string& result)
{
	if (m_result_labels[pid] == nullptr)
		return;

	m_result_labels[pid]->SetLabel(result);
	m_hashes.push_back(result);

	if (m_hashes.size() > 1)
	{
		bool matches = true;
		for (uint i = 1; i < m_hashes.size(); i++)
		{
			matches = matches && m_hashes[i] == m_hashes[i - 1];
		}

		wxString label = matches
			? _("Hashes match!")
			: _("Hashes do not match.");

		m_final_result_label->SetLabel(label);
	}
}

void Md5Dialog::OnClose(wxCloseEvent& event)
{
	m_netplay_server->AbortMd5();
}

void Md5Dialog::OnCloseBtnPressed(wxCommandEvent&)
{
	Close();
}
