// This file is public domain, in case it's useful to anyone. -comex

#pragma once
#include <functional>
#include <list>
#include <memory>
#include "Common/Common.h"
#include "Common/Thread.h"
#include "Common/TraversalProto.h"

#include "enet/enet.h"

class TraversalClientClient
{
public:
	virtual ~TraversalClientClient(){};
	virtual void OnTraversalStateChanged()=0;
	virtual void OnConnectReady(ENetAddress addr)=0;
	virtual void OnConnectFailed(u8 reason)=0;
};

class TraversalClient
{
public:
	enum State
	{
		Connecting,
		Connected,
		Failure
	};

	enum FailureReason
	{
		BadHost = 0x300,
		VersionTooOld,
		ServerForgotAboutUs,
		SocketSendError,
		ResendTimeout,
		ConnectFailedError = 0x400,
	};

	TraversalClient(ENetHost* netHost, const std::string& server);
	~TraversalClient();
	void Reset();
	void ConnectToClient(const std::string& host);
	void ReconnectToServer();
	void Update();

	// called from NetHost
	bool TestPacket(u8* data, size_t size, ENetAddress* from);
	void HandleResends();

	ENetHost* m_NetHost;
	TraversalClientClient* m_Client;
	TraversalHostId m_HostId;
	State m_State;
	int m_FailureReason;

private:
	struct OutgoingTraversalPacketInfo
	{
		TraversalPacket packet;
		int tries;
		enet_uint32 sendTime;
	};
	
	void HandleServerPacket(TraversalPacket* packet);
	void ResendPacket(OutgoingTraversalPacketInfo* info);
	TraversalRequestId SendTraversalPacket(const TraversalPacket& packet);
	void OnFailure(int reason);
	void HandlePing();
	static int ENET_CALLBACK InterceptCallback(ENetHost* host, ENetEvent* event);

	TraversalRequestId m_ConnectRequestId;
	bool m_PendingConnect;
	std::list<OutgoingTraversalPacketInfo> m_OutgoingTraversalPackets;
	ENetAddress m_ServerAddress;
	std::string m_Server;
	enet_uint32 m_PingTime;
};

extern std::unique_ptr<TraversalClient> g_TraversalClient;
// the NetHost connected to the TraversalClient.
extern std::unique_ptr<ENetHost> g_MainNetHost;

// Create g_TraversalClient and g_MainNetHost if necessary.
bool EnsureTraversalClient(const std::string& server, u16 port);
void ReleaseTraversalClient();
