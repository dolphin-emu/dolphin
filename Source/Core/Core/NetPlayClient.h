// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <queue>
#include <sstream>

#include <SFML/Network.hpp>

#include "Common/CommonTypes.h"
#include "Common/FifoQueue.h"
#include "Common/Thread.h"
#include "Common/Timer.h"

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

class NetPlayClient
{
public:
	void ThreadFunc();

	NetPlayClient(const std::string& address, const u16 port, NetPlayUI* dialog, const std::string& name);
	~NetPlayClient();

	void GetPlayerList(std::string& list, std::vector<int>& pid_list);
	void GetPlayers(std::vector<const Player *>& player_list);

	bool is_connected;

	bool StartGame(const std::string &path);
	bool StopGame();
	void Stop();
	bool ChangeGame(const std::string& game);
	void SendChatMessage(const std::string& msg);

	// Send and receive pads values
	bool WiimoteUpdate(int _number, u8* data, const u8 size);
	bool GetNetPads(const u8 pad_nb, GCPadStatus* pad_status);

	u8 LocalPadToInGamePad(u8 localPad);
	u8 InGamePadToLocalPad(u8 localPad);

	u8 LocalWiimoteToInGameWiimote(u8 local_pad);

protected:
	void ClearBuffers();

	struct
	{
		std::recursive_mutex game;
		// lock order
		std::recursive_mutex players, send;
	} m_crit;

	Common::FifoQueue<GCPadStatus> m_pad_buffer[4];
	Common::FifoQueue<NetWiimote>  m_wiimote_buffer[4];

	NetPlayUI*    m_dialog;
	sf::SocketTCP m_socket;
	std::thread   m_thread;
	sf::Selector<sf::SocketTCP> m_selector;

	std::string   m_selected_game;
	volatile bool m_is_running;
	volatile bool m_do_loop;

	unsigned int  m_target_buffer_size;

	Player* m_local_player;

	u32 m_current_game;

	PadMapping m_pad_map[4];
	PadMapping m_wiimote_map[4];

	bool m_is_recording;

private:
	void UpdateDevices();
	void SendPadState(const PadMapping in_game_pad, const GCPadStatus& np);
	void SendWiimoteState(const PadMapping in_game_pad, const NetWiimote& nw);
	unsigned int OnData(sf::Packet& packet);

	PlayerId m_pid;
	std::map<PlayerId, Player> m_players;
};

void NetPlay_Enable(NetPlayClient* const np);
void NetPlay_Disable();
