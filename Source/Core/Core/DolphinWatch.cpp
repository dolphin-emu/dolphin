
#include <thread>
#include <atomic>
#include <Windows.h>

#include "DolphinWatch.h"
#include "Common/Logging/LogManager.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/HW/Memmap.h"

namespace DolphinWatch {

	using namespace std;

	static sf::TcpListener server;
	static vector<Client> clients;
	static char cbuf[1024];

	static thread thr;
	static atomic<bool> running=true;

	void Init(unsigned short port) {
		server.listen(port);
		// avoid threads or complicated select()'s, just poll in update.
		server.setBlocking(false);

		thr = thread([]() {
			while (running) {
				update();
				Sleep(100);
			}
		});
	}

	void Shutdown() {
		running = false;
		if (thr.joinable()) thr.join();
		// socket closing is implicit for sfml library during destruction
	}

	void process(Client &client, string &line) {
		// turn line into another stream
		istringstream parts(line);
		string cmd;

		NOTICE_LOG(CONSOLE, "PROCESSING %s", line.c_str());

		if (!(parts >> cmd)) {
			// no command, empty line, skip
			NOTICE_LOG(CONSOLE, "empty command line %s", line.c_str());
			return;
		}

		if (cmd == "MEMSET") {

			if (!Memory::IsInitialized()) {
				NOTICE_LOG(CONSOLE, "PowerPC memory not initialized, can't execute command: %s", line.c_str());
				return;
			}

			u32 mode, addr, val;

			if (!(parts >> mode >> addr >> val)) {
				NOTICE_LOG(CONSOLE, "Invalid command line: %s", line.c_str());
				return;
			}

			// Parsing OK
			switch (mode) {
			case 8:
				PowerPC::HostWrite_U8(val, addr);
				break;
			case 16:
				PowerPC::HostWrite_U16(val, addr);
				break;
			case 32:
				PowerPC::HostWrite_U32(val, addr);
				break;
			default:
				NOTICE_LOG(CONSOLE, "Wrong mode for writing, 8/16/32 required as 1st parameter. Command: %s", line.c_str());
			}
		}
		else if (cmd == "MEMGET") {

			if (!Memory::IsInitialized()) {
				NOTICE_LOG(CONSOLE, "PowerPC memory not initialized, can't execute command: %s", line.c_str());
				return;
			}

			u32 addr, val;

			if (!(parts >> addr)) {
				NOTICE_LOG(CONSOLE, "Invalid command line: %s", line.c_str());
				return;
			}

			// Parsing OK
			val = PowerPC::HostRead_U32(addr);

			ostringstream message;
			message << "MEM " << addr << " " << val << endl;
			send(*client.socket, message.str());

		}
		else if (cmd == "SUBSCRIBE") {

			u32 addr;

			if (!(parts >> addr)) {
				// no valid parameters, skip
				NOTICE_LOG(CONSOLE, "Invalid command line: %s", line.c_str());
				return;
			}

			for (auto &sub : client.subs) {
				if (sub.addr == addr) {
					return;
				}
			}
			client.subs.push_back(Subscription(addr));

		}
		else if (cmd == "UNSUBSCRIBE") {

			u32 addr;

			if (!(parts >> addr)) {
				// no valid parameters, skip
				NOTICE_LOG(CONSOLE, "Invalid command line: %s", line.c_str());
				return;
			}

			for (auto iter = client.subs.begin(); iter != client.subs.end(); ++iter) {
				if (iter->addr == addr) {
					client.subs.erase(iter);
					return;
				}
			}

		}
		else {
			NOTICE_LOG(CONSOLE, "Unknown command: %s", cmd.c_str());
		}

	}

	void update() {

		string s;

		// poll for new clients, nonblocking
		auto socket = make_shared<sf::TcpSocket>();
		if (server.accept(*socket) == sf::Socket::Done) {
			socket->setBlocking(false);
			Client client(socket);
			clients.push_back(client);
		}


		// poll incoming data from clients, then process
		for (Client &client : clients) {

			// clean the client's buffer.
			// By default a stringbuffer keeps already read data.
			// a deque would do what we want, by not keeping that data, but then we would not have
			// access to nice stream-features like <</>> operators and getline(), so we do this manually.

			client.buf.clear(); // reset eol flag
			getline(client.buf, s, '\0'); // read everything
			client.buf.clear(); // reset eol flag again
			client.buf.str(""); // empty stringstream
			client.buf << s; // insert rest at beginning again

			size_t received = 0;
			auto status = client.socket->receive(cbuf, sizeof(cbuf) - 1, received);
			if (status == sf::Socket::Status::Disconnected || status == sf::Socket::Status::Error) {
				client.disconnected = true;
			}
			else if (status == sf::Socket::Status::Done) {
				// add nullterminator, then add to client's buffer
				cbuf[received] = '\0';
				client.buf << cbuf;

				// process the client's buffer
				size_t posg = 0;
				while (getline(client.buf, s)) {
					if (client.buf.eof()) {
						client.buf.clear();
						client.buf.seekg(posg);
						break;
					}
					posg = client.buf.tellg();

					// Might contain semicolons to further split several commands.
					// Doing that ensures that those commands are executed at once / in the same emulated frame.
					string s2;
					istringstream subcmds(s);
					while (getline(subcmds, s2, ';')) {
						process(client, s2);
					}
				}
			}

			// check subscriptions
			if (Memory::IsInitialized()) for (auto &sub : client.subs) {
				u32 val = PowerPC::HostRead_U32(sub.addr);
				if (val != sub.prev) {
					sub.prev = val;
					ostringstream message;
					message << "MEM " << sub.addr << " " << val << endl;
					send(*client.socket, message.str());
				}
			}
		}

		// remove disconnected clients
		auto new_end = remove_if(clients.begin(), clients.end(), [](Client &c) {
			return c.disconnected;
		});
		clients.erase(new_end, clients.end());
	}

	void send(sf::TcpSocket &socket, string &message) {
		socket.setBlocking(true);
		socket.send(message.c_str(), message.size());
		socket.setBlocking(false);
	}
}
