#ifndef _NETPLAY_H
#define _NETPLAY_H

#include "Common.h"
#include "CommonTypes.h"
//#define WIN32_LEAN_AND_MEAN
#include "Thread.h"

// hax, i hope something like this isn't needed on non-windows
#define _WINSOCK2API_
#include <SFML/Network.hpp>

#include "pluginspecs_pad.h"
#include "svnrev.h"

//#include <wx/wx.h>

#include <map>
#include <queue>

class NetPlayDiag;

class NetPad
{
public:
	NetPad();
	NetPad(const SPADStatus* const);

	u32 nHi;
	u32 nLo;
};

#define NETPLAY_VERSION		"Dolphin NetPlay 2.0"

#ifdef _M_X64
	#define	NP_ARCH "x64"
#else
	#define	NP_ARCH "x86"
#endif

#ifdef _WIN32
	#define NETPLAY_DOLPHIN_VER		SVN_REV_STR" win-"NP_ARCH
#elif __APPLE__
	#define NETPLAY_DOLPHIN_VER		SVN_REV_STR" osx-"NP_ARCH
#else
	#define NETPLAY_DOLPHIN_VER		SVN_REV_STR" nix-"NP_ARCH
#endif

// messages
#define NP_MSG_PLAYER_JOIN		0x10
#define NP_MSG_PLAYER_LEAVE		0x11

#define NP_MSG_CHAT_MESSAGE		0x30

#define NP_MSG_PAD_DATA			0x60
#define NP_MSG_PAD_MAPPING		0x61
#define NP_MSG_PAD_BUFFER		0x62

#define NP_MSG_START_GAME		0xA0
#define NP_MSG_CHANGE_GAME		0xA1

#define NP_MSG_READY			0xD0
#define NP_MSG_NOT_READY		0xD1

#define NP_MSG_PING				0xE0
#define NP_MSG_PONG				0xE1
// end messages

// gui messages
#define NP_GUI_UPDATE			0x10
// end messages

typedef u8	MessageId;
typedef u8	PlayerId;
typedef s8	PadMapping;

class NetPlay
{
public:
	NetPlay() : m_is_running(false), m_do_loop(true) {}
	virtual ~NetPlay();
	virtual void Entry() = 0;

	bool	is_connected;
	
	// Send and receive pads values
	virtual bool GetNetPads(const u8 pad_nb, const SPADStatus* const, NetPad* const netvalues) = 0;
	virtual bool SetSelectedGame(const std::string& game) = 0;
	virtual void GetPlayerList(std::string& list) = 0;
	virtual void SendChatMessage(const std::string& msg) = 0;
	virtual bool StartGame(const std::string &path) = 0;

protected:
	//NetPlay(Common::ThreadFunc entry, void* arg) : m_thread(entry, arg) {}

	void UpdateGUI();

	struct
	{
		Common::CriticalSection	send, players, buffer, other;
	} m_crit;

	//LockingQueue<NetPad>	m_pad_buffer[4];
	std::queue<NetPad>	m_pad_buffer[4];

	NetPlayDiag*	m_dialog;
	sf::SocketTCP	m_socket;
	Common::Thread*	m_thread;
	sf::Selector<sf::SocketTCP>		m_selector;

	std::string		m_selected_game;
	bool	m_is_running;
	volatile bool	m_do_loop;

private:

};

class NetPlayServer : public NetPlay
{
public:
	void Entry();

	NetPlayServer(const u16 port, const std::string& name, NetPlayDiag* const npd = NULL, const std::string& game = "");
	~NetPlayServer();

	void GetPlayerList(std::string& list);

	// Send and receive pads values
	bool GetNetPads(const u8 pad_nb, const SPADStatus* const, NetPad* const netvalues);
	bool SetSelectedGame(const std::string& game);
	void SendChatMessage(const std::string& msg);
	bool StartGame(const std::string &path);

private:
	class Player
	{
	public:
		Player();

		PlayerId		pid;
		sf::SocketTCP	socket;
		std::string		name;
		PadMapping		pad_map[4];
		std::string		revision;
	};

	void SendToClients(sf::Packet& packet, const PlayerId skip_pid = 0);
	unsigned int OnConnect(sf::SocketTCP& socket);
	unsigned int OnDisconnect(sf::SocketTCP& socket);
	unsigned int OnData(sf::Packet& packet, sf::SocketTCP& socket);

	std::map<sf::SocketTCP, Player>	m_players;
};

class NetPlayClient : public NetPlay
{
public:
	void Entry();

	NetPlayClient(const std::string& address, const u16 port, const std::string& name, NetPlayDiag* const npd = NULL);
	~NetPlayClient();

	void GetPlayerList(std::string& list);

	// Send and receive pads values
	bool GetNetPads(const u8 pad_nb, const SPADStatus* const, NetPad* const netvalues);
	bool SetSelectedGame(const std::string& game);
	void SendChatMessage(const std::string& msg);
	bool StartGame(const std::string &path);

private:
	class Player
	{
	public:
		Player();

		PlayerId		pid;
		std::string		name;
		PadMapping		pad_map[4];
		std::string		revision;
	};

	unsigned int OnData(sf::Packet& packet);

	PlayerId		m_pid;
	std::map<PlayerId, Player>	m_players;
};

#endif
