// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "ReliableUDPConnection.h"

class ReliableUnitTest
{
public:
	ReliableUnitTest(sf::IpAddress adr, bool server);
	~ReliableUnitTest();

	void TestsServer();
	void TestClient();

	bool TestConnectionS();
	bool TestConnectionC();

	bool TestSendingRecievingS();
	bool TestSendingRecievingC();

	bool TestDroppingS();
	bool TestDroppingC();

	void UnreliableSentTest1();
	void RecieveUnreliableSentTest1();

	void ReliableSentTest();
	void ReliableRecieveTest();

private:
	#define MaxWords  5

	bool Recieve(sf::Packet& packet);
	void Recieve();
	void RecieveDrop();
	void Send(sf::Packet& packet, bool ack = true);
	void Send(std::string str[MaxWords], bool ack = true);
	void Send(int i, bool ack = true);
	void SendAck();
	void CheckString();

	void SocketReset();

	std::string sendStr[2][MaxWords];
	std::string recvStr[MaxWords*20];

	sf::IpAddress m_adr;
	u16 m_port;
	std::shared_ptr<sf::UdpSocket> m_socket;
	std::shared_ptr<ReliableUDPConnection> m_udp;

	sf::TcpSocket m_controlSock;

	int m_token;
	int m_recvToken;
};