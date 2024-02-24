// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "UICommon/DiscordPresence.h"

#include "Core/Config/NetplaySettings.h"
#include "Core/Config/UISettings.h"
#include "Core/ConfigManager.h"

#ifdef USE_DISCORD_PRESENCE

#include <algorithm>
#include <ctime>
#include <set>
#include <string>

#include <discord_rpc.h>
#include <fmt/format.h>

#include "Common/Hash.h"
#include "Common/HttpRequest.h"
#include "Common/StringUtil.h"

#include "Core/AchievementManager.h"
#include "Core/Config/AchievementSettings.h"
#include "Core/System.h"

#endif

namespace Discord
{
#ifdef USE_DISCORD_PRESENCE
static bool s_using_custom_client = false;

namespace
{
Handler* event_handler = nullptr;
const char* username = "";
static int64_t s_start_timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                                       std::chrono::system_clock::now().time_since_epoch())
                                       .count();

void HandleDiscordReady(const DiscordUser* user)
{
  username = user->username;
}

void HandleDiscordJoinRequest(const DiscordUser* user)
{
  if (event_handler == nullptr)
    return;

  const std::string discord_tag = fmt::format("{}#{}", user->username, user->discriminator);
  event_handler->DiscordJoinRequest(user->userId, discord_tag, user->avatar);
}

void HandleDiscordJoin(const char* join_secret)
{
  if (event_handler == nullptr)
    return;

  if (Config::Get(Config::NETPLAY_NICKNAME) == Config::NETPLAY_NICKNAME.GetDefaultValue())
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

std::string ArtworkForGameId()
{
  const DiscIO::Region region = SConfig::GetInstance().m_region;
  const bool is_wii = Core::System::GetInstance().IsWii();
  const std::string region_code = SConfig::GetInstance().GetGameTDBImageRegionCode(is_wii, region);

  static constexpr char cover_url[] = "https://discord.dolphin-emu.org/cover-art/{}/{}.png";
  return fmt::format(cover_url, region_code, SConfig::GetInstance().GetGameTDBID());
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
  Discord_Initialize(DEFAULT_CLIENT_ID.c_str(), &handlers, 1, nullptr);
  UpdateDiscordPresence();
#endif
}

void UpdateClientID(const std::string& new_client)
{
#ifdef USE_DISCORD_PRESENCE
  if (!Config::Get(Config::MAIN_USE_DISCORD_PRESENCE))
    return;

  s_using_custom_client = new_client.empty() || new_client.compare(DEFAULT_CLIENT_ID) != 0;

  Shutdown();
  if (s_using_custom_client)
    Discord_Initialize(new_client.c_str(), nullptr, 0, nullptr);
  else  // if initialising dolphin's client ID, make sure to restore event handlers
    Init();
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

bool UpdateDiscordPresenceRaw(const std::string& details, const std::string& state,
                              const std::string& large_image_key,
                              const std::string& large_image_text,
                              const std::string& small_image_key,
                              const std::string& small_image_text, const int64_t start_timestamp,
                              const int64_t end_timestamp, const int party_size,
                              const int party_max)
{
#ifdef USE_DISCORD_PRESENCE
  if (!Config::Get(Config::MAIN_USE_DISCORD_PRESENCE))
    return false;

  // only /dev/dolphin sets this, don't let homebrew change official client ID raw presence
  if (!s_using_custom_client)
    return false;

  DiscordRichPresence discord_presence = {};
  discord_presence.details = details.c_str();
  discord_presence.state = state.c_str();
  discord_presence.largeImageKey = large_image_key.c_str();
  discord_presence.largeImageText = large_image_text.c_str();
  discord_presence.smallImageKey = small_image_key.c_str();
  discord_presence.smallImageText = small_image_text.c_str();
  discord_presence.startTimestamp = start_timestamp;
  discord_presence.endTimestamp = end_timestamp;
  discord_presence.partySize = party_size;
  discord_presence.partyMax = party_max;
  Discord_UpdatePresence(&discord_presence);

  return true;
#else
  return false;
#endif
}

void UpdateDiscordPresence(int party_size, SecretType type, const std::string& secret,
                           const std::string& current_game, bool reset_timer)
{
#ifdef USE_DISCORD_PRESENCE
  if (!Config::Get(Config::MAIN_USE_DISCORD_PRESENCE))
    return;

  // reset the client ID if running homebrew has changed it
  if (s_using_custom_client)
    UpdateClientID(DEFAULT_CLIENT_ID);

  const std::string& title =
      current_game.empty() ? SConfig::GetInstance().GetTitleDescription() : current_game;
  std::string game_artwork =
      SConfig::GetInstance().GetGameTDBID().empty() ? "" : ArtworkForGameId();

  DiscordRichPresence discord_presence = {};
  if (game_artwork.empty())
  {
    discord_presence.largeImageKey = "dolphin_logo";
    discord_presence.largeImageText = "Dolphin is an emulator for the GameCube and the Wii.";
  }
  else
  {
    discord_presence.largeImageKey = game_artwork.c_str();
    discord_presence.largeImageText = title.c_str();
    discord_presence.smallImageKey = "dolphin_logo";
    discord_presence.smallImageText = "Dolphin is an emulator for the GameCube and the Wii.";
  }
  discord_presence.details = title.empty() ? "Not in-game" : title.c_str();
  if (reset_timer)
  {
    s_start_timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
  }
  discord_presence.startTimestamp = s_start_timestamp;

#ifdef USE_RETRO_ACHIEVEMENTS
  std::string state_string;
#endif  // USE_RETRO_ACHIEVEMENTS
  if (party_size > 0)
  {
    if (party_size < 4)
    {
      discord_presence.state = "In a party";
      discord_presence.partySize = party_size;
      discord_presence.partyMax = 4;
    }
    else
    {
      // others can still join to spectate
      discord_presence.state = "In a full party";
      discord_presence.partySize = party_size;
      // Note: joining still works without partyMax
    }
  }
#ifdef USE_RETRO_ACHIEVEMENTS
  else if (Config::Get(Config::RA_ENABLED) && Config::Get(Config::RA_DISCORD_PRESENCE_ENABLED))
  {
    state_string = AchievementManager::GetInstance().GetRichPresence().data();
    if (state_string.length() >= 128)
    {
      // 124 characters + 3 dots + null terminator - thanks to Stenzek for format
      state_string.resize(124);
      state_string += "...";
    }
    discord_presence.state = state_string.c_str();
  }
#endif  // USE_RETRO_ACHIEVEMENTS

  std::string party_id;
  std::string secret_final;
  if (type != SecretType::Empty)
  {
    // Declearing party_id or secret_final here will deallocate the variable before passing the
    // values over to Discord_UpdatePresence.

    const size_t secret_length = secret.length();
    party_id = std::to_string(
        Common::HashAdler32(reinterpret_cast<const u8*>(secret.c_str()), secret_length));

    const std::string secret_type = std::to_string(static_cast<int>(type));
    secret_final.reserve(secret_type.length() + 1 + secret_length);
    secret_final += secret_type;
    secret_final += '\n';
    secret_final += secret;
  }
  discord_presence.partyId = party_id.c_str();
  discord_presence.joinSecret = secret_final.c_str();

  Discord_UpdatePresence(&discord_presence);
#endif
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
