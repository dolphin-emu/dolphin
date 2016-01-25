// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <SFML/Network/Packet.hpp>
#include "Common/CommonTypes.h"
#include "Common/FifoQueue.h"
#include "Common/TraversalClient.h"
#include "Core/NetPlayProto.h"
#include "InputCommon/GCPadStatus.h"


class NetPlayUI
{
public:
	virtual ~NetPlayUI() {}

	virtual void BootGame(const std::string& filename) = 0;
	virtual void StopGame() = 0;

	virtual void Update() = 0;
	virtual void AppendChat(const std::string& msg) = 0;

	virtual void OnMsgChangeGame(const std::string& filename) = 0;
	virtual void OnMsgStartGame() = 0;
	virtual void OnMsgStopGame() = 0;
	virtual bool IsRecording() = 0;
};

class Player
{
public:
	PlayerId    pid;
	std::string name;
	std::string revision;
	u32         ping;
};

class NetPlayClient : public TraversalClientClient
{
public:
	void ThreadFunc();
	void SendAsync(sf::Packet* packet);

	NetPlayClient(const std::string& address, const u16 port, NetPlayUI* dialog, const std::string& name, bool traversal, const std::string& centralServer, u16 centralPort);
	~NetPlayClient();

	void GetPlayerList(std::string& list, std::vector<int>& pid_list);
	std::vector<const Player*> GetPlayers();

	// Called from the GUI thread.
	bool IsConnected() const { return m_is_connected; }

	bool StartGame(const std::string &path);
	bool StopGame();
	void Stop();
	bool ChangeGame(const std::string& game);
	void SendChatMessage(const std::string& msg);

	// Send and receive pads values
	bool WiimoteUpdate(int _number, u8* data, const u8 size);
	bool GetNetPads(const u8 pad_nb, GCPadStatus* pad_status);

	void OnTraversalStateChanged() override;
	void OnConnectReady(ENetAddress addr) override;
	void OnConnectFailed(u8 reason) override;

	u8 LocalPadToInGamePad(u8 localPad);
	u8 InGamePadToLocalPad(u8 localPad);

	u8 LocalWiimoteToInGameWiimote(u8 local_pad);

	static void SendTimeBase();

protected:
	void ClearBuffers();

	struct
	{
		std::recursive_mutex game;
		// lock order
		std::recursive_mutex players;
		std::recursive_mutex async_queue_write;
	} m_crit;

	Common::FifoQueue<std::unique_ptr<sf::Packet>, false> m_async_queue;

	Common::FifoQueue<GCPadStatus> m_pad_buffer[4];
	Common::FifoQueue<NetWiimote>  m_wiimote_buffer[4];

	NetPlayUI*   m_dialog;

	ENetHost*    m_client;
	ENetPeer*    m_server;
	std::thread  m_thread;

	std::string       m_selected_game;
	std::atomic<bool> m_is_running;
	std::atomic<bool> m_do_loop;

	unsigned int  m_target_buffer_size;

	Player* m_local_player;

	u32 m_current_game;

	PadMappingArray m_pad_map;
	PadMappingArray m_wiimote_map;

	bool m_is_recording;

private:
	enum class ConnectionState
	{
		WaitingForTraversalClientConnection,
		WaitingForTraversalClientConnectReady,
		Connecting,
		WaitingForHelloResponse,
		Connected,
		Failure
	};

	void UpdateDevices();
	void SendPadState(const PadMapping in_game_pad, const GCPadStatus& np);
	void SendWiimoteState(const PadMapping in_game_pad, const NetWiimote& nw);
	unsigned int OnData(sf::Packet& packet);
	void Send(sf::Packet& packet);
	void Disconnect();
	bool Connect();

	bool m_is_connected = false;
	ConnectionState m_connection_state = ConnectionState::Failure;

	PlayerId m_pid;
	std::map<PlayerId, Player> m_players;
	std::string m_host_spec;
	std::string m_player_name;
	bool m_connecting;
	TraversalClient* m_traversal_client;

	u32 m_timebase_frame;
};

void NetPlay_Enable(NetPlayClient* const np);
void NetPlay_Disable();
