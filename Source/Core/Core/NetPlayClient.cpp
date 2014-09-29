// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Movie.h"
#include "Core/NetPlayClient.h"
#include "Core/HW/EXI_DeviceIPL.h"
#include "Core/HW/SI.h"
#include "Core/HW/SI_DeviceDanceMat.h"
#include "Core/HW/SI_DeviceGCController.h"
#include "Core/HW/SI_DeviceGCSteeringWheel.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb.h"
#include "Core/IPC_HLE/WII_IPC_HLE_WiiMote.h"


static std::mutex crit_netplay_client;
static NetPlayClient * netplay_client = nullptr;
NetSettings g_NetPlaySettings;

NetPad::NetPad()
{
	nHi = 0x00808080;
	nLo = 0x80800000;
}

NetPad::NetPad(const GCPadStatus* const pad_status)
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

	if (m_socket.Connect(port, address, 5) == sf::Socket::Done)
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
			m_thread = std::thread(&NetPlayClient::ThreadFunc, this);
		}
	}
	else
	{
		PanicAlertT("Failed to Connect!");
	}
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
			for (PadMapping& mapping : m_pad_map)
			{
				packet >> mapping;
			}

			UpdateDevices();

			m_dialog->Update();
		}
		break;

	case NP_MSG_WIIMOTE_MAPPING :
		{
			for (PadMapping& mapping : m_wiimote_map)
			{
				packet >> mapping;
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
			m_pad_buffer[map].Push(np);
		}
		break;

	case NP_MSG_WIIMOTE_DATA :
		{
			PadMapping map = 0;
			NetWiimote nw;
			u8 size;
			packet >> map >> size;

			nw.resize(size);
			u8* data = new u8[size];
			for (unsigned int i = 0; i < size; ++i)
				packet >> data[i];
			nw.assign(data,data+size);
			delete[] data;

			// trusting server for good map value (>=0 && <4)
			// add to wiimote buffer
			m_wiimote_buffer[(unsigned)map].Push(nw);
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
			packet >> g_NetPlaySettings.m_CPUcore;
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
		for (unsigned int j = 0; j < 4; j++)
		{
			if (m_wiimote_map[j] == player->pid)
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

// called from ---CPU--- thread
void NetPlayClient::SendWiimoteState(const PadMapping in_game_pad, const NetWiimote& nw)
{
	// send to server
	sf::Packet spac;
	spac << (MessageId)NP_MSG_WIIMOTE_DATA;
	spac << in_game_pad;
	spac << (u8)nw.size();
	for (auto it : nw)
	{
		spac << it;
	}

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
			if (m_wiimote_map[i] > 0)
				controllers_mask |= (1 << (i + 4));
		}
		Movie::BeginRecordingInput(controllers_mask);
	}

	// boot game

	m_dialog->BootGame(path);

	UpdateDevices();

	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bWii)
	{
		for (unsigned int i = 0; i < 4; ++i)
			WiimoteReal::ChangeWiimoteSource(i, m_wiimote_map[i] > 0 ? WIIMOTE_SRC_EMU : WIIMOTE_SRC_NONE);

		// Needed to prevent locking up at boot if (when) the wiimotes connect out of order.
		NetWiimote nw;
		nw.resize(4, 0);

		for (unsigned int w = 0; w < 4; ++w)
		{
			if (m_wiimote_map[w] != -1)
				// probably overkill, but whatever
				for (unsigned int i = 0; i < 7; ++i)
					m_wiimote_buffer[w].Push(nw);
		}
	}

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
	}
}

// called from ---CPU--- thread
bool NetPlayClient::GetNetPads(const u8 pad_nb, const GCPadStatus* const pad_status, NetPad* const netvalues)
{
	// The interface for this is extremely silly.
	//
	// Imagine a physical device that links three Gamecubes together
	// and emulates NetPlay that way. Which GameCube controls which
	// in-game controllers can be configured on the device (m_pad_map)
	// but which sockets on each individual GameCube should be used
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

	GCPadStatus tmp;
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
bool NetPlayClient::WiimoteUpdate(int _number, u8* data, const u8 size)
{
	NetWiimote nw;
	static u8 previousSize[4] = {4,4,4,4};
	{
	std::lock_guard<std::recursive_mutex> lkp(m_crit.players);

	// in game mapping for this local wiimote
	unsigned int in_game_num = LocalWiimoteToInGameWiimote(_number);
	// does this local wiimote map in game?
	if (in_game_num < 4)
	{
		if (previousSize[in_game_num] == size)
		{
			nw.assign(data, data + size);
			do
			{
				// add to buffer
				m_wiimote_buffer[in_game_num].Push(nw);

				SendWiimoteState(in_game_num, nw);
			} while (m_wiimote_buffer[in_game_num].Size() <= m_target_buffer_size * 200 / 120); // TODO: add a seperate setting for wiimote buffer?
		}
		else
		{
			while (m_wiimote_buffer[in_game_num].Size() > 0)
			{
				// Reporting mode changed, so previous buffer is no good.
				m_wiimote_buffer[in_game_num].Pop();
			}
			nw.resize(size, 0);

			m_wiimote_buffer[in_game_num].Push(nw);
			m_wiimote_buffer[in_game_num].Push(nw);
			m_wiimote_buffer[in_game_num].Push(nw);
			m_wiimote_buffer[in_game_num].Push(nw);
			m_wiimote_buffer[in_game_num].Push(nw);
			m_wiimote_buffer[in_game_num].Push(nw);
			previousSize[in_game_num] = size;
		}
	}

	} // unlock players

	while (previousSize[_number] == size && !m_wiimote_buffer[_number].Pop(nw))
	{
		// wait for receiving thread to push some data
		Common::SleepCurrentThread(1);
		if (false == m_is_running)
			return false;
	}

	// Use a blank input, since we may not have any valid input.
	if (previousSize[_number] != size)
	{
		nw.resize(size, 0);
		m_wiimote_buffer[_number].Push(nw);
		m_wiimote_buffer[_number].Push(nw);
		m_wiimote_buffer[_number].Push(nw);
		m_wiimote_buffer[_number].Push(nw);
		m_wiimote_buffer[_number].Push(nw);
	}

	// We should have used a blank input last time, so now we just need to pop through the old buffer, until we reach a good input
	if (nw.size() != size)
	{
		u8 tries = 0;
		// Clear the buffer and wait for new input, since we probably just changed reporting mode.
		while (nw.size() != size)
		{
			while (!m_wiimote_buffer[_number].Pop(nw))
			{
				Common::SleepCurrentThread(1);
				if (false == m_is_running)
					return false;
			}
			++tries;
			if (tries > m_target_buffer_size * 200 / 120)
				break;
		}

		// If it still mismatches, it surely desynced
		if (size != nw.size())
		{
			PanicAlert("Netplay has desynced. There is no way to recover from this.");
			return false;
		}
	}

	previousSize[_number] = size;
	memcpy(data, nw.data(), size);
	return true;
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
	for (PadMapping mapping : m_pad_map)
	{
		if (mapping == m_local_player->pid)
		{
			isPadMapped = true;
		}
	}
	for (PadMapping mapping : m_wiimote_map)
	{
		if (mapping == m_local_player->pid)
		{
			isPadMapped = true;
		}
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

u8 NetPlayClient::LocalWiimoteToInGameWiimote(u8 local_pad)
{
	// Figure out which in-game pad maps to which local pad.
	// The logic we have here is that the local slots always
	// go in order.
	int local_pad_count = -1;
	int ingame_pad = 0;
	for (; ingame_pad < 4; ingame_pad++)
	{
		if (m_wiimote_map[ingame_pad] == m_local_player->pid)
			local_pad_count++;

		if (local_pad_count == local_pad)
			break;
	}

	return ingame_pad;
}

// stuff hacked into dolphin

// called from ---CPU--- thread
// Actual Core function which is called on every frame
bool CSIDevice_GCController::NetPlay_GetInput(u8 numPAD, GCPadStatus PadStatus, u32 *PADStatus)
{
	std::lock_guard<std::mutex> lk(crit_netplay_client);

	if (netplay_client)
		return netplay_client->GetNetPads(numPAD, &PadStatus, (NetPad*)PADStatus);
	else
		return false;
}

bool WiimoteEmu::Wiimote::NetPlay_GetWiimoteData(int wiimote, u8* data, u8 size)
{
	std::lock_guard<std::mutex> lk(crit_netplay_client);

	if (netplay_client)
		return netplay_client->WiimoteUpdate(wiimote, data, size);
	else
		return false;
}

bool CSIDevice_GCSteeringWheel::NetPlay_GetInput(u8 numPAD, GCPadStatus PadStatus, u32 *PADStatus)
{
	return CSIDevice_GCController::NetPlay_GetInput(numPAD, PadStatus, PADStatus);
}

bool CSIDevice_DanceMat::NetPlay_GetInput(u8 numPAD, GCPadStatus PadStatus, u32 *PADStatus)
{
	return CSIDevice_GCController::NetPlay_GetInput(numPAD, PadStatus, PADStatus);
}

// called from ---CPU--- thread
// so all players' games get the same time
u32 CEXIIPL::NetPlay_GetGCTime()
{
	std::lock_guard<std::mutex> lk(crit_netplay_client);

	if (netplay_client)
		return NETPLAY_INITIAL_GCTIME; // watev
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

bool NetPlay::IsNetPlayRunning()
{
	return netplay_client != nullptr;
}

void NetPlay_Enable(NetPlayClient* const np)
{
	std::lock_guard<std::mutex> lk(crit_netplay_client);
	netplay_client = np;
}

void NetPlay_Disable()
{
	std::lock_guard<std::mutex> lk(crit_netplay_client);
	netplay_client = nullptr;
}
