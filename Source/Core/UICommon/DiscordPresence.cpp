// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "UICommon/DiscordPresence.h"
#include "Core/Config/UISettings.h"
#include "Core/ConfigManager.h"

#ifdef USE_DISCORD_PRESENCE

#include <ctime>
#include <discord-rpc/include/discord_rpc.h>

#endif

namespace Discord
{
void Init()
{
#ifdef USE_DISCORD_PRESENCE
  if (!Config::Get(Config::MAIN_USE_DISCORD_PRESENCE))
    return;

  DiscordEventHandlers handlers = {};
  // The number is the client ID for Dolphin, it's used for images and the appication name
  Discord_Initialize("455712169795780630", &handlers, 1, nullptr);
  UpdateDiscordPresence();
#endif
}

void UpdateDiscordPresence()
{
#ifdef USE_DISCORD_PRESENCE
  if (!Config::Get(Config::MAIN_USE_DISCORD_PRESENCE))
    return;

  const std::string& title = SConfig::GetInstance().GetTitleDescription();

  DiscordRichPresence discord_presence = {};
  discord_presence.largeImageKey = "dolphin_logo";
  discord_presence.largeImageText = "Dolphin is an emulator for the GameCube and the Wii.";
  discord_presence.details = title.empty() ? "Not in-game" : title.c_str();
  discord_presence.startTimestamp = std::time(nullptr);
  Discord_UpdatePresence(&discord_presence);
#endif
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
