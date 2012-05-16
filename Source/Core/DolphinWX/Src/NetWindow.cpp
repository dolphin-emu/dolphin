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

#include <FileUtil.h>
#include <IniFile.h>

#include "NetPlay.h"
#include "NetWindow.h"
#include "Frame.h"

#include <sstream>

#define _connect_macro_(b, f, c, s)	\
	(b)->Connect(wxID_ANY, (c), wxCommandEventHandler(f), (wxObject*)0, (wxEvtHandler*)s)

#define NETPLAY_TITLEBAR	"Dolphin NetPlay"

BEGIN_EVENT_TABLE(NetPlayDiag, wxFrame)
	EVT_COMMAND(wxID_ANY, wxEVT_THREAD, NetPlayDiag::OnThread)
END_EVENT_TABLE()

static NetPlay* netplay_ptr = NULL;
extern CFrame* main_frame;
NetPlayDiag *NetPlayDiag::npd = NULL;

NetPlaySetupDiag::NetPlaySetupDiag(wxWindow* const parent, const CGameListCtrl* const game_list)
	: wxFrame(parent, wxID_ANY, wxT(NETPLAY_TITLEBAR), wxDefaultPosition, wxDefaultSize)
	, m_game_list(game_list)
{
	IniFile inifile;
	inifile.Load(File::GetUserPath(D_CONFIG_IDX) + "Dolphin.ini");
	IniFile::Section& netplay_section = *inifile.GetOrCreateSection("NetPlay");

	wxPanel* const panel = new wxPanel(this);

	// top row
	wxStaticText* const nick_lbl = new wxStaticText(panel, wxID_ANY, _("Nickname :"),
			wxDefaultPosition, wxDefaultSize);
	
	std::string nickname;
	netplay_section.Get("Nickname", &nickname, "Player");
	m_nickname_text = new wxTextCtrl(panel, wxID_ANY, wxString::From8BitData(nickname.c_str()));

	wxBoxSizer* const nick_szr = new wxBoxSizer(wxHORIZONTAL);
	nick_szr->Add(nick_lbl, 0, wxCENTER);
	nick_szr->Add(m_nickname_text, 0, wxALL, 5);


	// tabs
	wxNotebook* const notebook = new wxNotebook(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize);
	wxPanel* const connect_tab = new wxPanel(notebook, wxID_ANY, wxDefaultPosition, wxDefaultSize);
	notebook->AddPage(connect_tab, _("Connect"));
	wxPanel* const host_tab = new wxPanel(notebook, wxID_ANY, wxDefaultPosition, wxDefaultSize);
	notebook->AddPage(host_tab, _("Host"));


	// connect tab
	{
	wxStaticText* const ip_lbl = new wxStaticText(connect_tab, wxID_ANY, _("Address :"),
			wxDefaultPosition, wxDefaultSize);
	
	std::string address;
	netplay_section.Get("Address", &address, "localhost");
	m_connect_ip_text = new wxTextCtrl(connect_tab, wxID_ANY, wxString::FromAscii(address.c_str()));

	wxStaticText* const port_lbl = new wxStaticText(connect_tab, wxID_ANY, _("Port :"),
			wxDefaultPosition, wxDefaultSize);
	
	// string? w/e
	std::string port;
	netplay_section.Get("ConnectPort", &port, "2626");	
	m_connect_port_text = new wxTextCtrl(connect_tab, wxID_ANY, wxString::FromAscii(port.c_str()));

	wxButton* const connect_btn = new wxButton(connect_tab, wxID_ANY, _("Connect"));
	_connect_macro_(connect_btn, NetPlaySetupDiag::OnJoin, wxEVT_COMMAND_BUTTON_CLICKED, this);

	wxStaticText* const alert_lbl = new wxStaticText(connect_tab, wxID_ANY,
			_("ALERT:\n\nNetPlay will currently only work properly when using the following settings:\n - Dual Core [OFF]\n - Audio Throttle [OFF]\n - DSP-HLE with \"Null Audio\" or DSP-LLE\n - Manually set the exact number of controllers that will be used to [Standard Controller]\n\nAll players should try to use the same Dolphin version and settings.\nDisable all memory cards or send them to all players before starting.\nWiimote support has not been implemented.\n\nYou must forward TCP port to host!!"),
			wxDefaultPosition, wxDefaultSize);

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
	wxStaticText* const port_lbl = new wxStaticText(host_tab, wxID_ANY, _("Port :"),
			wxDefaultPosition, wxDefaultSize);
	
	// string? w/e
	std::string port;
	netplay_section.Get("HostPort", &port, "2626");	
	m_host_port_text = new wxTextCtrl(host_tab, wxID_ANY, wxString::FromAscii(port.c_str()));

	wxButton* const host_btn = new wxButton(host_tab, wxID_ANY, _("Host"));
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
	wxButton* const quit_btn = new wxButton(panel, wxID_ANY, _("Quit"));
	_connect_macro_(quit_btn, NetPlaySetupDiag::OnQuit, wxEVT_COMMAND_BUTTON_CLICKED, this);

	// main sizer
	wxBoxSizer* const main_szr = new wxBoxSizer(wxVERTICAL);
	main_szr->Add(nick_szr, 0, wxALL | wxALIGN_RIGHT, 5);
	main_szr->Add(notebook, 1, wxLEFT | wxRIGHT | wxEXPAND, 5);
	main_szr->Add(quit_btn, 0, wxALL | wxALIGN_RIGHT, 5);

	panel->SetSizerAndFit(main_szr);

	//wxBoxSizer* const diag_szr = new wxBoxSizer(wxVERTICAL);
	//diag_szr->Add(panel, 1, wxEXPAND);
	//SetSizerAndFit(diag_szr);

	main_szr->SetSizeHints(this);

	Center();
	Show();
}

NetPlaySetupDiag::~NetPlaySetupDiag()
{
	IniFile inifile;
	const std::string dolphin_ini = File::GetUserPath(D_CONFIG_IDX) + "Dolphin.ini";
	inifile.Load(dolphin_ini);
	IniFile::Section& netplay_section = *inifile.GetOrCreateSection("NetPlay");

	netplay_section.Set("Nickname", m_nickname_text->GetValue().mb_str());
	netplay_section.Set("Address", m_connect_ip_text->GetValue().mb_str());
	netplay_section.Set("ConnectPort", m_connect_port_text->GetValue().mb_str());
	netplay_section.Set("HostPort", m_host_port_text->GetValue().mb_str());

	inifile.Save(dolphin_ini);
	main_frame->g_NetPlaySetupDiag = NULL;
}

void NetPlaySetupDiag::OnHost(wxCommandEvent&)
{
	NetPlayDiag *&npd = NetPlayDiag::GetInstance();
	if (npd)
	{
		PanicAlertT("A NetPlay window is already open!!");
		return;
	}

	if (-1 == m_game_lbox->GetSelection())
	{
		PanicAlertT("You must choose a game!!");
		return;
	}

	std::string game(m_game_lbox->GetStringSelection().mb_str());

	npd = new NetPlayDiag(m_parent, m_game_list, game, true);
	unsigned long port = 0;
	m_host_port_text->GetValue().ToULong(&port);
	netplay_ptr = new NetPlayServer(u16(port)
		, std::string(m_nickname_text->GetValue().mb_str()), npd, game);
	if (netplay_ptr->is_connected)
	{
		npd->Show();
		Destroy();
	}
	else
	{
		PanicAlertT("Failed to Listen!!");
		npd->Destroy();
	}
}

void NetPlaySetupDiag::OnJoin(wxCommandEvent&)
{
	NetPlayDiag *&npd = NetPlayDiag::GetInstance();
	if (npd)
	{
		PanicAlertT("A NetPlay window is already open!!");
		return;
	}

	npd = new NetPlayDiag(m_parent, m_game_list, "");
	unsigned long port = 0;
	m_connect_port_text->GetValue().ToULong(&port);
	netplay_ptr = new NetPlayClient(std::string(m_connect_ip_text->GetValue().mb_str())
		, (u16)port, npd, std::string(m_nickname_text->GetValue().mb_str()));
	if (netplay_ptr->is_connected)
	{
		npd->Show();
		Destroy();
	}
	else
	{
		npd->Destroy();
	}
}

void NetPlaySetupDiag::OnQuit(wxCommandEvent&)
{
	Destroy();
}

NetPlayDiag::NetPlayDiag(wxWindow* const parent, const CGameListCtrl* const game_list,
		const std::string& game, const bool is_hosting)
	: wxFrame(parent, wxID_ANY, wxT(NETPLAY_TITLEBAR), wxDefaultPosition, wxDefaultSize)
	, m_selected_game(game)
	, m_game_list(game_list)
{
	wxPanel* const panel = new wxPanel(this);

	// top crap
	m_game_btn = new wxButton(panel, wxID_ANY,
			wxString(m_selected_game.c_str(), *wxConvCurrent).Prepend(_(" Game : ")),
			wxDefaultPosition, wxDefaultSize, wxBU_LEFT);
	
	if (is_hosting)
		_connect_macro_(m_game_btn, NetPlayDiag::OnChangeGame, wxEVT_COMMAND_BUTTON_CLICKED, this);
	else
		m_game_btn->Disable();

	// middle crap

	// chat
	m_chat_text = new wxTextCtrl(panel, wxID_ANY, wxEmptyString
		, wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_MULTILINE);

	m_chat_msg_text = new wxTextCtrl(panel, wxID_ANY, wxEmptyString
		, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
	_connect_macro_(m_chat_msg_text, NetPlayDiag::OnChat, wxEVT_COMMAND_TEXT_ENTER, this);

	wxButton* const chat_msg_btn = new wxButton(panel, wxID_ANY, _("Send"));
	_connect_macro_(chat_msg_btn, NetPlayDiag::OnChat, wxEVT_COMMAND_BUTTON_CLICKED, this);

	wxBoxSizer* const chat_msg_szr = new wxBoxSizer(wxHORIZONTAL);
	chat_msg_szr->Add(m_chat_msg_text, 1);
	chat_msg_szr->Add(chat_msg_btn, 0);

	wxStaticBoxSizer* const chat_szr = new wxStaticBoxSizer(wxVERTICAL, panel, _("Chat"));
	chat_szr->Add(m_chat_text, 1, wxEXPAND);
	chat_szr->Add(chat_msg_szr, 0, wxEXPAND | wxTOP, 5);

	m_player_lbox = new wxListBox(panel, wxID_ANY, wxDefaultPosition, wxSize(192,-1));

	wxStaticBoxSizer* const player_szr = new wxStaticBoxSizer(wxVERTICAL, panel, _("Players"));
	player_szr->Add(m_player_lbox, 1, wxEXPAND);
	// player list
	if (is_hosting)
	{
		wxButton* const player_config_btn = new wxButton(panel, wxID_ANY, _("Configure Pads"));
		_connect_macro_(player_config_btn, NetPlayDiag::OnConfigPads, wxEVT_COMMAND_BUTTON_CLICKED, this);
		player_szr->Add(player_config_btn, 0, wxEXPAND | wxTOP, 5);
	}

	wxBoxSizer* const mid_szr = new wxBoxSizer(wxHORIZONTAL);
	mid_szr->Add(chat_szr, 1, wxEXPAND | wxRIGHT, 5);
	mid_szr->Add(player_szr, 0, wxEXPAND);

	// bottom crap
	wxButton* const quit_btn = new wxButton(panel, wxID_ANY, _("Quit"));
	_connect_macro_(quit_btn, NetPlayDiag::OnQuit, wxEVT_COMMAND_BUTTON_CLICKED, this);

	wxBoxSizer* const bottom_szr = new wxBoxSizer(wxHORIZONTAL);
	if (is_hosting)
	{
		wxButton* const start_btn = new wxButton(panel, wxID_ANY, _("Start"));
		_connect_macro_(start_btn, NetPlayDiag::OnStart, wxEVT_COMMAND_BUTTON_CLICKED, this);
		bottom_szr->Add(start_btn);
		wxButton* const stop_btn = new wxButton(panel, wxID_ANY, _("Stop"));
		_connect_macro_(stop_btn, NetPlayDiag::OnStop, wxEVT_COMMAND_BUTTON_CLICKED, this);
		bottom_szr->Add(stop_btn);
		bottom_szr->Add(new wxStaticText(panel, wxID_ANY, _("Buffer:")), 0, wxLEFT | wxCENTER, 5 );
		wxSpinCtrl* const padbuf_spin = new wxSpinCtrl(panel, wxID_ANY, wxT("20")
			, wxDefaultPosition, wxSize(64, -1), wxSP_ARROW_KEYS, 0, 200, 20);
		_connect_macro_(padbuf_spin, NetPlayDiag::OnAdjustBuffer, wxEVT_COMMAND_SPINCTRL_UPDATED, this);
		wxButton* const padbuf_btn = new wxButton(panel, wxID_ANY, wxT("?"), wxDefaultPosition, wxSize(22, -1));
		_connect_macro_(padbuf_btn, NetPlayDiag::OnPadBuffHelp, wxEVT_COMMAND_BUTTON_CLICKED, this);
		bottom_szr->Add(padbuf_spin, 0, wxCENTER);
		bottom_szr->Add(padbuf_btn);
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
	if (netplay_ptr)
	{
		delete netplay_ptr;
		netplay_ptr = NULL;
	}
	npd = NULL;
}

void NetPlayDiag::OnChat(wxCommandEvent&)
{
	wxString s = m_chat_msg_text->GetValue();

	if (s.Length())
	{
		netplay_ptr->SendChatMessage(std::string(s.mb_str()));
		m_chat_text->AppendText(s.Prepend(wxT(" >> ")).Append(wxT('\n')));
		m_chat_msg_text->Clear();
	}
}

void NetPlayDiag::OnStart(wxCommandEvent&)
{
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
		netplay_ptr->StartGame(path);
	else
		PanicAlertT("Game not found!!");
}

void NetPlayDiag::OnStop(wxCommandEvent&)
{
	netplay_ptr->StopGame();
}

void NetPlayDiag::BootGame(const std::string& filename)
{
	main_frame->BootGame(filename);
}

void NetPlayDiag::StopGame()
{
	main_frame->DoStop();
}

// NetPlayUI methods called from ---NETPLAY--- thread
void NetPlayDiag::Update()
{
	wxCommandEvent evt(wxEVT_THREAD, 1);
	GetEventHandler()->AddPendingEvent(evt);
}

void NetPlayDiag::AppendChat(const std::string& msg)
{
	chat_msgs.Push(msg);
	// silly
	Update();
}

void NetPlayDiag::OnMsgChangeGame(const std::string& filename)
{
	wxCommandEvent evt(wxEVT_THREAD, NP_GUI_EVT_CHANGE_GAME);
	// TODO: using a wxString in AddPendingEvent from another thread is unsafe i guess?
	evt.SetString(wxString(filename.c_str(), *wxConvCurrent));
	GetEventHandler()->AddPendingEvent(evt);
}

void NetPlayDiag::OnMsgStartGame()
{
	wxCommandEvent evt(wxEVT_THREAD, NP_GUI_EVT_START_GAME);
	GetEventHandler()->AddPendingEvent(evt);
}

void NetPlayDiag::OnMsgStopGame()
{
	wxCommandEvent evt(wxEVT_THREAD, NP_GUI_EVT_STOP_GAME);
	GetEventHandler()->AddPendingEvent(evt);
}

void NetPlayDiag::OnPadBuffHelp(wxCommandEvent&)
{
	const u64 time = ((NetPlayServer*)netplay_ptr)->CalculateMinimumBufferTime();
	std::ostringstream ss;
	ss << "< Calculated from pings: required buffer: "
		<< time * (60.0f/1000) << "(60fps) / "
		<< time * (50.0f/1000) << "(50fps) >\n";

	m_chat_text->AppendText(wxString(ss.str().c_str(), *wxConvCurrent));
}

void NetPlayDiag::OnAdjustBuffer(wxCommandEvent& event)
{
	const int val = ((wxSpinCtrl*)event.GetEventObject())->GetValue();
	((NetPlayServer*)netplay_ptr)->AdjustPadBufferSize(val);

	std::ostringstream ss;
	ss << "< Pad Buffer: " << val << " >";
	netplay_ptr->SendChatMessage(ss.str());
	m_chat_text->AppendText(wxString(ss.str().c_str(), *wxConvCurrent).Append(wxT('\n')));
}

void NetPlayDiag::OnQuit(wxCommandEvent&)
{
	Destroy();
}

// update gui
void NetPlayDiag::OnThread(wxCommandEvent& event)
{
	// player list
	m_playerids.clear();
	std::string tmps;
	netplay_ptr->GetPlayerList(tmps, m_playerids);

	const int selection = m_player_lbox->GetSelection();

	m_player_lbox->Clear();
	std::istringstream ss(tmps);
	while (std::getline(ss, tmps))
		m_player_lbox->Append(wxString(tmps.c_str(), *wxConvCurrent));

	m_player_lbox->SetSelection(selection);

	switch (event.GetId())
	{
	case NP_GUI_EVT_CHANGE_GAME :
		// update selected game :/
		{
		m_selected_game.assign(event.GetString().mb_str());
		m_game_btn->SetLabel(event.GetString().Prepend(_(" Game : ")));
		}
		break;
	case NP_GUI_EVT_START_GAME :
		// client start game :/
		{
		wxCommandEvent evt;
		OnStart(evt);
		}
		break;
	case NP_GUI_EVT_STOP_GAME :
		// client stop game
		{
		wxCommandEvent evt;
		OnStop(evt);
		}
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

void NetPlayDiag::OnChangeGame(wxCommandEvent&)
{
	wxString game_name;
	ChangeGameDiag* const cgd = new ChangeGameDiag(this, m_game_list, game_name);
	cgd->ShowModal();

	if (game_name.length())
	{
		m_selected_game = std::string(game_name.mb_str());
		netplay_ptr->ChangeGame(m_selected_game);
		m_game_btn->SetLabel(game_name.Prepend(_(" Game : ")));
	}
}

void NetPlayDiag::OnConfigPads(wxCommandEvent&)
{
	int mapping[4];

	// get selected player id
	int pid = m_player_lbox->GetSelection();
	if (pid < 0)
		return;
	pid = m_playerids.at(pid);

	if (false == ((NetPlayServer*)netplay_ptr)->GetPadMapping(pid, mapping))
		return;

	PadMapDiag pmd(this, mapping);
	pmd.ShowModal();

	if (false == ((NetPlayServer*)netplay_ptr)->SetPadMapping(pid, mapping))
		PanicAlertT("Could not set pads. The player left or the game is currently running!\n"
				"(setting pads while the game is running is not yet supported)");
}

ChangeGameDiag::ChangeGameDiag(wxWindow* const parent, const CGameListCtrl* const game_list, wxString& game_name)
	: wxDialog(parent, wxID_ANY, _("Change Game"), wxDefaultPosition, wxDefaultSize)
	, m_game_name(game_name)
{
	m_game_lbox = new wxListBox(this, wxID_ANY);
	_connect_macro_(m_game_lbox, ChangeGameDiag::OnPick, wxEVT_COMMAND_LISTBOX_DOUBLECLICKED, this);

	// fill list with games
	std::istringstream ss(game_list->GetGameNames());
	std::string game;
	while (std::getline(ss,game))
		m_game_lbox->Append(wxString(game.c_str(), *wxConvCurrent));

	wxButton* const ok_btn = new wxButton(this, wxID_OK, _("Change"));
	_connect_macro_(ok_btn, ChangeGameDiag::OnPick, wxEVT_COMMAND_BUTTON_CLICKED, this);

	wxBoxSizer* const szr = new wxBoxSizer(wxVERTICAL);
	szr->Add(m_game_lbox, 1, wxLEFT | wxRIGHT | wxTOP | wxEXPAND, 5);
	szr->Add(ok_btn, 0, wxALL | wxALIGN_RIGHT, 5);

	SetSizerAndFit(szr);
	SetFocus();
}

void ChangeGameDiag::OnPick(wxCommandEvent& event)
{
	// return the selected game name
	m_game_name = m_game_lbox->GetStringSelection();
	EndModal(wxID_OK);
}

PadMapDiag::PadMapDiag(wxWindow* const parent, int map[])
	: wxDialog(parent, wxID_ANY, _("Configure Pads"), wxDefaultPosition, wxDefaultSize)
	, m_mapping(map)
{
	wxBoxSizer* const h_szr = new wxBoxSizer(wxHORIZONTAL);

	h_szr->AddSpacer(20);

	// labels
	wxBoxSizer* const label_szr = new wxBoxSizer(wxVERTICAL);
	label_szr->Add(new wxStaticText(this, wxID_ANY, _("Local")), 0, wxALIGN_TOP);
	label_szr->AddStretchSpacer(1);
	label_szr->Add(new wxStaticText(this, wxID_ANY, _("In-Game")), 0, wxALIGN_BOTTOM);

	h_szr->Add(label_szr, 1, wxTOP | wxEXPAND, 20);

	// set up choices
	wxString pad_names[5];
	pad_names[0] = _("None");
	for (unsigned int i=1; i<5; ++i)
		pad_names[i] = wxString(_("Pad ")) + (wxChar)(wxT('0')+i);

	for (unsigned int i=0; i<4; ++i)
	{
		wxChoice* const pad_cbox = m_map_cbox[i]
			= new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 5, pad_names);
		pad_cbox->Select(m_mapping[i] + 1);

		_connect_macro_(pad_cbox, PadMapDiag::OnAdjust, wxEVT_COMMAND_CHOICE_SELECTED, this);

		wxBoxSizer* const v_szr = new wxBoxSizer(wxVERTICAL);
		v_szr->Add(new wxStaticText(this,wxID_ANY, pad_names[i + 1]), 1, wxALIGN_CENTER_HORIZONTAL);
		v_szr->Add(pad_cbox, 1);

		h_szr->Add(v_szr, 1, wxTOP | wxEXPAND, 20);
	}

	h_szr->AddSpacer(20);

	wxBoxSizer* const main_szr = new wxBoxSizer(wxVERTICAL);
	main_szr->Add(h_szr);
	main_szr->AddSpacer(5);
	main_szr->Add(CreateButtonSizer(wxOK), 0, wxEXPAND | wxLEFT | wxRIGHT, 20);
	main_szr->AddSpacer(5);
	SetSizerAndFit(main_szr);
	SetFocus();
}

void PadMapDiag::OnAdjust(wxCommandEvent& event)
{
	(void)event;
	for (unsigned int i=0; i<4; ++i)
		m_mapping[i] = m_map_cbox[i]->GetSelection() - 1;
}
