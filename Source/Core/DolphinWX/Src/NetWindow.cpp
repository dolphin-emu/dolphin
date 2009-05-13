// Copyright (C) 2003-2009 Dolphin Project.

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

#include "NetWindow.h"

///////////////////////
// Main Frame window

BEGIN_EVENT_TABLE(NetPlay, wxDialog)
	EVT_BUTTON(ID_BUTTON_JOIN,     NetPlay::OnJoin)
	EVT_BUTTON(ID_BUTTON_HOST,     NetPlay::OnHost)

	EVT_HOST_COMMAND(wxID_ANY,     NetPlay::OnNetEvent)

	EVT_CHECKBOX(ID_READY,         NetPlay::OnGUIEvent)
	EVT_CHECKBOX(ID_RECORD,        NetPlay::OnGUIEvent)
	EVT_BUTTON(ID_CHANGEGAME,      NetPlay::OnGUIEvent)
	EVT_BUTTON(ID_BUTTON_GETIP,    NetPlay::OnGUIEvent)
	EVT_BUTTON(ID_BUTTON_GETPING,  NetPlay::OnGUIEvent)
	EVT_BUTTON(ID_BUTTON_CHAT,     NetPlay::OnGUIEvent)
	EVT_TEXT_ENTER(ID_CHAT,        NetPlay::OnGUIEvent)
	EVT_BUTTON(ID_BUTTON_QUIT,     NetPlay::OnDisconnect)
	EVT_CLOSE(NetPlay::OnQuit)
END_EVENT_TABLE()

NetPlay::NetPlay(wxWindow* parent, std::string GamePaths, std::string GameNames) :
	wxDialog(parent, wxID_ANY, _T("Net Play"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE & ~ wxMAXIMIZE_BOX )
{
	m_selectedGame = 'a'; m_hostaddr = 'a';
	m_games = GameNames;  m_paths = GamePaths;
	m_isHosting = 2;      m_ready = m_clients_ready = false;
	m_loopframe = m_frame = 0;

	DrawGUI();
}

void NetPlay::OnJoin(wxCommandEvent& WXUNUSED(event))
{
	wxString addr = m_ConAddr->GetValue();
	wxString host = addr.substr(0, addr.find(':'));
	wxString port_str = addr.substr(addr.find(':')+1);
	int port; TryParseInt(port_str.mb_str(), &port);
	
	m_nick = std::string(m_SetNick->GetValue().mb_str());
	if (m_nick.size() > 255)
		m_nick = m_nick.substr(0 , 255);

	// Create the client socket
	sf::SocketTCP sock_client;

	if (sock_client.Connect(port, host.mb_str(), 1.5) == sf::Socket::Done)
	{
		m_sock_client = new ClientSide(this, sock_client, std::string(addr.mb_str()), m_nick);
		m_sock_client->Create();
		m_sock_client->Run();

		// Create the GUI
		m_isHosting = false;
		DrawNetWindow();
	}
	else
	{
		PanicAlert("Can't connect to the specified IP Address ! \nMake sure Hosting port is forwarded !");
	}
}

void NetPlay::OnHost(wxCommandEvent& WXUNUSED(event))
{
	TryParseInt(m_SetPort->GetValue().mb_str(), (int*)&m_port);

	if (m_GameList->GetSelection() == wxNOT_FOUND) {
		PanicAlert("No Game Selected ! Please select a Game...");
		return;
	}
	if (!m_SetPort->GetValue().size() || m_port < 100 || m_port > 65535) {
		PanicAlert("Bad Port entered (%d) ! Please enter a working socket port...", m_port);
		return;
	}
	
	m_nick = std::string(m_SetNick->GetValue().mb_str());
	if (m_nick.size() > 255) m_nick = m_nick.substr(0 , 255);

	m_NetModel = m_NetMode->GetSelection();
	m_selectedGame = std::string(m_GameList_str[m_GameList->GetSelection()].mb_str());
	m_numClients = 0;

	// Start the listening socket
	if (m_listensocket.Listen(m_port))
	{
		m_sock_server = new ServerSide(this, m_listensocket, m_NetModel, m_nick);
		m_sock_server->Create();
		m_sock_server->Run();

		// Create the GUI
		m_isHosting = true;
		DrawNetWindow();
		m_Logging->AppendText(wxString::Format(wxT("WARNING : Hosting requires port to be forwarded in firewall!\n"
					"*Creation Successful on port %d : Waiting for peers...\n"), m_port));
	}
	else
	{
		PanicAlert("Could not listen at specified port !\nMake sure hosting port is not in use !");
		return;
	}
}

void NetPlay::DrawGUI()
{
	int str_end = -1;
	int str_start = -1;
	wxArrayString netmodes_str;

	for(int i = 0; i < (int)m_games.size(); i++)
	{
		str_start = str_end + 1;
		str_end = (int)m_games.find('\n', str_start);
		std::string buffer = m_games.substr(str_start, str_end - str_start);

		if (str_end == (int)std::string::npos || buffer.size() < 1)
			break; // we reached the end of the string

		m_GameList_str.Add(wxString::FromAscii(buffer.c_str()));
	}

	netmodes_str.Add(wxT("P2P Versus (2 players, faster)"));
	netmodes_str.Add(wxT("Server Mode (4 players, slower)"));

	// Tabs
	m_Notebook = new wxNotebook(this, ID_NOTEBOOK, wxDefaultPosition, wxDefaultSize);
	m_Tab_Connect = new wxPanel(m_Notebook, ID_TAB_CONN, wxDefaultPosition, wxDefaultSize);
	m_Notebook->AddPage(m_Tab_Connect, wxT("Connect"));
	m_Tab_Host = new wxPanel(m_Notebook, ID_TAB_HOST, wxDefaultPosition, wxDefaultSize);
	m_Notebook->AddPage(m_Tab_Host, wxT("Host"));

	// Nickname setting
	m_SetNick_text = new wxStaticText(this, ID_SETNICK_TXT, wxT("  Nickname : "), wxDefaultPosition, wxDefaultSize);
	m_SetNick = new wxTextCtrl(this, ID_SETNICK, wxT("Mingebag(r)"), wxDefaultPosition, wxDefaultSize, 0);
	m_NetMode = new wxChoice(this, ID_NETMODE, wxDefaultPosition, wxDefaultSize, netmodes_str, 0, wxDefaultValidator);
	m_NetMode->SetSelection(0);
	
	// CONNECTION TAB
	m_ConAddr_text = new wxStaticText(m_Tab_Connect, ID_CONNADDR_TXT, wxT(" IP Address :"), wxDefaultPosition, wxDefaultSize);
	m_ConAddr = new wxTextCtrl(m_Tab_Connect, ID_CONNADDR, wxT("127.0.0.1:12345"), wxDefaultPosition, wxSize(250,20), 0);
	m_UseRandomPort = new wxCheckBox(m_Tab_Connect, ID_USE_RANDOMPORT, wxT("Use random client port for connection"));
	m_UseRandomPort->SetValue(true); m_UseRandomPort->Enable(false);
	m_JoinGame = new wxButton(m_Tab_Connect, ID_BUTTON_JOIN, wxT("Connect"), wxDefaultPosition, wxDefaultSize);

	// Sizers CONNECT
	wxBoxSizer* sConnectTop = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* sConnectSizer = new wxBoxSizer(wxVERTICAL);

	sConnectTop->Add(m_ConAddr_text, 0, wxALL|wxALIGN_CENTER, 5);
	sConnectTop->Add(m_ConAddr, 3, wxALL|wxEXPAND, 5);
	sConnectTop->Add(m_JoinGame, 0, wxALL|wxALIGN_RIGHT, 5);
	sConnectSizer->Add(sConnectTop, 0, wxALL|wxEXPAND, 5);
	sConnectSizer->Add(m_UseRandomPort, 0, wxALL|wxALIGN_CENTER, 5);

	m_Tab_Connect->SetSizer(sConnectSizer);

	// HOSTING TAB
	m_SetPort_text = new wxStaticText(m_Tab_Host, ID_SETPORT_TXT, wxT(" Use Port : "), wxDefaultPosition, wxDefaultSize);
	m_SetPort = new wxTextCtrl(m_Tab_Host, ID_SETPORT, wxT("12345"), wxDefaultPosition, wxDefaultSize, 0);
	m_GameList_text = new wxStaticText(m_Tab_Host, ID_GAMELIST_TXT, wxT("Warning: Use a forwarded port ! Select Game and press Host :"), wxDefaultPosition, wxDefaultSize);
	m_GameList = new wxListBox(m_Tab_Host, ID_GAMELIST, wxDefaultPosition, wxDefaultSize,
				m_GameList_str, wxLB_SINGLE | wxLB_SORT | wxLB_NEEDED_SB);
	m_HostGame = new wxButton(m_Tab_Host, ID_BUTTON_HOST, wxT("Host"), wxDefaultPosition, wxDefaultSize);

	// Sizers HOST
	wxBoxSizer *sHostBox = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer *sHostTop = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer *sHostMid = new wxBoxSizer(wxHORIZONTAL);

	sHostTop->Add(m_SetPort_text, 0, wxALL|wxALIGN_CENTER, 5);
	sHostTop->Add(m_SetPort, 1, wxALL|wxEXPAND, 5);
	sHostMid->Add(m_GameList, 1, wxALL|wxEXPAND, 5);

	sHostBox->Add(m_GameList_text, 0, wxALL|wxALIGN_CENTER, 5);
	sHostBox->Add(sHostTop, 0, wxALL|wxEXPAND, 1);
	sHostBox->Add(sHostMid, 1, wxALL|wxEXPAND, 1);
	sHostBox->Add(m_HostGame, 0, wxALL|wxALIGN_RIGHT, 10);

	m_Tab_Host->SetSizer(sHostBox);
	sHostBox->Layout();

	// main sizers
	wxBoxSizer* sMain = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* sMain_top = new wxBoxSizer(wxHORIZONTAL);

	sMain_top->Add(m_SetNick_text, 0, wxALL|wxALIGN_CENTER, 3);
	sMain_top->Add(m_SetNick, 0, wxALL, 3);
	sMain_top->AddStretchSpacer(1);
	sMain_top->Add(m_NetMode, 0, wxALL|wxALIGN_RIGHT, 3);

	sMain->Add(sMain_top, 0, wxALL|wxEXPAND, 5);
	sMain->Add(m_Notebook, 1, wxALL|wxEXPAND, 5);

	SetSizerAndFit(sMain);
	Center(); Layout(); Show();
}

void NetPlay::DrawNetWindow()
{
	// Remove everything from the precedent GUI :D
	DestroyChildren();

	SetTitle(_("Connection Window"));

	wxBoxSizer* sMain = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* sTop = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* sBottom = new wxBoxSizer(wxHORIZONTAL);
	
	m_Game_str = new wxButton(this, wxID_ANY, wxT(" Game : "), wxDefaultPosition, wxSize(400, 25), wxBU_LEFT);
	m_Game_str->Disable();

	m_Logging  = new wxTextCtrl(this, ID_LOGGING_TXT, wxEmptyString,
			wxDefaultPosition, wxSize(400, 250),
			wxTE_RICH2 | wxTE_MULTILINE | wxTE_READONLY);

	// Too bad this doesn't work...
	//m_Logging->SetDefaultStyle(wxTextAttr(*wxRED));

	m_Chat = new wxTextCtrl(this, ID_CHAT, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
	m_Chat_ok = new wxButton(this, ID_BUTTON_CHAT, wxT("Send"));;
	
	m_Ready = new wxCheckBox(this, ID_READY, wxT("Click here when ready"), wxDefaultPosition, wxDefaultSize, 0);
	m_RecordGame = new wxCheckBox(this, ID_RECORD, wxT("Record Game Input"), wxDefaultPosition, wxDefaultSize, 0);
	// TODO: Fix the recording ?
	m_RecordGame->Disable();

	m_ConInfo_text = new wxStaticText(this, ID_CONNINFO_TXT, wxT("  Fps : 0 | Ping : 00 ms"));
	m_GetPing = new wxButton(this, ID_BUTTON_GETPING, wxT("Ping"), wxDefaultPosition, wxDefaultSize);
	m_Disconnect = new wxButton(this, ID_BUTTON_QUIT, wxT("Disconnect"), wxDefaultPosition, wxDefaultSize);
	
	wxBoxSizer* sChat = new wxBoxSizer(wxHORIZONTAL);

	sTop->Add(m_Game_str, 0, wxALL|wxEXPAND, 1);
	sTop->Add(m_Logging, 1, wxALL|wxEXPAND, 5);
	sChat->Add(m_Chat, 1, wxALL|wxEXPAND, 2);
	sChat->Add(m_Chat_ok, 0, wxALL, 2);
	sTop->Add(sChat, 0, wxALL|wxEXPAND, 2);

	wxBoxSizer* sBottom0 = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* sBottom1 = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* sBottomM = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* sBottom2 = new wxBoxSizer(wxVERTICAL);

	sBottom0->Add(m_Ready, 0, wxALL, 5);
	sBottom0->Add(m_RecordGame, 0, wxALL, 5);
	sBottomM->Add(m_ConInfo_text, 0, wxALL, 5);
	sBottom1->Add(m_Disconnect, 0, wxALL|wxALIGN_LEFT, 5);
	sBottom1->AddStretchSpacer(1);
	sBottom1->Add(m_GetPing, 0, wxALL|wxALIGN_RIGHT, 5);

	sBottom2->Add(sBottom0, 0, wxALL, 0);
	sBottom2->Add(sBottomM, 0, wxALL | wxALIGN_LEFT, 0);
	sBottom2->Add(sBottom1, 0, wxALL | wxEXPAND, 5);

	sBottom->Add(sBottom2, 2, wxALL | wxEXPAND | wxALIGN_CENTER, 0);

	if (m_isHosting)
	{
		m_wtfismyip = new wxButton(this, ID_BUTTON_GETIP, wxT("What is my IP"));
		m_ChangeGame = new wxButton(this, ID_CHANGEGAME, wxT("Change Game"));

		wxStaticBoxSizer* sBottom3 = new wxStaticBoxSizer(wxVERTICAL, this, _("Host"));

		sBottom3->Add(m_ChangeGame, 0, wxALL | wxEXPAND, 5);
		sBottom3->Add(m_wtfismyip, 0, wxALL | wxEXPAND, 5);
		sBottom->Add(sBottom3, 1, wxALL | wxEXPAND, 0);

		UpdateNetWindow(false);
	}

	sMain->Add(sTop, 1, wxALL | wxEXPAND, 5);
	sMain->Add(sBottom, 0, wxALL | wxEXPAND | wxALIGN_CENTER, 2);

	SetSizerAndFit(sMain);
	Center(); Layout();
	Show();
}

void NetPlay::UpdateNetWindow(bool update_infos, wxString infos/*int fps, float ping, int frame_delay*/)
{
	std::vector<std::string> str_arr;

	if (update_infos)
	{
		// String of the type : FPSxPINGxFRAME_DELAY
		SplitString(std::string(infos.mb_str()), "x", str_arr);

		m_ConInfo_text->SetLabel(
			wxString::Format( "  Fps : %s | Ping : %s | Frame Delay : %s",
			str_arr[0].c_str(), str_arr[1].c_str(), str_arr[2].c_str() ));
	}
	else
	{
		m_critical.Enter();
			m_Game_str->SetLabel(wxString::Format(" Game : %s", m_selectedGame.c_str()));
		m_critical.Leave();
	}
}

void NetPlay::OnGUIEvent(wxCommandEvent& event)
{
	unsigned char value;;
	switch (event.GetId())
	{
	case ID_READY:
		{
			std::string buffer;
			value = 0x40;

			if (!m_ready)
				buffer = ">> "+m_nick+" is now ready !\n";
			else
				buffer = ">> "+m_nick+" is now Unready !\n";

			m_ready = !m_ready;

			if (m_isHosting == 1)
			{
				if (m_numClients > 0)
				{
					int buffer_size = (int)buffer.size();
					for (int i=0; i < m_numClients ; i++)
					{
						m_sock_server->Write(i, (const char*)&value, 1);

						m_sock_server->Write(i, (const char*)&buffer_size, 4);
						m_sock_server->Write(i, buffer.c_str(), buffer_size + 1);
					}
				}
					
				m_Logging->AppendText(wxString::FromAscii(buffer.c_str()));

				if (m_ready && m_clients_ready)
					LoadGame();
			}
			else {
				if (m_numClients > 0)
					m_sock_client->Write((const char*)&value, 1);	// 0x40 -> Ready
			}
			break;		
		}
	case ID_BUTTON_GETIP:
		{
			if (m_numClients == 0)	// Get IP Address from the Internet
			{
				// simple IP address caching
				if (m_hostaddr.at(0) != 'a')
				{
					m_Logging->AppendText(wxString::FromAscii(m_hostaddr.c_str()));
					return;
				}
			
				char buffer[8];
				sprintf(buffer, "%d", m_port);
				
				m_hostaddr = "> Your IP is : " + sf::IPAddress::GetPublicAddress().ToString() +
					':' + std::string(buffer) + '\n';
				m_Logging->AppendText(wxString::FromAscii(m_hostaddr.c_str()));
			}
			else	// Ask client to send server IP
			{
				value = 0x20;
				m_sock_server->Write(0, (const char*)&value, 1);
			}
			break;
		}
	case ID_BUTTON_GETPING:
		{
			if (m_numClients == 0)
				return;

			// TODO : This is not designed for > 2 players
			long ping[3] = {0};
			float fping;

			if (m_isHosting == 1) {
				m_sock_server->Write(0, 0, 0, ping);
				fping = (ping[0]+ping[1]+ping[2])/(float)m_numClients;
			}
			else {
				m_sock_client->Write(0, 0, ping);
				fping = ping[0];
			}

			UpdateNetWindow( true, wxString::Format(wxT("000x%fx%d"), fping, (int)ceil(fping/(1000.0/60.0))) );
			break;
		}
	case ID_BUTTON_CHAT:
	case ID_CHAT:
		{
			value = 0x30;
			wxString chat_str = wxString::Format(wxT("> %s : %s\n"), m_nick.c_str(), m_Chat->GetValue().c_str());
			int chat_size = (int)chat_str.size(); 

			// If there's no distant connection, we write but we don't send
			if (m_numClients == 0) {
				m_Logging->AppendText(chat_str);
				return;
			}
			// Max size that we handle is 1024, there's no need for more
			if ((chat_str.size()+1) * sizeof(char) > 1024) {
				m_Logging->AppendText(wxT("ERROR : Packet too large !\n"));
				return;
			}

			// Send to all
			if (m_isHosting == 1)
			{
				for (int i=0; i < m_numClients ; i++) {
					// Send Chat command
					m_sock_server->Write(i, (const char*)&value, 1); // 0x30 -> Chat

					// Send Chat string
					m_sock_server->Write(i, (const char*)&chat_size, 4);
					m_sock_server->Write(i, chat_str.c_str(), chat_size + 1);
				}
			}
			else {
				m_sock_client->Write((const char*)&value, 1);
				m_sock_client->Write((const char*)&chat_size, 4);
				m_sock_client->Write(chat_str.c_str(), chat_size + 1);
			}

			m_Chat->Clear();

			// We should maybe wait for the server to send it... 
			// but we write it anyway :p
			m_Logging->AppendText(chat_str);

			break;
		}
	case ID_RECORD:
		// TODO :
		// Record raw pad data
		break;
	case ID_CHANGEGAME:
		{
			GameListPopup PopUp(this, m_GameList_str);
			PopUp.ShowModal();
			break;
		}
	}
}

/////////////////////////
// GameList popup window 

BEGIN_EVENT_TABLE(GameListPopup, wxDialog)
	EVT_BUTTON(wxID_OK,      GameListPopup::OnButtons)
	EVT_BUTTON(wxID_CANCEL,  GameListPopup::OnButtons)
END_EVENT_TABLE()

GameListPopup::GameListPopup(NetPlay *parent, wxArrayString GameNames) :
	wxDialog(parent, wxID_ANY, _T("Choose a Game :"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE)
{
	m_netParent = parent;
	m_GameList_str = GameNames;
	m_GameList = new wxListBox(this, ID_GAMELIST, wxDefaultPosition, wxSize(300, 250),
				GameNames, wxLB_SINGLE | wxLB_SORT | wxLB_NEEDED_SB);
	m_Cancel = new wxButton(this, wxID_CANCEL, wxT("Cancel"), wxDefaultPosition, wxDefaultSize);
	m_Accept = new wxButton(this, wxID_OK, wxT("Apply"), wxDefaultPosition, wxDefaultSize);

	wxBoxSizer* sButtons = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* sMain = new wxBoxSizer(wxVERTICAL);

	sButtons->Add(m_Cancel, 0, wxALL, 0);
	sButtons->AddStretchSpacer(1);
	sButtons->Add(m_Accept, 0, wxALL | wxALIGN_RIGHT, 0);

	sMain->Add(m_GameList, 0, wxALL | wxEXPAND, 2);
	sMain->Add(sButtons, 0, wxALL | wxEXPAND, 5);

	SetSizerAndFit(sMain);
	Center(); Layout(); Show();
}

void GameListPopup::OnButtons(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case wxID_OK:
		m_netParent->ChangeSelectedGame(std::string(m_GameList_str[m_GameList->GetSelection()].mb_str()));
		Destroy();
		break;
	case wxID_CANCEL:
		Destroy();
		break;
	}
}

