// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


#include "ReliableUDPConnection.h"

ReliableUDPConnection::ReliableUDPConnection(std::shared_ptr<sf::UdpSocket> sock, sf::IpAddress adr, u16 port)
: m_socket(sock)
, m_remoteAddress(adr)
, m_remotePort(port)
, m_expectedSequence(1)//1
, m_nextInOrder(1)//1
, m_header(117) //probably should make this an agreed number upon accepting a connection
, m_mySequenceNumber(1)//1
, m_theirSequenceNumber(0)//0
, m_missingBitField(UINT32_MAX)
, m_ackTime(20)
, m_disconnectTime(10.0)
, m_sentEmptyAck(false)
{
	m_keepAlive.Start();
	m_sendAck.Start();
}

ReliableUDPConnection::~ReliableUDPConnection()
{
}

void ReliableUDPConnection::StoreSend(sf::Packet& packet)
{
	//will be changed to add size checks and such when i decide what a max packet size should be
	
	m_toBeSent.push(packet);

	
}


sf::Socket::Status ReliableUDPConnection::Send(bool sendAck)
{
	
	if (m_sentEmptyAck && (!m_resend.empty() || !m_toBeSent.empty()))
	{
		// -- if the last message was an ack and now we have messages to send make sure that the sequence is a new one
		++m_mySequenceNumber;
		if (m_mySequenceNumber == UINT16_MAX)
		{
			m_mySequenceNumber = 0;
		}
		m_sentEmptyAck = false;
	}

	//header: protocal ID (8 bit)|ack (16)|nack(16)|missing bit field (32)| packet order (16)| packet|
	//add header id, ack, nack, and 32 bit field
	sf::Packet pack;
	pack << m_header; // protocal ID
	pack << m_mySequenceNumber; //our current ack
	pack << m_theirSequenceNumber; //the last ack we received
	pack << m_missingBitField; //field of messages we are missing
	
	// -- Send
	if (!m_resend.empty())
	{
		// -- They had dropped a message so it must be resent!
		pack << m_resend.front().packetOrder;
		pack.append(m_resend.front().packet.getData(), m_resend.front().packet.getDataSize());
		m_backupMess[m_mySequenceNumber] = Palette(m_resend.front().packetOrder, m_resend.front().packet);
		
		m_resend.pop();

		sf::Socket::Status stat;
		stat = m_socket->send(pack, m_remoteAddress, m_remotePort);

		++m_mySequenceNumber;
		if (m_mySequenceNumber == UINT16_MAX)
		{
			m_mySequenceNumber = 0;
		}

		m_sendAck.Stop();
		m_sendAck.Start();

		return stat;
	}
	else if (!m_toBeSent.empty())
	{

		// -- Send A Message from our queue
		pack << m_nextInOrder;

		pack.append(m_toBeSent.front().getData(), m_toBeSent.front().getDataSize());
		m_backupMess[m_mySequenceNumber] = Palette(m_nextInOrder, m_toBeSent.front());
		
		m_toBeSent.pop();
		++m_nextInOrder;
		if (m_nextInOrder == UINT16_MAX)
		{
			m_nextInOrder = 0;
		}

		sf::Socket::Status stat;
		stat = m_socket->send(pack, m_remoteAddress, m_remotePort);

		++m_mySequenceNumber;
		if (m_mySequenceNumber == UINT16_MAX)
		{
			m_mySequenceNumber = 0;
		}

		m_sendAck.Stop();
		m_sendAck.Start();

		return stat;
	}
	
	if (sendAck && m_sendAck.GetTimeDifference() > m_ackTime)
	{ 
		// -- else we have no info to send so just send an ACK instead
		m_sentEmptyAck = true;
		pack << UINT16_MAX;
		sf::Socket::Status stat;
		stat = m_socket->send(pack, m_remoteAddress, m_remotePort);
		
		m_sendAck.Stop();
		m_sendAck.Start();
		
		return stat;
	}
	
	return sf::Socket::NotReady;
}


sf::Socket::Status ReliableUDPConnection::SendUnreliable(sf::Packet& packet)
{
	// -- add header id and special message ack
	sf::Packet pack;
	pack << m_header;
	u16 neg = UINT16_MAX;
	pack << neg; 
	pack.append(packet.getData(), packet.getDataSize());

	sf::Socket::Status stat;
	stat = m_socket->send(pack, m_remoteAddress, m_remotePort);

	return stat;
}



bool ReliableUDPConnection::Receive(sf::Packet& packet)
{
	// -- we recieved something so we need to update the timer
	m_keepAlive.Stop();
	m_keepAlive.Start();

	u16 lastGivenAck = 0;
	u32 resendBitField = 0;

	// -- save their last given sequence number before replacing it
	u16 previousSequence = m_theirSequenceNumber; 
	++previousSequence;
	if (previousSequence == UINT16_MAX)
	{
		previousSequence = 0;
	}

	// -- unpack header with make a new m_theirSequenceNumber
	u8 headerCheck;
	packet >> headerCheck;
	if (m_header != headerCheck)
	{
		u16 check;
		packet >> check;
		if (m_header == 0 && check == UINT16_MAX)
		{
			m_disconnectTime = 0;
		}
		return false;
	}
	
	// -- get the received sequence number
	u16 uiReceived;
	packet >> uiReceived;	

	// -- If its a special unreliable sent just push on as is
	if (uiReceived == UINT16_MAX)
	{
		m_recievedMess.push(packet);
		return true;
	}
	
	
	// -- unpack the rest of the header
	m_theirSequenceNumber = uiReceived;
	int prevSeq = IfWrappedConvertToNeg(m_theirSequenceNumber, previousSequence, UINT16_MAX);
	int theirSeq = IfWrappedConvertToNeg(previousSequence, m_theirSequenceNumber, UINT16_MAX);

	// -- First check if this message is the next, old, or skipped over other messages to request to send new messages
	if (previousSequence == m_theirSequenceNumber)
	{
		// -- shift _missingBitField with 1 at the end 1101 -> 1011
		m_missingBitField = (m_missingBitField << 1) | 1;
	}
	else if (prevSeq<theirSeq)
	{
		// -- this has skipped over 1 or more messages, skip over to make the proper amount of 0 in bitfield
		m_missingBitField = (m_missingBitField << (theirSeq - (prevSeq - 1))) | 1;

		if ((theirSeq - prevSeq)>30)
		{
			// -- We missed far to many messages lets just disconnect
			Disconnect();
			return false;
		}
	}	
		
	packet >> lastGivenAck;
	packet >> resendBitField;

	// -- If the bit field has missing message we need to make sure they get resent
	UpdateBackUp(lastGivenAck, resendBitField);
	
	// -- Grab the order id
	u16 nextPacket;
	packet >> nextPacket;
	
	// -- Check if this was just an update ack
	if (nextPacket == UINT16_MAX)
	{
		return false;
	}

	// -- properly wrap the order id
	int next = IfWrappedConvertToNeg(m_expectedSequence, nextPacket, UINT16_MAX);
	int expected = IfWrappedConvertToNeg(nextPacket, m_expectedSequence, UINT16_MAX);

	// -- check wether we need to drop, take, or store this message
	if (nextPacket == m_expectedSequence)
	{
		m_recievedMess.push(packet);

		++m_expectedSequence;
		if (m_expectedSequence == UINT16_MAX)
			m_expectedSequence = 0;

		//empty out our buffer for messages that came too soon
		auto itr = m_bufferMess.find(m_expectedSequence);

		while (itr != m_bufferMess.end())
		{
			m_recievedMess.push(itr->second);
			++m_expectedSequence;
			if (m_expectedSequence == UINT16_MAX)
				m_expectedSequence = 0;

			m_bufferMess.erase(itr);
			itr = m_bufferMess.find(m_expectedSequence);
		}

		return true;
	}
	else if (next>expected)
	{
		m_bufferMess[nextPacket] = packet;
	}

	return false;
}

bool ReliableUDPConnection::GrabMessage(sf::Packet& packet)
{
	if (!m_recievedMess.empty())
	{
		packet = m_recievedMess.front();
		m_recievedMess.pop();
		return true;
	}
	return false;

}

bool ReliableUDPConnection::CheckIfAlive()
{
	if (m_keepAlive.GetTimeDifference() > m_disconnectTime * 1000)
	{
		return false;
	}
	return true;
}

void ReliableUDPConnection::Disconnect()
{
	sf::Packet pack;
	u8 disHeader = 0;
	pack << disHeader;
	u16 filler = UINT16_MAX;
	pack << filler; //sets my sequence number to neg 1 to say that it isn't a regular message

	sf::Socket::Status stat;
	stat = m_socket->send(pack, m_remoteAddress, m_remotePort);

}

void  ReliableUDPConnection::ClearBuffers()
{
	while (!m_toBeSent.empty())
	{
		m_toBeSent.pop();
	}

	while (!m_recievedMess.empty())
	{
		m_recievedMess.pop();
	}

}

bool ReliableUDPConnection::WasRecieved(const u32& bitField, const u16& currentAck, const u16& ackCheck)
{
	int offset = currentAck - ackCheck;
	if (offset < 0)
		return false;

	u32 check = 1 << offset;

	return (bitField & check)!=0;
}


void ReliableUDPConnection::UpdateBackUp(u16 pAck, u32 pBitfield)
{

	if (m_backupMess.empty())
	{
		return;
	}

	//--remove the given sequence number because we know that one is already in there
	m_backupMess.erase(pAck);

	// -- loop through the bit field
	for (u16 n =  sizeof(pBitfield)* 8; n > 0; --n)
	{
		//n is what bit we are on (starts on 32 because 0 is our current ack and is always =1)

		//--last ack is the int representation
		u16 lastAck;

		if (pAck >= n)
		{
			lastAck = pAck - n;
		}
		else
		{
			lastAck = (_UI16_MAX) - (n - pAck);
		}


		if ((pBitfield & (1 << n)) != 0)
		{
			m_backupMess.erase(lastAck);
		}
		else
		{
			auto itr = m_backupMess.find(lastAck);

			if (itr != m_backupMess.end())
			{
				m_resend.push(itr->second);

				m_backupMess.erase(lastAck);
			}
		}
	}
}

int  ReliableUDPConnection::IfWrappedConvertToNeg(int current, int previous, int max)
{
	if (current<previous && (previous - current)>(max - 32))
	{
		return (previous)-(_UI16_MAX);
	}
	return previous;
}