// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "ReliableUDPManager.h"
#include "Common/Timer.h"
#include <list>


ReliableUDPManager::ReliableUDPManager(unsigned short port)
	: m_listen(false)
	, m_udpSocket(NULL)
	, m_id(1)
	, m_dropMess(0)
	, m_generator((unsigned int) Common::Timer::GetTimeSinceJan1970)
	, m_distribution(0.0, 1.0)
{
	m_udpSocket = std::make_shared<sf::UdpSocket>();
	if (port != 0)
	{
		m_udpSocket->bind(port);
	}
	else
	{
		m_udpSocket->bind(sf::Socket::AnyPort);
	}
	m_udpSocket->setBlocking(false);


}

ReliableUDPManager::~ReliableUDPManager()
{
	auto itr = m_IDtoConnection.begin();

	for (; itr != m_IDtoConnection.end();)
	{
		
		itr->second->Disconnect();
		//IpAndPort inp(itr->second->GetAddress().toInteger(), itr->second->GetPort());
		std::pair<int, unsigned short> inp = std::make_pair(itr->second->GetAddress().toInteger(), itr->second->GetPort());
		m_AddresstoConnection.erase(inp);
		itr = m_IDtoConnection.erase(itr);

	}

	m_udpSocket->unbind();
	
}

void ReliableUDPManager::Update()
{
	
	sf::IpAddress ip;
	unsigned short port;
	sf::Packet pack;

	//This probably should be a timer rather then just an abiterary number
	for (int i = 0; i < 100; ++i)
	{
		sf::Socket::Status stat = m_udpSocket->receive(pack, ip, port);

		if (stat == sf::Socket::Done)
		{
			//IpAndPort inp(ip.toInteger(), port);
			std::pair<int, unsigned short> inp = std::make_pair(ip.toInteger(), port);
			auto itr = m_AddresstoConnection.find( inp );
			if (m_listen && itr == m_AddresstoConnection.end())
			{
				//This is a new connection store it and start accepting packets
				//Also store it as a new connection ID so the user can decide 
				//if he wants it
				std::shared_ptr<ReliableUDPConnection> newConn = std::make_shared<ReliableUDPConnection>(m_udpSocket, ip, port);
				int id = StoreConnection(newConn);
				m_newConnection.push(id);
				newConn->Receive(pack);
				newConn->Send();
			}
			else if (itr != m_AddresstoConnection.end())
			{
				itr->second->Receive(pack);
			}

		}
	}
	
	//--for now just send all messages in the message buffer
	//--in the future we should just send a certain amount a bytes per millisecond
	std::list<u16> eraseList;
	for (auto c : m_IDtoConnection)
	{
		if (!c.second->CheckIfAlive())
		{
			//if we havn't recieved a message for a long time, get rid of it
			eraseList.push_front(c.first);
			m_disconnection.push(c.first);
			continue;
		}
	
		do
		{
			c.second->Send();
		} while (c.second->isReadyToSend());
		
	}

	for (auto er : eraseList)
	{
		RemoveConnectionStore(er);
	}

}

bool ReliableUDPManager::SendMess(u16 ID, sf::Packet& packet)
{
	auto con = m_IDtoConnection.find(ID);
	if (con != m_IDtoConnection.end())
	{
		con->second->StoreSend(packet);
		return true;
	}
	return false;
}

void ReliableUDPManager::SendMessageToAll(sf::Packet& packet, u16 exceptionID)
{
	for (auto c : m_IDtoConnection)
	{
		if (c.first != exceptionID)
		{
			c.second->StoreSend(packet);
		}
	}
	
}

bool ReliableUDPManager::GrabMessage(u16 ID, sf::Packet& packet)
{
	auto con = m_IDtoConnection.find(ID);

	if (con != m_IDtoConnection.end())
	{
		
		return con->second->GrabMessage(packet);;
	}
	return false;
}

u16 ReliableUDPManager::CheckNewConnections(sf::IpAddress& ip, unsigned short& port)
{
	if (!m_newConnection.empty())
	{
		u16 id = m_newConnection.front();
		m_newConnection.pop();

		ip = m_IDtoConnection[id]->GetAddress();
		port = m_IDtoConnection[id]->GetPort();

		return id;
	}
	return 0;
}


u16 ReliableUDPManager::Connect(std::string ip, u16 port, float wait)
{
	bool oldListen = m_listen;
	m_listen = true;
	sf::Packet pack;
	ReliableUDPConnection newConn(m_udpSocket, ip, port);
	newConn.Send();
	
	Common::Timer sendTimer;
	sendTimer.Start();
	
	Common::Timer giveUpTimer;
	giveUpTimer.Start();

	//keep trying to get a connection until our wait time is over
	do
	{
		
		Update();

		//check new connection list to see if ours was added
		sf::IpAddress ip;
		unsigned short port;
		int id=0;
		do
		{
			int id = CheckNewConnections(ip, port);
			if (id != 0)
			{ 
				if (ip == newConn.GetAddress())
				{	
					
					//if we accidently added other connections 
					//during a non-listening time destroy those connections
					if (!oldListen)
					{
					
						while (!m_newConnection.empty())
						{
							Disconnect(m_newConnection.front());
							m_newConnection.pop();
						}

						m_listen = false;
					}
					
					return id;
				}
				else 
				{

					m_newConnection.push(id);

				}
			}
		} while (id != 0);
		
		u64 waitTill = 30;
		if (sendTimer.GetTimeElapsed()>waitTill)
		{
			newConn.Send();
			sendTimer.Stop();
			sendTimer.Start();
		}
		
	} while (giveUpTimer.GetTimeElapsed() < wait * 1000.0f);

	return 0;
}

void ReliableUDPManager::Disconnect(u16 ID)
{
	auto itr = m_IDtoConnection.find(ID);

	if (itr != m_IDtoConnection.end())
	{
		itr->second->Disconnect();
		RemoveConnectionStore(ID);
	}
}

bool ReliableUDPManager::DisconnectList(u16& returnID)
{
	if (!m_disconnection.empty())
	{
		returnID = m_disconnection.front();
		m_disconnection.pop();
		return true;
	}
	return false;
}

sf::IpAddress ReliableUDPManager::GetAddress(u16 ID)
{
	auto itr = m_IDtoConnection.find(ID);

	if (itr != m_IDtoConnection.end())
	{
		return itr->second->GetAddress();
	}
	return sf::IpAddress();
}
short unsigned ReliableUDPManager::GetPort(u16 ID)
{

	auto itr = m_IDtoConnection.find(ID);

	if (itr != m_IDtoConnection.end())
	{
		return itr->second->GetPort();
	}
	return 0;
}


void ReliableUDPManager::ClearBuffer(u16 ID)
{
	auto itr = m_IDtoConnection.find(ID);

	if (itr != m_IDtoConnection.end())
	{
		itr->second->ClearBuffers();
	}

}

u16 ReliableUDPManager::StoreConnection(std::shared_ptr<ReliableUDPConnection> connection)
{
	//IpAndPort inp(connection->GetAddress().toInteger(), connection->GetPort());
	std::pair<int, unsigned short> inp = std::make_pair(connection->GetAddress().toInteger(), connection->GetPort());
	if (m_AddresstoConnection.find(inp) == m_AddresstoConnection.end())
	{
		u16 id = m_id;

		m_IDtoConnection[id] = connection;
		m_AddresstoConnection[inp] = connection;

		++m_id;
		return id;
	}
	return 0;
}

void ReliableUDPManager::RemoveConnectionStore(u16 ID)
{
	auto itr = m_IDtoConnection.find(ID);
	if (itr != m_IDtoConnection.end())
	{
		//IpAndPort inp(itr->second->GetAddress().toInteger(), itr->second->GetPort());
		std::pair<int, unsigned short> inp = std::make_pair(itr->second->GetAddress().toInteger(), itr->second->GetPort());
		m_AddresstoConnection.erase(inp);
		m_IDtoConnection.erase(itr);
	}
}