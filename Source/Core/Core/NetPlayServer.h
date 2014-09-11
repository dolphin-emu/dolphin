// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <queue>
#include <sstream>

#include <SFML/Network.hpp>

#include "Common/CommonTypes.h"
#include "Common/CommonTypes.h"
#include "Common/Thread.h"
#include "Common/Timer.h"

#include "Core/NetPlayProto.h"

class NetPlayServer
{
public:
	void ThreadFunc();

	NetPlayServer(const u16 port);
	~NetPlayServer();

	bool ChangeGame(const std::string& game);
	void SendChatMessage(const std::string& msg);

	void SetNetSettings(const NetSettings &settings);

	bool StartGame();

	void GetPadMapping(PadMapping map[]);
	void SetPadMapping(const PadMapping map[]);

	void GetWiimoteMapping(PadMapping map[]);
	void SetWiimoteMapping(const PadMapping map[]);

	void AdjustPadBufferSize(unsigned int size);

	void KickPlayer(u8 player);

	bool is_connected;

#ifdef USE_UPNP
	void TryPortmapping(u16 port);
#endif

private:
	class Client
	{
	public:
		PlayerId    pid;
		std::string name;
		std::string revision;

		sf::SocketTCP socket;
		u32 ping;
		u32 current_game;
	};

	void SendToClients(sf::Packet& packet, const PlayerId skip_pid = 0);
	unsigned int OnConnect(sf::SocketTCP& socket);
	unsigned int OnDisconnect(sf::SocketTCP& socket);
	unsigned int OnData(sf::Packet& packet, sf::SocketTCP& socket);
	void UpdatePadMapping();
	void UpdateWiimoteMapping();

	NetSettings     m_settings;

	bool            m_is_running;
	bool            m_do_loop;
	Common::Timer   m_ping_timer;
	u32             m_ping_key;
	bool            m_update_pings;
	u32             m_current_game;
	unsigned int    m_target_buffer_size;
	PadMapping      m_pad_map[4];
	PadMapping      m_wiimote_map[4];

	std::map<sf::SocketTCP, Client> m_players;

	struct
	{
		std::recursive_mutex game;
		// lock order
		std::recursive_mutex players, send;
	} m_crit;

	std::string m_selected_game;

	sf::SocketTCP m_socket;
	std::thread m_thread;
	sf::Selector<sf::SocketTCP> m_selector;

#ifdef USE_UPNP
	static void mapPortThread(const u16 port);
	static void unmapPortThread();

	static bool initUPnP();
	static bool UPnPMapPort(const std::string& addr, const u16 port);
	static bool UPnPUnmapPort(const u16 port);

	static struct UPNPUrls m_upnp_urls;
	static struct IGDdatas m_upnp_data;
	static u16 m_upnp_mapped;
	static bool m_upnp_inited;
	static bool m_upnp_error;
	static std::thread m_upnp_thread;
#endif
};
