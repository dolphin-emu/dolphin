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

		std::unique_ptr<sf::TcpSocket> socket;
		u32 ping;
		u32 current_game;

		// VS2013 does not generate the right constructors here automatically
		//  like GCC does, so we implement them manually
		Client() = default;
		Client(const Client& other) = delete;
		Client(Client&& other)
			: pid(other.pid), name(std::move(other.name)), revision(std::move(other.revision)),
			socket(std::move(other.socket)), ping(other.ping), current_game(other.current_game)
		{
		}

		bool operator==(const Client& other) const
		{
			return this == &other;
		}
	};

	void SendToClients(sf::Packet& packet, const PlayerId skip_pid = 0);
	unsigned int OnConnect(std::unique_ptr<sf::TcpSocket>& socket);
	unsigned int OnDisconnect(Client& player);
	unsigned int OnData(sf::Packet& packet, Client& player);
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

	std::list<Client> m_players;

	struct
	{
		std::recursive_mutex game;
		// lock order
		std::recursive_mutex players, send;
	} m_crit;

	std::string m_selected_game;

	sf::TcpListener m_socket;
	std::thread m_thread;
	sf::SocketSelector m_selector;

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
