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

#include "NetSockets.h"
#include "NetWindow.h"

//--------------------------------
//			GUI EVENTS
//--------------------------------

void NetEvent::AppendText(const wxString text)
{
	// I have the feeling SendEvent may be a bit safer/better...
#if 0
	SendEvent(ADD_TEXT, std::string(text.mb_str())); //NETPLAY: FIXME std::string(text.mb_str())
#else
	wxMutexGuiEnter();
		m_netptr->AppendText(text);
	wxMutexGuiLeave();
#endif
}

void NetEvent::SendEvent(int EventType, const std::string text, int integer)
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, wxID_ANY);

	event.SetId( EventType );
	event.SetInt( integer );
	event.SetString( wxString::FromAscii(text.c_str()) );
#if ! wxCHECK_VERSION(2, 9, 0)
	m_netptr->AddPendingEvent(event);
#endif
}

//--------------------------------
//		SERVER SIDE THREAD
//--------------------------------

ServerSide::ServerSide(NetPlay* netptr, sf::SocketTCP socket, sf::SocketUDP socketUDP, int netmodel, std::string nick)
	: wxThread()
{
	m_numplayers = 0;
	m_data_received = false;
	m_netmodel = netmodel;
	m_socket = socket;
	m_socketUDP = socketUDP;
    m_netptr = netptr;
	m_nick = nick;
	Event = new NetEvent(m_netptr);
}

char ServerSide::GetSocket(sf::SocketTCP Socket)
{
	for (int i=0; i < m_numplayers; i++)
	{
		if(m_client[i].socket == Socket)
			return i;
	}

	return 0xE;
}

bool ServerSide::RecvT(sf::SocketUDP Socket, char * Data, size_t Max, size_t& Recvd, float Time)
{
	sf::SelectorUDP Selector;
	sf::IPAddress Addr;
	unsigned short Port;
	Selector.Add(Socket);

	if (Selector.Wait(Time) > 0)
	{
		Socket.Receive(Data, Max, Recvd, Addr, Port);
		return true;
	}
	else
	{
		return false;
	}
}

void *ServerSide::Entry()
{
	// Add listening socket
	m_selector.Add(m_socket);

	while (1)
	{
		char nbSocketReady = m_selector.Wait(0.5);

		for (char i = 0; i < nbSocketReady; ++i)
		{
			m_CriticalSection.Enter();
			sf::SocketTCP Socket = m_selector.GetSocketReady(i);
			if (Socket == m_socket)
			{
				// Incoming connection
				Event->AppendText(_("*Connection Request... "));

				sf::SocketTCP Incoming;
				sf::IPAddress Address;
				m_socket.Accept(Incoming, &Address);

				unsigned char sent = 0x12;
				if ((m_netmodel == 0 && m_numplayers > 0) || m_numplayers == 3)
				{
					Incoming.Send((const char *)&sent, 1);	// Tell it the server is full...
					Incoming.Close();						// Then close the connection

					Event->AppendText(_(" Server is Full !\n"));
				}
				else
				{
					Event->AppendText(_(" Connection accepted\n"));
					m_client[m_numplayers].socket = Incoming;
					m_client[m_numplayers].address = Address;
					
					if (SyncValues(m_numplayers, Address))
					{
						// Add it to the selector
						m_selector.Add(Incoming);
						Event->SendEvent(HOST_NEWPLAYER);
						m_numplayers++;
					}
					else
					{
						Event->AppendText(_("ERROR : Unable to establish UDP connection !\n"));
						Incoming.Close();
					}
				}
			}
			else
			{
				unsigned char recv;
				int socket_nb;
				size_t recv_size;
				sf::Socket::Status recv_status;
				socket_nb = GetSocket(Socket);

				if ((recv_status = Socket.Receive((char *)&recv, 1, recv_size)) == sf::Socket::Done)
				{
#ifdef NET_DEBUG
					char recv_str[32];
					sprintf(recv_str, "received: 0x%02x | %c\n", recv, recv);
					Event->AppendText(wxString::FromAscii(recv_str));
#endif 
					OnServerData(socket_nb, recv);
				}
				else
				{
					if (recv_status == sf::Socket::Disconnected)
					{
						Event->SendEvent(HOST_PLAYERLEFT);
						m_numplayers--;

						std::string player_left = m_client[socket_nb].nick;
						Event->AppendText( wxString::Format(wxT("*Player : %s left the game.\n\n"),
							player_left.c_str()) );

						// We need to adjust the struct...
						for (int j = socket_nb; j < m_numplayers; j++)
						{
							m_client[j].socket = m_client[j+1].socket;
							m_client[j].nick   = m_client[j+1].nick;
							m_client[j].ready  = m_client[j+1].ready;
						}

						// Send disconnected message to all
						unsigned char send = 0x11;
						unsigned int str_size = (int)player_left.size();

						for (int j=0; j < m_numplayers ; j++)
						{
							m_client[j].socket.Send((const char*)&send, 1);
							m_client[j].socket.Send((const char*)&str_size, 4);
							m_client[j].socket.Send(player_left.c_str(), (int)str_size + 1);
						}
					}
					else
					{
						// Hopefully this should never happen, the client is not
						// Even warned that he is being dropped...
						Event->SendEvent(HOST_ERROR, m_client[socket_nb].nick);
					}

					m_selector.Remove(Socket);
					Socket.Close();
				}
			}

			m_CriticalSection.Leave();
		}

		if(TestDestroy())
		{
			// Stop listening
			m_socket.Close();

			// Delete the Thread and close clients sockets
			for (int i=0; i < m_numplayers ; i++)
				m_client[i].socket.Close();

			break;
		}
	}

	return NULL;
}

bool ServerSide::SyncValues(unsigned char socketnb, sf::IPAddress Address)
{
	sf::SocketTCP Socket = m_client[socketnb].socket;

	std::string buffer_str = m_netptr->GetSelectedGame();
	char *buffer = NULL;
	unsigned char init_number;
	u32 buffer_size = (u32)buffer_str.size();
	size_t received;
	bool errorUDP = false;

	// First, Send the number of connected clients & netmodel
	Socket.Send((const char *)&m_numplayers, 1);
	Socket.Send((const char *)&m_netmodel, 4);

	// Send the Game String
	Socket.Send((const char *)&buffer_size, 4);
	Socket.Send(buffer_str.c_str(), buffer_size + 1);

	// Send the host Nickname
	buffer_size = (u32)m_nick.size();
	Socket.Send((const char *)&buffer_size, 4);
	Socket.Send(m_nick.c_str(), buffer_size + 1);

	// Read client's UDP Port
	Socket.Receive((char *)&m_client[m_numplayers].port, sizeof(short), received);

	// Read returned nickname
	Socket.Receive((char *)&buffer_size, 4, received);
		buffer = new char[buffer_size + 1];
	Socket.Receive(buffer, buffer_size + 1, received);

	m_client[socketnb].nick = std::string(buffer);
	m_client[socketnb].ready = false;

	
	// Test UDP Socket 
	
	if (m_socketUDP.Send((const char*)&m_numplayers, 1, Address, m_client[m_numplayers].port) == sf::Socket::Done)
	{
		// Test UDP Socket Receive, 2s timeout
		if (!RecvT(m_socketUDP, (char*)&init_number, 1, received, 2))
		{
			ERROR_LOG(NETPLAY,"Connection to client timed out or closed");
			errorUDP = true;
		}
	}
	else
	{
		ERROR_LOG(NETPLAY,"Failed to send info! closing connection!");
		errorUDP = true;
	}

	// Check if the client has the game
	Socket.Receive((char *)&init_number, 1, received);

	delete[] buffer;

	if (!errorUDP)
	{
		// Send to all connected clients
		if (m_numplayers > 0)
		{
			unsigned char send = 0x10;
			buffer_size = (int)m_client[socketnb].nick.size();
			for (int i=0; i < m_numplayers ; i++)
			{
				// Do not send to connecting player
				if (i == socketnb)
					continue;

				m_client[i].socket.Send((const char *)&send, 1);		// Init new connection
				m_client[i].socket.Send((const char *)&init_number, 1);	// Send Game found ?
				m_client[i].socket.Send((const char *)&buffer_size, 4);	// Send client nickname
				m_client[i].socket.Send(m_client[socketnb].nick.c_str(), buffer_size + 1);
			}
		}
		Event->AppendText(  wxString::FromAscii(StringFromFormat("*Connection established to %s (%s:%d)\n",
			m_client[socketnb].nick.c_str(), Address.ToString().c_str(), m_client[m_numplayers].port).c_str()));

		if (init_number != 0x1F) // Not Found
			//for (int i = 0; i < 4; i++)
			//note for sl1nk3 : what is that for doing there?
				Event->AppendText(_("WARNING : Game Not Found on Client Side !\n"));

		// UDP connecton successful
		init_number = 0x16;
		Socket.Send((const char *)&init_number, 1);
	}
	else	// UDP Error, disconnect client
	{
		// UDP connecton failed
		init_number = 0x17;
		Socket.Send((const char *)&init_number, 1);
		return false;
	}

	return true;
}

void ServerSide::Write(int socknb, const char *data, size_t size, long *ping)
{
	wxCriticalSectionLocker lock(m_CriticalSection);

		if (ping != NULL)
		{
			// Ask for ping
			unsigned char value = 0x15;
			size_t recv_size;
			u32 four_bytes = 0x101A7FA6;

			Common::Timer timer;
			timer.Start();

			for (int i=0; i < m_numplayers ; i++)
			{
				m_client[i].socket.Send((const char*)&value, 1);

				timer.Update();
				m_client[i].socket.Send((const char*)&four_bytes, 4);
				m_client[i].socket.Receive((char*)&four_bytes, 4, recv_size);
				ping[i] = (long)timer.GetTimeDifference();
			}

			return;
		}

	// Send the data safely, without intefering with another call 
	m_client[socknb].socket.Send(data, size);
}

void ServerSide::WriteUDP(int socknb, const char *data, size_t size)
{
	wxCriticalSectionLocker lock(m_CriticalSection);

	m_socketUDP.Send(data, size, m_client[socknb].address, m_client[socknb].port);
}

//--------------------------------
//		CLIENT SIDE THREAD
//--------------------------------

ClientSide::ClientSide(NetPlay* netptr, sf::SocketTCP socket, sf::SocketUDP socketUDP, std::string addr, std::string nick)
	: wxThread()
{
	m_numplayers = 0;
	m_data_received = false;
	m_netmodel = 0;
	m_socket = socket;
	m_socketUDP = socketUDP;
	m_port = m_socketUDP.GetPort();
    m_netptr = netptr;
	m_nick = nick;
	m_addr = addr;
	Event = new NetEvent(m_netptr);
}

bool ClientSide::RecvT(sf::SocketUDP Socket, char * Data, size_t Max, size_t& Recvd, float Time)
{
	sf::SelectorUDP Selector;
	sf::IPAddress Addr;
	unsigned short Port;
	Selector.Add(Socket);

	if (Selector.Wait(Time) > 0)
	{
		Socket.Receive(Data, Max, Recvd, Addr, Port);
		return true;
	}
	else
	{
		return false;
	}
}

void *ClientSide::Entry()
{
	Event->AppendText(_("*Connection Request... "));

	// If we get here, the connection is already accepted, however the game may be full
	if (SyncValues())
	{
		// Tell the server if the client has the game	
		CheckGameFound();

		// Check UDP connection
		unsigned char value;
		size_t val_sz;
		m_socket.Receive((char *)&value, 1, val_sz);
		if (value == 0x16)	// UDP connection successful
		{
			Event->AppendText(_("Connection successful !\n"));
			Event->AppendText( wxString::Format( wxT("*Connection established to %s (%s)\n*Game is : %s\n "),
												 m_hostnick.c_str(), m_addr.c_str(), m_selectedgame.c_str()));
		}
		else
		{
			Event->AppendText(_("UDP Connection FAILED !\nERROR : Unable to establish UDP Connection, please Check UDP Port forwarding !\n"));
			m_socket.Close();
			Event->SendEvent(HOST_ERROR);
			return NULL;
		}
	}
	else // Server is Full  
	{
		m_socket.Close();
		Event->SendEvent(HOST_FULL);
		return NULL;
	}

	m_netptr->ChangeSelectedGame(m_selectedgame);
	Event->SendEvent(HOST_NEWPLAYER, "NULL", m_netmodel);
	Event->SendEvent(GUI_UPDATE);

	m_selector.Add(m_socket);

	while (1)
	{
		unsigned char recv;
		size_t recv_size;
		sf::Socket::Status recv_status;

		// we use a selector because of the useful timeout
		if (m_selector.Wait(0.5) > 0)
		{
			m_CriticalSection.Enter();

			if ((recv_status = m_socket.Receive((char *)&recv, 1, recv_size)) == sf::Socket::Done)
			{
#ifdef NET_DEBUG
				char recv_str[32];
				sprintf(recv_str, "received: 0x%02x | %c\n", recv, recv);
				Event->AppendText(wxString::FromAscii(recv_str));
#endif 
				OnClientData(recv);
			}
			else
			{
				if (recv_status == sf::Socket::Disconnected)
				{
					Event->SendEvent(HOST_DISCONNECTED);
				}
				else
				{
					Event->SendEvent(HOST_ERROR);
				}

				m_selector.Remove(m_socket);
				m_socket.Close();

				return NULL;
			}

			m_CriticalSection.Leave();
		}
		
		if(TestDestroy())
		{
			m_socket.Close();

			break;
		}
	}

	return NULL;
}

bool ClientSide::SyncValues()
{
	unsigned int buffer_size = (int)m_nick.size();
	unsigned char byterecv;
	bool errorUDP;
	char *buffer = NULL;
	size_t recv_size;

	unsigned short server_port;
	std::string host = m_addr.substr(0, m_addr.find(':'));
	TryParseInt(m_addr.substr(m_addr.find(':')+1).c_str(), (int *)&server_port);

	// First, Read the init number : nbplayers (0-2) or server full (0x12)
	m_socket.Receive((char *)&m_numplayers, 1, recv_size);
	if (m_numplayers == 0x12)
		return false;
	m_socket.Receive((char *)&m_netmodel, 4, recv_size);

	// Send client's UDP Port
	// TODO : fix port sending. it sends the set port in the main window. not the actual using port
	// when checked to use random this will , ofcourse , send wrong port
	m_socket.Send((const char *)&m_port, sizeof(short));

	// Send client's nickname
	m_socket.Send((const char *)&buffer_size, 4);
	m_socket.Send(m_nick.c_str(), buffer_size + 1);

	// Read the Game String
	m_socket.Receive((char *)&buffer_size, 4, recv_size);
		buffer = new char[buffer_size + 1];
	m_socket.Receive(buffer, buffer_size + 1, recv_size);
		m_selectedgame = std::string(buffer);

	// Read the host Nickname
	m_socket.Receive((char *)&buffer_size, 4, recv_size); 
		buffer = new char[buffer_size + 1];
	m_socket.Receive(buffer, buffer_size + 1, recv_size);
		m_hostnick = std::string(buffer);

	
	// Test UDP Socket
	
	if (m_socketUDP.Send((const char*)&m_numplayers, 1, host.c_str(), server_port) == sf::Socket::Done)
	{
		// Test UDP Socket Receive, 2s timeout
		if (!RecvT(m_socketUDP, (char*)&byterecv, 1, recv_size, 2))
		{
			errorUDP = true;
			ERROR_LOG(NETPLAY,"Connection Timed Out or closed");
		}
	}
	else
		errorUDP = true;

	delete[] buffer;
	return true;
}

void ClientSide::CheckGameFound()
{
	unsigned char send_value;

	// Check if the game selected by Host is in Client's Game List
	m_netptr->IsGameFound(&send_value, m_selectedgame);

	if (send_value == 0x1F) // Found
	{	
		m_socket.Send((const char *)&send_value, 1);
	}
	else
	{	
		m_socket.Send((const char *)&send_value, 1);

		for (int i = 0; i < 2; i++)
		{
			Event->AppendText(wxT("WARNING : You do not have the Selected Game !\n"));
			NOTICE_LOG(NETPLAY, "Game '%s' not found!", m_selectedgame.c_str());
			
		}			
	}
}

void ClientSide::Write(const char *data, size_t size, long *ping)
{
	wxCriticalSectionLocker lock(m_CriticalSection);

		if (ping != NULL)
		{
			// Ask for ping
			unsigned char value = 0x15;
			size_t recv_size;
			int four_bytes = 0x101A7FA6;

			Common::Timer timer;
			timer.Start();

			m_socket.Send((const char*)&value, 1);
			m_socket.Send((const char*)&four_bytes, 4);
			m_socket.Receive((char*)&four_bytes, 4, recv_size);

			*ping = (long)timer.GetTimeElapsed();

			return;
		}

	m_socket.Send(data, size);
}

void ClientSide::WriteUDP(const char *data, size_t size)
{
	wxCriticalSectionLocker lock(m_CriticalSection);

	unsigned short server_port;
	std::string host = m_addr.substr(0, m_addr.find(':'));
	TryParseInt(m_addr.substr(m_addr.find(':')+1).c_str(), (int *)&server_port);

	m_socketUDP.Send(data, size, host.c_str(), server_port);
}
