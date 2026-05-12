// Copyright 2003 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "UICommon/GameFile.h"
#include "UICommon/UICommon.h"
#include "NetPlayUICallbacks.h"
#include "Core/Boot/Boot.h"
#include "Core/Core.h"
#include "Core/System.h"
#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"

namespace NetPlay {

NetPlayUICallbacks::NetPlayUICallbacks(jobject netplay_session,
                                       std::vector<std::shared_ptr<const UICommon::GameFile>> games)
    : m_netplay_session(IDCache::GetEnvForThread()->NewWeakGlobalRef(netplay_session)),
      m_games(std::move(games))
{
  m_state_changed_hook = Core::AddOnStateChangedCallback([this](Core::State state) {
    if ((state == Core::State::Uninitialized || state == Core::State::Stopping) &&
        !m_got_stop_request)
    {
      JNIEnv* env = IDCache::GetEnvForThread();
      jobject netplay_session = GetNetplaySessionLocalRef(env);
      if (!netplay_session)
        return;

      auto* client = reinterpret_cast<NetPlay::NetPlayClient*>(
          env->GetLongField(netplay_session, IDCache::GetNetPlayClientPointer()));
      if (client)
        client->RequestStopGame();

      env->DeleteLocalRef(netplay_session);
    }
  });
}

NetPlayUICallbacks::~NetPlayUICallbacks()
{
  JNIEnv* env = IDCache::GetEnvForThread();
  env->DeleteWeakGlobalRef(m_netplay_session);
}

jobject NetPlayUICallbacks::GetNetplaySessionLocalRef(JNIEnv* env) const
{
  return env->NewLocalRef(m_netplay_session);
}

void NetPlayUICallbacks::BootGame(const std::string& filename,
                                  std::unique_ptr<BootSessionData> boot_session_data)
{
  m_got_stop_request = false;

  JNIEnv* env = IDCache::GetEnvForThread();
  jobject netplay_session = GetNetplaySessionLocalRef(env);
  if (!netplay_session)
    return;

  env->CallVoidMethod(netplay_session, IDCache::GetNetplayOnBootGame(), ToJString(env, filename),
                      reinterpret_cast<jlong>(boot_session_data.release()));
  env->DeleteLocalRef(netplay_session);
}

void NetPlayUICallbacks::StopGame()
{
  if (m_got_stop_request)
    return;

  m_got_stop_request = true;

  JNIEnv* env = IDCache::GetEnvForThread();
  jobject netplay_session = GetNetplaySessionLocalRef(env);
  if (!netplay_session)
    return;

  env->CallVoidMethod(netplay_session, IDCache::GetNetplayOnStopGame());
  env->DeleteLocalRef(netplay_session);
}

// Only used by Qt UI code, never by the C++ core. On Android, hosting state
// is tracked in Kotlin (NetplaySession.isHosting).
bool NetPlayUICallbacks::IsHosting() const { return false; }

void NetPlayUICallbacks::Update()
{
  JNIEnv* env = IDCache::GetEnvForThread();
  jobject netplay_session = GetNetplaySessionLocalRef(env);
  if (!netplay_session)
    return;

  auto* client = reinterpret_cast<NetPlay::NetPlayClient*>(
      env->GetLongField(netplay_session, IDCache::GetNetPlayClientPointer()));
  if (!client)
  {
    env->DeleteLocalRef(netplay_session);
    return;
  }

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

  env->CallVoidMethod(netplay_session, IDCache::GetNetplayUpdate(), player_array);
  env->DeleteLocalRef(player_array);
  env->DeleteLocalRef(netplay_session);
}

void NetPlayUICallbacks::AppendChat(const std::string& message)
{
  JNIEnv* env = IDCache::GetEnvForThread();
  jobject netplay_session = GetNetplaySessionLocalRef(env);
  if (!netplay_session)
    return;

  env->CallVoidMethod(netplay_session, IDCache::GetNetplayOnChatMessageReceived(),
                      ToJString(env, message));
  env->DeleteLocalRef(netplay_session);
}

void NetPlayUICallbacks::OnMsgChangeGame(const NetPlay::SyncIdentifier& sync_identifier,
                                         const std::string& netplay_name)
{
  m_current_game_identifier = sync_identifier;
  m_current_game_name = netplay_name;

  JNIEnv* env = IDCache::GetEnvForThread();
  jobject netplay_session = GetNetplaySessionLocalRef(env);
  if (!netplay_session)
    return;

  env->CallVoidMethod(netplay_session, IDCache::GetNetplayOnGameChanged(),
                      ToJString(env, netplay_name));
  env->DeleteLocalRef(netplay_session);
}

void NetPlayUICallbacks::OnMsgChangeGBARom(int, const NetPlay::GBAConfig&) {}

void NetPlayUICallbacks::OnMsgStartGame()
{
  JNIEnv* env = IDCache::GetEnvForThread();
  jobject netplay_session = GetNetplaySessionLocalRef(env);
  if (!netplay_session)
    return;

  auto* client = reinterpret_cast<NetPlay::NetPlayClient*>(
      env->GetLongField(netplay_session, IDCache::GetNetPlayClientPointer()));
  if (client)
  {
    if (const auto game = FindGameFile(m_current_game_identifier))
      client->StartGame(game->GetFilePath());
  }

  env->DeleteLocalRef(netplay_session);
}

void NetPlayUICallbacks::OnMsgStopGame() {}

void NetPlayUICallbacks::OnMsgPowerButton()
{
  if (Core::IsRunning(Core::System::GetInstance()))
    UICommon::TriggerSTMPowerEvent();
}

void NetPlayUICallbacks::OnPlayerConnect(const std::string&) {}
void NetPlayUICallbacks::OnPlayerDisconnect(const std::string&) {}

void NetPlayUICallbacks::OnPadBufferChanged(u32 buffer)
{
  JNIEnv* env = IDCache::GetEnvForThread();
  jobject netplay_session = GetNetplaySessionLocalRef(env);
  if (!netplay_session)
    return;

  env->CallVoidMethod(netplay_session, IDCache::GetNetplayOnPadBufferChanged(),
                      static_cast<jint>(buffer));
  env->DeleteLocalRef(netplay_session);
}

void NetPlayUICallbacks::OnHostInputAuthorityChanged(bool enabled)
{
  JNIEnv* env = IDCache::GetEnvForThread();
  jobject netplay_session = GetNetplaySessionLocalRef(env);
  if (!netplay_session)
    return;

  env->CallVoidMethod(netplay_session, IDCache::GetNetplayOnHostInputAuthorityChanged(),
                      static_cast<jboolean>(enabled));
  env->DeleteLocalRef(netplay_session);
}

void NetPlayUICallbacks::OnDesync(u32 frame, const std::string& player)
{
  JNIEnv* env = IDCache::GetEnvForThread();
  jobject netplay_session = GetNetplaySessionLocalRef(env);
  if (!netplay_session)
    return;

  env->CallVoidMethod(netplay_session, IDCache::GetNetplayOnDesync(),
                      static_cast<jint>(frame), ToJString(env, player));
  env->DeleteLocalRef(netplay_session);
}

void NetPlayUICallbacks::OnConnectionLost()
{
  JNIEnv* env = IDCache::GetEnvForThread();
  jobject netplay_session = GetNetplaySessionLocalRef(env);
  if (!netplay_session)
    return;

  env->CallVoidMethod(netplay_session, IDCache::GetNetplayOnConnectionLost());
  env->DeleteLocalRef(netplay_session);
}

void NetPlayUICallbacks::OnConnectionError(const std::string& message)
{
  JNIEnv* env = IDCache::GetEnvForThread();
  jobject netplay_session = GetNetplaySessionLocalRef(env);
  if (!netplay_session)
    return;

  env->CallVoidMethod(netplay_session, IDCache::GetNetplayOnConnectionError(),
                      ToJString(env, message));
  env->DeleteLocalRef(netplay_session);
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

void NetPlayUICallbacks::ShowGameDigestDialog(const std::string& title)
{
  JNIEnv* env = IDCache::GetEnvForThread();
  jobject netplay_session = GetNetplaySessionLocalRef(env);
  if (!netplay_session)
    return;

  env->CallVoidMethod(netplay_session, IDCache::GetNetplayOnShowGameDigestDialog(),
                      ToJString(env, title));
  env->DeleteLocalRef(netplay_session);
}

void NetPlayUICallbacks::SetGameDigestProgress(int pid, int progress)
{
  JNIEnv* env = IDCache::GetEnvForThread();
  jobject netplay_session = GetNetplaySessionLocalRef(env);
  if (!netplay_session)
    return;

  env->CallVoidMethod(netplay_session, IDCache::GetNetplayOnSetGameDigestProgress(),
                      static_cast<jint>(pid), static_cast<jint>(progress));
  env->DeleteLocalRef(netplay_session);
}

void NetPlayUICallbacks::SetGameDigestResult(int pid, const std::string& result)
{
  JNIEnv* env = IDCache::GetEnvForThread();
  jobject netplay_session = GetNetplaySessionLocalRef(env);
  if (!netplay_session)
    return;

  env->CallVoidMethod(netplay_session, IDCache::GetNetplayOnSetGameDigestResult(),
                      static_cast<jint>(pid), ToJString(env, result));
  env->DeleteLocalRef(netplay_session);
}

void NetPlayUICallbacks::AbortGameDigest()
{
  JNIEnv* env = IDCache::GetEnvForThread();
  jobject netplay_session = GetNetplaySessionLocalRef(env);
  if (!netplay_session)
    return;

  env->CallVoidMethod(netplay_session, IDCache::GetNetplayOnAbortGameDigest());
  env->DeleteLocalRef(netplay_session);
}

void NetPlayUICallbacks::ShowChunkedProgressDialog(const std::string& title, u64 data_size,
                                                    std::span<const int> players)
{
  JNIEnv* env = IDCache::GetEnvForThread();
  jobject netplay_session = GetNetplaySessionLocalRef(env);
  if (!netplay_session)
    return;

  jintArray j_players = env->NewIntArray(static_cast<jsize>(players.size()));
  env->SetIntArrayRegion(j_players, 0, static_cast<jsize>(players.size()), players.data());

  env->CallVoidMethod(netplay_session, IDCache::GetNetplayOnShowChunkedProgressDialog(),
                      ToJString(env, title), static_cast<jlong>(data_size), j_players);
  env->DeleteLocalRef(j_players);
  env->DeleteLocalRef(netplay_session);
}

void NetPlayUICallbacks::HideChunkedProgressDialog()
{
  JNIEnv* env = IDCache::GetEnvForThread();
  jobject netplay_session = GetNetplaySessionLocalRef(env);
  if (!netplay_session)
    return;

  env->CallVoidMethod(netplay_session, IDCache::GetNetplayOnHideChunkedProgressDialog());
  env->DeleteLocalRef(netplay_session);
}

void NetPlayUICallbacks::SetChunkedProgress(int pid, u64 progress)
{
  JNIEnv* env = IDCache::GetEnvForThread();
  jobject netplay_session = GetNetplaySessionLocalRef(env);
  if (!netplay_session)
    return;

  env->CallVoidMethod(netplay_session, IDCache::GetNetplayOnSetChunkedProgress(),
                      static_cast<jint>(pid), static_cast<jlong>(progress));
  env->DeleteLocalRef(netplay_session);
}

void NetPlayUICallbacks::SetHostWiiSyncData(std::vector<u64>, std::string) {}

}  // namespace NetPlay
