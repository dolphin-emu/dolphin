// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <SFML/Network.hpp>
#include "Common/CommonTypes.h"
#include "ReliableUDPConnection.h"


class ReliableUDPManager
{
public:
	ReliableUDPManager(unsigned short port = 0);
	~ReliableUDPManager();

	void Update();
	
	bool SendMess(u16 ID, sf::Packet& packet);
	void SendMessageToAll(sf::Packet& packet, u16 exceptionID = 0);

	bool GrabMessage(u16 ID, sf::Packet& packet);

	u16 CheckNewConnections(sf::IpAddress& ip, unsigned short& port);

	u16 Connect(std::string ip, unsigned short port, float wait);
	void Disconnect(u16 ID);

	bool DisconnectList(u16& returnID);
	
	sf::IpAddress GetAddress(u16 ID);
	short unsigned GetPort(u16 ID);
	void	ClearBuffer(u16 ID);

	//Sets wether we listen to new connections
	void SetListen(bool listen){ m_listen = listen; };

	bool GetListen(){ return m_listen; };
	
	

private:

	u16 StoreConnection(std::shared_ptr<ReliableUDPConnection> connection);
	void RemoveConnectionStore(u16 ID);

	std::map<u16, std::shared_ptr<ReliableUDPConnection>> m_IDtoConnection;
	std::map<sf::IpAddress, std::shared_ptr<ReliableUDPConnection>> m_AddresstoConnection;

	std::queue<u16> m_newConnection;
	std::queue<u16> m_disconnection;

	bool m_listen;
	std::shared_ptr<sf::UdpSocket> m_udpSocket;
	u16 m_id;
	
	u16 m_dropMess;
};