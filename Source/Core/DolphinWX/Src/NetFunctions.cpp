// Copyright (C) 2003-2009 Dolphin Project.

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

#include "NetWindow.h"

NetPlay *NetClass_ptr = NULL;

void NetPlay::IsGameFound(unsigned char * ptr, std::string m_selected)
{
	m_critical.Enter();
		
		m_selectedGame = m_selected;

		if (m_games.find(m_selected) != std::string::npos)
		{
			*ptr = 0x1F;
		}
		else
			*ptr = 0x1A;

	m_critical.Leave();
}

void NetPlay::OnNetEvent(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case HOST_FULL:
		{
			AppendText(_(" Server is Full !\n*You have been Disconnected.\n\n"));
			m_isHosting = 2;
		}
		break;
	case HOST_ERROR:
		{
			if (m_isHosting == 0)
			{
				AppendText(_("ERROR : Network Error !\n*You have been Disconnected.\n\n"));
				m_isHosting = 2;
			}
			else
			{
				m_numClients--;
				AppendText( wxString::Format(wxT("ERROR : Network Error !\n"
					"*Player : %s has been dropped from the game.\n\n"),
					event.GetString().mb_str()) );
			}
		}
		break;
	case HOST_DISCONNECTED:
		{
			// Event sent from Client's thread, it means that the thread 
			// has been killed and so we tell the GUI thread.
			AppendText(_("*Connection to Host lost.\n*You have been Disconnected.\n\n"));
			m_isHosting = 2;
		}
		break;
	case HOST_PLAYERLEFT:
		{
			m_numClients--;
		}
		break;
	case HOST_NEWPLAYER:
		{
			m_numClients++;
		}
		break;
	case CLIENTS_READY:
		{
			m_clients_ready = true;
			if (m_ready || event.GetInt())
				LoadGame();
		}
		break;
	case CLIENTS_NOTREADY:
		{
			m_clients_ready = false;
		}
		break;
	case GUI_UPDATE:
		UpdateNetWindow(false);
		break;
	case ADD_TEXT:
		AppendText(event.GetString());
		break;
	case ADD_INFO:
		UpdateNetWindow(true, event.GetString());
		break;
	}
}

void ServerSide::IsEveryoneReady()
{
	int nb_ready = 0;

	for (int i=0; i < m_numplayers ; i++)
		if (m_client[i].ready)
			nb_ready++;

	if (nb_ready == m_numplayers)
		Event->SendEvent(CLIENTS_READY);
	else
		Event->SendEvent(CLIENTS_NOTREADY);
}

// Actual Core function which is called on every frame
int CSIDevice_GCController::GetNetInput(u8 numPAD, SPADStatus PadStatus, u32 *PADStatus)
{
	if (NetClass_ptr != NULL)
		return NetClass_ptr->GetNetPads(numPAD, PadStatus, PADStatus) ? 1 : 0;
	else
		return 2;
}

void NetPlay::LoadGame()
{
	// Two implementations, one "p2p" implementation which sends to peer
	// and receive from peer 2 players max. and another which uses server model
	// and always sends to the server which then send it back to all the clients
	// -> P2P model is faster, but is limited to 2 players
	// -> Server model is slower, but supports up to 4 players 

	m_Logging->AppendText(_("** Everyone is ready... Loading Game ! **\n"));

	// Tell clients everyone is ready...
	if (m_isHosting == 1)
	{
		unsigned char value = 0x50;

		for (int i=0; i < m_numClients ; i++)
			m_sock_server->Write(i, (const char*)&value, 1);
	}

	// TODO : Throttle should be on by default, to avoid stuttering
	//soundStream->GetMixer()->SetThrottle(true);
	
	int line_p = 0;
	int line_n = 0;

	std::string tmp = m_games.substr(0, m_games.find(m_selectedGame));
	for (int i=0; i < (int)tmp.size(); i++)
		if (tmp.c_str()[i] == '\n')
			line_n++;

	// Enable
	NetClass_ptr = this;
	m_timer.Start();

	// Find corresponding game path
	for (int i=0; i < (int)m_paths.size(); i++)
	{
		if (m_paths.c_str()[i] == '\n')
			line_p++;

		if (line_n == line_p)	{
			// Game path found, get its string
			int str_pos = line_p > 0 ? i+1 : i; 
			int str_end = (int)m_paths.find('\n', str_pos);
			// Boot the selected game
			BootManager::BootCore(m_paths.substr(str_pos, str_end - str_pos));
			break;
		}
	}
}

bool NetPlay::GetNetPads(u8 padnb, SPADStatus PadStatus, u32 *netValues)
{
	// Store current pad status in netValues[]
	netValues[0] = (u32)((u8)PadStatus.stickY);
	netValues[0] |= (u32)((u8)PadStatus.stickX << 8);
	netValues[0] |= (u32)((u16)PadStatus.button << 16);
	netValues[0] |= 0x00800000;
	netValues[1] = (u8)PadStatus.triggerRight;
	netValues[1] |= (u32)((u8)PadStatus.triggerLeft << 8);
	netValues[1] |= (u32)((u8)PadStatus.substickY << 16);
	netValues[1] |= (u32)((u8)PadStatus.substickX << 24);

	// TODO : actually show this on the GUI :p
	// Update the timer and increment total frame number
	m_frame++;
	m_timer.Update();

	// We make sure everyone's pad is enabled
	for (char i = 0; i < m_numClients; i++)
		SerialInterface::ChangeDevice(SI_GC_CONTROLLER, (int)i);

	// Better disable unused ports 
	for (char i = m_numClients; i < 3; i++)
		SerialInterface::ChangeDevice(SI_DUMMY, (int)i);

	if (m_NetModel == 0 && m_numClients == 1) // Use P2P Model
	{
		if (padnb == 0)
		{
#ifdef NET_DEBUG
			char sent[64];
			sprintf(sent, "Sent Values: 0x%08x : 0x%08x \n", netValues[0], netValues[1]);
			m_Logging->AppendText(wxString::FromAscii(sent));
#endif
			unsigned char init_value = 0xA1;
			unsigned char recv_value = 0;
			unsigned char player = 0;

			if (m_isHosting == 1) {
				// Send pads values
				m_sock_server->Write(0, (const char*)&init_value, 1);
				m_sock_server->Write(0, (const char*)netValues, 8);
				player = 0; // Host is player 1
			}
			else {
				// Send pads values
				m_sock_client->Write((const char*)&init_value, 1);
				m_sock_client->Write((const char*)netValues, 8);
				player = 1; // Client is player 2
			}

			if (!m_data_received)
			{
				// Save pad values
				m_pads[player].nHi[m_loopframe] = netValues[0];
				m_pads[player].nLow[m_loopframe] = netValues[1];				

				// Try to read from peer...
				if (m_isHosting == 1)
					m_data_received = m_sock_server->isNewPadData(0, m_data_received);
				else
					m_data_received = m_sock_client->isNewPadData(0, m_data_received);

				if (m_data_received)
				{
					// First Data has been received !
					m_Logging->AppendText(_("** Data received from Peer. Starting Sync !"));

					// Set our practical frame delay
					if (recv_value == 0xA1) // init number
					{
						m_frameDelay = m_loopframe;
						m_loopframe = 0;
						m_Logging->AppendText(wxString::Format(wxT(" Frame Delay : %d **\n"), m_frameDelay));
					}
				}
				else {
					m_loopframe++;
					return false;
				}
			}

			if (m_data_received)
			{
				// We have successfully received the data, now use it...
				// If we received data, we can update our pads on each frame, here's the behaviour :
				// we received our init number, so we should receive our pad values on each frames
				// with a frame delay of 'm_frameDelay' frames from the peer. So here, we just wait 
				// for the pad status. note : if the peer can't keep up, sending the values 
				// (i.e : framerate is too low) we have to wait for it thus slowing down emulation 

				// Save current pad values, it will be used in 'm_frameDelay' frames :D
				int saveslot = (m_loopframe-1 < 0 ? m_frameDelay : m_loopframe-1);

				m_pads[player].nHi[saveslot]  = netValues[0];
				m_pads[player].nLow[saveslot] = netValues[1];

				// Read the socket for pad values
				if (m_isHosting == 1)
					while (!m_sock_server->isNewPadData(netValues, 1)) { /* Wait Data */ }
				else
					while (!m_sock_client->isNewPadData(netValues, 1)) { }

				if (player == 0) 
				{
					// Store received peer values
					m_pads[1].nHi[m_loopframe]  = netValues[0];
					m_pads[1].nLow[m_loopframe] = netValues[1];
					// Apply synced pad values
					netValues[0] = m_pads[0].nHi[m_loopframe];
					netValues[1] = m_pads[0].nLow[m_loopframe];
				}
			}

#ifdef NET_DEBUG
			char usedval[64];
			sprintf(usedval, "Player 1 Values: 0x%08x : 0x%08x \n", netValues[0], netValues[1]);
			m_Logging->AppendText(wxString::FromAscii(usedval));
#endif
			return true;
		}
		else if (padnb == 1)
		{
			if (m_data_received)
			{
				netValues[0] = m_pads[1].nHi[m_loopframe];
				netValues[1] = m_pads[1].nLow[m_loopframe];
				
				// Reset the loop to avoid reading unused values
				if (m_loopframe == m_frameDelay)
					m_loopframe = 0;
				else
					m_loopframe++;
			}
			else
				return false;
#ifdef NET_DEBUG
			char usedval[64];
			sprintf(usedval, "Player 2 Values: 0x%08x : 0x%08x \n", netValues[0], netValues[1]);
			m_Logging->AppendText(wxString::FromAscii(usedval));
#endif

			return true;
		}
	}
	else 
	{
		// TODO : :D
		return false;
	}

	return false;
}

void NetPlay::ChangeSelectedGame(std::string game)
{
	wxCriticalSectionLocker lock(m_critical);
	if (m_isHosting == 0)
	{
		m_selectedGame = game;
		return;
	}

	if (game != m_selectedGame)
	{
		unsigned char value = 0x35;
		int game_size = (int)game.size();

		// Send command then Game String
		for (int i=0; i < m_numClients ; i++)
		{
			m_sock_server->Write(i, (const char*)&value, 1); // 0x35 -> Change game

			m_sock_server->Write(i, (const char*)&game_size, 4);
			//m_sock_server->Write(i, m_selectedGame.c_str(), game_size + 1);
			m_sock_server->Write(i, game.c_str(), game_size + 1);
		}

		m_selectedGame = game;
		UpdateNetWindow(false);
		m_Logging->AppendText(wxString::Format(wxT("*Game has been changed to : %s\n"), game.c_str()));
	}
}

void NetPlay::OnQuit(wxCloseEvent& WXUNUSED(event))
{
	NetClass_ptr = NULL;

	// We Kill the threads
	if (m_isHosting == 0)
		m_sock_client->Delete();
	else if (m_isHosting == 1) {
		m_sock_server->Delete();
		// Stop listening, we're doing it here cause Doing it in the thread
		// Cause SFML to crash when built in release build, odd ? :(
		m_listensocket.Close();
	}

	// Destroy the Frame
	Destroy();
}

void NetPlay::OnDisconnect(wxCommandEvent& WXUNUSED(event))
{
	wxCloseEvent close;
	OnQuit(close);
}

bool ClientSide::isNewPadData(u32 *netValues, bool current, bool isVersus)
{
	// TODO : adapt it to more than 2 players
	wxCriticalSectionLocker lock(m_CriticalSection);

	if (current)
	{
		if (m_data_received)
		{
			if (isVersus) {
				netValues[0] = m_netvalues[0][0];
				netValues[1] = m_netvalues[0][1];
			}
		}
		else
			return false;
	}

	return m_data_received;
}

bool ServerSide::isNewPadData(u32 *netValues, bool current, char client)
{
	wxCriticalSectionLocker lock(m_CriticalSection);
	
	if (current)
	{
		if (m_data_received)
		{
			netValues[0] = m_netvalues[client][0];
			netValues[1] = m_netvalues[client][1];
		}
		else
			return false;
	}

	return m_data_received;
}

