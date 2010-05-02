// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "NetPlay.h"
#include "NetWindow.h"

#include <sstream>

#define _connect_macro_( b, f, c, s )	(b)->Connect( wxID_ANY, (c), wxCommandEventHandler( f ), (wxObject*)0, (wxEvtHandler*)s )

#define NETPLAY_TITLEBAR	"Dolphin NetPlay"

BEGIN_EVENT_TABLE(NetPlayDiag, wxFrame)
	EVT_COMMAND(wxID_ANY, wxEVT_THREAD, NetPlayDiag::OnThread)
END_EVENT_TABLE()

NetPlay* netplay_ptr = NULL;

NetPlaySetupDiag::NetPlaySetupDiag(wxWindow* parent, const CGameListCtrl* const game_list)
	: wxFrame(parent, wxID_ANY, wxT(NETPLAY_TITLEBAR), wxDefaultPosition, wxDefaultSize)
	, m_game_list(game_list)
{
	//PanicAlert("ALERT: NetPlay is not 100%% functional !!!!");

	wxPanel* const panel = new wxPanel(this);

	// top row
	wxStaticText* const nick_lbl = new wxStaticText(panel, wxID_ANY, wxT("Nickname :"), wxDefaultPosition, wxDefaultSize);
	m_nickname_text = new wxTextCtrl(panel, wxID_ANY, wxT("Player"));

	wxBoxSizer* const nick_szr = new wxBoxSizer(wxHORIZONTAL);
	nick_szr->Add(nick_lbl, 0, wxCENTER);
	nick_szr->Add(m_nickname_text, 0, wxALL, 5);


	// tabs
	wxNotebook* const notebook = new wxNotebook(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize);
	wxPanel* const connect_tab = new wxPanel(notebook, wxID_ANY, wxDefaultPosition, wxDefaultSize);
	notebook->AddPage(connect_tab, wxT("Connect"));
	wxPanel* const host_tab = new wxPanel(notebook, wxID_ANY, wxDefaultPosition, wxDefaultSize);
	notebook->AddPage(host_tab, wxT("Host"));


	// connect tab
	{
	wxStaticText* const ip_lbl = new wxStaticText(connect_tab, wxID_ANY, wxT("Address :"), wxDefaultPosition, wxDefaultSize);
	m_connect_ip_text = new wxTextCtrl(connect_tab, wxID_ANY, wxT("localhost"));
	wxStaticText* const port_lbl = new wxStaticText(connect_tab, wxID_ANY, wxT("Port :"), wxDefaultPosition, wxDefaultSize);
	m_connect_port_text = new wxTextCtrl(connect_tab, wxID_ANY, wxT("2626"));

	wxButton* const connect_btn = new wxButton(connect_tab, wxID_ANY, wxT("Connect"));
	//connect_button->Disable();
	_connect_macro_(connect_btn, NetPlaySetupDiag::OnJoin, wxEVT_COMMAND_BUTTON_CLICKED, this);

	wxStaticText* const alert_lbl = new wxStaticText(connect_tab, wxID_ANY
		, wxT("ALERT:\n\nNetPlay will currently only work properly when using the following settings:")
			wxT("\n - Dual Core [OFF]")
			wxT("\n - Audio Throttle [OFF] (if using DSP HLE)")
			wxT("\n - DSP LLE Plugin (may not be needed)\n - DSPLLE on thread [OFF]")
			wxT("\n - Manually set the exact number of controller that will be used to [Standard Controller]")
			wxT("\n\nAll players should try to use the same Dolphin version and settings.")
			wxT("\nDisable all memory cards or send them to all players before starting.")
			wxT("\nWiimote support has not been implemented.\nWii games will likely desync for other reasons as well.")
			wxT("\n\nYou must forward TCP port to host!!")
		, wxDefaultPosition, wxDefaultSize);

	wxBoxSizer* const top_szr = new wxBoxSizer(wxHORIZONTAL);
	top_szr->Add(ip_lbl, 0, wxCENTER | wxRIGHT, 5);
	top_szr->Add(m_connect_ip_text, 3);
	top_szr->Add(port_lbl, 0, wxCENTER | wxRIGHT | wxLEFT, 5);
	top_szr->Add(m_connect_port_text, 1);

	wxBoxSizer* const con_szr = new wxBoxSizer(wxVERTICAL);
	con_szr->Add(top_szr, 0, wxALL | wxEXPAND, 5);
	con_szr->AddStretchSpacer(1);
	con_szr->Add(alert_lbl, 0, wxLEFT | wxRIGHT | wxEXPAND, 5);
	con_szr->AddStretchSpacer(1);
	con_szr->Add(connect_btn, 0, wxALL | wxALIGN_RIGHT, 5);

	connect_tab->SetSizerAndFit(con_szr);
	}

	// host tab
	{
	wxStaticText* const port_lbl = new wxStaticText(host_tab, wxID_ANY, wxT("Port :"), wxDefaultPosition, wxDefaultSize);
	m_host_port_text = new wxTextCtrl(host_tab, wxID_ANY, wxT("2626"));

	wxButton* const host_btn = new wxButton(host_tab, wxID_ANY, wxT("Host"));
	//host_button->Disable();
	_connect_macro_(host_btn, NetPlaySetupDiag::OnHost, wxEVT_COMMAND_BUTTON_CLICKED, this);

	m_game_lbox = new wxListBox(host_tab, wxID_ANY);
	_connect_macro_(m_game_lbox, NetPlaySetupDiag::OnHost, wxEVT_COMMAND_LISTBOX_DOUBLECLICKED, this);

	std::istringstream ss(game_list->GetGameNames());
	std::string game;
	while (std::getline(ss,game))
		m_game_lbox->Append(wxString(game.c_str(), *wxConvCurrent));

	wxBoxSizer* const top_szr = new wxBoxSizer(wxHORIZONTAL);
	top_szr->Add(port_lbl, 0, wxCENTER | wxRIGHT, 5);
	top_szr->Add(m_host_port_text, 0);

	wxBoxSizer* const host_szr = new wxBoxSizer(wxVERTICAL);
	host_szr->Add(top_szr, 0, wxALL | wxEXPAND, 5);
	host_szr->Add(m_game_lbox, 1, wxLEFT | wxRIGHT | wxEXPAND, 5);
	host_szr->Add(host_btn, 0, wxALL | wxALIGN_RIGHT, 5);

	host_tab->SetSizerAndFit(host_szr);
	}

	// bottom row
	wxButton* const quit_btn = new wxButton(panel, wxID_ANY, wxT("Quit"));
	_connect_macro_(quit_btn, NetPlaySetupDiag::OnQuit, wxEVT_COMMAND_BUTTON_CLICKED, this);

	// main sizer
	wxBoxSizer* const main_szr = new wxBoxSizer(wxVERTICAL);
	main_szr->Add(nick_szr, 0, wxALL | wxALIGN_RIGHT, 5);
	main_szr->Add(notebook, 1, wxLEFT | wxRIGHT | wxEXPAND, 5);
	main_szr->Add(quit_btn, 0, wxALL | wxALIGN_RIGHT, 5);

	panel->SetSizerAndFit(main_szr);

	//wxBoxSizer* const diag_szr = new wxBoxSizer(wxVERTICAL);
	//diag_szr->Add(panel);
	//SetSizerAndFit(diag_szr);

	main_szr->SetSizeHints(this);

	Center();
	Show();
}

void NetPlaySetupDiag::OnHost(wxCommandEvent& event)
{
	// warning removal
	event.GetId();

	if (::netplay_ptr)
	{
		PanicAlert("A NetPlay window is already open!!");
		return;
	}

	if (-1 == m_game_lbox->GetSelection())
	{
		PanicAlert("You must choose a game!!");
		return;
	}

	std::string game(m_game_lbox->GetStringSelection().mb_str());

	NetPlayDiag* const npd = new NetPlayDiag(m_parent, m_game_list, game, true);
	unsigned long port = 0;
	m_host_port_text->GetValue().ToULong(&port);
	::netplay_ptr = new NetPlayServer(u16(port)
		, std::string(m_nickname_text->GetValue().mb_str()), npd, game);
	if (::netplay_ptr->is_connected)
	{
		//NetPlayServerDiag* const npsd = 
		npd->Show();
		Destroy();
	}
	else
	{
		PanicAlert("Failed to Listen!!");
		npd->Destroy();
		// dialog will delete netplay
		//delete ::netplay_ptr;
	}
}

void NetPlaySetupDiag::OnJoin(wxCommandEvent& event)
{
	// warning removal
	event.GetId();

	if (::netplay_ptr)
	{
		PanicAlert("A NetPlay window is already open!!");
		return;
	}

	NetPlayDiag* const npd = new NetPlayDiag(m_parent, m_game_list, "");
	unsigned long port = 0;
	m_connect_port_text->GetValue().ToULong(&port);
	::netplay_ptr = new NetPlayClient(std::string(m_connect_ip_text->GetValue().mb_str())
		, (u16)port, std::string(m_nickname_text->GetValue().mb_str()), npd);
	if (::netplay_ptr->is_connected)
	{
		//NetPlayServerDiag* const npsd = 
		npd->Show();
		Destroy();
	}
	else
	{
		//PanicAlert("Failed to Connect!!");
		npd->Destroy();
		// dialog will delete netplay
		//delete ::netplay_ptr;
	}
}

void NetPlaySetupDiag::OnQuit(wxCommandEvent& event)
{
	// warning removal
	event.GetId();

	Destroy();
}

NetPlayDiag::NetPlayDiag(wxWindow* parent, const CGameListCtrl* const game_list
						 , const std::string& game, const bool is_hosting)
	: wxFrame(parent, wxID_ANY, wxT(NETPLAY_TITLEBAR), wxDefaultPosition, wxDefaultSize)
	, m_selected_game(game)
	, m_game_list(game_list)
{
	wxPanel* const panel = new wxPanel(this);

	// top crap
	m_game_btn = new wxButton(panel, wxID_ANY
		, wxString(m_selected_game.c_str(), *wxConvCurrent).Prepend(wxT(" Game : ")), wxDefaultPosition, wxDefaultSize, wxBU_LEFT);
	m_game_btn->Disable();

	// middle crap

	// chat
	m_chat_text = new wxTextCtrl(panel, wxID_ANY, wxEmptyString
		, wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_MULTILINE);

	m_chat_msg_text = new wxTextCtrl(panel, wxID_ANY, wxEmptyString
		, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
	_connect_macro_(m_chat_msg_text, NetPlayDiag::OnChat, wxEVT_COMMAND_TEXT_ENTER, this);

	wxButton* const chat_msg_btn = new wxButton(panel, wxID_ANY, wxT("Send"));
	_connect_macro_(chat_msg_btn, NetPlayDiag::OnChat, wxEVT_COMMAND_BUTTON_CLICKED, this);

	wxBoxSizer* const chat_msg_szr = new wxBoxSizer(wxHORIZONTAL);
	chat_msg_szr->Add(m_chat_msg_text, 1);
	chat_msg_szr->Add(chat_msg_btn, 0);

	wxStaticBoxSizer* const chat_szr = new wxStaticBoxSizer(wxVERTICAL, panel, wxT("Chat"));
	chat_szr->Add(m_chat_text, 1, wxEXPAND);
	chat_szr->Add(chat_msg_szr, 0, wxEXPAND | wxTOP, 5);

	m_player_lbox = new wxListBox(panel, wxID_ANY, wxDefaultPosition, wxSize(192,-1));

	wxStaticBoxSizer* const player_szr = new wxStaticBoxSizer(wxVERTICAL, panel, wxT("Players"));
	player_szr->Add(m_player_lbox, 1, wxEXPAND);
	// player list
	if (is_hosting)
	{
		wxButton* const player_config_btn = new wxButton(panel, wxID_ANY, wxT("Configure Pads [not implemented]"));
		player_config_btn->Disable();
		player_szr->Add(player_config_btn, 0, wxEXPAND | wxTOP, 5);
	}

	wxBoxSizer* const mid_szr = new wxBoxSizer(wxHORIZONTAL);
	mid_szr->Add(chat_szr, 1, wxEXPAND | wxRIGHT, 5);
	mid_szr->Add(player_szr, 0, wxEXPAND);

	// bottom crap
	wxButton* const quit_btn = new wxButton(panel, wxID_ANY, wxT("Quit"));
	_connect_macro_(quit_btn, NetPlayDiag::OnQuit, wxEVT_COMMAND_BUTTON_CLICKED, this);

	wxBoxSizer* const bottom_szr = new wxBoxSizer(wxHORIZONTAL);
	if (is_hosting)
	{
		wxButton* const start_btn = new wxButton(panel, wxID_ANY, wxT("Start"));
		_connect_macro_(start_btn, NetPlayDiag::OnStart, wxEVT_COMMAND_BUTTON_CLICKED, this);
		bottom_szr->Add(start_btn);
	}
		
	bottom_szr->AddStretchSpacer(1);
	bottom_szr->Add(quit_btn);

	// main sizer
	wxBoxSizer* const main_szr = new wxBoxSizer(wxVERTICAL);
	main_szr->Add(m_game_btn, 0, wxEXPAND | wxALL, 5);
	main_szr->Add(mid_szr, 1, wxEXPAND | wxLEFT | wxRIGHT, 5);
	main_szr->Add(bottom_szr, 0, wxEXPAND | wxALL, 5);

	panel->SetSizerAndFit(main_szr);

	main_szr->SetSizeHints(this);
	SetSize(512, 512-128);

	Center();
}

NetPlayDiag::~NetPlayDiag()
{
	if (::netplay_ptr)
	{
		delete netplay_ptr;
		::netplay_ptr = NULL;
	}
}

void NetPlayDiag::OnChat(wxCommandEvent& event)
{
	// warning removal
	event.GetId();

	wxString s = m_chat_msg_text->GetValue();

	if (s.Length())
	{
		::netplay_ptr->SendChatMessage(std::string(s.mb_str()));
		m_chat_text->AppendText(s.Prepend(wxT(" >> ")).Append(wxT('\n')));
		m_chat_msg_text->Clear();
	}
}

void NetPlayDiag::OnStart(wxCommandEvent& event)
{
	// warning removal
	event.GetId();

	// find path for selected game
	std::string ntmp, ptmp, path;
	std::istringstream nss(m_game_list->GetGameNames()), pss(m_game_list->GetGamePaths());

	while(std::getline(nss,ntmp))
	{
		std::getline(pss,ptmp);
		if (m_selected_game == ntmp)
		{
			path = ptmp;
			break;
		}
	}

	if (path.length())
		::netplay_ptr->StartGame(path);
	else
		PanicAlert("Game not found!!");
}

void NetPlayDiag::OnQuit(wxCommandEvent& event)
{
	// warning removal
	event.GetId();

	Destroy();
}

// update gui
void NetPlayDiag::OnThread(wxCommandEvent& event)
{
	// warning removal
	event.GetId();

	// player list
	std::string tmps;
	::netplay_ptr->GetPlayerList(tmps);

	m_player_lbox->Clear();
	std::istringstream ss(tmps);
	while (std::getline(ss, tmps))
		m_player_lbox->Append(wxString(tmps.c_str(), *wxConvCurrent));

	switch (event.GetId())
	{
	case 45 :
		// update selected game :/
		m_selected_game.assign(event.GetString().mb_str());
		m_game_btn->SetLabel(event.GetString().Prepend(wxT(" Game : ")));
		break;
	case 46 :
		// client start game :/
		wxCommandEvent evt;
		OnStart(evt);
		break;
	}

	// chat messages
	while (chat_msgs.Size())
	{
		std::string s;
		chat_msgs.Pop(s);
		//PanicAlert("message: %s", s.c_str());
		m_chat_text->AppendText(wxString(s.c_str(), *wxConvCurrent).Append(wxT('\n')));
	}
}
