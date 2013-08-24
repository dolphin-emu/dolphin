// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _NETPLAY_H
#define _NETPLAY_H

#include "Common.h"
#include "CommonTypes.h"
#include "Thread.h"
#include "Timer.h"

#include <SFML/Network.hpp>

#include "NetPlayProto.h"
#include "GCPadStatus.h"

#include <functional>
#include <map>
#include <queue>
#include <sstream>

#include "FifoQueue.h"

class NetPad
{
public:
	NetPad();
	NetPad(const SPADStatus* const);

	u32 nHi;
	u32 nLo;
};

class NetPlayUI
{
public:
	virtual ~NetPlayUI() {};

	virtual void BootGame(const std::string& filename) = 0;
	virtual void StopGame() = 0;

	virtual void Update() = 0;
	virtual void AppendChat(const std::string& msg) = 0;

	virtual void OnMsgChangeGame(const std::string& filename) = 0;
	virtual void OnMsgStartGame() = 0;
	virtual void OnMsgStopGame() = 0;
};

extern NetSettings g_NetPlaySettings;

class Player
{
 public:
	PlayerId		pid;
	std::string		name;
	std::string		revision;
	u32                     ping;
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
	bool ChangeGame(const std::string& game);
	void SendChatMessage(const std::string& msg);

	// Send and receive pads values
	void WiimoteInput(int _number, u16 _channelID, const void* _pData, u32 _Size);
	void WiimoteUpdate(int _number);
	bool GetNetPads(const u8 pad_nb, const SPADStatus* const, NetPad* const netvalues);

	u8 GetPadNum(u8 numPAD);

protected:
	void ClearBuffers();

	struct
	{
		std::recursive_mutex game;
		// lock order
		std::recursive_mutex players, send;
	} m_crit;

	Common::FifoQueue<NetPad>		m_pad_buffer[4];
	Common::FifoQueue<NetWiimote>	m_wiimote_buffer[4];

	NetWiimote		m_wiimote_input[4];

	NetPlayUI*		m_dialog;
	sf::SocketTCP	m_socket;
	std::thread		m_thread;
	sf::Selector<sf::SocketTCP>		m_selector;

	std::string		m_selected_game;
	volatile bool	m_is_running;
	volatile bool	m_do_loop;

	unsigned int	m_target_buffer_size;

	Player*		m_local_player;

	u32		m_current_game;

	PadMapping	m_pad_map[4];

private:
	void UpdateDevices();
	void SendPadState(const PadMapping in_game_pad, const NetPad& np);
	unsigned int OnData(sf::Packet& packet);

	PlayerId		m_pid;
	std::map<PlayerId, Player>	m_players;
};

namespace NetPlay {
	bool IsNetPlayRunning();
};

void NetPlay_Enable(NetPlayClient* const np);
void NetPlay_Disable();

#endif
