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

#ifndef _NETSOCKETS_H_
#define _NETSOCKETS_H_

#include <SFML/Network.hpp>

class NetPlay;

#include "Common.h"
#include "NetStructs.h"

#include <wx/wx.h>


struct Clients {
	std::string       nick;
	sf::SocketTCP     socket;
	unsigned short    port;
	sf::IPAddress     address;
	bool              ready;
};

class NetEvent
{
	public:
		NetEvent(NetPlay* netptr) { m_netptr = netptr; }
		~NetEvent() {};

		void SendEvent(int EventType, std::string="NULL", int=NULL);
		void AppendText(const wxString text);

	private:
		NetPlay          *m_netptr;
};

class ServerSide : public wxThread
{
	public:
		ServerSide(NetPlay* netptr, sf::SocketTCP, sf::SocketUDP, int netmodel, std::string nick);
		~ServerSide() {};

		virtual void *Entry();

		void Write(int socknb, const char *data, size_t size, long *ping=NULL);
		void WriteUDP(int socknb, const char *data, size_t size);
		bool isNewPadData(u32 *netValues, bool current, int client=0);

	private:
		bool SyncValues(unsigned char, sf::IPAddress);
		bool RecvT(sf::SocketUDP Socket, char * Data, size_t Max, size_t& Recvd, float Time = 0);
		char GetSocket(sf::SocketTCP Socket);
		void OnServerData(int sock, unsigned char data);
		void IsEveryoneReady();

		NetPlay          *m_netptr;
		NetEvent         *Event;

		u32               m_netvalues[3][3];
		bool              m_data_received; // New Pad data received ?

		unsigned char     m_numplayers;
		int               m_netmodel;
		std::string       m_nick;
		
		Clients           m_client[3];      // Connected client objects
		sf::SelectorTCP   m_selector;
		sf::SocketTCP     m_socket;	        // Server 'listening' socket
		sf::SocketUDP     m_socketUDP;

		wxCriticalSection m_CriticalSection;
};

class ClientSide : public wxThread
{
	public:
		ClientSide(NetPlay* netptr, sf::SocketTCP, sf::SocketUDP, std::string addr, std::string nick);
		~ClientSide() {}

		virtual void *Entry();

		void Write(const char *data, size_t size, long *ping=NULL);
		void WriteUDP(const char *data, size_t size);
		bool isNewPadData(u32 *netValues, bool current, bool isVersus=true);

	private:
		bool SyncValues();
		void CheckGameFound();
		void OnClientData(unsigned char data);
		bool RecvT(sf::SocketUDP Socket, char * Data, size_t Max, size_t& Recvd, float Time=0);

		NetPlay          *m_netptr;
		NetEvent         *Event;

		u32               m_netvalues[3][3];
		bool              m_data_received; // New Pad data received ?

		unsigned char     m_numplayers;
		int               m_netmodel;
		std::string       m_nick;
		std::string       m_hostnick;
		std::string       m_selectedgame;

		sf::SelectorTCP   m_selector;
		sf::SocketTCP     m_socket;	        // Client I/O socket
		sf::SocketUDP     m_socketUDP;
		unsigned short    m_port;
		std::string       m_addr;           // Contains the server addr

		wxCriticalSection m_CriticalSection;
};

#endif
