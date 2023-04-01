// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <atomic>
#include <map>
#include <memory>
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
	void SendAsync(std::unique_ptr<sf::Packet> packet);

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

	bool IsFirstInGamePad(u8 ingame_pad) const;
	u8 NumLocalPads() const;

	u8 InGamePadToLocalPad(u8 ingame_pad);
	u8 LocalPadToInGamePad(u8 localPad);

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

	std::array<Common::FifoQueue<GCPadStatus>, 4> m_pad_buffer;
	std::array<Common::FifoQueue<NetWiimote >, 4> m_wiimote_buffer;

	NetPlayUI*   m_dialog = nullptr;

	ENetHost*    m_client = nullptr;
	ENetPeer*    m_server = nullptr;
	std::thread  m_thread;

	std::string       m_selected_game;
	std::atomic<bool> m_is_running{false};
	std::atomic<bool> m_do_loop{true};

	unsigned int m_target_buffer_size = 20;

	Player* m_local_player = nullptr;

	u32 m_current_game = 0;

	PadMappingArray m_pad_map;
	PadMappingArray m_wiimote_map;

	bool m_is_recording = false;

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

	bool LocalPlayerHasControllerMapped() const;

	void SendStartGamePacket();
	void SendStopGamePacket();

	void UpdateDevices();
	void SendPadState(const PadMapping in_game_pad, const GCPadStatus& np);
	void SendWiimoteState(const PadMapping in_game_pad, const NetWiimote& nw);
	unsigned int OnData(sf::Packet& packet);
	void Send(sf::Packet& packet);
	void Disconnect();
	bool Connect();

	bool m_is_connected = false;
	ConnectionState m_connection_state = ConnectionState::Failure;

	PlayerId m_pid = 0;
	std::map<PlayerId, Player> m_players;
	std::string m_host_spec;
	std::string m_player_name;
	bool m_connecting = false;
	TraversalClient* m_traversal_client = nullptr;

	u32 m_timebase_frame = 0;
};

void NetPlay_Enable(NetPlayClient* const np);
void NetPlay_Disable();
