// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "NetPlayClient.h"

// for wiimote
#include "IPC_HLE/WII_IPC_HLE_Device_usb.h"
#include "IPC_HLE/WII_IPC_HLE_WiiMote.h"
// for gcpad
#include "HW/SI.h"
#include "HW/SI_DeviceGCController.h"
#include "HW/SI_DeviceGCSteeringWheel.h"
#include "HW/SI_DeviceDanceMat.h"
// for gctime
#include "HW/EXI_DeviceIPL.h"
// for wiimote/ OSD messages
#include "Core.h"
#include "ConfigManager.h"
#include "Movie.h"

std::mutex crit_netplay_client;
static NetPlayClient * netplay_client = NULL;
NetSettings g_NetPlaySettings;

#define RPT_SIZE_HACK	(1 << 16)

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
			for (PadMapping i = 0; i < 4; i++)
				packet >> m_pad_map[i];

			UpdateDevices();

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
			m_pad_buffer[map].Push(np);
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
			int tmp;
			packet >> tmp;
			g_NetPlaySettings.m_EXIDevice[0] = (TEXIDevices) tmp;
			packet >> tmp;
			g_NetPlaySettings.m_EXIDevice[1] = (TEXIDevices) tmp;
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
		const Player *player = &(i->second);
		ss << player->name << "[" << (int)player->pid << "] : " << player->revision << " | ";
		for (unsigned int j = 0; j < 4; j++)
		{
			if (m_pad_map[j] == player->pid)
				ss << j + 1;
			else
				ss << '-';
		}
		ss << " | " << player->ping << "ms\n";
		pid_list.push_back(player->pid);
	}

	list = ss.str();
}

// called from ---GUI--- thread
void NetPlayClient::GetPlayers(std::vector<const Player *> &player_list)
{
	std::lock_guard<std::recursive_mutex> lkp(m_crit.players);
	std::map<PlayerId, Player>::const_iterator
		i = m_players.begin(),
		e = m_players.end();
	for ( ; i!=e; ++i)
	{
		const Player *player = &(i->second);
		player_list.push_back(player);
	}
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
void NetPlayClient::SendPadState(const PadMapping in_game_pad, const NetPad& np)
{
	// send to server
	sf::Packet spac;
	spac << (MessageId)NP_MSG_PAD_DATA;
	spac << in_game_pad;
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

	if (m_dialog->IsRecording())
	{

		if (Movie::IsReadOnly())
			Movie::SetReadOnly(false);

		u8 controllers_mask = 0;
		for (unsigned int i = 0; i < 4; ++i)
		{
			if (m_pad_map[i] > 0)
				controllers_mask |= (1 << i);
		}
		Movie::BeginRecordingInput(controllers_mask);
	}

	// boot game
	m_dialog->BootGame(path);

	UpdateDevices();

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
void NetPlayClient::UpdateDevices()
{
	for (PadMapping i = 0; i < 4; i++)
	{
		// XXX: add support for other device types? does it matter?
		SerialInterface::AddDevice(m_pad_map[i] > 0 ? SIDEVICE_GC_CONTROLLER : SIDEVICE_NONE, i);
	}
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
	// The interface for this is extremely silly.
	//
	// Imagine a physical device that links three Gamecubes together
	// and emulates NetPlay that way. Which Gamecube controls which
	// in-game controllers can be configured on the device (m_pad_map)
	// but which sockets on each individual Gamecube should be used
	// to control which players? The solution that Dolphin uses is
	// that we hardcode the knowledge that they go in order, so if
	// you have a 3P game with three gamecubes, then every single
	// controller should be plugged into slot 1.
	//
	// If you have a 4P game, then one of the Gamecubes will have
	// a controller plugged into slot 1, and another in slot 2.
	//
	// The slot number is the "local" pad number, and what  player
	// it actually means is the "in-game" pad number.
	//
	// The interface here gives us the status of local pads, and
	// expects to get back "in-game" pad numbers back in response.
	// e.g. it asks "here's the input that slot 1 has, and by the
	// way, what's the state of P1?"
	//
	// We should add this split between "in-game" pads and "local"
	// pads higher up.

	int in_game_num = LocalPadToInGamePad(pad_nb);

	// If this in-game pad is one of ours, then update from the
	// information given.
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
			SendPadState(in_game_num, np);
		}
	}

	// Now, we need to swap out the local value with the values
	// retrieved from NetPlay. This could be the value we pushed
	// above if we're configured as P1 and the code is trying
	// to retrieve data for slot 1.
	while (!m_pad_buffer[pad_nb].Pop(*netvalues))
	{
		if (!m_is_running)
			return false;

		// TODO: use a condition instead of sleeping
		Common::SleepCurrentThread(1);
	}

	SPADStatus tmp;
	tmp.stickY = ((u8*)&netvalues->nHi)[0];
	tmp.stickX = ((u8*)&netvalues->nHi)[1];
	tmp.button = ((u16*)&netvalues->nHi)[1];

	tmp.substickX =  ((u8*)&netvalues->nLo)[3];
	tmp.substickY =  ((u8*)&netvalues->nLo)[2];
	tmp.triggerLeft = ((u8*)&netvalues->nLo)[1];
	tmp.triggerRight = ((u8*)&netvalues->nLo)[0];
	if (Movie::IsRecordingInput())
	{
		Movie::RecordInput(&tmp, pad_nb);
		Movie::InputUpdate();
	}
	else
	{
		Movie::CheckPadStatus(&tmp, pad_nb);
	}

	return true;
}

// called from ---CPU--- thread
void NetPlayClient::WiimoteInput(int _number, u16 _channelID, const void* _pData, u32 _Size)
{
	// XXX
}

// called from ---CPU--- thread
void NetPlayClient::WiimoteUpdate(int _number)
{
	// XXX
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

void NetPlayClient::Stop()
{
	if (m_is_running == false)
		return;
	bool isPadMapped = false;
	for (unsigned int i = 0; i < 4; ++i)
	{
		if (m_pad_map[i] == m_local_player->pid)
			isPadMapped = true;
	}
	// tell the server to stop if we have a pad mapped in game.
	if (isPadMapped)
	{
		sf::Packet spac;
		spac << (MessageId)NP_MSG_STOP_GAME;
		m_socket.Send(spac);
	}
}

u8 NetPlayClient::InGamePadToLocalPad(u8 ingame_pad)
{
	// not our pad
	if (m_pad_map[ingame_pad] != m_local_player->pid)
		return 4;

	int local_pad = 0;
	int pad = 0;

	for (; pad < ingame_pad; pad++)
	{
		if (m_pad_map[pad] == m_local_player->pid)
			local_pad++;
	}

	return local_pad;
}

u8 NetPlayClient::LocalPadToInGamePad(u8 local_pad)
{
	// Figure out which in-game pad maps to which local pad.
	// The logic we have here is that the local slots always
	// go in order.
	int local_pad_count = -1;
	int ingame_pad = 0;
	for (; ingame_pad < 4; ingame_pad++)
	{
		if (m_pad_map[ingame_pad] == m_local_player->pid)
			local_pad_count++;

		if (local_pad_count == local_pad)
			break;
	}

	return ingame_pad;
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
		return NETPLAY_INITIAL_GCTIME;	// watev
	else
		return 0;
}

// called from ---CPU--- thread
// return the local pad num that should rumble given a ingame pad num
u8 CSIDevice_GCController::NetPlay_InGamePadToLocalPad(u8 numPAD)
{
	std::lock_guard<std::mutex> lk(crit_netplay_client);

	if (netplay_client)
		return netplay_client->InGamePadToLocalPad(numPAD);
	else
		return numPAD;
}

u8 CSIDevice_GCSteeringWheel::NetPlay_InGamePadToLocalPad(u8 numPAD)
{
	return CSIDevice_GCController::NetPlay_InGamePadToLocalPad(numPAD);
}

u8 CSIDevice_DanceMat::NetPlay_InGamePadToLocalPad(u8 numPAD)
{
	return CSIDevice_GCController::NetPlay_InGamePadToLocalPad(numPAD);
}

// called from ---CPU--- thread
// wiimote update / used for frame counting
//void CWII_IPC_HLE_Device_usb_oh1_57e_305::NetPlay_WiimoteUpdate(int _number)
void CWII_IPC_HLE_Device_usb_oh1_57e_305::NetPlay_WiimoteUpdate(int)
{
}

// called from ---CPU--- thread
//
int CWII_IPC_HLE_WiiMote::NetPlay_GetWiimoteNum(int _number)
{
	return _number;
}

// called from ---CPU--- thread
// intercept wiimote input callback
//bool CWII_IPC_HLE_WiiMote::NetPlay_WiimoteInput(int _number, u16 _channelID, const void* _pData, u32& _Size)
bool CWII_IPC_HLE_WiiMote::NetPlay_WiimoteInput(int, u16, const void*, u32&)
{
	std::lock_guard<std::mutex> lk(crit_netplay_client);

	if (netplay_client)
		return true;
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
