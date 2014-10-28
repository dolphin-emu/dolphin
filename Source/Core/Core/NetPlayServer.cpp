// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <string>
#include <vector>

#include "Common/StringUtil.h"
#include "Core/NetPlayServer.h"
#include "InputCommon/GCPadStatus.h"

NetPlayServer::~NetPlayServer()
{
	if (is_connected)
	{
		m_do_loop = false;
		m_thread.join();
		m_socket.Close();
	}

#ifdef USE_UPNP
	if (m_upnp_thread.joinable())
		m_upnp_thread.join();
	m_upnp_thread = std::thread(&NetPlayServer::unmapPortThread);
	m_upnp_thread.join();
#endif
}

// called from ---GUI--- thread
NetPlayServer::NetPlayServer(const u16 port) : is_connected(false), m_is_running(false)
{
	memset(m_pad_map, -1, sizeof(m_pad_map));
	memset(m_wiimote_map, -1, sizeof(m_wiimote_map));
	if (m_socket.Listen(port))
	{
		is_connected = true;
		m_do_loop = true;
		m_selector.Add(m_socket);
		m_thread = std::thread(&NetPlayServer::ThreadFunc, this);
		m_target_buffer_size = 5;
	}
}

// called from ---NETPLAY--- thread
void NetPlayServer::ThreadFunc()
{
	while (m_do_loop)
	{
		// update pings every so many seconds
		if ((m_ping_timer.GetTimeElapsed() > (10 * 1000)) || m_update_pings)
		{
			//PanicAlertT("Sending pings");

			m_ping_key = Common::Timer::GetTimeMs();

			sf::Packet spac;
			spac << (MessageId)NP_MSG_PING;
			spac << m_ping_key;

			std::lock_guard<std::recursive_mutex> lks(m_crit.send);
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

				unsigned int error;
				{
				std::lock_guard<std::recursive_mutex> lkg(m_crit.game);
				error = OnConnect(accept_socket);
				}

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
					// if a bad packet is received, disconnect the client
					if (0 == OnData(rpac, ready_socket))
						break;

				//case sf::Socket::Disconnected :
				default :
					{
					std::lock_guard<std::recursive_mutex> lkg(m_crit.game);
					OnDisconnect(ready_socket);
					break;
					}
				}
			}
		}
	}

	// close listening socket and client sockets
	for (auto& player_entry : m_players)
		player_entry.second.socket.Close();
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
	PlayerId pid = 1;
	for (auto i = m_players.begin(); i != m_players.end(); ++i)
	{
		if (i->second.pid == pid)
		{
			pid++;
			i = m_players.begin();
		}
	}
	player.pid = pid;

	// try to automatically assign new user a pad
	for (PadMapping& mapping : m_pad_map)
	{
		if (mapping == -1)
		{
			mapping = player.pid;
			break;
		}
	}

	{
	std::lock_guard<std::recursive_mutex> lks(m_crit.send);

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
	if (m_selected_game != "")
	{
		spac.Clear();
		spac << (MessageId)NP_MSG_CHANGE_GAME;
		spac << m_selected_game;
		socket.Send(spac);
	}

	// send the pad buffer value
	spac.Clear();
	spac << (MessageId)NP_MSG_PAD_BUFFER;
	spac << (u32)m_target_buffer_size;
	socket.Send(spac);

	// sync values with new client
	for (const auto& p : m_players)
	{
		spac.Clear();
		spac << (MessageId)NP_MSG_PLAYER_JOIN;
		spac << p.second.pid << p.second.name << p.second.revision;
		socket.Send(spac);
	}

	} // unlock send

	// add client to the player list
	{
	std::lock_guard<std::recursive_mutex> lkp(m_crit.players);
	m_players[socket] = player;
	std::lock_guard<std::recursive_mutex> lks(m_crit.send);
	UpdatePadMapping(); // sync pad mappings with everyone
	UpdateWiimoteMapping();
	}


	// add client to selector/ used for receiving
	m_selector.Add(socket);

	return 0;
}

// called from ---NETPLAY--- thread
unsigned int NetPlayServer::OnDisconnect(sf::SocketTCP& socket)
{
	PlayerId pid = m_players[socket].pid;

	if (m_is_running)
	{
		for (PadMapping mapping : m_pad_map)
		{
			if (mapping == pid)
			{
				PanicAlertT("Client disconnect while game is running!! NetPlay is disabled. You must manually stop the game.");
				std::lock_guard<std::recursive_mutex> lkg(m_crit.game);
				m_is_running = false;

				sf::Packet spac;
				spac << (MessageId)NP_MSG_DISABLE_GAME;
				// this thread doesn't need players lock
				std::lock_guard<std::recursive_mutex> lks(m_crit.send);
				SendToClients(spac);
				break;
			}
		}
	}

	sf::Packet spac;
	spac << (MessageId)NP_MSG_PLAYER_LEAVE;
	spac << pid;

	m_selector.Remove(socket);

	std::lock_guard<std::recursive_mutex> lkp(m_crit.players);
	m_players.erase(m_players.find(socket));

	// alert other players of disconnect
	std::lock_guard<std::recursive_mutex> lks(m_crit.send);
	SendToClients(spac);

	for (PadMapping& mapping : m_pad_map)
	{
		if (mapping == pid)
		{
			mapping = -1;
		}
	}
	UpdatePadMapping();

	for (PadMapping& mapping : m_wiimote_map)
	{
		if (mapping == pid)
		{
			mapping = -1;
		}
	}
	UpdateWiimoteMapping();

	return 0;
}

// called from ---GUI--- thread
void NetPlayServer::GetPadMapping(PadMapping map[4])
{
	for (int i = 0; i < 4; i++)
		map[i] = m_pad_map[i];
}

void NetPlayServer::GetWiimoteMapping(PadMapping map[4])
{
	for (int i = 0; i < 4; i++)
		map[i] = m_wiimote_map[i];
}

// called from ---GUI--- thread
void NetPlayServer::SetPadMapping(const PadMapping map[4])
{
	for (int i = 0; i < 4; i++)
		m_pad_map[i] = map[i];
	UpdatePadMapping();
}

// called from ---GUI--- thread
void NetPlayServer::SetWiimoteMapping(const PadMapping map[4])
{
	for (int i = 0; i < 4; i++)
		m_wiimote_map[i] = map[i];
	UpdateWiimoteMapping();
}

// called from ---GUI--- thread and ---NETPLAY--- thread
void NetPlayServer::UpdatePadMapping()
{
	sf::Packet spac;
	spac << (MessageId)NP_MSG_PAD_MAPPING;
	for (PadMapping mapping : m_pad_map)
	{
		spac << mapping;
	}
	SendToClients(spac);
}

// called from ---NETPLAY--- thread
void NetPlayServer::UpdateWiimoteMapping()
{
	sf::Packet spac;
	spac << (MessageId)NP_MSG_WIIMOTE_MAPPING;
	for (PadMapping mapping : m_wiimote_map)
	{
		spac << mapping;
	}
	SendToClients(spac);
}

// called from ---GUI--- thread and ---NETPLAY--- thread
void NetPlayServer::AdjustPadBufferSize(unsigned int size)
{
	std::lock_guard<std::recursive_mutex> lkg(m_crit.game);

	m_target_buffer_size = size;

	// tell clients to change buffer size
	sf::Packet spac;
	spac << (MessageId)NP_MSG_PAD_BUFFER;
	spac << (u32)m_target_buffer_size;

	std::lock_guard<std::recursive_mutex> lkp(m_crit.players);
	std::lock_guard<std::recursive_mutex> lks(m_crit.send);
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

			{
			std::lock_guard<std::recursive_mutex> lks(m_crit.send);
			SendToClients(spac, player.pid);
			}
		}
		break;

	case NP_MSG_PAD_DATA :
		{
			// if this is pad data from the last game still being received, ignore it
			if (player.current_game != m_current_game)
				break;

			PadMapping map = 0;
			GCPadStatus pad;
			packet >> map >> pad.button >> pad.analogA >> pad.analogB >> pad.stickX >> pad.stickY >> pad.substickX >> pad.substickY >> pad.triggerLeft >> pad.triggerRight;

			// If the data is not from the correct player,
			// then disconnect them.
			if (m_pad_map[map] != player.pid)
				return 1;

			// Relay to clients
			sf::Packet spac;
			spac << (MessageId)NP_MSG_PAD_DATA;
			spac << map << pad.button << pad.analogA << pad.analogB << pad.stickX << pad.stickY << pad.substickX << pad.substickY << pad.triggerLeft << pad.triggerRight;

			std::lock_guard<std::recursive_mutex> lks(m_crit.send);
			SendToClients(spac, player.pid);
		}
		break;

		case NP_MSG_WIIMOTE_DATA :
		{
			// if this is wiimote data from the last game still being received, ignore it
			if (player.current_game != m_current_game)
				break;

			PadMapping map = 0;
			u8 size;
			packet >> map >> size;
			std::vector<u8> data(size);
			for (size_t i = 0; i < data.size(); ++i)
				packet >> data[i];

			// If the data is not from the correct player,
			// then disconnect them.
			if (m_wiimote_map[map] != player.pid)
			{
				return 1;
			}

			// relay to clients
			sf::Packet spac;
			spac << (MessageId)NP_MSG_WIIMOTE_DATA;
			spac << map;
			spac << size;
			for (const u8& byte : data)
				spac << byte;

			std::lock_guard<std::recursive_mutex> lks(m_crit.send);
			SendToClients(spac, player.pid);
		}
		break;

	case NP_MSG_PONG :
		{
			const u32 ping = (u32)m_ping_timer.GetTimeElapsed();
			u32 ping_key = 0;
			packet >> ping_key;

			if (m_ping_key == ping_key)
			{
				player.ping = ping;
			}

			sf::Packet spac;
			spac << (MessageId)NP_MSG_PLAYER_PING_DATA;
			spac << player.pid;
			spac << player.ping;

			std::lock_guard<std::recursive_mutex> lks(m_crit.send);
			SendToClients(spac);
		}
		break;

	case NP_MSG_START_GAME :
		{
			packet >> player.current_game;
		}
		break;

	case NP_MSG_STOP_GAME:
		{
			// tell clients to stop game
			sf::Packet spac;
			spac << (MessageId)NP_MSG_STOP_GAME;

			std::lock_guard<std::recursive_mutex> lkp(m_crit.players);
			std::lock_guard<std::recursive_mutex> lks(m_crit.send);
			SendToClients(spac);

			m_is_running = false;
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

// called from ---GUI--- thread / and ---NETPLAY--- thread
void NetPlayServer::SendChatMessage(const std::string& msg)
{
	sf::Packet spac;
	spac << (MessageId)NP_MSG_CHAT_MESSAGE;
	spac << (PlayerId)0; // server id always 0
	spac << msg;

	std::lock_guard<std::recursive_mutex> lkp(m_crit.players);
	std::lock_guard<std::recursive_mutex> lks(m_crit.send);
	SendToClients(spac);
}

// called from ---GUI--- thread
bool NetPlayServer::ChangeGame(const std::string &game)
{
	std::lock_guard<std::recursive_mutex> lkg(m_crit.game);

	m_selected_game = game;

	// send changed game to clients
	sf::Packet spac;
	spac << (MessageId)NP_MSG_CHANGE_GAME;
	spac << game;

	std::lock_guard<std::recursive_mutex> lkp(m_crit.players);
	std::lock_guard<std::recursive_mutex> lks(m_crit.send);
	SendToClients(spac);

	return true;
}

// called from ---GUI--- thread
void NetPlayServer::SetNetSettings(const NetSettings &settings)
{
	m_settings = settings;
}

// called from ---GUI--- thread
bool NetPlayServer::StartGame()
{
	std::lock_guard<std::recursive_mutex> lkg(m_crit.game);
	m_current_game = Common::Timer::GetTimeMs();

	// no change, just update with clients
	AdjustPadBufferSize(m_target_buffer_size);

	// tell clients to start game
	sf::Packet spac;
	spac << (MessageId)NP_MSG_START_GAME;
	spac << m_current_game;
	spac << m_settings.m_CPUthread;
	spac << m_settings.m_CPUcore;
	spac << m_settings.m_DSPEnableJIT;
	spac << m_settings.m_DSPHLE;
	spac << m_settings.m_WriteToMemcard;
	spac << m_settings.m_EXIDevice[0];
	spac << m_settings.m_EXIDevice[1];

	std::lock_guard<std::recursive_mutex> lkp(m_crit.players);
	std::lock_guard<std::recursive_mutex> lks(m_crit.send);
	SendToClients(spac);

	m_is_running = true;

	return true;
}

// called from multiple threads
void NetPlayServer::SendToClients(sf::Packet& packet, const PlayerId skip_pid)
{
	for (std::pair<const sf::SocketTCP, Client>& p : m_players)
	{
		if (p.second.pid &&
		    p.second.pid != skip_pid)
		{
			p.second.socket.Send(packet);
		}
	}
}

void NetPlayServer::KickPlayer(u8 player)
{
	for (auto& current_player : m_players)
	{
		if (current_player.second.pid == player)
		{
			current_player.second.socket.Close();
			return;
		}
	}
}

#ifdef USE_UPNP
#include <miniwget.h>
#include <miniupnpc.h>
#include <upnpcommands.h>

struct UPNPUrls NetPlayServer::m_upnp_urls;
struct IGDdatas NetPlayServer::m_upnp_data;
u16 NetPlayServer::m_upnp_mapped = 0;
bool NetPlayServer::m_upnp_inited = false;
bool NetPlayServer::m_upnp_error = false;
std::thread NetPlayServer::m_upnp_thread;

// called from ---GUI--- thread
void NetPlayServer::TryPortmapping(u16 port)
{
	if (m_upnp_thread.joinable())
		m_upnp_thread.join();
	m_upnp_thread = std::thread(&NetPlayServer::mapPortThread, port);
}

// UPnP thread: try to map a port
void NetPlayServer::mapPortThread(const u16 port)
{
	std::string ourIP = sf::IPAddress::GetLocalAddress().ToString();

	if (!m_upnp_inited)
		if (!initUPnP())
			goto fail;

	if (!UPnPMapPort(ourIP, port))
		goto fail;

	NOTICE_LOG(NETPLAY, "Successfully mapped port %d to %s.", port, ourIP.c_str());
	return;
fail:
	WARN_LOG(NETPLAY, "Failed to map port %d to %s.", port, ourIP.c_str());
	return;
}

// UPnP thread: try to unmap a port
void NetPlayServer::unmapPortThread()
{
	if (m_upnp_mapped > 0)
		UPnPUnmapPort(m_upnp_mapped);
}

// called from ---UPnP--- thread
// discovers the IGD
bool NetPlayServer::initUPnP()
{
	std::vector<UPNPDev *> igds;
	int descXMLsize = 0, upnperror = 0;

	// Don't init if already inited
	if (m_upnp_inited)
		return true;

	// Don't init if it failed before
	if (m_upnp_error)
		return false;

	memset(&m_upnp_urls, 0, sizeof(UPNPUrls));
	memset(&m_upnp_data, 0, sizeof(IGDdatas));

	// Find all UPnP devices
	UPNPDev *devlist = upnpDiscover(2000, nullptr, nullptr, 0, 0, &upnperror);
	if (!devlist)
	{
		WARN_LOG(NETPLAY, "An error occured trying to discover UPnP devices.");

		m_upnp_error = true;
		m_upnp_inited = false;

		return false;
	}

	// Look for the IGD
	for (UPNPDev* dev = devlist; dev; dev = dev->pNext)
	{
		if (strstr(dev->st, "InternetGatewayDevice"))
			igds.push_back(dev);
	}

	for (const UPNPDev* dev : igds)
	{
		char* descXML = (char*) miniwget(dev->descURL, &descXMLsize, 0);
		if (descXML)
		{
			parserootdesc(descXML, descXMLsize, &m_upnp_data);
			free(descXML);
			descXML = nullptr;
			GetUPNPUrls(&m_upnp_urls, &m_upnp_data, dev->descURL, 0);

			NOTICE_LOG(NETPLAY, "Got info from IGD at %s.", dev->descURL);
			break;
		}
		else
		{
			WARN_LOG(NETPLAY, "Error getting info from IGD at %s.", dev->descURL);
		}
	}

	freeUPNPDevlist(devlist);

	return true;
}

// called from ---UPnP--- thread
// Attempt to portforward!
bool NetPlayServer::UPnPMapPort(const std::string& addr, const u16 port)
{
	if (m_upnp_mapped > 0)
		UPnPUnmapPort(m_upnp_mapped);

	std::string port_str = StringFromFormat("%d", port);
	int result = UPNP_AddPortMapping(m_upnp_urls.controlURL, m_upnp_data.first.servicetype,
	                             port_str.c_str(), port_str.c_str(), addr.c_str(),
	                             (std::string("dolphin-emu TCP on ") + addr).c_str(),
	                             "TCP", nullptr, nullptr);

	if (result != 0)
		return false;

	m_upnp_mapped = port;

	return true;
}

// called from ---UPnP--- thread
// Attempt to stop portforwarding.
// --
// NOTE: It is important that this happens! A few very crappy routers
// apparently do not delete UPnP mappings on their own, so if you leave them
// hanging, the NVRAM will fill with portmappings, and eventually all UPnP
// requests will fail silently, with the only recourse being a factory reset.
// --
bool NetPlayServer::UPnPUnmapPort(const u16 port)
{
	std::string port_str = StringFromFormat("%d", port);
	UPNP_DeletePortMapping(m_upnp_urls.controlURL, m_upnp_data.first.servicetype,
	                       port_str.c_str(), "TCP", nullptr);

	return true;
}
#endif
