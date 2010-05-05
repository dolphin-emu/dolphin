// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "NetPlay.h"
#include "NetWindow.h"

// for wiimote
#include "IPC_HLE/WII_IPC_HLE_Device_usb.h"
#include "IPC_HLE/WII_IPC_HLE_WiiMote.h"
// for gcpad
#include "HW/SI_DeviceGCController.h"
// for gctime
#include "HW/EXI_DeviceIPL.h"
// to start/stop game
#include "Frame.h"
// for OSD msgs
#include "Core.h"

Common::CriticalSection		crit_netplay_ptr;
static NetPlay* netplay_ptr = NULL;
extern CFrame* main_frame;

#define RPT_SIZE_HACK	(1 << 16)

DEFINE_EVENT_TYPE(wxEVT_THREAD)

THREAD_RETURN NetPlayThreadFunc(void* arg)
{
	((NetPlay*)arg)->Entry();
	return 0;
}

//CritLocker::CritLocker(Common::CriticalSection& crit)
//	: m_crit(crit)
//{
//	m_crit.Enter();
//}
//
//CritLocker::~CritLocker()
//{
//	m_crit.Leave();
//}

// called from ---GUI--- thread
NetPlay::NetPlay()
	: m_is_running(false), m_do_loop(true)
{
	m_target_buffer_size = 20;
	ClearBuffers();
}

void NetPlay_Enable(NetPlay* const np)
{
	CritLocker crit(::crit_netplay_ptr);	// probably safe without a lock
	::netplay_ptr = np;
}

void NetPlay_Disable()
{
	CritLocker crit(::crit_netplay_ptr);
	::netplay_ptr = NULL;
}

// called from ---GUI--- thread
NetPlay::~NetPlay()
{
	CritLocker crit(crit_netplay_ptr);
	::netplay_ptr = NULL;
}

NetPlay::Player::Player()
{
	memset(pad_map, -1, sizeof(pad_map));
}

// called from ---NETPLAY--- thread
void NetPlay::UpdateGUI()
{
	if (m_dialog)
	{
		wxCommandEvent evt(wxEVT_THREAD, 1);
		m_dialog->GetEventHandler()->AddPendingEvent(evt);
	}
}

// called from ---NETPLAY--- thread
void NetPlay::AppendChatGUI(const std::string& msg)
{
	if (m_dialog)
	{
		m_dialog->chat_msgs.Push(msg);
		// silly
		UpdateGUI();
	}
}

// called from ---GUI--- thread
std::string NetPlay::Player::ToString() const
{
	std::ostringstream ss;
	ss << name << '[' << (char)(pid+'0') << "] : " << revision << " |";
	for (unsigned int i=0; i<4; ++i)
		ss << (pad_map[i]>=0 ? (char)(pad_map[i]+'1') : '-');
	ss << '|';
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

// called from ---NETPLAY--- thread
void NetPlay::ClearBuffers()
{
	// clear pad buffers, no clear method ?
	for (unsigned int i=0; i<4; ++i)
	{
		while (m_pad_buffer[i].size())
			m_pad_buffer[i].pop();
	}
}

// called from ---CPU--- thread
bool NetPlay::GetNetPads(const u8 pad_nb, const SPADStatus* const pad_status, NetPad* const netvalues)
{
	m_crit.players.Enter();	// lock players

	// in game mapping for this local pad
	unsigned int in_game_num = m_local_player->pad_map[pad_nb];

	// does this local pad map in game?
	if (in_game_num < 4)
	{
		NetPad np(pad_status);

		m_crit.buffer.Enter();	// lock buffer
		// adjust the buffer either up or down
		while (m_pad_buffer[in_game_num].size() <= m_target_buffer_size)
		{
			// add to buffer
			m_pad_buffer[in_game_num].push(np);

			// send
			SendPadState(pad_nb, np);
		}
		m_crit.buffer.Leave();
	}

	m_crit.players.Leave();

	//Common::Timer bufftimer;
	//bufftimer.Start();

	// get padstate from buffer and send to game
	m_crit.buffer.Enter();	// lock buffer
	while (0 == m_pad_buffer[pad_nb].size())
	{
		m_crit.buffer.Leave();
		// wait for receiving thread to push some data
		Common::SleepCurrentThread(1);
		if (false == m_is_running)
			return false;
		m_crit.buffer.Enter();

		// TODO: check the time of bufftimer here,
		// if it gets pretty high, ask the user if they want to disconnect

	}
	*netvalues = m_pad_buffer[pad_nb].front();
	m_pad_buffer[pad_nb].pop();
	m_crit.buffer.Leave();

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
void NetPlay::WiimoteInput(int _number, u16 _channelID, const void* _pData, u32 _Size)
{
	//m_crit.players.Enter();	// lock players

	//// in game mapping for this local wiimote
	//unsigned int in_game_num = m_local_player->pad_map[_number];	// just using gc pad_map for now

	//// does this local pad map in game?
	//if (in_game_num < 4)
	//{
	//	//NetPad np(pad_status);

	//	m_crit.buffer.Enter();	// lock buffer
	//	// adjust the buffer either up or down
	//	//while (m_pad_buffer[in_game_num].size() <= m_target_buffer_size)
	//	{
	//		// add to buffer
	//		//m_wiimote_buffer[in_game_num].push(np);

	//		// send
	//		//SendPadState(pad_nb, np);
	//	}
	//	m_crit.buffer.Leave();
	//}

	//// get pad from pad buffer and send to game
	//GetBufferedPad(pad_nb, netvalues);

}

// called from ---CPU--- thread
void NetPlay::WiimoteUpdate(int _number)
{
	// temporary
	if (_number)
		return;

	m_crit.buffer.Enter();	// lock buffer
	++m_wiimote_update_frame;
	m_crit.buffer.Leave();

	// TODO: prolly do some wiimote input here
}

// called from ---GUI--- thread
bool NetPlay::StartGame(const std::string &path)
{
	CritLocker game_lock(m_crit.game);	// lock game state

	if (m_is_running)
	{
		PanicAlert("Game is already running!");
		return false;
	}

	AppendChatGUI(" -- STARTING GAME -- ");

	m_is_running = true;
	NetPlay_Enable(this);

	// boot game
	::main_frame->BootGame(path);
	//BootManager::BootCore(path);

	return true;
}

// called from ---GUI--- thread and ---NETPLAY--- thread (client side)
bool NetPlay::StopGame()
{
	CritLocker game_lock(m_crit.game);	// lock game state

	if (false == m_is_running)
	{
		PanicAlert("Game isn't running!");
		return false;
	}

	AppendChatGUI(" -- STOPPING GAME -- ");

	m_is_running = false;
	NetPlay_Disable();

	// stop game
	::main_frame->DoStop();

	return true;
}

// called from ---CPU--- thread
u8 NetPlay::GetPadNum(u8 numPAD)
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
int CSIDevice_GCController::NetPlay_GetInput(u8 numPAD, SPADStatus PadStatus, u32 *PADStatus)
{
	CritLocker crit(::crit_netplay_ptr);

	if (::netplay_ptr)
		return netplay_ptr->GetNetPads(numPAD, &PadStatus, (NetPad*)PADStatus) ? 1 : 0;
	else
		return 2;
}

// called from ---CPU--- thread
// so all players' games get the same time
u32 CEXIIPL::NetPlay_GetGCTime()
{
	CritLocker crit(::crit_netplay_ptr);

	if (::netplay_ptr)
		return 1272737767;	// watev
	else
		return 0;
}

// called from ---CPU--- thread
// return the local pad num that should rumble given a ingame pad num
u8 CSIDevice_GCController::NetPlay_GetPadNum(u8 numPAD)
{
	CritLocker crit(::crit_netplay_ptr);

	if (::netplay_ptr)
		return ::netplay_ptr->GetPadNum(numPAD);
	else
		return numPAD;
}

// called from ---CPU--- thread
// wiimote update / used for frame counting
void CWII_IPC_HLE_Device_usb_oh1_57e_305::NetPlay_WiimoteUpdate(int _number)
{
	CritLocker crit(::crit_netplay_ptr);

	return;
}

// called from ---CPU--- thread
//
int CWII_IPC_HLE_WiiMote::NetPlay_GetWiimoteNum(int _number)
{
	CritLocker crit(::crit_netplay_ptr);

	if (::netplay_ptr)
		return ::netplay_ptr->GetPadNum(_number);		// just using gcpad mapping for now
	else
		return _number;
}

// called from ---CPU--- thread
// intercept wiimote input callback
bool CWII_IPC_HLE_WiiMote::NetPlay_WiimoteInput(int _number, u16 _channelID, const void* _pData, u32& _Size)
{
	CritLocker crit(::crit_netplay_ptr);

	if (::netplay_ptr)
	{
		if (_Size >= RPT_SIZE_HACK)
		{
			_Size -= RPT_SIZE_HACK;
			return false;
		}
		else
		{
			::netplay_ptr->WiimoteInput(_number, _channelID, _pData, _Size);
			return true;
		}
	}
	else
		return false;
}
