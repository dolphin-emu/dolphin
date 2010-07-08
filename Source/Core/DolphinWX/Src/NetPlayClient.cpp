#include "NetPlay.h"
#include "NetWindow.h"

// called from ---GUI--- thread
NetPlayClient::~NetPlayClient()
{
	if (is_connected)
	{
		m_do_loop = false;
		m_thread->WaitForDeath();
		delete m_thread;
	}	
}

// called from ---GUI--- thread
NetPlayClient::NetPlayClient(const std::string& address, const u16 port, const std::string& name, NetPlayDiag* const npd)
{
	m_dialog = npd;
	is_connected = false;

	// why is false successful? documentation says true is
	if (0 == m_socket.Connect(port, address, 5))
	{
		// send connect message
		sf::Packet spac;
		spac << NETPLAY_VERSION;
		spac << netplay_dolphin_ver;
		spac << name;
		m_socket.Send(spac);

		sf::Packet rpac;
		// TODO: make this not hang
		m_socket.Receive(rpac);
		MessageId error;
		rpac >> error;

		// got error message
		if (error)
		{
			switch (error)
			{
			case CON_ERR_SERVER_FULL :
				PanicAlert("The server is full!");
				break;
			case CON_ERR_VERSION_MISMATCH :
				PanicAlert("The server and client's NetPlay versions are incompatible!");
				break;
			case CON_ERR_GAME_RUNNING :
				PanicAlert("The server responded: the game is currently running!");
				break;
			default :
				PanicAlert("The server sent an unknown error message!");
				break;
			}
			m_socket.Close();
		}
		else
		{
			rpac >> m_pid;

			Player player;
			player.name = name;
			player.pid = m_pid;
			player.revision = netplay_dolphin_ver;

			// add self to player list
			m_players[m_pid] = player;
			m_local_player = &m_players[m_pid];

			UpdateGUI();

			//PanicAlert("Connection successful: assigned player id: %d", m_pid);
			is_connected = true;

			m_selector.Add(m_socket);
			m_thread = new Common::Thread(NetPlayThreadFunc, this);
		}
	}
	else
		PanicAlert("Failed to Connect!");

}

// called from ---NETPLAY--- thread
unsigned int NetPlayClient::OnData(sf::Packet& packet)
{
	MessageId mid;
	packet >> mid;

	switch (mid)
	{
	case NP_MSG_PLAYER_JOIN :
		{
			Player player;
			packet >> player.pid;
			packet >> player.name;
			packet >> player.revision;

			m_crit.players.Enter();	// lock players
			m_players[player.pid] = player;
			m_crit.players.Leave();

			UpdateGUI();
		}
		break;

	case NP_MSG_PLAYER_LEAVE :
		{
			PlayerId pid;
			packet >> pid;

			m_crit.players.Enter();	// lock players
			m_players.erase(m_players.find(pid));
			m_crit.players.Leave();

			UpdateGUI();
		}
		break;

	case NP_MSG_CHAT_MESSAGE :
		{
			PlayerId pid;
			packet >> pid;
			std::string msg;
			packet >> msg;

			// don't need lock to read in this thread
			const Player& player = m_players[pid];

			// add to gui
			std::ostringstream ss;
			ss << player.name << '[' << (char)(pid+'0') << "]: " << msg;

			AppendChatGUI(ss.str());
		}
		break;

	case NP_MSG_PAD_MAPPING :
		{
			PlayerId pid;
			packet >> pid;

			m_crit.players.Enter();	// lock players
			Player& player = m_players[pid];

			for (unsigned int i=0; i<4; ++i)
				packet >> player.pad_map[i];
			m_crit.players.Leave();

			UpdateGUI();
		}
		break;

	case NP_MSG_PAD_DATA :
		{
			PadMapping map = 0;
			NetPad np;
			packet >> map >> np.nHi >> np.nLo;

			// trusting server for good map value (>=0 && <4)
			// add to pad buffer
			m_crit.buffer.Enter();	// lock buffer
			m_pad_buffer[(unsigned)map].push(np);
			m_crit.buffer.Leave();
		}
		break;

	case NP_MSG_PAD_BUFFER :
		{
			u32 size = 0;
			packet >> size;

			m_crit.buffer.Enter();	// lock buffer
			m_target_buffer_size = size;
			m_crit.buffer.Leave();
		}
		break;

	case NP_MSG_CHANGE_GAME :
		{
			// lock here?
			m_crit.game.Enter();	// lock game state
			packet >> m_selected_game;
			m_crit.game.Leave();

			// update gui
			wxCommandEvent evt(wxEVT_THREAD, NP_GUI_EVT_CHANGE_GAME);
			// TODO: using a wxString in AddPendingEvent from another thread is unsafe i guess?
			evt.SetString(wxString(m_selected_game.c_str(), *wxConvCurrent));
			m_dialog->GetEventHandler()->AddPendingEvent(evt);
		}
		break;

	case NP_MSG_START_GAME :
		{
			m_crit.buffer.Enter();	// lock buffer
			packet >> m_on_game;
			m_crit.buffer.Leave();

			wxCommandEvent evt(wxEVT_THREAD, NP_GUI_EVT_START_GAME);
			m_dialog->GetEventHandler()->AddPendingEvent(evt);
		}
		break;

	case NP_MSG_STOP_GAME :
		{
			wxCommandEvent evt(wxEVT_THREAD, NP_GUI_EVT_STOP_GAME);
			m_dialog->GetEventHandler()->AddPendingEvent(evt);
		}
		break;

	case NP_MSG_DISABLE_GAME :
		{
			PanicAlert("Other client disconnected while game is running!! NetPlay is disabled. You manually stop the game.");
			CritLocker game_lock(m_crit.game);	// lock game state
			m_is_running = false;
			NetPlay_Disable();
		}
		break;

	case NP_MSG_PING :
		{
			u32 ping_key = 0;
			packet >> ping_key;

			sf::Packet spac;
			spac << (MessageId)NP_MSG_PONG;
			spac << ping_key;

			CritLocker send_lock(m_crit.send);
			m_socket.Send(spac);
		}
		break;

	default :
		PanicAlert("Unknown message received with id : %d", mid);
		break;

	}

	return 0;
}

// called from ---NETPLAY--- thread
void NetPlayClient::Entry()
{
	while (m_do_loop)
	{
		if (m_selector.Wait(0.01f))
		{
			sf::Packet rpac;
			switch (m_socket.Receive(rpac))
			{
			case sf::Socket::Done :
				OnData(rpac);
				break;

			//case sf::Socket::Disconnected :
			default :
				m_is_running = false;
				NetPlay_Disable();
				AppendChatGUI("< LOST CONNECTION TO SERVER >");
				PanicAlert("Lost connection to server!");
				m_do_loop = false;
				break;
			}
		}
	}

	m_socket.Close();

	return;
}

// called from ---GUI--- thread
void NetPlayClient::GetPlayerList(std::string& list, std::vector<int>& pid_list)
{
	CritLocker player_lock(m_crit.players);		// lock players

	std::ostringstream ss;

	std::map<PlayerId, Player>::const_iterator
		i = m_players.begin(),
		e = m_players.end();
	for ( ; i!=e; ++i)
	{
		ss << i->second.ToString() << '\n';
		pid_list.push_back(i->second.pid);
	}

	list = ss.str();
}


// called from ---GUI--- thread
void NetPlayClient::SendChatMessage(const std::string& msg)
{
	sf::Packet spac;
	spac << (MessageId)NP_MSG_CHAT_MESSAGE;
	spac << msg;

	CritLocker	player_lock(m_crit.players);	// lock players
	CritLocker	send_lock(m_crit.send);	// lock send
	m_socket.Send(spac);
}

// called from ---CPU--- thread
void NetPlayClient::SendPadState(const PadMapping local_nb, const NetPad& np)
{
	// send to server
	sf::Packet spac;
	spac << (MessageId)NP_MSG_PAD_DATA;
	spac << local_nb;	// local pad num
	spac << np.nHi << np.nLo;

	CritLocker	send_lock(m_crit.send);	// lock send
	m_socket.Send(spac);
}

// called from ---GUI--- thread
bool NetPlayClient::StartGame(const std::string &path)
{
	m_crit.buffer.Enter();	// lock buffer

	if (false == NetPlay::StartGame(path))
		return false;

	// tell server i started the game
	sf::Packet spac;
	spac << (MessageId)NP_MSG_START_GAME;
	spac << m_on_game;

	m_crit.buffer.Leave();

	CritLocker	send_lock(m_crit.send);	// lock send
	m_socket.Send(spac);

	return true;
}

// called from ---GUI--- thread
bool NetPlayClient::ChangeGame(const std::string &game)
{
	// warning removal
	game.size();

	return true;
}
