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

std::string ArtworkForGameId(const std::string& gameid)
{
  static const std::set<std::string> REGISTERED_GAMES{
      "GAF",  // GAFE01: Animal Crossing
      "RUU",  // RUUE01: Animal Crossing: City Folk
      "SF8",  // SF8E01: Donkey Kong Country Returns
      "RDB",  // RDBE70: Dragon Ball Z: Budokai Tenkaichi 2
      "RDS",  // RDSE70: Dragon Ball Z: Budokai Tenkaichi 3
      "GFZ",  // GFZE01: F-Zero GX
      "GFE",  // GFEE01: Fire Emblem: Path of Radiance
      "RFE",  // RFEE01: Fire Emblem: Radiant Dawn
      "S5S",  // S5SJHF: Inazuma Eleven GO: Strikers 2013
      "GKY",  // GKYE01: Kirby Air Ride
      "SUK",  // SUKE01: Kirby's Return to Dream Land
      "GLM",  // GLME01: Luigi's Mansion
      "GFT",  // GFTE01: Mario Golf: Toadstool Tour
      "RMC",  // RMCE01: Mario Kart Wii
      "GM4",  // GM4E01: Mario Kart: Double Dash!!
      "GMP",  // GMPE01: Mario Party 4
      "GP5",  // GP5E01: Mario Party 5
      "GP6",  // GP6E01: Mario Party 6
      "GP7",  // GP7E01: Mario Party 7
      "RM8",  // RM8E01: Mario Party 8
      "SSQ",  // SSQE01: Mario Party 9
      "GOM",  // GOME01: Mario Power Tennis
      "GYQ",  // GYQE01: Mario Superstar Baseball
      "GGS",  // GGSE01: Metal Gear Solid: The Twin Snakes
      "GM8",  // GM8E01: Metroid Prime
      "G2M",  // G2ME01: Metroid Prime 2: Echoes
      "RM3",  // RM3E01: Metroid Prime 3: Corruption
      "R3M",  // R3ME01: Metroid Prime: Trilogy
      "SMN",  // SMNE01: New Super Mario Bros. Wii
      "G8M",  // G8ME01: Paper Mario: The Thousand-Year Door
      "GPI",  // GPIE01: Pikmin (GC)
      "R9I",  // R9IE01: Pikmin (Wii)
      "GPV",  // GPVE01: Pikmin 2 (GC)
      "R92",  // R92E01: Pikmin 2 (Wii)
      "GC6",  // GC6E01: Pokemon Colosseum
      "GXX",  // GXXE01: Pokemon XD: Gale of Darkness
      "GBI",  // GBIE08: Resident Evil
      "GHA",  // GHAE08: Resident Evil 2
      "GLE",  // GLEE08: Resident Evil 3: Nemesis
      "G4B",  // G4BE08: Resident Evil 4
      "GSN",  // GSNE8P: Sonic Adventure 2: Battle
      "GXS",  // GXSE8P: Sonic Adventure DX: Director's Cut
      "SNC",  // SNCE8P: Sonic Colors
      "G9S",  // G9SE8P: Sonic Heroes
      "GRS",  // GRSEAF: SoulCalibur II
      "RSL",  // RSLEAF: SoulCalibur Legends
      "GK2",  // GK2E52: Spider-Man 2
      "GQP",  // GQPE78: SpongeBob SquarePants: Battle for Bikini Bottom
      "SVM",  // SVME01: Super Mario All-Stars: 25th Anniversary Edition
      "RMG",  // RMGE01: Super Mario Galaxy
      "SB4",  // SB4E01: Super Mario Galaxy 2
      "G4Q",  // G4QE01: Super Mario Strikers
      "GMS",  // GMSE01: Super Mario Sunshine
      "GMB",  // GMBE8P: Super Monkey Ball
      "GM2",  // GM2E8P: Super Monkey Ball 2
      "R8P",  // R8PE01: Super Paper Mario
      "RSB",  // RSBE01: Super Smash Bros. Brawl
      "GAL",  // GALE01: Super Smash Bros. Melee
      "PZL",  // PZLE01: The Legend of Zelda: Collector's Edition
      "G4S",  // G4SE01: The Legend of Zelda: Four Swords Adventures
      "D43",  // D43E01: The Legend of Zelda: Ocarina of Time / Master Quest
      "SOU",  // SOUE01: The Legend of Zelda: Skyward Sword
      "GZL",  // GZLE01: The Legend of Zelda: The Wind Waker
      "GZ2",  // GZ2E01: The Legend of Zelda: Twilight Princess (GC)
      "RZD",  // RZDE01: The Legend of Zelda: Twilight Princess (Wii)
      "GHQ",  // GHQE7D: The Simpsons: Hit & Run
      "RSP",  // RSPE01: Wii Sports
      "RZT",  // RZTE01: Wii Sports Resort
      "SX4",  // SX4E01: Xenoblade Chronicles
  };

  std::string region_neutral_gameid = gameid.substr(0, 3);
  if (REGISTERED_GAMES.count(region_neutral_gameid) != 0)
  {
    // Discord asset keys can only be lowercase.
    std::transform(region_neutral_gameid.begin(), region_neutral_gameid.end(),
                   region_neutral_gameid.begin(), tolower);
    return "game_" + region_neutral_gameid;
  }
  return "";
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
  Discord_Initialize("455712169795780630", &handlers, 1, nullptr);
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
#ifdef USE_DISCORD_PRESENCE
  if (!Config::Get(Config::MAIN_USE_DISCORD_PRESENCE))
    return;

  const std::string& title =
      current_game.empty() ? SConfig::GetInstance().GetTitleDescription() : current_game;
  std::string game_artwork = ArtworkForGameId(SConfig::GetInstance().GetGameID());

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
  discord_presence.startTimestamp = std::time(nullptr);

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
