#include "NetPlay.h"

// called from ---GUI--- thread
NetPlayClient::~NetPlayClient()
{
	if (is_connected)
	{
		m_do_loop = false;
		m_thread.join();
	}	
}

// called from ---GUI--- thread
NetPlayClient::NetPlayClient(const std::string& address, const u16 port, NetPlayUI* dialog, const std::string& name) : NetPlay(dialog)
{
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
				PanicAlertT("The server is full!");
				break;
			case CON_ERR_VERSION_MISMATCH :
				PanicAlertT("The server and client's NetPlay versions are incompatible!");
				break;
			case CON_ERR_GAME_RUNNING :
				PanicAlertT("The server responded: the game is currently running!");
				break;
			default :
				PanicAlertT("The server sent an unknown error message!");
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

			m_dialog->Update();

			//PanicAlertT("Connection successful: assigned player id: %d", m_pid);
			is_connected = true;

			m_selector.Add(m_socket);
			m_thread = std::thread(std::mem_fun(&NetPlayClient::ThreadFunc), this);
		}
	}
	else
		PanicAlertT("Failed to Connect!");

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

			{
			std::lock_guard<std::recursive_mutex> lkp(m_crit.players);
			m_players[player.pid] = player;
			}

			m_dialog->Update();
		}
		break;

	case NP_MSG_PLAYER_LEAVE :
		{
			PlayerId pid;
			packet >> pid;

			{
			std::lock_guard<std::recursive_mutex> lkp(m_crit.players);
			m_players.erase(m_players.find(pid));
			}

			m_dialog->Update();
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

			m_dialog->AppendChat(ss.str());
		}
		break;

	case NP_MSG_PAD_MAPPING :
		{
			PlayerId pid;
			packet >> pid;

			{
			std::lock_guard<std::recursive_mutex> lkp(m_crit.players);
			Player& player = m_players[pid];

			for (unsigned int i=0; i<4; ++i)
				packet >> player.pad_map[i];
			}

			m_dialog->Update();
		}
		break;

	case NP_MSG_PAD_DATA :
		{
			PadMapping map = 0;
			NetPad np;
			packet >> map >> np.nHi >> np.nLo;

			// trusting server for good map value (>=0 && <4)
			// add to pad buffer
			m_pad_buffer[(unsigned)map].Push(np);
		}
		break;

	case NP_MSG_PAD_BUFFER :
		{
			u32 size = 0;
			packet >> size;

			m_target_buffer_size = size;
		}
		break;

	case NP_MSG_CHANGE_GAME :
		{
			{
			std::lock_guard<std::recursive_mutex> lkg(m_crit.game);
			packet >> m_selected_game;
			}

			// update gui
			m_dialog->OnMsgChangeGame(m_selected_game);
		}
		break;

	case NP_MSG_START_GAME :
		{
			{
			std::lock_guard<std::recursive_mutex> lkg(m_crit.game);
			packet >> m_current_game;
			}

			m_dialog->OnMsgStartGame();
		}
		break;

	case NP_MSG_STOP_GAME :
		{
			m_dialog->OnMsgStopGame();
		}
		break;

	case NP_MSG_DISABLE_GAME :
		{
			PanicAlertT("Other client disconnected while game is running!! NetPlay is disabled. You manually stop the game.");
			std::lock_guard<std::recursive_mutex> lkg(m_crit.game);
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

			std::lock_guard<std::recursive_mutex> lks(m_crit.send);
			m_socket.Send(spac);
		}
		break;

	default :
		PanicAlertT("Unknown message received with id : %d", mid);
		break;

	}

	return 0;
}

// called from ---NETPLAY--- thread
void NetPlayClient::ThreadFunc()
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
				m_dialog->AppendChat("< LOST CONNECTION TO SERVER >");
				PanicAlertT("Lost connection to server!");
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
	std::lock_guard<std::recursive_mutex> lkp(m_crit.players);

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

	std::lock_guard<std::recursive_mutex> lks(m_crit.send);
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

	std::lock_guard<std::recursive_mutex> lks(m_crit.send);
	m_socket.Send(spac);
}

// called from ---GUI--- thread
bool NetPlayClient::StartGame(const std::string &path)
{
	std::lock_guard<std::recursive_mutex> lkg(m_crit.game);

	if (false == NetPlay::StartGame(path))
		return false;

	// tell server i started the game
	sf::Packet spac;
	spac << (MessageId)NP_MSG_START_GAME;
	spac << m_current_game;

	std::lock_guard<std::recursive_mutex> lks(m_crit.send);
	m_socket.Send(spac);

	return true;
}

// called from ---GUI--- thread
bool NetPlayClient::ChangeGame(const std::string&)
{
	return true;
}
