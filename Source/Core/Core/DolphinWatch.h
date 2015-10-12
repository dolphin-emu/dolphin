#pragma once

#include <sstream>
#include <SFML/Network.hpp>
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "InputCommon/InputConfig.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/GCPadEmu.h"
#include "Core/HW/GCPad.h"
#include "DolphinWX/Frame.h"

#define WATCH_TIMEOUT 50 // update 20 times per second.
#define HIJACK_TIMEOUT 500
#define NUM_WIIMOTES 4
#define NUM_GCPADS 4

namespace DolphinWatch {

	using namespace std;
	typedef uint32_t u32;

	struct Subscription {
		const u32 addr;
		u32 mode;
		u32 prev = ~0;
		Subscription(u32 val, u32 len) : addr(val), mode(len) {}
		bool operator=(Subscription &other) { return other.addr == addr && other.mode == mode; }
	};

	struct SubscriptionMulti {
		const u32 addr;
		u32 size;
		vector<u32> prev;
		SubscriptionMulti(u32 val, u32 len) : addr(val), size(len), prev(len, ~0) {}
		bool operator=(SubscriptionMulti &other) { return other.addr == addr && other.size == size; }
	};

	struct Client {
		shared_ptr<sf::TcpSocket> socket;
		vector<Subscription> subs;
		vector<SubscriptionMulti> subsMulti;
		bool disconnected = false;
		stringstream buf;
		// some stl algorithm support
		bool operator=(Client other) {
			return socket->getRemoteAddress() == other.socket->getRemoteAddress()
			&& socket->getRemotePort() == other.socket->getRemotePort();
		}
		Client(shared_ptr<sf::TcpSocket> sock) : socket(sock) {}
		// explicit copy semantics for smart pointer socket and stringstream
		Client(const Client& other) { socket = other.socket; }
	};

	void Init(unsigned short port, CFrame* main_frame);
	void Shutdown();
	void Process(Client& client, string& line);
	void CheckSubs(Client& client);
	void PollClient(Client& client);
	void Poll();
	void Send(sf::TcpSocket& socket, string& message);
	void SetVolume(int v);
	void SendFeedback(Client& client, bool success);

	WiimoteEmu::Wiimote* GetWiimote(int i_wiimote);
	GCPad* GetGCPad(int i_pad);
	void SendButtonsWii(int i_wiimote, u16 _buttons);
	void SendButtonsGC(int i_pad, u16 _buttons);
	void CheckHijacks();

}