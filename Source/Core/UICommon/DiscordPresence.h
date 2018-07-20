// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <functional>

namespace Discord
{
using JoinFunction = std::function<void()>;
using JoinRequestFunction =
    std::function<void(const char* id, const std::string& discord_tag, const char* avatar)>;

class Handler
{
public:
  virtual ~Handler();
  virtual void DiscordJoin() = 0;
  virtual void DiscordJoinRequest(const char* id, const std::string& discord_tag,
                                  const char* avatar) = 0;
};

enum class SecretType
{
  Empty,
  IPAddress,
  RoomID,
};

void Init();
void InitNetPlayFunctionality(Handler& handler);
void CallPendingCallbacks();
void UpdateDiscordPresence(int party_size = 0, SecretType type = SecretType::Empty,
                           const std::string& secret = {}, const std::string& current_game = {});
std::string CreateSecretFromIPAddress(const std::string& ip_address, int port);
void Shutdown();
void SetDiscordPresenceEnabled(bool enabled);
}  // namespace Discord
