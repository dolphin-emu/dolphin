// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <string>

namespace Discord
{
// The number is the client ID for Dolphin, it's used for images and the application name
const std::string DEFAULT_CLIENT_ID = "455712169795780630";

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
void UpdateClientID(const std::string& new_client = {});
bool UpdateDiscordPresenceRaw(const std::string& details = {}, const std::string& state = {},
                              const std::string& large_image_key = {},
                              const std::string& large_image_text = {},
                              const std::string& small_image_key = {},
                              const std::string& small_image_text = {},
                              const int64_t start_timestamp = 0, const int64_t end_timestamp = 0,
                              const int party_size = 0, const int party_max = 0);
void UpdateDiscordPresence(int party_size = 0, SecretType type = SecretType::Empty,
                           const std::string& secret = {}, const std::string& current_game = {},
                           const bool reset_timer = false);
std::string CreateSecretFromIPAddress(const std::string& ip_address, int port);
void Shutdown();
void SetDiscordPresenceEnabled(bool enabled);
}  // namespace Discord
