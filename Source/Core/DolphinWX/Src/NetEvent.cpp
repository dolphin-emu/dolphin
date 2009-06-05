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

void ClientSide::OnClientData(unsigned char data)
{
	unsigned char sent = 0;
	u32 buffer_size;
	size_t recv_size;
	char *buffer = NULL;

	switch (data)
	{
		case 0x10: // Player joined server
		{
			// Read GameFound
			m_socket.Receive((char*)&sent, 1, recv_size);
			
			// Read nickname
			m_socket.Receive((char*)&buffer_size, 4, recv_size);
				buffer = new char[buffer_size+1];
			m_socket.Receive(buffer, buffer_size+1, recv_size);
			Event->AppendText(wxString::Format(wxT("*Player : %s is now connected to Host...\n"), buffer));

			if (sent != 0x1F)
				for (int i = 0; i < 4; i++)
					Event->AppendText(_("WARNING : Game Not Found on Client Side!\n"));

			m_numplayers++;
			Event->SendEvent(HOST_NEWPLAYER);
			break;
		}
		case 0x11: // Player left server
		{
			// Read Nickname
			m_socket.Receive((char*)&buffer_size, 4, recv_size);
				buffer = new char[buffer_size+1];
			m_socket.Receive(buffer, buffer_size+1, recv_size);

			Event->AppendText(wxString::Format(wxT("*Player : %s left the game\n\n"), buffer));

			m_numplayers--;
			Event->SendEvent(HOST_PLAYERLEFT);
			break;
		}
		case 0x15: // Ping Player
		{
			m_socket.Receive((char*)&buffer_size, 4, recv_size);
			m_socket.Send((const char*)&buffer_size, 4);

			break;
		}
		case 0x20: // IP request
		{
			//buffer_size = m_addr.size();
			//m_socket.Send((const char*)&buffer_size, 4);
			m_socket.Send((const char*)&data, 1);
			m_socket.Send(m_addr.c_str(), m_addr.size() + 1);

			break;
		}
		case 0x30: // Chat message received from server 
		{
			m_socket.Receive((char*)&buffer_size, 4, recv_size);
				buffer = new char[buffer_size+1];
			m_socket.Receive(buffer, buffer_size+1, recv_size);

			if (recv_size > 1024)
			{
				//something wrong...
				delete[] buffer;
				return;
			}

			Event->AppendText(wxString::FromAscii(buffer));

			break;
		}
		case 0x35: // ChangeGame message received
		{
			m_socket.Receive((char*)&buffer_size, 4, recv_size);
				buffer = new char[buffer_size+1];
			m_socket.Receive(buffer, buffer_size+1, recv_size);

			m_selectedgame = std::string(buffer);
			Event->AppendText(wxString::Format(wxT("*Host changed Game to : %s\n"), buffer));

			// Tell the server if the game's been found
			m_socket.Send((const char*)&data, 1);
			CheckGameFound();
			
			Event->SendEvent(GUI_UPDATE);

			break;
		}
		case 0x40: // Ready message received
		{
			m_socket.Receive((char*)&buffer_size, 4, recv_size);
				buffer = new char[buffer_size+1];
			m_socket.Receive(buffer, buffer_size+1, recv_size);

			if (recv_size > 1024)
			{
				delete[] buffer;
				return;
			}

			Event->AppendText(wxString::FromAscii(buffer));

			break;
		}
		case 0x50: // Everyone is Ready message received
		{
			// Load the game and start synching
			m_netptr->LoadGame();

			break;
		}
		case 0xA1: // Received pad data from host in versus mode
		{
			if (m_data_received)
				wxThread::Sleep(10);
			
			m_socket.Receive((char*)m_netvalues[0], 8, recv_size);
			m_data_received = true;

#ifdef NET_DEBUG
			char sent[64];
			sprintf(sent, "Received Values: 0x%08x : 0x%08x \n", m_netvalues[0][0], m_netvalues[0][1]);
			Event->AppendText(wxString::FromAscii(sent));
#endif
			break;
		}
	}

	delete[] buffer;	
}

void ServerSide::OnServerData(int sock, unsigned char data)
{
	size_t recv_size;
	char *buffer = NULL;
	unsigned char sent;
	unsigned int four_bytes;

	switch (data)
	{
		case 0x15: // Ping Request
		{
			m_client[sock].socket.Receive((char*)&four_bytes, 4, recv_size);
			m_client[sock].socket.Send((const char*)&four_bytes, 4);

			break;
		}
		case 0x20:	// IP request response
		{
			buffer = new char[24];
			// Read IP Address
			m_client[sock].socket.Receive(buffer, 24, recv_size);

			Event->AppendText(wxString::Format(wxT("> Your IP is : %s\n"), buffer));

			break;
		}
		case 0x30:	// Chat message
		{
			buffer = new char[1024];
			
			m_client[sock].socket.Receive((char*)&four_bytes, 4, recv_size);
			m_client[sock].socket.Receive((char*)buffer, four_bytes + 1, recv_size);

			if (recv_size > 1024)
			{
				//something wrong...
				delete[] buffer;
				return;
			}

			sent = 0x30;
			// Send to all
			for (int i=0; i < m_numplayers ; i++)
			{
				if (i == sock)
					continue;

				m_client[i].socket.Send((const char*)&sent, 1); 

				m_client[1].socket.Send((const char*)&four_bytes, 4);
				m_client[i].socket.Send(buffer, recv_size);
			}

			Event->AppendText(wxString::FromAscii(buffer));

			break;
		}
		case 0x35:	// Change game response received
		{
			// Receive isGameFound response (0x1F / 0x1A) 
			m_client[sock].socket.Receive((char*)&sent, 1, recv_size);

			// If game is not found
			if (sent != 0x1F)
			{
				sent = 0x30;

				wxString error_str = wxString::Format(
					wxT("WARNING : Player %s does Not have this Game !\n"), m_client[sock].nick.c_str());
				four_bytes = (int)error_str.size();

				for (int i=0; i < 2; i++)
					Event->AppendText(error_str);

				// Send to all
				for (int i=0; i < m_numplayers ; i++)
				{
					if (i == sock)
						continue;
					m_client[i].socket.Send((const char*)&sent, 1);

					m_client[i].socket.Send((const char*)&four_bytes, 4);
					m_client[i].socket.Send(error_str.mb_str(), four_bytes + 1);
				}
			}

			break;
		}
		case 0x40: // Ready message received
		{
			std::string buffer_str;

			m_client[sock].ready = !m_client[sock].ready;

			if (m_client[sock].ready)
				buffer_str = ">> "+m_client[sock].nick+" is now ready !\n";
			else
				buffer_str = ">> "+m_client[sock].nick+" is now Unready !\n";

			four_bytes = (int)buffer_str.size();

			// Send to all
			for (int i=0; i < m_numplayers ; i++)
			{
				m_client[i].socket.Send((const char*)&data, 1); 

				m_client[i].socket.Send((const char*)&four_bytes, 4);
				m_client[i].socket.Send(buffer_str.c_str(), four_bytes+1);
			}

			Event->AppendText(wxString::FromAscii(buffer_str.c_str()));
			IsEveryoneReady();

			break;
		}
		case 0xA1: // Received pad data from a client
		{
			if (m_data_received)
				wxThread::Sleep(10);

			m_client[sock].socket.Receive((char*)m_netvalues[sock], 8, recv_size);
			m_data_received = true;

#ifdef NET_DEBUG
			char sent[64];
			sprintf(sent, "Received Values: 0x%08x : 0x%08x \n", m_netvalues[sock][0], m_netvalues[sock][1]);
			Event->AppendText(wxString::FromAscii(sent));
#endif
			break;
		}
	}

	delete[] buffer;
}

