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

	bool TestAcksS();
	bool TestAcksC();
	
	bool TestDuplicatesS();
	bool TestDuplicatesC();
	
	bool Test34RecievesNoSendsS();
	bool Test34RecievesNoSendsC();

	bool Test30ConsecutiveDropsS();
	bool Test30ConsecutiveDropsC();

	bool TestRandom30PercentDroppingS();
	bool TestRandom30PercentDroppingC();
	
	bool TestRandomResendsS();
	bool TestRandomResendsC();

	bool TestRandomS();
	bool TestRandomC();
	

	void UnreliableSentTest1();
	void RecieveUnreliableSentTest1();

	void ReliableSentTest();
	void ReliableRecieveTest();

	void ReliableSentTest2();
	void ReliableRecieveTest2();

	void ReliableSentTest3();
	void ReliableRecieveTest3();

private:
	#define MaxWords  5

	bool Recieve(sf::Packet& packet);
	void Recieve();
	void RecieveDrop();
	void Send(sf::Packet& packet, bool ack = true);
	void Send(std::string str[MaxWords], bool ack = true);
	void Send(int i, bool ack = true);
	void SendAck();
	//void Check();
	void CheckString();

	void SocketReset();

	std::string sendStr[2][MaxWords];
	std::string recvStr[MaxWords*20];

	sf::IpAddress m_adr;
	u16 m_port;
	std::shared_ptr<sf::UdpSocket> m_socket;
	ReliableUDPConnection* m_udp;

	sf::TcpSocket m_controlSock;

	int m_token;
	int m_recvToken;
};