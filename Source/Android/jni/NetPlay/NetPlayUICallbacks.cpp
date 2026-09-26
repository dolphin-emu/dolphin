// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <android/log.h>

#include <fmt/format.h>

#include "Common/TraversalClient.h"
#include "Core/Boot/Boot.h"
#include "Core/Core.h"
#include "Core/System.h"
#include "NetPlayUICallbacks.h"
#include "UICommon/GameFile.h"
#include "UICommon/UICommon.h"
#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"

namespace
{
std::string InetAddressToString(const Common::TraversalInetAddress& addr)
{
  std::string ip;

  if (addr.isIPV6)
  {
    ip = "IPv6-Not-Implemented";
  }
  else
  {
    const auto ipv4 = reinterpret_cast<const u8*>(addr.address);
    ip = std::to_string(ipv4[0]);
    for (u32 i = 1; i != 4; ++i)
    {
      ip += ".";
      ip += std::to_string(ipv4[i]);
    }
  }

  return fmt::format("{}:{}", ip, ntohs(addr.port));
}

const char* FailureReasonToString(Common::TraversalClient::FailureReason reason)
{
  switch (reason)
  {
  case Common::TraversalClient::FailureReason::BadHost:
    return "BadHost";
  case Common::TraversalClient::FailureReason::VersionTooOld:
    return "VersionTooOld";
  case Common::TraversalClient::FailureReason::ServerForgotAboutUs:
    return "ServerForgotAboutUs";
  case Common::TraversalClient::FailureReason::SocketSendError:
    return "SocketSendError";
  case Common::TraversalClient::FailureReason::ResendTimeout:
    return "ResendTimeout";
  default:
    return "Unknown";
  }
}
}  // namespace

namespace NetPlay
{

NetPlayUICallbacks::NetPlayUICallbacks(jobject netplay_session,
                                       std::vector<std::shared_ptr<const UICommon::GameFile>> games)
    : m_netplay_session(IDCache::GetEnvForThread()->NewWeakGlobalRef(netplay_session)),
      m_games(std::move(games))
{
  m_state_changed_hook = Core::AddOnStateChangedCallback([this](Core::State state) {
    if ((state == Core::State::Uninitialized || state == Core::State::Stopping) &&
        !m_got_stop_request)
    {
      WithSession([](JNIEnv* env, jobject session) {
        auto* client = reinterpret_cast<NetPlay::NetPlayClient*>(
            env->GetLongField(session, IDCache::GetNetPlayClientPointer()));
        if (client)
          client->RequestStopGame();
      });
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

  WithSession([&](JNIEnv* env, jobject session) {
    env->CallVoidMethod(session, IDCache::GetNetplayOnBootGame(), ToJString(env, filename),
                        reinterpret_cast<jlong>(boot_session_data.release()));
  });
}

void NetPlayUICallbacks::StopGame()
{
  if (m_got_stop_request)
    return;

  m_got_stop_request = true;

  WithSession([](JNIEnv* env, jobject session) {
    env->CallVoidMethod(session, IDCache::GetNetplayOnStopGame());
  });
}

// Only used by Qt UI code, never by the C++ core. On Android, hosting state
// is tracked in Kotlin (NetplaySession.isHosting).
bool NetPlayUICallbacks::IsHosting() const
{
  return false;
}

void NetPlayUICallbacks::Update()
{
  WithSession([](JNIEnv* env, jobject session) {
    auto* client = reinterpret_cast<NetPlay::NetPlayClient*>(
        env->GetLongField(session, IDCache::GetNetPlayClientPointer()));
    if (!client)
      return;

    const std::vector<const NetPlay::Player*> players = client->GetPlayers();

    jobjectArray player_array = env->NewObjectArray(static_cast<jsize>(players.size()),
                                                    IDCache::GetNetplayPlayerClass(), nullptr);

    for (jsize i = 0; i < static_cast<jsize>(players.size()); i++)
    {
      const NetPlay::Player* player = players[i];
      const std::string mapping =
          NetPlay::GetPlayerMappingString(player->pid, client->GetPadMapping(),
                                          client->GetGBAConfig(), client->GetWiimoteMapping());
      jobject player_obj =
          env->NewObject(IDCache::GetNetplayPlayerClass(), IDCache::GetNetplayPlayerConstructor(),
                         static_cast<jint>(player->pid), ToJString(env, player->name),
                         ToJString(env, player->revision), static_cast<jint>(player->ping),
                         static_cast<jboolean>(player->IsHost()), ToJString(env, mapping));
      env->SetObjectArrayElement(player_array, i, player_obj);
      env->DeleteLocalRef(player_obj);
    }

    env->CallVoidMethod(session, IDCache::GetNetplayUpdate(), player_array);
    env->DeleteLocalRef(player_array);
  });
}

void NetPlayUICallbacks::AppendChat(const std::string& message)
{
  WithSession([&](JNIEnv* env, jobject session) {
    env->CallVoidMethod(session, IDCache::GetNetplayOnChatMessageReceived(),
                        ToJString(env, message));
  });
}

void NetPlayUICallbacks::OnMsgChangeGame(const NetPlay::SyncIdentifier& sync_identifier,
                                         const std::string& netplay_name)
{
  m_current_game_identifier = sync_identifier;
  m_current_game_name = netplay_name;

  WithSession([&](JNIEnv* env, jobject session) {
    env->CallVoidMethod(session, IDCache::GetNetplayOnGameChanged(), ToJString(env, netplay_name));
  });
}

void NetPlayUICallbacks::OnMsgChangeGBARom(int, const NetPlay::GBAConfig&)
{
}

void NetPlayUICallbacks::OnMsgStartGame()
{
  WithSession([this](JNIEnv* env, jobject session) {
    auto* client = reinterpret_cast<NetPlay::NetPlayClient*>(
        env->GetLongField(session, IDCache::GetNetPlayClientPointer()));
    if (client)
    {
      if (const auto game = FindGameFile(m_current_game_identifier))
        client->StartGame(game->GetFilePath());
    }
  });
}

void NetPlayUICallbacks::OnMsgStopGame()
{
}

void NetPlayUICallbacks::OnMsgPowerButton()
{
  if (Core::IsRunning(Core::System::GetInstance()))
    UICommon::TriggerSTMPowerEvent();
}

void NetPlayUICallbacks::OnPlayerConnect(const std::string&)
{
}
void NetPlayUICallbacks::OnPlayerDisconnect(const std::string&)
{
}

void NetPlayUICallbacks::OnPadBufferChanged(u32 buffer)
{
  WithSession([&](JNIEnv* env, jobject session) {
    env->CallVoidMethod(session, IDCache::GetNetplayOnPadBufferChanged(),
                        static_cast<jint>(buffer));
  });
}

void NetPlayUICallbacks::OnHostInputAuthorityChanged(bool enabled)
{
  WithSession([&](JNIEnv* env, jobject session) {
    env->CallVoidMethod(session, IDCache::GetNetplayOnHostInputAuthorityChanged(),
                        static_cast<jboolean>(enabled));
  });
}

void NetPlayUICallbacks::OnDesync(u32 frame, const std::string& player)
{
  WithSession([&](JNIEnv* env, jobject session) {
    env->CallVoidMethod(session, IDCache::GetNetplayOnDesync(), static_cast<jint>(frame),
                        ToJString(env, player));
  });
}

void NetPlayUICallbacks::OnConnectionLost()
{
  WithSession([](JNIEnv* env, jobject session) {
    env->CallVoidMethod(session, IDCache::GetNetplayOnConnectionLost());
  });
}

void NetPlayUICallbacks::OnConnectionError(const std::string& message)
{
  WithSession([&](JNIEnv* env, jobject session) {
    env->CallVoidMethod(session, IDCache::GetNetplayOnConnectionError(), ToJString(env, message));
  });
}

// No-op — all error info is captured by OnTraversalStateChanged which always fires alongside.
void NetPlayUICallbacks::OnTraversalError(Common::TraversalClient::FailureReason)
{
}

void NetPlayUICallbacks::OnTraversalStateChanged(Common::TraversalClient::State state)
{
  WithSession([&](JNIEnv* env, jobject session) {
    if (!Common::g_TraversalClient)
      return;

    jstring host_code = nullptr;
    jstring external_address = nullptr;
    jstring failure_reason = nullptr;

    if (state == Common::TraversalClient::State::Connected)
    {
      const auto host_id = Common::g_TraversalClient->GetHostID();
      host_code = ToJString(env, std::string(host_id.begin(), host_id.end()));
      external_address =
          ToJString(env, InetAddressToString(Common::g_TraversalClient->GetExternalAddress()));
    }
    else if (state == Common::TraversalClient::State::Failure)
    {
      failure_reason =
          ToJString(env, FailureReasonToString(Common::g_TraversalClient->GetFailureReason()));
    }

    env->CallVoidMethod(session, IDCache::GetNetplayOnTraversalStateChanged(),
                        static_cast<jint>(state), host_code, external_address, failure_reason);

    if (host_code)
      env->DeleteLocalRef(host_code);
    if (external_address)
      env->DeleteLocalRef(external_address);
    if (failure_reason)
      env->DeleteLocalRef(failure_reason);
  });
}

void NetPlayUICallbacks::OnGameStartAborted()
{
}
void NetPlayUICallbacks::OnGolferChanged(bool, const std::string&)
{
}
void NetPlayUICallbacks::OnTtlDetermined(u8)
{
}
void NetPlayUICallbacks::OnIndexAdded(bool, std::string)
{
}
void NetPlayUICallbacks::OnIndexRefreshFailed(std::string)
{
}
bool NetPlayUICallbacks::IsRecording()
{
  return false;
}

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

std::string NetPlayUICallbacks::FindGBARomPath(const std::array<u8, 20>&, std::string_view, int)
{
  return {};
}

void NetPlayUICallbacks::ShowGameDigestDialog(const std::string& title)
{
  WithSession([&](JNIEnv* env, jobject session) {
    env->CallVoidMethod(session, IDCache::GetNetplayOnShowGameDigestDialog(),
                        ToJString(env, title));
  });
}

void NetPlayUICallbacks::SetGameDigestProgress(int pid, int progress)
{
  WithSession([&](JNIEnv* env, jobject session) {
    env->CallVoidMethod(session, IDCache::GetNetplayOnSetGameDigestProgress(),
                        static_cast<jint>(pid), static_cast<jint>(progress));
  });
}

void NetPlayUICallbacks::SetGameDigestResult(int pid, const std::string& result)
{
  WithSession([&](JNIEnv* env, jobject session) {
    env->CallVoidMethod(session, IDCache::GetNetplayOnSetGameDigestResult(), static_cast<jint>(pid),
                        ToJString(env, result));
  });
}

void NetPlayUICallbacks::AbortGameDigest()
{
  WithSession([](JNIEnv* env, jobject session) {
    env->CallVoidMethod(session, IDCache::GetNetplayOnAbortGameDigest());
  });
}

void NetPlayUICallbacks::ShowChunkedProgressDialog(const std::string& title, u64 data_size,
                                                   std::span<const int> players)
{
  WithSession([&](JNIEnv* env, jobject session) {
    jintArray j_players = env->NewIntArray(static_cast<jsize>(players.size()));
    env->SetIntArrayRegion(j_players, 0, static_cast<jsize>(players.size()), players.data());

    env->CallVoidMethod(session, IDCache::GetNetplayOnShowChunkedProgressDialog(),
                        ToJString(env, title), static_cast<jlong>(data_size), j_players);
    env->DeleteLocalRef(j_players);
  });
}

void NetPlayUICallbacks::HideChunkedProgressDialog()
{
  WithSession([](JNIEnv* env, jobject session) {
    env->CallVoidMethod(session, IDCache::GetNetplayOnHideChunkedProgressDialog());
  });
}

void NetPlayUICallbacks::SetChunkedProgress(int pid, u64 progress)
{
  WithSession([&](JNIEnv* env, jobject session) {
    env->CallVoidMethod(session, IDCache::GetNetplayOnSetChunkedProgress(), static_cast<jint>(pid),
                        static_cast<jlong>(progress));
  });
}

void NetPlayUICallbacks::SetHostWiiSyncData(std::vector<u64>, std::string)
{
}

}  // namespace NetPlay
