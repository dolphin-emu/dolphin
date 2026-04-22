// Copyright 2003 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "UICommon/GameFile.h"
#include "NetPlayUICallbacks.h"
#include "Core/Boot/Boot.h"
#include "Core/Core.h"
#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"

namespace NetPlay {

NetPlayUICallbacks::NetPlayUICallbacks(std::vector<std::shared_ptr<const UICommon::GameFile>> games)
    : m_games(std::move(games))
{
  m_state_changed_hook = Core::AddOnStateChangedCallback([this](Core::State state) {
    if ((state == Core::State::Uninitialized || state == Core::State::Stopping) &&
        !m_got_stop_request)
    {
      JNIEnv* env = IDCache::GetEnvForThread();
      auto* client = reinterpret_cast<NetPlay::NetPlayClient*>(
          env->GetStaticLongField(IDCache::GetNetplayClass(), IDCache::GetNetPlayClientPointer()));
      if (client)
        client->RequestStopGame();
    }
  });
}

NetPlayUICallbacks::~NetPlayUICallbacks() = default;

void NetPlayUICallbacks::BootGame(const std::string& filename, std::unique_ptr<BootSessionData> boot_session_data) {
    m_got_stop_request = false;
    JNIEnv* env = IDCache::GetEnvForThread();
    env->CallStaticVoidMethod(IDCache::GetNetplayClass(), IDCache::GetNetplayOnBootGame(),
                              ToJString(env, filename), reinterpret_cast<jlong>(boot_session_data.release()));
}

void NetPlayUICallbacks::StopGame()
{
  if (m_got_stop_request)
    return;

  m_got_stop_request = true;
  JNIEnv* env = IDCache::GetEnvForThread();
  env->CallStaticVoidMethod(IDCache::GetNetplayClass(), IDCache::GetNetplayOnStopGame());
}

bool NetPlayUICallbacks::IsHosting() const { return false; }

void NetPlayUICallbacks::Update()
{
  JNIEnv* env = IDCache::GetEnvForThread();
  auto* client = reinterpret_cast<NetPlay::NetPlayClient*>(
      env->GetStaticLongField(IDCache::GetNetplayClass(), IDCache::GetNetPlayClientPointer()));
  if (!client)
    return;

  const std::vector<const NetPlay::Player*> players = client->GetPlayers();

  jobjectArray player_array =
      env->NewObjectArray(static_cast<jsize>(players.size()), IDCache::GetNetplayPlayerClass(), nullptr);

  for (jsize i = 0; i < static_cast<jsize>(players.size()); i++)
  {
    const NetPlay::Player* player = players[i];
    const std::string mapping = NetPlay::GetPlayerMappingString(
        player->pid, client->GetPadMapping(), client->GetGBAConfig(), client->GetWiimoteMapping());
    jobject player_obj = env->NewObject(
        IDCache::GetNetplayPlayerClass(), IDCache::GetNetplayPlayerConstructor(),
        static_cast<jint>(player->pid),
        ToJString(env, player->name),
        ToJString(env, player->revision),
        static_cast<jint>(player->ping),
        static_cast<jboolean>(player->IsHost()),
        ToJString(env, mapping));
    env->SetObjectArrayElement(player_array, i, player_obj);
    env->DeleteLocalRef(player_obj);
  }

  env->CallStaticVoidMethod(IDCache::GetNetplayClass(), IDCache::GetNetplayUpdate(), player_array);
  env->DeleteLocalRef(player_array);
}

void NetPlayUICallbacks::AppendChat(const std::string& message)
{
  JNIEnv* env = IDCache::GetEnvForThread();
  env->CallStaticVoidMethod(IDCache::GetNetplayClass(),
                            IDCache::GetNetplayOnChatMessageReceived(),
                            ToJString(env, message));
}

void NetPlayUICallbacks::OnMsgChangeGame(const NetPlay::SyncIdentifier& sync_identifier,
                                         const std::string& netplay_name)
{
  m_current_game_identifier = sync_identifier;
  m_current_game_name = netplay_name;

  JNIEnv* env = IDCache::GetEnvForThread();
  env->CallStaticVoidMethod(IDCache::GetNetplayClass(), IDCache::GetNetplayOnGameChanged(),
                            ToJString(env, netplay_name));
}

void NetPlayUICallbacks::OnMsgChangeGBARom(int, const NetPlay::GBAConfig&) {}

void NetPlayUICallbacks::OnMsgStartGame()
{
  JNIEnv* env = IDCache::GetEnvForThread();
  auto* client = reinterpret_cast<NetPlay::NetPlayClient*>(
      env->GetStaticLongField(IDCache::GetNetplayClass(), IDCache::GetNetPlayClientPointer()));
  if (client)
  {
    if (const auto game = FindGameFile(m_current_game_identifier))
      client->StartGame(game->GetFilePath());
  }
}

void NetPlayUICallbacks::OnMsgStopGame() {}
void NetPlayUICallbacks::OnMsgPowerButton() {}
void NetPlayUICallbacks::OnPlayerConnect(const std::string&) {}
void NetPlayUICallbacks::OnPlayerDisconnect(const std::string&) {}

void NetPlayUICallbacks::OnPadBufferChanged(u32 buffer)
{
  JNIEnv* env = IDCache::GetEnvForThread();
  env->CallStaticVoidMethod(IDCache::GetNetplayClass(), IDCache::GetNetplayOnPadBufferChanged(),
                            static_cast<jint>(buffer));
}

void NetPlayUICallbacks::OnHostInputAuthorityChanged(bool enabled)
{
  JNIEnv* env = IDCache::GetEnvForThread();
  env->CallStaticVoidMethod(IDCache::GetNetplayClass(),
                            IDCache::GetNetplayOnHostInputAuthorityChanged(),
                            static_cast<jboolean>(enabled));
}

void NetPlayUICallbacks::OnDesync(u32, const std::string&) {}
void NetPlayUICallbacks::OnConnectionLost() {}

void NetPlayUICallbacks::OnConnectionError(const std::string& message)
{
  JNIEnv* env = IDCache::GetEnvForThread();
  env->CallStaticVoidMethod(IDCache::GetNetplayClass(), IDCache::GetNetplayOnConnectionError(),
                            ToJString(env, message));
}

void NetPlayUICallbacks::OnTraversalError(Common::TraversalClient::FailureReason) {}
void NetPlayUICallbacks::OnTraversalStateChanged(Common::TraversalClient::State) {}
void NetPlayUICallbacks::OnGameStartAborted() {}
void NetPlayUICallbacks::OnGolferChanged(bool, const std::string&) {}
void NetPlayUICallbacks::OnTtlDetermined(u8) {}
void NetPlayUICallbacks::OnIndexAdded(bool, std::string) {}
void NetPlayUICallbacks::OnIndexRefreshFailed(std::string) {}
bool NetPlayUICallbacks::IsRecording() { return false; }

std::shared_ptr<const UICommon::GameFile>
NetPlayUICallbacks::FindGameFile(const NetPlay::SyncIdentifier& sync_identifier,
                                 NetPlay::SyncIdentifierComparison* found)
{
  NetPlay::SyncIdentifierComparison temp;
  if (!found)
    found = &temp;

  *found = NetPlay::SyncIdentifierComparison::DifferentGame;

  std::shared_ptr<const UICommon::GameFile> result;
  for (const auto& game : m_games)
  {
    const auto cmp = game->CompareSyncIdentifier(sync_identifier);
    if (cmp < *found)
    {
      *found = cmp;
      result = game;
    }
  }
  return result;
}

std::string NetPlayUICallbacks::FindGBARomPath(const std::array<u8, 20>&, std::string_view, int) { return {}; }
void NetPlayUICallbacks::ShowGameDigestDialog(const std::string&) {}
void NetPlayUICallbacks::SetGameDigestProgress(int, int) {}
void NetPlayUICallbacks::SetGameDigestResult(int, const std::string&) {}
void NetPlayUICallbacks::AbortGameDigest() {}
void NetPlayUICallbacks::ShowChunkedProgressDialog(const std::string&, u64, std::span<const int>) {}
void NetPlayUICallbacks::HideChunkedProgressDialog() {}
void NetPlayUICallbacks::SetChunkedProgress(int, u64) {}
void NetPlayUICallbacks::SetHostWiiSyncData(std::vector<u64>, std::string) {}

}  // namespace NetPlay
