// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#ifdef USE_DISCORD_PRESENCE

#include "UICommon/DiscordPresence.h"

#include <ctime>
#include <discord-rpc/include/discord_rpc.h>

#include "Core/ConfigManager.h"

#endif

const Config::ConfigInfo<bool> MAIN_USE_DISCORD_PRESENCE{
    {Config::System::Main, "General", "UseDiscordPresence"}, true};

namespace Discord
{
void Init()
{
#ifdef USE_DISCORD_PRESENCE
  if (!Config::Get(MAIN_USE_DISCORD_PRESENCE))
    return;

  DiscordEventHandlers handlers = {};
  // The number is the client ID for Dolphin, it's used for images and the appication name
  Discord_Initialize("450033159212630028", &handlers, 1, nullptr);
  UpdateDiscordPresence();
#endif
}

void UpdateDiscordPresence()
{
#ifdef USE_DISCORD_PRESENCE
  if (!Config::Get(MAIN_USE_DISCORD_PRESENCE))
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
  if (!Config::Get(MAIN_USE_DISCORD_PRESENCE))
    return;

  Discord_ClearPresence();
  Discord_Shutdown();
#endif
}

void SetDiscordPresenceEnabled(bool enabled)
{
  if (Config::Get(MAIN_USE_DISCORD_PRESENCE) == enabled)
    return;

  if (Config::Get(MAIN_USE_DISCORD_PRESENCE))
    Discord::Shutdown();

  Config::SetBase(MAIN_USE_DISCORD_PRESENCE, enabled);

  if (Config::Get(MAIN_USE_DISCORD_PRESENCE))
    Discord::Init();
}

}  // namespace Discord
