// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <wx/button.h>
#include <wx/listbox.h>
#include <wx/sizer.h>

#include "DolphinWX/NetPlay/ChangeGameDialog.h"
#include "DolphinWX/NetPlay/NetWindow.h"

ChangeGameDialog::ChangeGameDialog(wxWindow* parent, const CGameListCtrl* const game_list)
	: wxDialog(parent, wxID_ANY, _("Change Game"))
{
	m_game_lbox = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxLB_SORT);
	m_game_lbox->Bind(wxEVT_LISTBOX_DCLICK, &ChangeGameDialog::OnPick, this);

	NetPlayDialog::FillWithGameNames(m_game_lbox, *game_list);

	wxButton* const ok_btn = new wxButton(this, wxID_OK, _("Change"));
	ok_btn->Bind(wxEVT_BUTTON, &ChangeGameDialog::OnPick, this);

	wxBoxSizer* const szr = new wxBoxSizer(wxVERTICAL);
	szr->Add(m_game_lbox, 1, wxLEFT | wxRIGHT | wxTOP | wxEXPAND, 5);
	szr->Add(ok_btn, 0, wxALL | wxALIGN_RIGHT, 5);

	SetSizerAndFit(szr);
	SetFocus();
}

wxString ChangeGameDialog::GetChosenGameName() const
{
	return m_game_name;
}

void ChangeGameDialog::OnPick(wxCommandEvent& event)
{
	m_game_name = m_game_lbox->GetStringSelection();
	EndModal(wxID_OK);
}
