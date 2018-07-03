// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <functional>

namespace Discord
{
using JoinFunction = std::function<void()>;

enum class SecretType : char
{
  Empty,
  IPAddress,
  RoomID,
};

void Init();
void InitNetPlayFunctionality(const JoinFunction& join);
void CallPendingCallbacks();
void UpdateDiscordPresence(int party_size = 0, SecretType type = SecretType::Empty,
                           const std::string& secret = {});
void Shutdown();
void SetDiscordPresenceEnabled(bool enabled);
}  // namespace Discord
