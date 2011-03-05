#ifndef _NETPLAY_H
#define _NETPLAY_H

#include "Common.h"
#include "CommonTypes.h"
#include "Thread.h"
#include "Timer.h"

#include <SFML/Network.hpp>

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

struct Rpt : public std::vector<u8>
{
	u16		channel;
};

typedef std::vector<Rpt>	NetWiimote;

#define NETPLAY_VERSION		"Dolphin NetPlay r6423"

// messages
enum
{
	NP_MSG_PLAYER_JOIN		= 0x10,
	NP_MSG_PLAYER_LEAVE		= 0x11,

	NP_MSG_CHAT_MESSAGE		= 0x30,

	NP_MSG_PAD_DATA			= 0x60,
	NP_MSG_PAD_MAPPING		= 0x61,
	NP_MSG_PAD_BUFFER		= 0x62,

	NP_MSG_WIIMOTE_DATA		= 0x70,
	NP_MSG_WIIMOTE_MAPPING	= 0x71,	// just using pad mapping for now

	NP_MSG_START_GAME		= 0xA0,
	NP_MSG_CHANGE_GAME		= 0xA1,
	NP_MSG_STOP_GAME		= 0xA2,
	NP_MSG_DISABLE_GAME		= 0xA3,

	NP_MSG_READY			= 0xD0,
	NP_MSG_NOT_READY		= 0xD1,

	NP_MSG_PING				= 0xE0,
	NP_MSG_PONG				= 0xE1,
};

typedef u8	MessageId;
typedef u8	PlayerId;
typedef s8	PadMapping;
typedef u32	FrameNum;

enum
{
	CON_ERR_SERVER_FULL = 1,
	CON_ERR_GAME_RUNNING,
	CON_ERR_VERSION_MISMATCH	
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

class NetPlay
{
public:
	NetPlay(NetPlayUI* _dialog);
	virtual ~NetPlay();
	//virtual void ThreadFunc() = 0;

	bool is_connected;
	
	// Send and receive pads values
	void WiimoteInput(int _number, u16 _channelID, const void* _pData, u32 _Size);
	void WiimoteUpdate(int _number);
	bool GetNetPads(const u8 pad_nb, const SPADStatus* const, NetPad* const netvalues);
	virtual bool ChangeGame(const std::string& game) = 0;
	virtual void GetPlayerList(std::string& list, std::vector<int>& pid_list) = 0;
	virtual void SendChatMessage(const std::string& msg) = 0;

	virtual bool StartGame(const std::string &path);
	virtual bool StopGame();

	//void PushPadStates(unsigned int count);

	u8 GetPadNum(u8 numPAD);

protected:
	//void GetBufferedPad(const u8 pad_nb, NetPad* const netvalues);
	void ClearBuffers();
	virtual void SendPadState(const PadMapping local_nb, const NetPad& np) = 0;

	struct
	{
		std::recursive_mutex game;
		// lock order
		std::recursive_mutex players, send;
	} m_crit;

	class Player
	{
	public:
		Player();
		std::string ToString() const;

		PlayerId		pid;
		std::string		name;
		PadMapping		pad_map[4];
		std::string		revision;
	};

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
};

void NetPlay_Enable(NetPlay* const np);
void NetPlay_Disable();

class NetPlayServer : public NetPlay
{
public:
	void ThreadFunc();

	NetPlayServer(const u16 port, const std::string& name, NetPlayUI* dialog, const std::string& game = "");
	~NetPlayServer();

	void GetPlayerList(std::string& list, std::vector<int>& pid_list);

	// Send and receive pads values
	//bool GetNetPads(const u8 pad_nb, const SPADStatus* const, NetPad* const netvalues);
	bool ChangeGame(const std::string& game);
	void SendChatMessage(const std::string& msg);

	bool StartGame(const std::string &path);
	bool StopGame();

	bool GetPadMapping(const int pid, int map[]);
	bool SetPadMapping(const int pid, const int map[]);

	u64 CalculateMinimumBufferTime();
	void AdjustPadBufferSize(unsigned int size);

private:
	class Client : public Player
	{
	public:
		Client() : ping(0), current_game(0) {}

		sf::SocketTCP	socket;
		u64				ping;	
		u32				current_game;
	};

	void SendPadState(const PadMapping local_nb, const NetPad& np);
	void SendToClients(sf::Packet& packet, const PlayerId skip_pid = 0);
	unsigned int OnConnect(sf::SocketTCP& socket);
	unsigned int OnDisconnect(sf::SocketTCP& socket);
	unsigned int OnData(sf::Packet& packet, sf::SocketTCP& socket);
	void UpdatePadMapping();

	std::map<sf::SocketTCP, Client>	m_players;

	Common::Timer	m_ping_timer;
	u32		m_ping_key;
	bool	m_update_pings;
};

class NetPlayClient : public NetPlay
{
public:
	void ThreadFunc();

	NetPlayClient(const std::string& address, const u16 port, NetPlayUI* dialog, const std::string& name);
	~NetPlayClient();

	void GetPlayerList(std::string& list, std::vector<int>& pid_list);

	// Send and receive pads values
	//bool GetNetPads(const u8 pad_nb, const SPADStatus* const, NetPad* const netvalues);
	bool StartGame(const std::string &path);
	bool ChangeGame(const std::string& game);
	void SendChatMessage(const std::string& msg);

private:
	void SendPadState(const PadMapping local_nb, const NetPad& np);
	unsigned int OnData(sf::Packet& packet);

	PlayerId		m_pid;
	std::map<PlayerId, Player>	m_players;
};

#endif
