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

#include "HW/SI_DeviceGCController.h"
#include "HW/EXI_DeviceIPL.h"

#include "NetPlay.h"
#include "NetWindow.h"

#include "Frame.h"

#include <sstream>

// this will be removed soon
#define TEMP_FIXED_BUFFER	20

enum
{
	CON_ERR_SERVER_FULL = 1,
	CON_ERR_GAME_RUNNING,
	CON_ERR_VERSION_MISMATCH	
};

NetPlay* NetClass_ptr = NULL;
extern CFrame* main_frame;

DEFINE_EVENT_TYPE(wxEVT_THREAD)

THREAD_RETURN NetPlayThreadFunc(void* arg)
{
	((NetPlay*)arg)->Entry();
	return 0;
}

NetPlay::~NetPlay()
{
	::NetClass_ptr = NULL;
}

NetPlayServer::~NetPlayServer()
{
	if (is_connected)
	{
		m_do_loop = false;
		m_thread->WaitForDeath();
		delete m_thread;
	}	
}

NetPlayClient::~NetPlayClient()
{
	if (is_connected)
	{
		m_do_loop = false;
		m_thread->WaitForDeath();
		delete m_thread;
	}	
}

NetPlayServer::Player::Player()
{
	memset(pad_map, -1, sizeof(pad_map));
}

NetPlayClient::Player::Player()
{
	memset(pad_map, -1, sizeof(pad_map));
}

NetPlayServer::NetPlayServer(const u16 port, const std::string& name, NetPlayDiag* const npd, const std::string& game)
{
	m_dialog = npd;
	m_selected_game = game;

	if (m_socket.Listen(port))
	{
		Player player;
		player.pid = 0;
		player.revision = NETPLAY_DOLPHIN_VER;
		player.socket = m_socket;
		player.name = name;

		// map local pad 1 to game pad 1
		player.pad_map[0] = 0;

		// add self to player list
		m_players[m_socket] = player;
		//PanicAlert("Listening");

		UpdateGUI();

		is_connected = true;

		m_selector.Add(m_socket);
		m_thread = new Common::Thread(NetPlayThreadFunc, this);
	}
	else
		is_connected = false;
}

NetPlayClient::NetPlayClient(const std::string& address, const u16 port, const std::string& name, NetPlayDiag* const npd)
{
	m_dialog = npd;
	is_connected = false;

	// why is false successful? documentation says true is
	if (0 == m_socket.Connect(port, address))
	{
		// send connect message
		sf::Packet spac;
		spac << NETPLAY_VERSION;
		spac << NETPLAY_DOLPHIN_VER;
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
			player.revision = NETPLAY_DOLPHIN_VER;

			// add self to player list
			m_players[m_pid] = player;

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

void NetPlayServer::Entry()
{
	while (m_do_loop)
	{
		const unsigned int num = m_selector.Wait(0.01f);

		for (unsigned int i=0; i<num; ++i)
		{
			sf::SocketTCP ready_socket = m_selector.GetSocketReady(i);

			// listening socket
			if (ready_socket == m_socket)
			{
				sf::SocketTCP accept_socket;
				m_socket.Accept(accept_socket);

				m_crit.other.Enter();
				unsigned int error = OnConnect(accept_socket);
				m_crit.other.Leave();

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

				case sf::Socket::Disconnected :
					m_crit.other.Enter();
					OnDisconnect(ready_socket);
					m_crit.other.Leave();
					break;
				}
			}
		}
	}

	// close listening socket and client sockets
	{
	std::map<sf::SocketTCP, Player>::reverse_iterator
		i = m_players.rbegin(),
		e = m_players.rend();
	for ( ; i!=e; ++i)
		i->second.socket.Close();
	}

	return;
}

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

	Player player;
	player.socket = socket;
	rpac >> player.revision;
	rpac >> player.name;

	// give new client first available id
	player.pid = 0;
	std::map<sf::SocketTCP, Player>::const_iterator
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
				is_mapped[i->second.pad_map[m]] = true;
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

	// send user their pad mapping
	spac.Clear();
	spac << (MessageId)NP_MSG_PAD_MAPPING;
	spac << player.pid;
	for (unsigned int pm = 0; pm<4; ++pm)
		spac << player.pad_map[pm];
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

		spac.Clear();
		spac << (MessageId)NP_MSG_PAD_MAPPING;
		spac << i->second.pid;
		for (unsigned int pm = 0; pm<4; ++pm)
			spac << i->second.pad_map[pm];
		socket.Send(spac);
	}

	// LEAVE
	m_crit.send.Leave();

	// add client to the player list
	m_crit.players.Enter();
	m_players[socket] = player;
	m_crit.players.Leave();

	// add client to selector/ used for receiving
	m_selector.Add(socket);

	UpdateGUI();

	return 0;
}

unsigned int NetPlayServer::OnDisconnect(sf::SocketTCP& socket)
{
	if (m_is_running)
		PanicAlert("Disconnect while game is running. Game will likely hang!");

	sf::Packet spac;
	spac << (MessageId)NP_MSG_PLAYER_LEAVE;
	spac << m_players[socket].pid;

	m_selector.Remove(socket);
	
	m_crit.players.Enter();
	m_players.erase(m_players.find(socket));
	m_crit.players.Leave();

	UpdateGUI();

	m_crit.send.Enter();
	SendToClients(spac);
	m_crit.send.Leave();

	return 0;
}

void NetPlay::UpdateGUI()
{
	if (m_dialog)
	{
		wxCommandEvent evt(wxEVT_THREAD, 1);
		m_dialog->AddPendingEvent(evt);
	}
}

void NetPlayServer::SendToClients(sf::Packet& packet, const PlayerId skip_pid)
{
	std::map<sf::SocketTCP, Player>::iterator
		i = m_players.begin(),
		e = m_players.end();
	for ( ; i!=e; ++i)
		if (i->second.pid && (i->second.pid != skip_pid))
			i->second.socket.Send(packet);
}

unsigned int NetPlayServer::OnData(sf::Packet& packet, sf::SocketTCP& socket)
{
	MessageId mid;
	packet >> mid;

	// don't need lock because this is the only thread that modifies the players
	// only need locks for writes to m_players in this thread
	Player& player = m_players[socket];

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

			m_crit.send.Enter();
			SendToClients(spac, player.pid);
			m_crit.send.Leave();

			// add to gui
			std::ostringstream ss;
			ss << player.name << '[' << (char)(player.pid+'0') << "]: " << msg;

			m_dialog->chat_msgs.Push(ss.str());

			UpdateGUI();
		}
		break;

	case NP_MSG_PAD_DATA :
		{
			PadMapping map;
			NetPad np;
			packet >> map >> np.nHi >> np.nLo;

			// check if client's pad indeed maps in game
			if (map >= 0 && map < 4)
				map = player.pad_map[map];
			else
				map = -1;
			
			// if not, they are hacking, so disconnect them
			// this could happen right after a pad map change, but that isn't implimented yet
			if (map < 0)
				return 1;
				
			// add to pad buffer
			m_crit.buffer.Enter();
			m_pad_buffer[map].push(np);
			m_crit.buffer.Leave();

			// relay to clients
			sf::Packet spac;
			spac << (MessageId)NP_MSG_PAD_DATA;
			spac << map;	// in game mapping
			spac << np.nHi << np.nLo;

			m_crit.send.Enter();
			SendToClients(spac, player.pid);
			m_crit.send.Leave();
		}
		break;

	default :
		PanicAlert("Unknown message with id:%d received from player:%d Kicking player!", mid, player.pid);
		// unknown message, kick the client
		return 1;
		break;
	}

	return 0;
}

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

			m_crit.players.Enter();
			m_players[player.pid] = player;
			m_crit.players.Leave();

			UpdateGUI();
		}
		break;

	case NP_MSG_PLAYER_LEAVE :
		{
			PlayerId pid;
			packet >> pid;

			m_crit.players.Enter();
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

			m_dialog->chat_msgs.Push(ss.str());
			UpdateGUI();
		}
		break;

	case NP_MSG_PAD_MAPPING :
		{
			PlayerId pid;
			packet >> pid;

			m_crit.players.Enter();
			Player& player = m_players[pid];

			for (unsigned int i=0; i<4; ++i)
				packet >> player.pad_map[i];
			m_crit.players.Leave();

			UpdateGUI();
		}
		break;

	case NP_MSG_PAD_DATA :
		{
			PadMapping map;
			NetPad np;
			packet >> map >> np.nHi >> np.nLo;

			// add to pad buffer
			m_crit.buffer.Enter();
			m_pad_buffer[map].push(np);
			m_crit.buffer.Leave();
		}
		break;

	case NP_MSG_CHANGE_GAME :
		{
			m_crit.other.Enter();
			packet >> m_selected_game;
			m_crit.other.Leave();

			// update gui
			wxCommandEvent evt(wxEVT_THREAD, 45);
			evt.SetString(wxString(m_selected_game.c_str(), *wxConvCurrent));
			m_dialog->AddPendingEvent(evt);
		}
		break;

	case NP_MSG_START_GAME :
		{
			// kinda silly
			wxCommandEvent evt(wxEVT_THREAD, 46);
			m_dialog->AddPendingEvent(evt);
		}
		break;
	
	default :
		PanicAlert("Unknown message received with id : %d", mid);
		break;

	}

	return 0;
}

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

			case sf::Socket::Disconnected :
				PanicAlert("Lost connection to server! If the game is running, it will likely hang!");
				m_do_loop = false;
				break;
			}
		}
	}

	m_socket.Close();

	return;
}

template <typename P>
static inline std::string PlayerToString(const P& p)
{
	std::ostringstream ss;
	ss << p.name << '[' << (char)(p.pid+'0') << "] : " << p.revision << " |";
	for (unsigned int i=0; i<4; ++i)
		ss << (p.pad_map[i]>=0 ? (char)(p.pad_map[i]+'1') : '-');
	ss << '|';
	return ss.str();
}

void NetPlayClient::GetPlayerList(std::string &list)
{
	m_crit.players.Enter();

	std::ostringstream ss;

	std::map<PlayerId, Player>::const_iterator
		i = m_players.begin(),
		e = m_players.end();
	for ( ; i!=e; ++i)
		ss << PlayerToString(i->second) << '\n';

	list = ss.str();

	m_crit.players.Leave();
}

void NetPlayServer::GetPlayerList(std::string &list)
{
	m_crit.players.Enter();

	std::ostringstream ss;

	std::map<sf::SocketTCP, Player>::const_iterator
		i = m_players.begin(),
		e = m_players.end();
	for ( ; i!=e; ++i)
		ss << PlayerToString(i->second) << '\n';

	list = ss.str();

	m_crit.players.Leave();
}

void NetPlayServer::SendChatMessage(const std::string& msg)
{
	sf::Packet spac;
	spac << (MessageId)NP_MSG_CHAT_MESSAGE;
	spac << (PlayerId)0;	// server id always 0
	spac << msg;

	m_crit.send.Enter();
	SendToClients(spac);
	m_crit.send.Leave();
}

void NetPlayClient::SendChatMessage(const std::string& msg)
{
	sf::Packet spac;
	spac << (MessageId)NP_MSG_CHAT_MESSAGE;
	spac << msg;

	m_crit.send.Enter();
	m_socket.Send(spac);
	m_crit.send.Leave();
}

NetPad::NetPad()
{
	//SPADStatus sp;
	//memset(&sp, 0, sizeof(sp));
	//memset(&sp.stickX, 0x80, 4);
	//*this = NetPad(&sp);

	// i'd rather do something like this
	nHi = 0x00808080;
	nLo = 0x80800000;
}

NetPad::NetPad(const SPADStatus* const pad_status)
{
	nHi = (u32)((u8)pad_status->stickY);
	nHi |= (u32)((u8)pad_status->stickX << 8);
	nHi |= (u32)((u16)pad_status->button << 16);
	nHi |= 0x00800000;
	nLo = (u8)pad_status->triggerRight;
	nLo |= (u32)((u8)pad_status->triggerLeft << 8);
	nLo |= (u32)((u8)pad_status->substickY << 16);
	nLo |= (u32)((u8)pad_status->substickX << 24);
}

bool NetPlayServer::GetNetPads(const u8 pad_nb, const SPADStatus* const pad_status, NetPad* const netvalues)
{
	m_crit.players.Enter();

	// in game mapping for this local pad, kinda silly method
	PadMapping in_game_num = m_players[m_socket].pad_map[pad_nb];

	// check if this local pad maps in game
	if (in_game_num >= 0)
	{
		NetPad np(pad_status);

		// add to buffer
		m_crit.buffer.Enter();
		m_pad_buffer[in_game_num].push(np);
		m_crit.buffer.Leave();

		// send to clients
		sf::Packet spac;
		spac << (MessageId)NP_MSG_PAD_DATA;
		spac << in_game_num;	// in game pad num
		spac << np.nHi << np.nLo;

		m_crit.send.Enter();
		SendToClients(spac);
		m_crit.send.Leave();
	}
	
	// get padstate from buffer and send to game
	m_crit.buffer.Enter();
	while (0 == m_pad_buffer[pad_nb].size())
	{
		m_crit.buffer.Leave();
		// wait for receiving thread to push some data
		Common::SleepCurrentThread(1);
		m_crit.buffer.Enter();
	}
	*netvalues = m_pad_buffer[pad_nb].front();
	m_pad_buffer[pad_nb].pop();
	m_crit.buffer.Leave();

	m_crit.players.Leave();

	return true;
}

bool NetPlayServer::SetSelectedGame(const std::string &game)
{
	// warning removal
	game.size();

	return true;
}

bool NetPlayClient::GetNetPads(const u8 pad_nb, const SPADStatus* const pad_status, NetPad* const netvalues)
{
	m_crit.players.Enter();

	// in game mapping for this local pad
	PadMapping in_game_num = m_players[m_pid].pad_map[pad_nb];

	// does this local pad map in game?
	if (in_game_num >= 0)
	{
		NetPad np(pad_status);

		// add to buffer
		m_crit.buffer.Enter();
		m_pad_buffer[in_game_num].push(np);
		m_crit.buffer.Leave();

		// send to server
		sf::Packet spac;
		spac << (MessageId)NP_MSG_PAD_DATA;
		spac << (PadMapping)pad_nb;	// local pad num
		spac << np.nHi << np.nLo;

		m_crit.send.Enter();
		m_socket.Send(spac);
		m_crit.send.Leave();
	}

	// get padstate from buffer and send to game
	m_crit.buffer.Enter();
	while (0 == m_pad_buffer[pad_nb].size())
	{
		m_crit.buffer.Leave();
		// wait for receiving thread to push some data
		Common::SleepCurrentThread(1);
		m_crit.buffer.Enter();
	}
	*netvalues = m_pad_buffer[pad_nb].front();
	m_pad_buffer[pad_nb].pop();
	m_crit.buffer.Leave();

	m_crit.players.Leave();
	return true;
}

bool NetPlayClient::SetSelectedGame(const std::string &game)
{
	// warning removal
	game.size();

	return true;
}

bool NetPlayServer::StartGame(const std::string &path)
{
	m_crit.other.Enter();

	if (m_is_running)
	{
		PanicAlert("Game is already running!");
		return false;
	}

	m_is_running = true;
	::NetClass_ptr = this;

	m_crit.buffer.Enter();
	// testing
	NetPad np;
	for (unsigned int i=0; i<TEMP_FIXED_BUFFER; ++i)
		for (unsigned int p=0; p<4; ++p)
			m_pad_buffer[p].push(np);
	m_crit.buffer.Leave();

	// tell clients to start game
	sf::Packet spac;
	spac << (MessageId)NP_MSG_START_GAME;

	m_crit.send.Enter();
	SendToClients(spac);
	m_crit.send.Leave();

	// boot game
	::main_frame->BootGame(path);
	//BootManager::BootCore(path);

	m_crit.other.Leave();

	return true;
}

bool NetPlayClient::StartGame(const std::string &path)
{
	m_crit.other.Enter();

	m_is_running = true;
	::NetClass_ptr = this;

	m_crit.buffer.Enter();
	// testing
	NetPad np;
	for (unsigned int i=0; i<TEMP_FIXED_BUFFER; ++i)
		for (unsigned int p=0; p<4; ++p)
			m_pad_buffer[p].push(np);
	m_crit.buffer.Leave();

	// boot game
	::main_frame->BootGame(path);
	//BootManager::BootCore(path);

	m_crit.other.Leave();

	return true;
}

// Actual Core function which is called on every frame
int CSIDevice_GCController::GetNetInput(u8 numPAD, SPADStatus PadStatus, u32 *PADStatus)
{
	if (NetClass_ptr)
		// TODO: NetClass_ptr could go null here, need a lock
		return NetClass_ptr->GetNetPads(numPAD, &PadStatus, (NetPad*)PADStatus) ? 1 : 0;
	else
		return 2;
}

// so all players' games get the same time
u32 CEXIIPL::GetNetGCTime()
{
	if (NetClass_ptr)
		// TODO: NetClass_ptr could go null here, need a lock
		return 1272737767;	// watev
	else
		return 0;
}
