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
	bool WasRecieved(const udpBitType& bitField, const u16& currentAck, const u16& ackCheck);
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
	/*
	struct
	{
		std::recursive_mutex send;
		// lock order
		std::recursive_mutex recieve;
	} m_crit;
	*/
	std::shared_ptr<sf::UdpSocket>	m_socket;
	
	sf::IpAddress	m_remoteAddress;
	unsigned short	m_remotePort;
	
	u8				m_header;

	//ack stuff for resending
	u16				m_mySequenceNumber;

	//ack stuff for to tell them what we havnt recieved
	u16				m_theirSequenceNumber;
	udpBitType		m_missingBitField;
	
	u16				m_theirLastAck;

	u16				m_expectedSequence;
	u16				m_nextInOrder;

	// -- buffers
	std::queue<sf::Packet>			m_toBeSent;
	std::queue<sf::Packet>			m_recievedMess;

	std::map <u16, Palette>			m_backupMess;
	
	std::map<u16, sf::Packet>		m_bufferMess;
	std::priority_queue<Palette>	m_resend;

	// -- timer
	Common::Timer				m_keepAlive;
	Common::Timer				m_sendAck;
	u64							m_ackTime;

	float						m_disconnectTime;
	bool						m_sentEmptyAck;

};