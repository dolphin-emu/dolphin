// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <string>
#include <wx/frame.h>

class CGameListCtrl;
class wxCheckBox;
class wxChoice;
class wxListBox;
class wxStaticText;
class wxTextCtrl;

class NetPlaySetupFrame final : public wxFrame
{
public:
	NetPlaySetupFrame(wxWindow* const parent, const CGameListCtrl* const game_list);
	~NetPlaySetupFrame();

private:
	void OnJoin(wxCommandEvent& event);
	void OnHost(wxCommandEvent& event);
	void OnQuit(wxCommandEvent& event);
	void OnChoice(wxCommandEvent& event);
	void OnResetTraversal(wxCommandEvent& event);

	void MakeNetPlayDiag(int port, const std::string& game, bool is_hosting);

	wxStaticText* m_ip_lbl;
	wxStaticText* m_client_port_lbl;
	wxTextCtrl*   m_nickname_text;
	wxStaticText* m_host_port_lbl;
	wxTextCtrl*   m_host_port_text;
	wxTextCtrl*   m_connect_port_text;
	wxTextCtrl*   m_connect_ip_text;
	wxChoice*     m_direct_traversal;
	wxStaticText* m_traversal_lbl;
	wxButton* m_trav_reset_btn;

	wxListBox*  m_game_lbox;
#ifdef USE_UPNP
	wxCheckBox* m_upnp_chk;
#endif

	const CGameListCtrl* const m_game_list;
};
