#pragma once

#include <atomic>
#include <chrono>
#include <map>
#include <thread>

#include <enet/enet.h>
#include "Common/SPSCQueue.h"
#pragma warning(push, 0)
#include "nlohmann/json.hpp"
#pragma warning(pop)
using json = nlohmann::json;

// Sockets in windows are unsigned
#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <sys/select.h>
typedef int SOCKET;
#endif

#define MAX_CLIENTS 4

#define HANDSHAKE_MSG_BUF_SIZE 128
#define HANDSHAKE_TYPE 1
#define PAYLOAD_TYPE 2
#define KEEPALIVE_TYPE 3
#define MENU_TYPE 4

class SlippiSocket
{
public:
  u64 m_cursor = 0;            // Index of the last game event this client sent
  u64 m_menu_cursor = 0;       // The latest menu event that this socket has sent
  bool m_shook_hands = false;  // Has this client shaken hands yet?
  ENetPeer* m_peer = NULL;     // The ENet peer object for the socket
};

class SlippiSpectateServer
{
public:
  // Singleton. Get an instance of the class here
  //   When SConfig::GetInstance().m_slippiNetworkingOutput is false, this
  //  instance exists and is callable, but does nothing
  static SlippiSpectateServer& getInstance();

  // Write the given game payload data to all listening sockets
  void write(u8* payload, u32 length);

  // Should be called each time a new game starts.
  //  This will clear out the old game event buffer and start a new one
  void startGame();

  // Clear the game event history buffer. Such as when a game ends.
  //  The slippi server keeps a history of events in a buffer. So that
  //  when a new client connects to the server mid-match, it can recieve all
  //  the game events that have happened so far. This buffer needs to be
  //  cleared when a match ends.
  // If this is called due to dolphin closing then dolphin_closed will be true
  void endGame(bool dolphin_closed = false);

private:
  // ACCESSED FROM BOTH DOLPHIN AND SERVER THREADS
  // This is a lockless queue that bridges the gap between the main
  //  dolphin thread and the spectator server thread. The purpose here
  //  is to avoid blocking (even if just for a brief mutex) on the main
  //  dolphin thread.
  Common::SPSCQueue<std::string> m_event_queue;
  // Bool gets flipped by the destrctor to tell the server thread to shut down
  //  bools are probably atomic by default, but just for safety...
  std::atomic<bool> m_stop_socket_thread;

  // ONLY ACCESSED FROM SERVER THREAD
  bool m_in_game = false;
  std::map<u16, std::shared_ptr<SlippiSocket>> m_sockets;
  std::string m_event_concat = "";
  std::vector<std::string> m_event_buffer;
  std::string m_menu_event;
  // In order to emulate Wii behavior, the cursor position should be strictly
  //  increasing. But internally, we need to index arrays by the cursor value.
  //  To solve this, we keep an "offset" value that is added to all outgoing
  //  cursor positions to give the appearance like it's going up
  u64 m_cursor_offset = 0;
  //  How many menu events have we sent so far? (Reset between matches)
  //    Is used to know when a client hasn't been sent a menu event
  u64 m_menu_cursor = 0;

  std::thread m_socketThread;

  SlippiSpectateServer();
  ~SlippiSpectateServer();
  SlippiSpectateServer(SlippiSpectateServer const&) = delete;
  SlippiSpectateServer& operator=(const SlippiSpectateServer&) = delete;
  SlippiSpectateServer(SlippiSpectateServer&&) = delete;
  SlippiSpectateServer& operator=(SlippiSpectateServer&&) = delete;

  // FUNCTIONS CALLED ONLY FROM SERVER THREAD
  // Server thread. Accepts new incoming connections and goes back to sleep
  void SlippicommSocketThread(void);
  // Handle an incoming message on a socket
  void handleMessage(u8* buffer, u32 length, u16 peer_id);
  // Catch up given socket to the latest events
  //  Does nothing if they're already caught up.
  void writeEvents(u16 peer_id);
  // Pop events
  void popEvents();
};
