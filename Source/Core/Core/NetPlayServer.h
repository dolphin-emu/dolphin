// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <queue>
#include <sstream>
#include <unordered_set>
#include <SFML/Network/Packet.hpp>
#include "Common/Thread.h"
#include "Common/Timer.h"
#include "Common/TraversalClient.h"
#include "Core/NetPlayProto.h"

class NetPlayUI;

class NetPlayServer : public TraversalClientClient
{
public:
	void ThreadFunc();
	void SendAsyncToClients(sf::Packet* packet);

	NetPlayServer(const u16 port, bool traversal, std::string centralServer, u16 centralPort);
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

	void KickPlayer(PlayerId player);

	u16 GetPort();

	void SetNetPlayUI(NetPlayUI* dialog);
	std::unordered_set<std::string> GetInterfaceSet();
	std::string GetInterfaceHost(const std::string inter);

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

		ENetPeer* socket;
		u32 ping;
		u32 current_game;

		bool operator==(const Client& other) const
		{
			return this == &other;
		}
	};

	void SendToClients(sf::Packet& packet, const PlayerId skip_pid = 0);
	void Send(ENetPeer* socket, sf::Packet& packet);
	unsigned int OnConnect(ENetPeer* socket);
	unsigned int OnDisconnect(Client& player);
	unsigned int OnData(sf::Packet& packet, Client& player);
	virtual void OnTraversalStateChanged();
	virtual void OnConnectReady(ENetAddress addr) {}
	virtual void OnConnectFailed(u8 reason) {}
	void UpdatePadMapping();
	void UpdateWiimoteMapping();
	std::vector<std::pair<std::string, std::string>> GetInterfaceListInternal();

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

	std::map<PlayerId, Client> m_players;

	struct
	{
		std::recursive_mutex game;
		// lock order
		std::recursive_mutex players;
		std::recursive_mutex async_queue_write;
	} m_crit;

	std::string m_selected_game;
	std::thread m_thread;
	Common::FifoQueue<std::unique_ptr<sf::Packet>, false> m_async_queue;

	ENetHost*        m_server;
	TraversalClient* m_traversal_client;
	NetPlayUI*       m_dialog;

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
