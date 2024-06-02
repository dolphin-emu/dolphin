#include "SlippiSpectate.h"
#include <Core/Config/MainSettings.h>
#include <Core/ConfigManager.h>
#include "Common/Base64.hpp"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Version.h"

// Networking
#ifdef _WIN32
#include <share.h>
#include <ws2tcpip.h>
#else
#include <errno.h>
#endif

inline bool isSpectatorEnabled()
{
  return Config::Get(Config::SLIPPI_ENABLE_SPECTATOR);
}

SlippiSpectateServer& SlippiSpectateServer::getInstance()
{
  static SlippiSpectateServer instance;
  return instance;
}

void SlippiSpectateServer::write(u8* payload, u32 length)
{
  if (isSpectatorEnabled())
  {
    std::string str_payload((char*)payload, length);
    m_event_queue.Push(str_payload);
  }
}

// CALLED FROM DOLPHIN MAIN THREAD
void SlippiSpectateServer::startGame()
{
  json start_game_message;
  start_game_message["type"] = "start_game";
  m_event_queue.Push(start_game_message.dump());
}

// CALLED FROM DOLPHIN MAIN THREAD
void SlippiSpectateServer::endGame(bool dolphin_closed)
{
  json end_game_message;
  end_game_message["type"] = "end_game";
  end_game_message["dolphin_closed"] = dolphin_closed;
  m_event_queue.Push(end_game_message.dump());
}

// CALLED FROM SERVER THREAD
void SlippiSpectateServer::writeEvents(u16 peer_id)
{
  // Send menu events
  if (!m_in_game && (m_sockets[peer_id]->m_menu_cursor != m_menu_cursor))
  {
    ENetPacket* packet =
        enet_packet_create(m_menu_event.data(), m_menu_event.length(), ENET_PACKET_FLAG_RELIABLE);
    // Batch for sending
    enet_peer_send(m_sockets[peer_id]->m_peer, 0, packet);
    // Record for the peer that it was sent
    m_sockets[peer_id]->m_menu_cursor = m_menu_cursor;
  }

  // Send game events
  // Loop through each event that needs to be sent
  //  send all the events starting at their cursor
  // If the client's cursor is beyond the end of the event buffer, then
  //  it's probably left over from an old game. (Or is invalid anyway)
  //  So reset it back to 0
  if (m_sockets[peer_id]->m_cursor > m_event_buffer.size())
  {
    m_sockets[peer_id]->m_cursor = 0;
  }

  for (u64 i = m_sockets[peer_id]->m_cursor; i < m_event_buffer.size(); i++)
  {
    ENetPacket* packet = enet_packet_create(m_event_buffer[i].data(), m_event_buffer[i].size(),
                                            ENET_PACKET_FLAG_RELIABLE);
    // Batch for sending
    enet_peer_send(m_sockets[peer_id]->m_peer, 0, packet);
    m_sockets[peer_id]->m_cursor++;
  }
}

// CALLED FROM SERVER THREAD
void SlippiSpectateServer::popEvents()
{
  // Loop through the event queue and keep popping off events and handling them
  while (!m_event_queue.Empty())
  {
    std::string event;
    m_event_queue.Pop(event);
    // These two are meta-events, used to signify the start/end of a game
    json json_message = json::parse(event, nullptr, false);
    if (!json_message.is_discarded() && (json_message.find("type") != json_message.end()))
    {
      if (json_message["type"] == "end_game")
      {
        u32 cursor = (u32)(m_event_buffer.size() + m_cursor_offset);
        json_message["cursor"] = cursor;
        json_message["next_cursor"] = cursor + 1;
        m_menu_cursor = 0;
        m_event_buffer.push_back(json_message.dump());
        m_cursor_offset += m_event_buffer.size();
        m_menu_event.clear();
        m_in_game = false;
        continue;
      }
      else if (json_message["type"] == "start_game")
      {
        m_event_buffer.clear();
        u32 cursor = (u32)(m_event_buffer.size() + m_cursor_offset);
        m_in_game = true;
        json_message["cursor"] = cursor;
        json_message["next_cursor"] = cursor + 1;
        m_event_buffer.push_back(json_message.dump());
        continue;
      }
    }

    // Make json wrapper for game event
    json game_event;

    if (!m_in_game)
    {
      game_event["payload"] = base64::Base64::Encode(event);
      m_menu_cursor += 1;
      game_event["type"] = "menu_event";
      m_menu_event = game_event.dump();
      continue;
    }

    u8 command = (u8)event[0];
    m_event_concat = m_event_concat + event;

    static std::unordered_map<u8, bool> sendEvents = {
        {0x36, true},  // GAME_INIT
        {0x3C, true},  // FRAME_END
        {0x39, true},  // GAME_END
        {0x10, true},  // SPLIT_MESSAGE
    };

    if (sendEvents.count(command))
    {
      u32 cursor = (u32)(m_event_buffer.size() + m_cursor_offset);
      game_event["payload"] = base64::Base64::Encode(m_event_concat);
      game_event["type"] = "game_event";
      game_event["cursor"] = cursor;
      game_event["next_cursor"] = cursor + 1;
      m_event_buffer.push_back(game_event.dump());

      m_event_concat = "";
    }
  }
}

// CALLED ONCE EVER, DOLPHIN MAIN THREAD
SlippiSpectateServer::SlippiSpectateServer()
{
  if (isSpectatorEnabled())
  {
    m_in_game = false;
    m_menu_cursor = 0;
    m_menu_event.clear();
    m_cursor_offset = 0;

    // Spawn thread for socket listener
    m_stop_socket_thread = false;
    m_socketThread = std::thread(&SlippiSpectateServer::SlippicommSocketThread, this);
  }
}

// CALLED FROM DOLPHIN MAIN THREAD
SlippiSpectateServer::~SlippiSpectateServer()
{
  // The socket thread will be blocked waiting for input
  // So to wake it up, let's connect to the socket!
  m_stop_socket_thread = true;
  if (m_socketThread.joinable())
  {
    m_socketThread.join();
  }
}

// CALLED FROM SERVER THREAD
void SlippiSpectateServer::handleMessage(u8* buffer, u32 length, u16 peer_id)
{
  // Unpack the message
  std::string message((char*)buffer, length);
  json json_message = json::parse(message, nullptr, false);
  if (!json_message.is_discarded() && (json_message.find("type") != json_message.end()))
  {
    // Check what type of message this is
    if (!json_message["type"].is_string())
    {
      return;
    }

    if (json_message["type"] == "connect_request")
    {
      // Get the requested cursor
      if (json_message.find("cursor") == json_message.end())
      {
        return;
      }
      if (!json_message["cursor"].is_number_integer())
      {
        return;
      }
      u32 requested_cursor = json_message["cursor"];
      u32 sent_cursor = 0;
      // Set the user's cursor position
      if (requested_cursor >= m_cursor_offset)
      {
        // If the requested cursor is past what events we even have, then just tell them to start
        // over
        if (requested_cursor > m_event_buffer.size() + m_cursor_offset)
        {
          m_sockets[peer_id]->m_cursor = 0;
        }
        // Requested cursor is in the middle of a live match, events that we have
        else
        {
          m_sockets[peer_id]->m_cursor = requested_cursor - m_cursor_offset;
        }
      }
      else
      {
        // The client requested a cursor that was too low. Bring them up to the present
        m_sockets[peer_id]->m_cursor = 0;
      }

      sent_cursor = (u32)m_sockets[peer_id]->m_cursor + (u32)m_cursor_offset;

      // If someone joins while at the menu, don't catch them up
      //  set their cursor to the end
      if (!m_in_game)
      {
        m_sockets[peer_id]->m_cursor = m_event_buffer.size();
      }

      json reply;
      reply["type"] = "connect_reply";
      reply["nick"] = "Slippi Online";
      reply["version"] = Common::GetSemVerStr();
      reply["cursor"] = sent_cursor;

      std::string packet_buffer = reply.dump();

      ENetPacket* packet = enet_packet_create(packet_buffer.data(), (u32)packet_buffer.length(),
                                              ENET_PACKET_FLAG_RELIABLE);

      // Batch for sending
      enet_peer_send(m_sockets[peer_id]->m_peer, 0, packet);
      // Put the client in the right in_game state
      m_sockets[peer_id]->m_shook_hands = true;
    }
  }
}

void SlippiSpectateServer::SlippicommSocketThread(void)
{
  if (enet_initialize() != 0)
  {
    WARN_LOG_FMT(SLIPPI, "An error occurred while initializing spectator server.");
    return;
  }

  ENetAddress server_address = {0};
  server_address.host = ENET_HOST_ANY;
  server_address.port = Config::Get(Config::SLIPPI_SPECTATOR_LOCAL_PORT);

  // Create the spectator server
  // This call can fail if the system is already listening on the specified port
  //  or for some period of time after it closes down. You basically have to just
  //  retry until the OS lets go of the port and we can claim it again
  //  This typically only takes a few seconds
  ENetHost* server = enet_host_create(&server_address, MAX_CLIENTS, 2, 0, 0);
  int tries = 0;
  while (server == nullptr && tries < 20)
  {
    server = enet_host_create(&server_address, MAX_CLIENTS, 2, 0, 0);
    tries += 1;
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }

  if (server == nullptr)
  {
    WARN_LOG_FMT(SLIPPI, "Could not create spectator server");
    enet_deinitialize();
    return;
  }

  // Main slippicomm server loop
  while (1)
  {
    // If we're told to stop, then quit
    if (m_stop_socket_thread)
    {
      enet_host_destroy(server);
      enet_deinitialize();
      return;
    }

    // Pop off any events in the queue
    popEvents();

    std::map<u16, std::shared_ptr<SlippiSocket>>::iterator it = m_sockets.begin();
    for (; it != m_sockets.end(); it++)
    {
      if (it->second->m_shook_hands)
      {
        writeEvents(it->first);
      }
    }

    ENetEvent event;
    while (enet_host_service(server, &event, 1) > 0)
    {
      switch (event.type)
      {
      case ENET_EVENT_TYPE_CONNECT:
      {
        INFO_LOG_FMT(SLIPPI, "A new spectator connected from {:x}:{}.\n", event.peer->address.host,
                     event.peer->address.port);

        std::shared_ptr<SlippiSocket> newSlippiSocket(new SlippiSocket());
        newSlippiSocket->m_peer = event.peer;
        m_sockets[event.peer->incomingPeerID] = newSlippiSocket;
        break;
      }
      case ENET_EVENT_TYPE_RECEIVE:
      {
        handleMessage(event.packet->data, (u32)event.packet->dataLength,
                      event.peer->incomingPeerID);
        /* Clean up the packet now that we're done using it. */
        enet_packet_destroy(event.packet);

        break;
      }
      case ENET_EVENT_TYPE_DISCONNECT:
      {
        INFO_LOG_FMT(SLIPPI, "A spectator disconnected from {:x}:{}.\n", event.peer->address.host,
                     event.peer->address.port);

        // Delete the item in the m_sockets map
        m_sockets.erase(event.peer->incomingPeerID);
        /* Reset the peer's client information. */
        event.peer->data = NULL;
        break;
      }
      default:
      {
        INFO_LOG_FMT(SLIPPI, "Spectator sent an unknown ENet event type");
        break;
      }
      }
    }
  }

  enet_host_destroy(server);
  enet_deinitialize();
}
