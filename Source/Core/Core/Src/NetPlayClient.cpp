// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "NetPlayClient.h"

// for wiimote
#include "IPC_HLE/WII_IPC_HLE_Device_usb.h"
#include "IPC_HLE/WII_IPC_HLE_WiiMote.h"
// for gcpad
#include "HW/SI_DeviceGCController.h"
#include "HW/SI_DeviceGCSteeringWheel.h"
#include "HW/SI_DeviceDanceMat.h"
// for gctime
#include "HW/EXI_DeviceIPL.h"
// for wiimote/ OSD messages
#include "Core.h"
#include "ConfigManager.h"

std::mutex crit_netplay_client;
static NetPlayClient * netplay_client = NULL;
NetSettings g_NetPlaySettings;

#define RPT_SIZE_HACK	(1 << 16)

NetPlayClient::Player::Player()
{
	memset(pad_map, -1, sizeof(pad_map));
}

// called from ---GUI--- thread
std::string NetPlayClient::Player::ToString() const
{
	std::ostringstream ss;
	ss << name << '[' << (char)(pid+'0') << "] : " << revision << " |";
	for (unsigned int i=0; i<4; ++i)
		ss << (pad_map[i]>=0 ? (char)(pad_map[i]+'1') : '-');
	ss << " | " << ping << "ms";
	return ss.str();
}

NetPad::NetPad()
{
	nHi = 0x00808080;
	nLo = 0x80800000;
}

NetPad::NetPad(const SPADStatus* const pad_status)
{
	nHi = (u32)((u8)pad_status->stickY);
	nHi |= (u32)((u8)pad_status->stickX << 8);
	nHi |= (u32)((u16)pad_status->button << 16);
	nHi |= 0x00800000;
	nLo = (u8)pad_status->triggerRight;
	nLo |= (u32)((u8)pad_status->triggerLeft << 8);
	nLo |= (u32)((u8)pad_status->substickY << 16);
	nLo |= (u32)((u8)pad_status->substickX << 24);
}

// called from ---GUI--- thread
NetPlayClient::~NetPlayClient()
{
	// not perfect
	if (m_is_running)
		StopGame();

	if (is_connected)
	{
		m_do_loop = false;
		m_thread.join();
	}
}

// called from ---GUI--- thread
NetPlayClient::NetPlayClient(const std::string& address, const u16 port, NetPlayUI* dialog, const std::string& name) : m_dialog(dialog), m_is_running(false), m_do_loop(true)
{
	m_target_buffer_size = 20;
	ClearBuffers();

	is_connected = false;

	// why is false successful? documentation says true is
	if (0 == m_socket.Connect(port, address, 5))
	{
		// send connect message
		sf::Packet spac;
		spac << NETPLAY_VERSION;
		spac << netplay_dolphin_ver;
		spac << name;
		m_socket.Send(spac);

		sf::Packet rpac;
		// TODO: make this not hang
		m_socket.Receive(rpac);
		MessageId error;
		rpac >> error;

		// got error message
		if (error)
		{
			switch (error)
			{
			case CON_ERR_SERVER_FULL :
				PanicAlertT("The server is full!");
				break;
			case CON_ERR_VERSION_MISMATCH :
				PanicAlertT("The server and client's NetPlay versions are incompatible!");
				break;
			case CON_ERR_GAME_RUNNING :
				PanicAlertT("The server responded: the game is currently running!");
				break;
			default :
				PanicAlertT("The server sent an unknown error message!");
				break;
			}
			m_socket.Close();
		}
		else
		{
			rpac >> m_pid;

			Player player;
			player.name = name;
			player.pid = m_pid;
			player.revision = netplay_dolphin_ver;

			// add self to player list
			m_players[m_pid] = player;
			m_local_player = &m_players[m_pid];

			m_dialog->Update();

			//PanicAlertT("Connection successful: assigned player id: %d", m_pid);
			is_connected = true;

			m_selector.Add(m_socket);
			m_thread = std::thread(std::mem_fun(&NetPlayClient::ThreadFunc), this);
		}
	}
	else
		PanicAlertT("Failed to Connect!");

}

// called from ---NETPLAY--- thread
unsigned int NetPlayClient::OnData(sf::Packet& packet)
{
	MessageId mid;
	packet >> mid;

	switch (mid)
	{
	case NP_MSG_PLAYER_JOIN :
		{
			Player player;
			packet >> player.pid;
			packet >> player.name;
			packet >> player.revision;

			{
			std::lock_guard<std::recursive_mutex> lkp(m_crit.players);
			m_players[player.pid] = player;
			}

			m_dialog->Update();
		}
		break;

	case NP_MSG_PLAYER_LEAVE :
		{
			PlayerId pid;
			packet >> pid;

			{
			std::lock_guard<std::recursive_mutex> lkp(m_crit.players);
			m_players.erase(m_players.find(pid));
			}

			m_dialog->Update();
		}
		break;

	case NP_MSG_CHAT_MESSAGE :
		{
			PlayerId pid;
			packet >> pid;
			std::string msg;
			packet >> msg;

			// don't need lock to read in this thread
			const Player& player = m_players[pid];

			// add to gui
			std::ostringstream ss;
			ss << player.name << '[' << (char)(pid+'0') << "]: " << msg;

			m_dialog->AppendChat(ss.str());
		}
		break;

	case NP_MSG_PAD_MAPPING :
		{
			PlayerId pid;
			packet >> pid;

			{
			std::lock_guard<std::recursive_mutex> lkp(m_crit.players);
			Player& player = m_players[pid];

			for (unsigned int i=0; i<4; ++i)
				packet >> player.pad_map[i];
			}

			m_dialog->Update();
		}
		break;

	case NP_MSG_PAD_DATA :
		{
			PadMapping map = 0;
			NetPad np;
			packet >> map >> np.nHi >> np.nLo;

			// trusting server for good map value (>=0 && <4)
			// add to pad buffer
			m_pad_buffer[(unsigned)map].Push(np);
		}
		break;

	case NP_MSG_PAD_BUFFER :
		{
			u32 size = 0;
			packet >> size;

			m_target_buffer_size = size;
		}
		break;

	case NP_MSG_CHANGE_GAME :
		{
			{
			std::lock_guard<std::recursive_mutex> lkg(m_crit.game);
			packet >> m_selected_game;
			}

			// update gui
			m_dialog->OnMsgChangeGame(m_selected_game);
		}
		break;

	case NP_MSG_START_GAME :
		{
			{
			std::lock_guard<std::recursive_mutex> lkg(m_crit.game);
			packet >> m_current_game;
			packet >> g_NetPlaySettings.m_CPUthread;
			packet >> g_NetPlaySettings.m_DSPEnableJIT;
			packet >> g_NetPlaySettings.m_DSPHLE;
			packet >> g_NetPlaySettings.m_WriteToMemcard;
			for (unsigned int i = 0; i < 4; ++i)
				packet >> g_NetPlaySettings.m_Controllers[i];
			}

			m_dialog->OnMsgStartGame();
		}
		break;

	case NP_MSG_STOP_GAME :
		{
			m_dialog->OnMsgStopGame();
		}
		break;

	case NP_MSG_DISABLE_GAME :
		{
			PanicAlertT("Other client disconnected while game is running!! NetPlay is disabled. You manually stop the game.");
			std::lock_guard<std::recursive_mutex> lkg(m_crit.game);
			m_is_running = false;
			NetPlay_Disable();
		}
		break;

	case NP_MSG_PING :
		{
			u32 ping_key = 0;
			packet >> ping_key;

			sf::Packet spac;
			spac << (MessageId)NP_MSG_PONG;
			spac << ping_key;

			std::lock_guard<std::recursive_mutex> lks(m_crit.send);
			m_socket.Send(spac);
		}
		break;

	case NP_MSG_PLAYER_PING_DATA:
		{
			PlayerId pid;
			packet >> pid;

			{
			std::lock_guard<std::recursive_mutex> lkp(m_crit.players);
			Player& player = m_players[pid];
			packet >> player.ping;
			}

			m_dialog->Update();
		}
		break;

	default :
		PanicAlertT("Unknown message received with id : %d", mid);
		break;

	}

	return 0;
}

// called from ---NETPLAY--- thread
void NetPlayClient::ThreadFunc()
{
	while (m_do_loop)
	{
		if (m_selector.Wait(0.01f))
		{
			sf::Packet rpac;
			switch (m_socket.Receive(rpac))
			{
			case sf::Socket::Done :
				OnData(rpac);
				break;

			//case sf::Socket::Disconnected :
			default :
				m_is_running = false;
				NetPlay_Disable();
				m_dialog->AppendChat("< LOST CONNECTION TO SERVER >");
				PanicAlertT("Lost connection to server!");
				m_do_loop = false;
				break;
			}
		}
	}

	m_socket.Close();

	return;
}

// called from ---GUI--- thread
void NetPlayClient::GetPlayerList(std::string& list, std::vector<int>& pid_list)
{
	std::lock_guard<std::recursive_mutex> lkp(m_crit.players);

	std::ostringstream ss;

	std::map<PlayerId, Player>::const_iterator
		i = m_players.begin(),
		e = m_players.end();
	for ( ; i!=e; ++i)
	{
		ss << i->second.ToString() << '\n';
		pid_list.push_back(i->second.pid);
	}

	list = ss.str();
}


// called from ---GUI--- thread
void NetPlayClient::SendChatMessage(const std::string& msg)
{
	sf::Packet spac;
	spac << (MessageId)NP_MSG_CHAT_MESSAGE;
	spac << msg;

	std::lock_guard<std::recursive_mutex> lks(m_crit.send);
	m_socket.Send(spac);
}

// called from ---CPU--- thread
void NetPlayClient::SendPadState(const PadMapping local_nb, const NetPad& np)
{
	// send to server
	sf::Packet spac;
	spac << (MessageId)NP_MSG_PAD_DATA;
	spac << local_nb;	// local pad num
	spac << np.nHi << np.nLo;

	std::lock_guard<std::recursive_mutex> lks(m_crit.send);
	m_socket.Send(spac);
}

// called from ---GUI--- thread
bool NetPlayClient::StartGame(const std::string &path)
{
	std::lock_guard<std::recursive_mutex> lkg(m_crit.game);

	// tell server i started the game
	sf::Packet spac;
	spac << (MessageId)NP_MSG_START_GAME;
	spac << m_current_game;
	spac << (char *)&g_NetPlaySettings;

	std::lock_guard<std::recursive_mutex> lks(m_crit.send);
	m_socket.Send(spac);

	if (m_is_running)
	{
		PanicAlertT("Game is already running!");
		return false;
	}

	m_dialog->AppendChat(" -- STARTING GAME -- ");

	m_is_running = true;
	NetPlay_Enable(this);

	ClearBuffers();

	// boot game
	m_dialog->BootGame(path);

	// temporary
	NetWiimote nw;
	for (unsigned int i = 0; i<4; ++i)
		for (unsigned int f = 0; f<2; ++f)
			m_wiimote_buffer[i].Push(nw);

	return true;
}

// called from ---GUI--- thread
bool NetPlayClient::ChangeGame(const std::string&)
{
	return true;
}

// called from ---NETPLAY--- thread
void NetPlayClient::ClearBuffers()
{
	// clear pad buffers, Clear method isn't thread safe
	for (unsigned int i=0; i<4; ++i)
	{
		while (m_pad_buffer[i].Size())
			m_pad_buffer[i].Pop();

		while (m_wiimote_buffer[i].Size())
			m_wiimote_buffer[i].Pop();

		m_wiimote_input[i].clear();
	}
}

// called from ---CPU--- thread
bool NetPlayClient::GetNetPads(const u8 pad_nb, const SPADStatus* const pad_status, NetPad* const netvalues)
{
	{
	std::lock_guard<std::recursive_mutex> lkp(m_crit.players);

	// in game mapping for this local pad
	unsigned int in_game_num = m_local_player->pad_map[pad_nb];

	// does this local pad map in game?
	if (in_game_num < 4)
	{
		NetPad np(pad_status);

		// adjust the buffer either up or down
		// inserting multiple padstates or dropping states
		while (m_pad_buffer[in_game_num].Size() <= m_target_buffer_size)
		{
			// add to buffer
			m_pad_buffer[in_game_num].Push(np);

			// send
			SendPadState(pad_nb, np);
		}
	}

	}	// unlock players

	//Common::Timer bufftimer;
	//bufftimer.Start();

	// get padstate from buffer and send to game
	while (!m_pad_buffer[pad_nb].Pop(*netvalues))
	{
		// wait for receiving thread to push some data
		Common::SleepCurrentThread(1);

		if (false == m_is_running)
			return false;

		// TODO: check the time of bufftimer here,
		// if it gets pretty high, ask the user if they want to disconnect

	}

	//u64 hangtime = bufftimer.GetTimeElapsed();
	//if (hangtime > 10)
	//{
	//	std::ostringstream ss;
	//	ss << "Pad " << (int)pad_nb << ": Had to wait " << hangtime << "ms for pad data. (increase pad Buffer maybe)";
	//	Core::DisplayMessage(ss.str(), 1000);
	//}

	return true;
}

// called from ---CPU--- thread
void NetPlayClient::WiimoteInput(int _number, u16 _channelID, const void* _pData, u32 _Size)
{
	//// in game mapping for this local wiimote
	unsigned int in_game_num = m_local_player->pad_map[_number];	// just using gc pad_map for now

	// does this local pad map in game?
	if (in_game_num < 4)
	{		
		m_wiimote_input[_number].resize(m_wiimote_input[_number].size() + 1);
		m_wiimote_input[_number].back().assign((char*)_pData, (char*)_pData + _Size);
		m_wiimote_input[_number].back().channel = _channelID;
	}
}

// called from ---CPU--- thread
void NetPlayClient::WiimoteUpdate(int _number)
{
	{
	std::lock_guard<std::recursive_mutex> lkp(m_crit.players);

	// in game mapping for this local wiimote
	unsigned int in_game_num = m_local_player->pad_map[_number];	// just using gc pad_map for now

	// does this local pad map in game?
	if (in_game_num < 4)
	{
		m_wiimote_buffer[in_game_num].Push(m_wiimote_input[_number]);

		// TODO: send it

		m_wiimote_input[_number].clear();
	}

	} // unlock players

	if (0 == m_wiimote_buffer[_number].Size())
	{
		//PanicAlert("PANIC");
		return;
	}

	NetWiimote nw;
	m_wiimote_buffer[_number].Pop(nw);

	NetWiimote::const_iterator
		i = nw.begin(), e = nw.end();
	for ( ; i!=e; ++i)
		Core::Callback_WiimoteInterruptChannel(_number, i->channel, &(*i)[0], (u32)i->size() + RPT_SIZE_HACK);
}

// called from ---GUI--- thread and ---NETPLAY--- thread (client side)
bool NetPlayClient::StopGame()
{
	std::lock_guard<std::recursive_mutex> lkg(m_crit.game);

	if (false == m_is_running)
	{
		PanicAlertT("Game isn't running!");
		return false;
	}

	m_dialog->AppendChat(" -- STOPPING GAME -- ");

	m_is_running = false;
	NetPlay_Disable();

	// stop game
	m_dialog->StopGame();

	return true;
}

// called from ---CPU--- thread
u8 NetPlayClient::GetPadNum(u8 numPAD)
{
	// TODO: i don't like that this loop is running everytime there is rumble
	unsigned int i = 0;
	for (; i<4; ++i)
		if (numPAD == m_local_player->pad_map[i])
			break;

	return i;
}

// stuff hacked into dolphin

// called from ---CPU--- thread
// Actual Core function which is called on every frame
bool CSIDevice_GCController::NetPlay_GetInput(u8 numPAD, SPADStatus PadStatus, u32 *PADStatus)
{
	std::lock_guard<std::mutex> lk(crit_netplay_client);

	if (netplay_client)
		return netplay_client->GetNetPads(numPAD, &PadStatus, (NetPad*)PADStatus);
	else
		return false;
}

bool CSIDevice_GCSteeringWheel::NetPlay_GetInput(u8 numPAD, SPADStatus PadStatus, u32 *PADStatus)
{
	return CSIDevice_GCController::NetPlay_GetInput(numPAD, PadStatus, PADStatus);
}

bool CSIDevice_DanceMat::NetPlay_GetInput(u8 numPAD, SPADStatus PadStatus, u32 *PADStatus)
{
	return CSIDevice_GCController::NetPlay_GetInput(numPAD, PadStatus, PADStatus);
}

// called from ---CPU--- thread
// so all players' games get the same time
u32 CEXIIPL::NetPlay_GetGCTime()
{
	std::lock_guard<std::mutex> lk(crit_netplay_client);

	if (netplay_client)
		return 1272737767;	// watev
	else
		return 0;
}

// called from ---CPU--- thread
// return the local pad num that should rumble given a ingame pad num
u8 CSIDevice_GCController::NetPlay_GetPadNum(u8 numPAD)
{
	std::lock_guard<std::mutex> lk(crit_netplay_client);

	if (netplay_client)
		return netplay_client->GetPadNum(numPAD);
	else
		return numPAD;
}

u8 CSIDevice_GCSteeringWheel::NetPlay_GetPadNum(u8 numPAD)
{
	return CSIDevice_GCController::NetPlay_GetPadNum(numPAD);
}

u8 CSIDevice_DanceMat::NetPlay_GetPadNum(u8 numPAD)
{
	return CSIDevice_GCController::NetPlay_GetPadNum(numPAD);
}

// called from ---CPU--- thread
// wiimote update / used for frame counting
//void CWII_IPC_HLE_Device_usb_oh1_57e_305::NetPlay_WiimoteUpdate(int _number)
void CWII_IPC_HLE_Device_usb_oh1_57e_305::NetPlay_WiimoteUpdate(int)
{
	//CritLocker crit(crit_netplay_client);

	//if (netplay_client)
	//	netplay_client->WiimoteUpdate(_number);
}

// called from ---CPU--- thread
//
int CWII_IPC_HLE_WiiMote::NetPlay_GetWiimoteNum(int _number)
{
	//CritLocker crit(crit_netplay_client);

	//if (netplay_client)
	//	return netplay_client->GetPadNum(_number);		// just using gcpad mapping for now
	//else
		return _number;
}

// called from ---CPU--- thread
// intercept wiimote input callback
//bool CWII_IPC_HLE_WiiMote::NetPlay_WiimoteInput(int _number, u16 _channelID, const void* _pData, u32& _Size)
bool CWII_IPC_HLE_WiiMote::NetPlay_WiimoteInput(int, u16, const void*, u32&)
{
	std::lock_guard<std::mutex> lk(crit_netplay_client);

	if (netplay_client)
	//{
	//	if (_Size >= RPT_SIZE_HACK)
	//	{
	//		_Size -= RPT_SIZE_HACK;
	//		return false;
	//	}
	//	else
	//	{
	//		netplay_client->WiimoteInput(_number, _channelID, _pData, _Size);
	//		// don't use this packet
			return true;
	//	}
	//}
	else
		return false;
}

bool NetPlay::IsNetPlayRunning()
{
	return netplay_client != NULL;
}

void NetPlay_Enable(NetPlayClient* const np)
{
	std::lock_guard<std::mutex> lk(crit_netplay_client);
	netplay_client = np;
}

void NetPlay_Disable()
{
	std::lock_guard<std::mutex> lk(crit_netplay_client);
	netplay_client = NULL;
}
