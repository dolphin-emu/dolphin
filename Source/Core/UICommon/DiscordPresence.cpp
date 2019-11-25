// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "UICommon/DiscordPresence.h"

#include "Common/Hash.h"
#include "Common/StringUtil.h"

#include "Core/Config/NetplaySettings.h"
#include "Core/Config/UISettings.h"
#include "Core/ConfigManager.h"

#ifdef USE_DISCORD_PRESENCE

#include <Core/Discord.h>
#include <algorithm>
#include <cctype>
#include <ctime>
#include <discord-rpc/include/discord_rpc.h>
#include <string>
#endif

namespace Discord
{
#ifdef USE_DISCORD_PRESENCE
namespace
{
Handler* event_handler = nullptr;
const char* username = "";

void HandleDiscordReady(const DiscordUser* user)
{
  username = user->username;
}

void HandleDiscordJoinRequest(const DiscordUser* user)
{
  if (event_handler == nullptr)
    return;

  const std::string discord_tag = StringFromFormat("%s#%s", user->username, user->discriminator);
  event_handler->DiscordJoinRequest(user->userId, discord_tag, user->avatar);
}

void HandleDiscordJoin(const char* join_secret)
{
  if (event_handler == nullptr)
    return;

  if (Config::Get(Config::NETPLAY_NICKNAME) == Config::NETPLAY_NICKNAME.default_value)
    Config::SetCurrent(Config::NETPLAY_NICKNAME, username);

  std::string secret(join_secret);

  std::string type = secret.substr(0, secret.find('\n'));
  size_t offset = type.length() + 1;

  switch (static_cast<SecretType>(std::stol(type)))
  {
  default:
  case SecretType::Empty:
    return;

  case SecretType::IPAddress:
  {
    // SetBaseOrCurrent will save the ip address, which isn't what's wanted in this situation
    Config::SetCurrent(Config::NETPLAY_TRAVERSAL_CHOICE, "direct");

    std::string host = secret.substr(offset, secret.find_last_of(':') - offset);
    Config::SetCurrent(Config::NETPLAY_ADDRESS, host);

    offset += host.length();
    if (secret[offset] == ':')
      Config::SetCurrent(Config::NETPLAY_CONNECT_PORT, std::stoul(secret.substr(offset + 1)));
  }
  break;

  case SecretType::RoomID:
  {
    Config::SetCurrent(Config::NETPLAY_TRAVERSAL_CHOICE, "traversal");

    Config::SetCurrent(Config::NETPLAY_HOST_CODE, secret.substr(offset));
  }
  break;
  }

  event_handler->DiscordJoin();
}

}  // namespace
#endif

Discord::Handler::~Handler() = default;

void Init()
{
#ifdef USE_DISCORD_PRESENCE
  if (!Config::Get(Config::MAIN_USE_DISCORD_PRESENCE))
    return;

  DiscordEventHandlers handlers = {};

  handlers.ready = HandleDiscordReady;
  handlers.joinRequest = HandleDiscordJoinRequest;
  handlers.joinGame = HandleDiscordJoin;
  // The number is the client ID for Dolphin, it's used for images and the application name
  Discord_Initialize("554345859526033411", &handlers, 1, nullptr);
  UpdateDiscordPresence();
#endif
}

void CallPendingCallbacks()
{
#ifdef USE_DISCORD_PRESENCE
  if (!Config::Get(Config::MAIN_USE_DISCORD_PRESENCE))
    return;

  Discord_RunCallbacks();

#endif
}

void InitNetPlayFunctionality(Handler& handler)
{
#ifdef USE_DISCORD_PRESENCE
  event_handler = &handler;
#endif
}

void UpdateDiscordPresence(int party_size, SecretType type, const std::string& secret,
                           const std::string& current_game)
{
}

std::string CreateSecretFromIPAddress(const std::string& ip_address, int port)
{
  const std::string port_string = std::to_string(port);
  std::string secret;
  secret.reserve(ip_address.length() + 1 + port_string.length());
  secret += ip_address;
  secret += ':';
  secret += port_string;
  return secret;
}

void Shutdown()
{
#ifdef USE_DISCORD_PRESENCE
  if (!Config::Get(Config::MAIN_USE_DISCORD_PRESENCE))
    return;

  Discord_ClearPresence();
  Discord_Shutdown();
#endif
}

void SetDiscordPresenceEnabled(bool enabled)
{
  if (Config::Get(Config::MAIN_USE_DISCORD_PRESENCE) == enabled)
    return;

  if (Config::Get(Config::MAIN_USE_DISCORD_PRESENCE))
    Discord::Shutdown();

  Config::SetBase(Config::MAIN_USE_DISCORD_PRESENCE, enabled);

  if (Config::Get(Config::MAIN_USE_DISCORD_PRESENCE))
    Discord::Init();
}

}  // namespace Discord
