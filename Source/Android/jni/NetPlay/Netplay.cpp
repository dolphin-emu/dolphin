// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <memory>
#include <string>
#include <vector>

#include <jni.h>

#include "Common/CommonTypes.h"
#include "Common/TraversalClient.h"
#include "Core/Boot/Boot.h"
#include "Core/Config/NetplaySettings.h"
#include "Core/NetPlayClient.h"
#include "Core/NetPlayCommon.h"
#include "Core/NetPlayServer.h"
#include "UICommon/GameFile.h"

#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"
#include "jni/NetPlay/NetPlayUICallbacks.h"

static NetPlay::NetPlayUICallbacks* GetUICallbacksPointer(JNIEnv* env, jobject obj)
{
  return reinterpret_cast<NetPlay::NetPlayUICallbacks*>(
      env->GetLongField(obj, IDCache::GetNetPlayUICallbacksPointer()));
}

static NetPlay::NetPlayClient* GetClientPointer(JNIEnv* env, jobject obj)
{
  return reinterpret_cast<NetPlay::NetPlayClient*>(
      env->GetLongField(obj, IDCache::GetNetPlayClientPointer()));
}

static NetPlay::NetPlayServer* GetServerPointer(JNIEnv* env, jobject obj)
{
  return reinterpret_cast<NetPlay::NetPlayServer*>(
      env->GetLongField(obj, IDCache::GetNetPlayServerPointer()));
}

extern "C" {

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetplaySession_nativeSendMessage(JNIEnv* env,
                                                                                 jobject obj,
                                                                                 jstring jmessage)
{
  if (auto* client = GetClientPointer(env, obj))
    client->SendChatMessage(GetJString(env, jmessage));
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetplaySession_nativeSetHostInputAuthority(
    JNIEnv* env, jobject obj, jboolean enable)
{
  if (auto* server = GetServerPointer(env, obj))
    server->SetHostInputAuthority(static_cast<bool>(enable));
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetplaySession_nativeAdjustClientPadBufferSize(
    JNIEnv* env, jobject obj, jint buffer)
{
  if (auto* client = GetClientPointer(env, obj))
    client->AdjustPadBufferSize(static_cast<u32>(buffer));
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetplaySession_nativeAdjustServerPadBufferSize(
    JNIEnv* env, jobject obj, jint buffer)
{
  if (auto* server = GetServerPointer(env, obj))
    server->AdjustPadBufferSize(static_cast<u32>(buffer));
}

JNIEXPORT jlong JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetplaySession_nativeCreateUICallbacks(
    JNIEnv* env, jobject obj, jobjectArray jgame_files)
{
  std::vector<std::shared_ptr<const UICommon::GameFile>> games;
  const jsize count = env->GetArrayLength(jgame_files);
  games.reserve(count);
  for (jsize i = 0; i < count; i++)
  {
    jobject jgame_file = env->GetObjectArrayElement(jgame_files, i);
    auto* game_ptr = reinterpret_cast<std::shared_ptr<const UICommon::GameFile>*>(
        env->GetLongField(jgame_file, IDCache::GetGameFilePointer()));
    if (game_ptr)
      games.push_back(*game_ptr);
    env->DeleteLocalRef(jgame_file);
  }

  return reinterpret_cast<jlong>(new NetPlay::NetPlayUICallbacks(obj, std::move(games)));
}

JNIEXPORT jlong JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetplaySession_nativeJoin(JNIEnv* env, jobject obj)
{
  auto* ui = GetUICallbacksPointer(env, obj);

  const std::string traversal_host = Config::Get(Config::NETPLAY_TRAVERSAL_SERVER);
  const u16 traversal_port = Config::Get(Config::NETPLAY_TRAVERSAL_PORT);
  const std::string nickname = Config::Get(Config::NETPLAY_NICKNAME);

  std::string host_ip;
  u16 host_port;
  bool is_traversal;

  // When hosting, join our own server on localhost
  if (auto* server = GetServerPointer(env, obj))
  {
    host_ip = "127.0.0.1";
    host_port = server->GetPort();
    is_traversal = false;
  }
  else
  {
    const std::string traversal_choice = Config::Get(Config::NETPLAY_TRAVERSAL_CHOICE);
    is_traversal = traversal_choice == "traversal";
    host_ip = is_traversal ? Config::Get(Config::NETPLAY_HOST_CODE) :
                             Config::Get(Config::NETPLAY_ADDRESS);
    host_port = Config::Get(Config::NETPLAY_CONNECT_PORT);
  }

  auto client = std::make_unique<NetPlay::NetPlayClient>(
      host_ip, host_port, ui, nickname,
      NetPlay::NetTraversalConfig{is_traversal, traversal_host, traversal_port});

  if (!client->IsConnected())
    return 0;

  return reinterpret_cast<jlong>(client.release());
}

JNIEXPORT jlong JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetplaySession_nativeHost(JNIEnv* env, jobject obj)
{
  auto* ui = GetUICallbacksPointer(env, obj);

  const std::string traversal_choice = Config::Get(Config::NETPLAY_TRAVERSAL_CHOICE);
  const bool is_traversal = traversal_choice == "traversal";
  const bool use_upnp = Config::Get(Config::NETPLAY_USE_UPNP);
  const std::string traversal_host = Config::Get(Config::NETPLAY_TRAVERSAL_SERVER);
  const u16 traversal_port = Config::Get(Config::NETPLAY_TRAVERSAL_PORT);
  const u16 traversal_port_alt = Config::Get(Config::NETPLAY_TRAVERSAL_PORT_ALT);

  const u16 host_port = is_traversal ? Config::Get(Config::NETPLAY_LISTEN_PORT) :
                                       Config::Get(Config::NETPLAY_HOST_PORT);

  auto server = std::make_unique<NetPlay::NetPlayServer>(
      host_port, use_upnp, ui,
      NetPlay::NetTraversalConfig{is_traversal, traversal_host, traversal_port,
                                  traversal_port_alt});

  if (!server->is_connected)
    return 0;

  const std::string network_mode = Config::Get(Config::NETPLAY_NETWORK_MODE);
  const bool host_input_authority = network_mode == "hostinputauthority" || network_mode == "golf";
  server->SetHostInputAuthority(host_input_authority);
  server->AdjustPadBufferSize(Config::Get(Config::NETPLAY_BUFFER_SIZE));

  return reinterpret_cast<jlong>(server.release());
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetplaySession_nativeChangeGame(JNIEnv* env,
                                                                                jobject obj,
                                                                                jobject jgame_file)
{
  auto* server = GetServerPointer(env, obj);
  if (!server)
    return;

  const auto& game_file = *reinterpret_cast<std::shared_ptr<const UICommon::GameFile>*>(
      env->GetLongField(jgame_file, IDCache::GetGameFilePointer()));

  server->ChangeGame(game_file->GetSyncIdentifier(), game_file->GetLongName());
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetplaySession_nativeDoAllPlayersHaveGame(
    JNIEnv* env, jobject obj)
{
  if (auto* client = GetClientPointer(env, obj))
    return static_cast<jboolean>(client->DoAllPlayersHaveGame());
  return JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetplaySession_nativeStartGame(JNIEnv* env,
                                                                               jobject obj)
{
  auto* server = GetServerPointer(env, obj);
  if (!server)
    return;

  server->RequestStartGame();
}

JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_features_netplay_NetplaySession_nativeGetPort(
    JNIEnv* env, jobject obj)
{
  if (auto* server = GetServerPointer(env, obj))
    return static_cast<jint>(server->GetPort());
  return 0;
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetplaySession_nativeGetExternalIpAddress(
    JNIEnv* env, jobject)
{
  std::string ip = NetPlay::GetExternalIPAddress();
  if (ip.empty())
    return nullptr;
  return ToJString(env, ip);
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetplaySession_nativeReconnectTraversal(JNIEnv*,
                                                                                        jobject)
{
  if (Common::g_TraversalClient)
    Common::g_TraversalClient->ReconnectToServer();
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetplaySession_nativeReleaseUICallbacks(
    JNIEnv*, jobject, jlong pointer)
{
  delete reinterpret_cast<NetPlay::NetPlayUICallbacks*>(pointer);
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetplaySession_nativeReleaseBootSessionData(
    JNIEnv*, jobject, jlong pointer)
{
  delete reinterpret_cast<BootSessionData*>(pointer);
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetplaySession_nativeReleaseClient(JNIEnv*, jobject,
                                                                                   jlong pointer)
{
  delete reinterpret_cast<NetPlay::NetPlayClient*>(pointer);
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetplaySession_nativeReleaseServer(JNIEnv*, jobject,
                                                                                   jlong pointer)
{
  delete reinterpret_cast<NetPlay::NetPlayServer*>(pointer);
}

}  // extern "C"
