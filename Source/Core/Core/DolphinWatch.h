#pragma once

#include <sstream>
#include <SFML/Network.hpp>

namespace DolphinWatch {

	using namespace std;
	typedef uint32_t u32;

	/*void Init();
	void Shutdown();
	void Subscribe(sockaddr_in si, u32 addr);
	void Unsubscribe(sockaddr_in si, u32 addr);
	void RecvLoop();
	void SubscribeLoop();*/

	struct Subscription {
		const u32 addr;
		mutable u32 prev = 0;
		Subscription(u32 val) : addr(val) {}
		bool operator=(Subscription other) { return other.addr == addr; }
	};

	struct Client {
		shared_ptr<sf::TcpSocket> socket;
		vector<Subscription> subs;
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
	void update();
	void send(sf::TcpSocket &socket, string& data);

}