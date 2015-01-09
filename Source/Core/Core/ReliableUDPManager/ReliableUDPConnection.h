// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <SFML/Network.hpp>
#include "Common/CommonTypes.h"
#include "Common/Timer.h"
#include <queue>
#include <map>
#include <memory>
#include "Common/Thread.h"

class ReliableUDPConnection
{
#define udpBitType	u32
#define udpBitMax	UINT32_MAX

public:
	ReliableUDPConnection(std::shared_ptr<sf::UdpSocket> socket, sf::IpAddress adr, u16 port);
	~ReliableUDPConnection();
	
	void StoreSend(sf::Packet& packet);
	sf::Socket::Status Send(bool sendAck = true);
	sf::Socket::Status SendUnreliable(sf::Packet& packet);

	bool Receive(sf::Packet& packet);
	bool GrabMessage(sf::Packet& packet);
	
	bool CheckIfAlive();
	void Disconnect();

	void ClearBuffers();
	
	sf::IpAddress GetAddress(){ return m_remoteAddress; };
	unsigned short  GetPort(){ return m_remotePort; };

	int AmountToSend(){ return (int) m_toBeSent.size();};
	bool isReadyToSend(){ return (!m_toBeSent.empty() || !m_resend.empty()); }
	
	//function
private:

	void UpdateBackUp(u16 ack, udpBitType bitfield);
	int  IfWrappedConvertToNeg(int current, int previous, int max);

	//variables
private:

	struct Palette
	{
		u16 packetOrder;
		sf::Packet packet;

		Palette()
			:packetOrder(0)
			, packet()
		{}

		Palette(u16 seq, sf::Packet pack)
			:packetOrder(seq)
			, packet(pack)
		{}

		bool operator< (const Palette& rhs) const
		{
			return rhs.packetOrder< packetOrder;
		}

	};
	
	std::shared_ptr<sf::UdpSocket>	m_socket;
	
	sf::IpAddress	m_remoteAddress;
	unsigned short	m_remotePort;
	
	//header to know we are recieving message from the right program
	u8				m_header;

	//our sequence number to send to connection
	u16				m_mySequenceNumber;

	//their last recieved ack
	u16				m_theirSequenceNumber;

	//bit field where 0 represent a message we missed
	udpBitType		m_missingBitField;
	
	//the last ack the other side confirmed
	u16				m_theirLastAck;

	//which in order number we are waiting for
	u16				m_expectedSequence;

	//which in order number we recieved
	u16				m_nextInOrder;

	//messages that have yet to be sent
	std::queue<sf::Packet>			m_toBeSent;

	//messages that we have recieved
	std::queue<sf::Packet>			m_recievedMess;

	//messages that are on the ready to be sent back incase of drop
	std::map <u16, Palette>			m_backupMess;
	
	//messages that we recieved out of order
	std::map<u16, sf::Packet>		m_bufferMess;

	//messages that have been confirmed drop and need to be resent
	std::priority_queue<Palette>	m_resend;

	// -- timer
	Common::Timer				m_keepAlive;
	Common::Timer				m_sendAck;
	u64							m_ackTime;

	float						m_disconnectTime;
	bool						m_sentEmptyAck;

};