#pragma once

#include <sstream>
#include <SFML/Network.hpp>
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "InputCommon/InputConfig.h"
#include "Core/HW/Wiimote.h"

#define WATCH_TIMEOUT 50 // update 20 times per second.
#define HIJACK_TIMEOUT 500
#define NUM_WIIMOTES 4

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

	void Init(unsigned short port);
	void Shutdown();
	void process(Client &client, string &line);
	void checkSubs(Client &client);
	void update();
	void send(sf::TcpSocket &socket, string& data);
	void setVolume(int v);
	void sendFeedback(Client &client, bool success);

	WiimoteEmu::Wiimote* getWiimote(int i_wiimote);
	void sendButtons(int i_wiimote, u16 _buttons);
	void checkHijacks();

}