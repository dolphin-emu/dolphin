// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>
#include <string>
#include <vector>
#include "Common/Common.h"
#include "Common/ENetUtil.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/StringUtil.h"
#include "Core/ConfigManager.h"
#include "Core/NetPlayClient.h" //for NetPlayUI
#include "Core/NetPlayServer.h"
#include "Core/HW/EXI_DeviceIPL.h"
#include "Core/HW/Sram.h"
#include "InputCommon/GCPadStatus.h"
#if !defined(_WIN32)
#include <sys/types.h>
#include <sys/socket.h>
#ifndef ANDROID
#include <ifaddrs.h>
#endif
#include <arpa/inet.h>
#endif

static const char* NETPLAY_VERSION = scm_rev_git_str;
u64 g_netplay_initial_gctime = 1272737767;

NetPlayServer::~NetPlayServer()
{
	if (is_connected)
	{
		m_do_loop = false;
		m_thread.join();
		enet_host_destroy(m_server);

		if (g_MainNetHost.get() == m_server)
		{
			g_MainNetHost.release();
		}

		if (m_traversal_client)
		{
			g_TraversalClient->m_Client = nullptr;
			ReleaseTraversalClient();
		}
	}

#ifdef USE_UPNP
	if (m_upnp_thread.joinable())
		m_upnp_thread.join();
	m_upnp_thread = std::thread(&NetPlayServer::unmapPortThread);
	m_upnp_thread.join();
#endif
}

// called from ---GUI--- thread
NetPlayServer::NetPlayServer(const u16 port, bool traversal, const std::string& centralServer, u16 centralPort)
{
	//--use server time
	if (enet_initialize() != 0)
	{
		PanicAlertT("Enet Didn't Initialize");
	}

	m_pad_map.fill(-1);
	m_wiimote_map.fill(-1);

	if (traversal)
	{
		if (!EnsureTraversalClient(centralServer, centralPort, port))
			return;

		g_TraversalClient->m_Client = this;
		m_traversal_client = g_TraversalClient.get();

		m_server = g_MainNetHost.get();

		if (g_TraversalClient->m_State == TraversalClient::Failure)
			g_TraversalClient->ReconnectToServer();
	}
	else
	{
		ENetAddress serverAddr;
		serverAddr.host = ENET_HOST_ANY;
		serverAddr.port = port;
		m_server = enet_host_create(&serverAddr, 10, 3, 0, 0);
		if (m_server != nullptr)
			m_server->intercept = ENetUtil::InterceptCallback;
	}
	if (m_server != nullptr)
	{
		is_connected = true;
		m_do_loop = true;
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
		if ((m_ping_timer.GetTimeElapsed() > 1000) || m_update_pings)
		{
			m_ping_key = Common::Timer::GetTimeMs();

			sf::Packet spac;
			spac << (MessageId)NP_MSG_PING;
			spac << m_ping_key;

			m_ping_timer.Start();
			SendToClients(spac);
			m_update_pings = false;
		}

		ENetEvent netEvent;
		int net;
		if (m_traversal_client)
			m_traversal_client->HandleResends();
		net = enet_host_service(m_server, &netEvent, 1000);
		while (!m_async_queue.Empty())
		{
			{
				std::lock_guard<std::recursive_mutex> lkp(m_crit.players);
				SendToClients(*(m_async_queue.Front().get()));
			}
			m_async_queue.Pop();
		}
		if (net > 0)
		{
			switch (netEvent.type)
			{
			case ENET_EVENT_TYPE_CONNECT:
			{
				ENetPeer* accept_peer = netEvent.peer;
				unsigned int error;
				{
					std::lock_guard<std::recursive_mutex> lkg(m_crit.game);
					error = OnConnect(accept_peer);
				}

				if (error)
				{
					sf::Packet spac;
					spac << (MessageId)error;
					// don't need to lock, this client isn't in the client map
					Send(accept_peer, spac);
					if (netEvent.peer->data)
					{
						delete (PlayerId *) netEvent.peer->data;
						netEvent.peer->data = nullptr;
					}
					enet_peer_disconnect(accept_peer, 0);
				}
			}
			break;
			case ENET_EVENT_TYPE_RECEIVE:
			{
				sf::Packet rpac;
				rpac.append(netEvent.packet->data, netEvent.packet->dataLength);

				auto it = m_players.find(*(PlayerId *)netEvent.peer->data);
				Client& client = it->second;
				if (OnData(rpac, client) != 0)
				{
					// if a bad packet is received, disconnect the client
					std::lock_guard<std::recursive_mutex> lkg(m_crit.game);
					OnDisconnect(client);

					if (netEvent.peer->data)
					{
						delete (PlayerId *)netEvent.peer->data;
						netEvent.peer->data = nullptr;
					}
				}
				enet_packet_destroy(netEvent.packet);
			}
			break;
			case ENET_EVENT_TYPE_DISCONNECT:
			{
				std::lock_guard<std::recursive_mutex> lkg(m_crit.game);
				if  (!netEvent.peer->data)
					break;
				auto it = m_players.find(*(PlayerId *)netEvent.peer->data);
				if (it != m_players.end())
				{
					Client& client = it->second;
					OnDisconnect(client);

					if (netEvent.peer->data)
					{
						delete (PlayerId *)netEvent.peer->data;
						netEvent.peer->data = nullptr;
					}
				}
			}
			break;
			default:
			break;
			}
		}
	}

	// close listening socket and client sockets
	for (auto& player_entry : m_players)
	{
		delete (PlayerId *)player_entry.second.socket->data;
		player_entry.second.socket->data = nullptr;
		enet_peer_disconnect(player_entry.second.socket, 0);
	}
}

// called from ---NETPLAY--- thread
unsigned int NetPlayServer::OnConnect(ENetPeer* socket)
{
	sf::Packet rpac;
	ENetPacket* epack;
	do
	{
		epack = enet_peer_receive(socket, nullptr);
	} while (epack == nullptr);
	rpac.append(epack->data, epack->dataLength);

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
	socket->data = new PlayerId(pid);

	std::string npver;
	rpac >> npver;
	// Dolphin netplay version
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
	player.pid = pid;
	player.socket = socket;
	rpac >> player.revision;
	rpac >> player.name;

	enet_packet_destroy(epack);
	// try to automatically assign new user a pad
	for (PadMapping& mapping : m_pad_map)
	{
		if (mapping == -1)
		{
			mapping = player.pid;
			break;
		}
	}

	// send join message to already connected clients
	sf::Packet spac;
	spac << (MessageId)NP_MSG_PLAYER_JOIN;
	spac << player.pid << player.name << player.revision;
	SendToClients(spac);

	// send new client success message with their id
	spac.clear();
	spac << (MessageId)0;
	spac << player.pid;
	Send(player.socket, spac);

	// send new client the selected game
	if (m_selected_game != "")
	{
		spac.clear();
		spac << (MessageId)NP_MSG_CHANGE_GAME;
		spac << m_selected_game;
		Send(player.socket, spac);
	}

	// send the pad buffer value
	spac.clear();
	spac << (MessageId)NP_MSG_PAD_BUFFER;
	spac << (u32)m_target_buffer_size;
	Send(player.socket, spac);

	// sync GC SRAM with new client
	if (!g_SRAM_netplay_initialized)
	{
		SConfig::GetInstance().m_strSRAM = File::GetUserPath(F_GCSRAM_IDX);
		InitSRAM();
		g_SRAM_netplay_initialized = true;
	}
	spac.clear();
	spac << (MessageId)NP_MSG_SYNC_GC_SRAM;
	for (size_t i = 0; i < sizeof(g_SRAM.p_SRAM); ++i)
	{
		spac << g_SRAM.p_SRAM[i];
	}
	Send(player.socket, spac);

	// sync values with new client
	for (const auto& p : m_players)
	{
		spac.clear();
		spac << (MessageId)NP_MSG_PLAYER_JOIN;
		spac << p.second.pid << p.second.name << p.second.revision;
		Send(player.socket, spac);
	}

	// add client to the player list
	{
		std::lock_guard<std::recursive_mutex> lkp(m_crit.players);
		m_players.emplace(*(PlayerId *)player.socket->data, player);
		UpdatePadMapping(); // sync pad mappings with everyone
		UpdateWiimoteMapping();
	}

	return 0;
}

// called from ---NETPLAY--- thread
unsigned int NetPlayServer::OnDisconnect(Client& player)
{
	PlayerId pid = player.pid;

	if (m_is_running)
	{
		for (PadMapping mapping : m_pad_map)
		{
			if (mapping == pid && pid != 1)
			{
				PanicAlertT("Client disconnect while game is running!! NetPlay is disabled. You must manually stop the game.");
				std::lock_guard<std::recursive_mutex> lkg(m_crit.game);
				m_is_running = false;

				sf::Packet spac;
				spac << (MessageId)NP_MSG_DISABLE_GAME;
				// this thread doesn't need players lock
				SendToClients(spac, 1);
				break;
			}
		}
	}

	sf::Packet spac;
	spac << (MessageId)NP_MSG_PLAYER_LEAVE;
	spac << pid;

	enet_peer_disconnect(player.socket, 0);

	std::lock_guard<std::recursive_mutex> lkp(m_crit.players);
	auto it = m_players.find(player.pid);
	if (it != m_players.end())
		m_players.erase(it);

	// alert other players of disconnect
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
PadMappingArray NetPlayServer::GetPadMapping() const
{
	return m_pad_map;
}

PadMappingArray NetPlayServer::GetWiimoteMapping() const
{
	return m_wiimote_map;
}

// called from ---GUI--- thread
void NetPlayServer::SetPadMapping(const PadMappingArray& mappings)
{
	m_pad_map = mappings;
	UpdatePadMapping();
}

// called from ---GUI--- thread
void NetPlayServer::SetWiimoteMapping(const PadMappingArray& mappings)
{
	m_wiimote_map = mappings;
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
	sf::Packet* spac = new sf::Packet;
	*spac << (MessageId)NP_MSG_PAD_BUFFER;
	*spac << (u32)m_target_buffer_size;

	SendAsyncToClients(spac);
}

void NetPlayServer::SendAsyncToClients(sf::Packet* packet)
{
	{
		std::lock_guard<std::recursive_mutex> lkq(m_crit.async_queue_write);
		m_async_queue.Push(std::unique_ptr<sf::Packet>(packet));
	}
	ENetUtil::WakeupThread(m_server);
}

// called from ---NETPLAY--- thread
unsigned int NetPlayServer::OnData(sf::Packet& packet, Client& player)
{
	MessageId mid;
	packet >> mid;

	// don't need lock because this is the only thread that modifies the players
	// only need locks for writes to m_players in this thread

	switch (mid)
	{
	case NP_MSG_CHAT_MESSAGE:
	{
		std::string msg;
		packet >> msg;

		// send msg to other clients
		sf::Packet spac;
		spac << (MessageId)NP_MSG_CHAT_MESSAGE;
		spac << player.pid;
		spac << msg;

		SendToClients(spac, player.pid);
	}
	break;

	case NP_MSG_PAD_DATA:
	{
		// if this is pad data from the last game still being received, ignore it
		if (player.current_game != m_current_game)
			break;

		PadMapping map = 0;
		GCPadStatus pad;
		packet >> map
		       >> pad.button
		       >> pad.analogA
		       >> pad.analogB
		       >> pad.stickX
		       >> pad.stickY
		       >> pad.substickX
		       >> pad.substickY
		       >> pad.triggerLeft
		       >> pad.triggerRight;

		// If the data is not from the correct player,
		// then disconnect them.
		if (map < 0 || map > 3 || m_pad_map[map] != player.pid)
		{
			return 1;
		}

		// Relay to clients
		sf::Packet spac;
		spac << (MessageId)NP_MSG_PAD_DATA;
		spac << map
		     << pad.button
		     << pad.analogA
		     << pad.analogB
		     << pad.stickX
		     << pad.stickY
		     << pad.substickX
		     << pad.substickY
		     << pad.triggerLeft
		     << pad.triggerRight;

		SendToClients(spac, player.pid);
	}
	break;

	case NP_MSG_WIIMOTE_DATA:
	{
		// if this is Wiimote data from the last game still being received, ignore it
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
		if (map < 0 || map > 3 || m_wiimote_map[map] != player.pid)
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

		SendToClients(spac, player.pid);
	}
	break;

	case NP_MSG_PONG:
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

		SendToClients(spac);
	}
	break;

	case NP_MSG_START_GAME:
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
		SendToClients(spac);

		m_is_running = false;
	}
	break;

	case NP_MSG_TIMEBASE:
	{
		u32 x, y, frame;
		packet >> x;
		packet >> y;
		packet >> frame;

		if (m_desync_detected)
			break;

		u64 timebase = x | ((u64)y << 32);
		std::vector<std::pair<PlayerId, u64>>& timebases = m_timebase_by_frame[frame];
		timebases.emplace_back(player.pid, timebase);
		if (timebases.size() >= m_players.size())
		{
			// we have all records for this frame

			if (!std::all_of(timebases.begin(), timebases.end(), [&](std::pair<PlayerId, u64> pair){ return pair.second == timebases[0].second; }))
			{
				int pid_to_blame = -1;
				if (timebases.size() > 2)
				{
					for (auto pair : timebases)
					{
						if (std::all_of(timebases.begin(), timebases.end(), [&](std::pair<PlayerId, u64> other) {
							return other.first == pair.first || other.second != pair.second;
						}))
						{
							// we are the only outlier
							pid_to_blame = pair.first;
							break;
						}
					}
				}

				sf::Packet spac;
				spac << (MessageId) NP_MSG_DESYNC_DETECTED;
				spac << pid_to_blame;
				spac << frame;
				SendToClients(spac);

				m_desync_detected = true;
			}
			m_timebase_by_frame.erase(frame);
		}
	}
	break;
	default:
		PanicAlertT("Unknown message with id:%d received from player:%d Kicking player!", mid, player.pid);
		// unknown message, kick the client
		return 1;
		break;
	}

	return 0;
}


void NetPlayServer::OnTraversalStateChanged()
{
	if (m_dialog)
		m_dialog->Update();
}

// called from ---GUI--- thread
void NetPlayServer::SendChatMessage(const std::string& msg)
{
	sf::Packet* spac = new sf::Packet;
	*spac << (MessageId)NP_MSG_CHAT_MESSAGE;
	*spac << (PlayerId)0; // server id always 0
	*spac << msg;

	SendAsyncToClients(spac);
}

// called from ---GUI--- thread
bool NetPlayServer::ChangeGame(const std::string &game)
{
	std::lock_guard<std::recursive_mutex> lkg(m_crit.game);

	m_selected_game = game;

	// send changed game to clients
	sf::Packet* spac = new sf::Packet;
	*spac << (MessageId)NP_MSG_CHANGE_GAME;
	*spac << game;

	SendAsyncToClients(spac);

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
	m_timebase_by_frame.clear();
	m_desync_detected = false;
	std::lock_guard<std::recursive_mutex> lkg(m_crit.game);
	m_current_game = Common::Timer::GetTimeMs();

	// no change, just update with clients
	AdjustPadBufferSize(m_target_buffer_size);

	g_netplay_initial_gctime = Common::Timer::GetLocalTimeSinceJan1970();

	// tell clients to start game
	sf::Packet* spac = new sf::Packet;
	*spac << (MessageId)NP_MSG_START_GAME;
	*spac << m_current_game;
	*spac << m_settings.m_CPUthread;
	*spac << m_settings.m_CPUcore;
	*spac << m_settings.m_SelectedLanguage;
	*spac << m_settings.m_OverrideGCLanguage;
	*spac << m_settings.m_ProgressiveScan;
	*spac << m_settings.m_PAL60;
	*spac << m_settings.m_DSPEnableJIT;
	*spac << m_settings.m_DSPHLE;
	*spac << m_settings.m_WriteToMemcard;
	*spac << m_settings.m_OCEnable;
	*spac << m_settings.m_OCFactor;
	*spac << m_settings.m_EXIDevice[0];
	*spac << m_settings.m_EXIDevice[1];
	*spac << (u32)g_netplay_initial_gctime;
	*spac << (u32)(g_netplay_initial_gctime >> 32);

	SendAsyncToClients(spac);

	m_is_running = true;

	return true;
}

// called from multiple threads
void NetPlayServer::SendToClients(sf::Packet& packet, const PlayerId skip_pid)
{
	for (auto& p : m_players)
	{
		if (p.second.pid && p.second.pid != skip_pid)
		{
			Send(p.second.socket, packet);
		}
	}
}

void NetPlayServer::Send(ENetPeer* socket, sf::Packet& packet)
{
	ENetPacket* epac = enet_packet_create(packet.getData(), packet.getDataSize(), ENET_PACKET_FLAG_RELIABLE);
	enet_peer_send(socket, 0, epac);
}

void NetPlayServer::KickPlayer(PlayerId player)
{
	for (auto& current_player : m_players)
	{
		if (current_player.second.pid == player)
		{
			enet_peer_disconnect(current_player.second.socket, 0);
			return;
		}
	}
}

u16 NetPlayServer::GetPort()
{
	return m_server->address.port;
}

void NetPlayServer::SetNetPlayUI(NetPlayUI* dialog)
{
	m_dialog = dialog;
}

// called from ---GUI--- thread
std::unordered_set<std::string> NetPlayServer::GetInterfaceSet()
{
	std::unordered_set<std::string> result;
	auto lst = GetInterfaceListInternal();
	for (auto list_entry : lst)
		result.emplace(list_entry.first);
	return result;
}

// called from ---GUI--- thread
std::string NetPlayServer::GetInterfaceHost(const std::string& inter)
{
	char buf[16];
	sprintf(buf, ":%d", GetPort());
	auto lst = GetInterfaceListInternal();
	for (const auto& list_entry : lst)
	{
		if (list_entry.first == inter)
		{
			return list_entry.second + buf;
		}
	}
	return "?";
}

// called from ---GUI--- thread
std::vector<std::pair<std::string, std::string>> NetPlayServer::GetInterfaceListInternal()
{
	std::vector<std::pair<std::string, std::string>> result;
#if defined(_WIN32)

#elif defined(ANDROID)
	// Android has no getifaddrs for some stupid reason.  If this
	// functionality ends up actually being used on Android, fix this.
#else
	ifaddrs* ifp = nullptr;
	char buf[512];
	if (getifaddrs(&ifp) != -1)
	{
		for (ifaddrs* curifp = ifp; curifp; curifp = curifp->ifa_next)
		{
			sockaddr* sa = curifp->ifa_addr;
			if (sa->sa_family != AF_INET)
				continue;
			sockaddr_in* sai = (struct sockaddr_in*) sa;
			if (ntohl(((struct sockaddr_in*) sa)->sin_addr.s_addr) == 0x7f000001)
				continue;
			const char* ip = inet_ntop(sa->sa_family, &sai->sin_addr, buf, sizeof(buf));
			if (ip == nullptr)
				continue;
			result.emplace_back(std::make_pair(curifp->ifa_name, ip));
		}
		freeifaddrs(ifp);
	}
#endif
	if (result.empty())
		result.emplace_back(std::make_pair("!local!", "127.0.0.1"));
	return result;
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
	ENetAddress adr = { ENET_HOST_ANY, port };
	char cIP[20];

	enet_address_get_host(&adr, cIP, 20);
	std::string ourIP(cIP);

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
#if MINIUPNPC_API_VERSION >= 14
	UPNPDev *devlist = upnpDiscover(2000, nullptr, nullptr, 0, 0, 2, &upnperror);
#else
	UPNPDev *devlist = upnpDiscover(2000, nullptr, nullptr, 0, 0, &upnperror);
#endif
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
		char* descXML = (char*)miniwget(dev->descURL, &descXMLsize, 0);
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
		(std::string("dolphin-emu UDP on ") + addr).c_str(),
		"UDP", nullptr, nullptr);

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
		port_str.c_str(), "UDP", nullptr);

	return true;
}
#endif
