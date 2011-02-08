#include "NetPlay.h"

// called from ---GUI--- thread
NetPlayServer::~NetPlayServer()
{
	if (is_connected)
	{
		m_do_loop = false;
		m_thread.join();
	}	
}

// called from ---GUI--- thread
NetPlayServer::NetPlayServer(const u16 port, const std::string& name, NetPlayUI* dialog, const std::string& game) : NetPlay(dialog)
{
	m_selected_game = game;

	m_update_pings = true;

	if (m_socket.Listen(port))
	{
		Client player;
		player.pid = 0;
		player.revision = netplay_dolphin_ver;
		player.socket = m_socket;
		player.name = name;

		// map local pad 1 to game pad 1
		player.pad_map[0] = 0;

		// add self to player list
		m_players[m_socket] = player;
		m_local_player = &m_players[m_socket];
		//PanicAlertT("Listening");

		m_dialog->Update();

		is_connected = true;

		m_selector.Add(m_socket);
		m_thread = std::thread(std::mem_fun(&NetPlayServer::ThreadFunc), this);
	}
	else
		is_connected = false;
}

// called from ---NETPLAY--- thread
void NetPlayServer::ThreadFunc()
{
	while (m_do_loop)
	{
		// update pings every so many seconds
		if ((m_ping_timer.GetTimeElapsed() > (10 * 1000)) || m_update_pings)
		{
			//PanicAlertT("sending pings");

			m_ping_key = Common::Timer::GetTimeMs();

			sf::Packet spac;
			spac << (MessageId)NP_MSG_PING;
			spac << m_ping_key;
			
			//CritLocker player_lock(m_crit.players);
			CritLocker send_lock(m_crit.send);
			m_ping_timer.Start();
			SendToClients(spac);

			m_update_pings = false;
		}

		// check which sockets need attention
		const unsigned int num = m_selector.Wait(0.01f);
		for (unsigned int i=0; i<num; ++i)
		{
			sf::SocketTCP ready_socket = m_selector.GetSocketReady(i);

			// listening socket
			if (ready_socket == m_socket)
			{
				sf::SocketTCP accept_socket;
				m_socket.Accept(accept_socket);

				m_crit.game.Enter();	// lock game state
				const unsigned int error = OnConnect(accept_socket);
				m_crit.game.Leave();

				if (error)
				{
					sf::Packet spac;
					spac << (MessageId)error;
					// don't need to lock, this client isn't in the client map
					accept_socket.Send(spac);

					// TODO: not sure if client gets the message if i close right away
					accept_socket.Close();
				}
			}
			// client socket
			else
			{
				sf::Packet rpac;
				switch (ready_socket.Receive(rpac))
				{
				case sf::Socket::Done :
					// if a bad packet is recieved, disconnect the client
					if (0 == OnData(rpac, ready_socket))
						break;

				//case sf::Socket::Disconnected :
				default :
					m_crit.game.Enter();	// lock game state
					OnDisconnect(ready_socket);
					m_crit.game.Leave();
					break;
				}
			}
		}
	}

	// close listening socket and client sockets
	{
	std::map<sf::SocketTCP, Client>::reverse_iterator
		i = m_players.rbegin(),
		e = m_players.rend();
	for ( ; i!=e; ++i)
		i->second.socket.Close();
	}

	return;
}

// called from ---NETPLAY--- thread
unsigned int NetPlayServer::OnConnect(sf::SocketTCP& socket)
{
	sf::Packet rpac;
	// TODO: make this not hang / check if good packet
	socket.Receive(rpac);

	std::string npver;
	rpac >> npver;
	// dolphin netplay version
	if (npver != NETPLAY_VERSION)
		return CON_ERR_VERSION_MISMATCH;

	// game is currently running
	if (m_is_running)
		return CON_ERR_GAME_RUNNING;

	// too many players
	if (m_players.size() >= 255)
		return CON_ERR_SERVER_FULL;

	// cause pings to be updated
	m_update_pings = true;

	Client player;
	player.socket = socket;
	rpac >> player.revision;
	rpac >> player.name;

	// give new client first available id
	player.pid = 0;
	std::map<sf::SocketTCP, Client>::const_iterator
		i,
		e = m_players.end();
	for (PlayerId p = 1; 0 == player.pid; ++p)
	{
		for (i = m_players.begin(); ; ++i)
		{
			if (e == i)
			{
				player.pid = p;
				break;
			}
			if (p == i->second.pid)
				break;
		}
	}

	// TODO: this is crappy
	// try to automatically assign new user a pad
	{
	bool is_mapped[4] = {false,false,false,false};

	for ( unsigned int m = 0; m<4; ++m)
	{
		for (i = m_players.begin(); i!=e; ++i)
		{
			if (i->second.pad_map[m] >= 0)
				is_mapped[(unsigned)i->second.pad_map[m]] = true;
		}
	}

	for ( unsigned int m = 0; m<4; ++m)
		if (false == is_mapped[m])
		{
			player.pad_map[0] = m;
			break;
		}

	}

	// ENTER
	m_crit.send.Enter();

	// send join message to already connected clients
	sf::Packet spac;
	spac << (MessageId)NP_MSG_PLAYER_JOIN;
	spac << player.pid << player.name << player.revision;
	SendToClients(spac);

	// send new client success message with their id
	spac.Clear();
	spac << (MessageId)0;
	spac << player.pid;
	socket.Send(spac);

	// send new client the selected game
	spac.Clear();
	spac << (MessageId)NP_MSG_CHANGE_GAME;
	spac << m_selected_game;
	socket.Send(spac);

	// sync values with new client
	for (i = m_players.begin(); i!=e; ++i)
	{
		spac.Clear();
		spac << (MessageId)NP_MSG_PLAYER_JOIN;
		spac << i->second.pid << i->second.name << i->second.revision;
		socket.Send(spac);
	}

	// LEAVE
	m_crit.send.Leave();

	// add client to the player list
	m_crit.players.Enter();	// lock players
	m_players[socket] = player;
	m_crit.send.Enter();	// lock send
	UpdatePadMapping();	// sync pad mappings with everyone
	m_crit.send.Leave();
	m_crit.players.Leave();

	// add client to selector/ used for receiving
	m_selector.Add(socket);

	m_dialog->Update();

	return 0;
}

// called from ---NETPLAY--- thread
unsigned int NetPlayServer::OnDisconnect(sf::SocketTCP& socket)
{
	if (m_is_running)
	{
		PanicAlertT("Client disconnect while game is running!! NetPlay is disabled. You must manually stop the game.");
		CritLocker game_lock(m_crit.game);	// lock game state
		m_is_running = false;
		NetPlay_Disable();

		sf::Packet spac;
		spac << (MessageId)NP_MSG_DISABLE_GAME;
		// this thread doesnt need players lock
		CritLocker	send_lock(m_crit.send);	// lock send
		SendToClients(spac);
	}

	sf::Packet spac;
	spac << (MessageId)NP_MSG_PLAYER_LEAVE;
	spac << m_players[socket].pid;

	m_selector.Remove(socket);
	
	CritLocker	player_lock(m_crit.players);	// lock players
	m_players.erase(m_players.find(socket));

	// alert other players of disconnect
	CritLocker	send_lock(m_crit.send);	// lock send
	SendToClients(spac);

	m_dialog->Update();

	return 0;
}

// called from ---GUI--- thread
bool NetPlayServer::GetPadMapping(const int pid, int map[])
{
	CritLocker	player_lock(m_crit.players);	// lock players
	std::map<sf::SocketTCP, Client>::const_iterator
		i = m_players.begin(),
		e = m_players.end();
	for (; i!=e; ++i)
		if (pid == i->second.pid)
			break;

	// player not found
	if (i == e)
		return false;

	// get pad mapping
	for (unsigned int m = 0; m<4; ++m)
		map[m] = i->second.pad_map[m];

	return true;
}

// called from ---GUI--- thread
bool NetPlayServer::SetPadMapping(const int pid, const int map[])
{
	CritLocker	game_lock(m_crit.game);	// lock game
	if (m_is_running)
		return false;

	CritLocker	player_lock(m_crit.players);	// lock players
	std::map<sf::SocketTCP, Client>::iterator
		i = m_players.begin(),
		e = m_players.end();
	for (; i!=e; ++i)
		if (pid == i->second.pid)
			break;

	// player not found
	if (i == e)
		return false;

	Client& player = i->second;

	// set pad mapping
	for (unsigned int m = 0; m<4; ++m)
	{
		player.pad_map[m] = (PadMapping)map[m];

		// remove duplicate mappings
		for (i = m_players.begin(); i!=e; ++i)
			for (unsigned int p = 0; p<4; ++p)
				if (p != m || i->second.pid != pid)
					if (player.pad_map[m] == i->second.pad_map[p])
						i->second.pad_map[p] = -1;
	}

	CritLocker	send_lock(m_crit.send);	// lock send
	UpdatePadMapping();	// sync pad mappings with everyone

	m_dialog->Update();

	return true;
}

// called from ---NETPLAY--- thread
void NetPlayServer::UpdatePadMapping()
{
	std::map<sf::SocketTCP, Client>::const_iterator
		i = m_players.begin(),
		e = m_players.end();
	for (; i!=e; ++i)
	{
		sf::Packet spac;
		spac << (MessageId)NP_MSG_PAD_MAPPING;
		spac << i->second.pid;
		for (unsigned int pm = 0; pm<4; ++pm)
			spac << i->second.pad_map[pm];
		SendToClients(spac);
	}

}

// called from ---GUI--- thread and ---NETPLAY--- thread
u64 NetPlayServer::CalculateMinimumBufferTime()
{
	CritLocker player_lock(m_crit.players);

	std::map<sf::SocketTCP, Client>::const_iterator
		i = m_players.begin(),
		e = m_players.end();
	std::priority_queue<unsigned int>	pings;
	for ( ;i!=e; ++i)
		pings.push(i->second.ping/2);

	unsigned int required_ms = pings.top();
	// if there is more than 1 client, buffersize must be >= (2 highest ping times combined)
	if (pings.size() > 1)
	{
		pings.pop();
		required_ms += pings.top();
	}

	return required_ms;
}

// called from ---GUI--- thread and ---NETPLAY--- thread
void NetPlayServer::AdjustPadBufferSize(unsigned int size)
{
	CritLocker game_lock(m_crit.game);	// lock game state

	m_target_buffer_size = size;

	// tell clients to change buffer size
	sf::Packet spac;
	spac << (MessageId)NP_MSG_PAD_BUFFER;
	spac << (u32)m_target_buffer_size;

	CritLocker	player_lock(m_crit.players);	// lock players
	CritLocker	send_lock(m_crit.send);	// lock send
	SendToClients(spac);
}

// called from ---NETPLAY--- thread
unsigned int NetPlayServer::OnData(sf::Packet& packet, sf::SocketTCP& socket)
{
	MessageId mid;
	packet >> mid;

	// don't need lock because this is the only thread that modifies the players
	// only need locks for writes to m_players in this thread
	Client& player = m_players[socket];

	switch (mid)
	{
	case NP_MSG_CHAT_MESSAGE :
		{
			std::string msg;
			packet >> msg;

			// send msg to other clients
			sf::Packet spac;
			spac << (MessageId)NP_MSG_CHAT_MESSAGE;
			spac << player.pid;
			spac << msg;

			m_crit.send.Enter();	// lock send
			SendToClients(spac, player.pid);
			m_crit.send.Leave();

			// add to gui
			std::ostringstream ss;
			ss << player.name << '[' << (char)(player.pid+'0') << "]: " << msg;

			m_dialog->AppendChat(ss.str());
		}
		break;

	case NP_MSG_PAD_DATA :
		{
			// if this is pad data from the last game still being received, ignore it
			if (player.current_game != m_current_game)
				break;

			PadMapping map = 0;
			NetPad np;
			packet >> map >> np.nHi >> np.nLo;

			// check if client's pad indeed maps in game
			if (map >= 0 && map < 4)
				map = player.pad_map[(unsigned)map];
			else
				map = -1;
			
			// if not, they are hacking, so disconnect them
			// this could happen right after a pad map change, but that isn't implimented yet
			if (map < 0)
				return 1;
				
			// add to pad buffer
			m_pad_buffer[(unsigned)map].Push(np);

			// relay to clients
			sf::Packet spac;
			spac << (MessageId)NP_MSG_PAD_DATA;
			spac << map;	// in game mapping
			spac << np.nHi << np.nLo;

			CritLocker	send_lock(m_crit.send);	// lock send
			SendToClients(spac, player.pid);
		}
		break;

	case NP_MSG_PONG :
		{
			const u32 ping = m_ping_timer.GetTimeElapsed();
			u32 ping_key = 0;
			packet >> ping_key;

			if (m_ping_key == ping_key)
			{
				//PanicAlertT("good pong");
				player.ping = ping;
			}
			m_dialog->Update();
		}
		break;

	case NP_MSG_START_GAME :
		{
			packet >> player.current_game;
		}
		break;

	default :
		PanicAlertT("Unknown message with id:%d received from player:%d Kicking player!", mid, player.pid);
		// unknown message, kick the client
		return 1;
		break;
	}

	return 0;
}

// called from ---GUI--- thread
void NetPlayServer::GetPlayerList(std::string& list, std::vector<int>& pid_list)
{
	CritLocker player_lock(m_crit.players);	// lock players

	std::ostringstream ss;

	std::map<sf::SocketTCP, Client>::const_iterator
		i = m_players.begin(),
		e = m_players.end();
	for ( ; i!=e; ++i)
	{
		ss << i->second.ToString() << " " << i->second.ping <<  "ms\n";
		pid_list.push_back(i->second.pid);
	}

	list = ss.str();
}

// called from ---GUI--- thread / and ---NETPLAY--- thread
void NetPlayServer::SendChatMessage(const std::string& msg)
{
	sf::Packet spac;
	spac << (MessageId)NP_MSG_CHAT_MESSAGE;
	spac << (PlayerId)0;	// server id always 0
	spac << msg;

	CritLocker	player_lock(m_crit.players);	// lock players
	CritLocker	send_lock(m_crit.send);	// lock send
	SendToClients(spac);
}

// called from ---GUI--- thread
bool NetPlayServer::ChangeGame(const std::string &game)
{
	CritLocker game_lock(m_crit.game);	// lock game state

	m_selected_game = game;

	// send changed game to clients
	sf::Packet spac;
	spac << (MessageId)NP_MSG_CHANGE_GAME;
	spac << game;

	CritLocker	player_lock(m_crit.players);	// lock players
	CritLocker	send_lock(m_crit.send);	// lock send
	SendToClients(spac);

	return true;
}

// called from ---CPU--- thread
void NetPlayServer::SendPadState(const PadMapping local_nb, const NetPad& np)
{
	// send to server
	sf::Packet spac;
	spac << (MessageId)NP_MSG_PAD_DATA;
	spac << m_local_player->pad_map[local_nb];	// in-game pad num
	spac << np.nHi << np.nLo;

	CritLocker	send_lock(m_crit.send);	// lock send
	SendToClients(spac);
}

// called from ---GUI--- thread
bool NetPlayServer::StartGame(const std::string &path)
{
	CritLocker game_lock(m_crit.game);	// lock game state

	if (false == NetPlay::StartGame(path))
		return false;

	// TODO: i dont like this here
	m_current_game = Common::Timer::GetTimeMs();

	// no change, just update with clients
	AdjustPadBufferSize(m_target_buffer_size);

	// tell clients to start game
	sf::Packet spac;
	spac << (MessageId)NP_MSG_START_GAME;
	spac << m_current_game;

	CritLocker	player_lock(m_crit.players);	// lock players
	CritLocker	send_lock(m_crit.send);	// lock send
	SendToClients(spac);

	return true;
}


// called from ---GUI--- thread
bool NetPlayServer::StopGame()
{
	if (false == NetPlay::StopGame())
		return false;

	// tell clients to stop game
	sf::Packet spac;
	spac << (MessageId)NP_MSG_STOP_GAME;

	CritLocker	player_lock(m_crit.players);	// lock players
	CritLocker	send_lock(m_crit.send);	// lock send
	SendToClients(spac);

	return true;
}

// called from multiple threads
void NetPlayServer::SendToClients(sf::Packet& packet, const PlayerId skip_pid)
{
	std::map<sf::SocketTCP, Client>::iterator
		i = m_players.begin(),
		e = m_players.end();
	for ( ; i!=e; ++i)
		if (i->second.pid && (i->second.pid != skip_pid))
			i->second.socket.Send(packet);
}
