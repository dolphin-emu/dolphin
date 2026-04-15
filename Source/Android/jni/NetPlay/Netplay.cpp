// Copyright 2003 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <memory>
#include <string>
#include <vector>

#include <jni.h>

#include "Common/CommonTypes.h"
#include "Core/Config/NetplaySettings.h"
#include "Core/NetPlayClient.h"
#include "UICommon/GameFile.h"
#include "UICommon/GameFileCache.h"

#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"
#include "jni/NetPlay/NetPlayUICallbacks.h"

static NetPlay::NetPlayClient* GetPointer(JNIEnv* env)
{
  return reinterpret_cast<NetPlay::NetPlayClient*>(
      env->GetStaticLongField(IDCache::GetNetplayClass(), IDCache::GetNetPlayClientPointer()));
}

extern "C" {

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_Netplay_getNickname(JNIEnv* env, jclass)
{
  return ToJString(env, Config::Get(Config::NETPLAY_NICKNAME));
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_Netplay_getTraversalChoice(JNIEnv* env, jclass)
{
  return ToJString(env, Config::Get(Config::NETPLAY_TRAVERSAL_CHOICE));
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_Netplay_getAddress(JNIEnv* env, jclass)
{
  return ToJString(env, Config::Get(Config::NETPLAY_ADDRESS));
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_Netplay_getHostCode(JNIEnv* env, jclass)
{
  return ToJString(env, Config::Get(Config::NETPLAY_HOST_CODE));
}

JNIEXPORT jint JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_Netplay_getConnectPort(JNIEnv*, jclass)
{
  return static_cast<jint>(Config::Get(Config::NETPLAY_CONNECT_PORT));
}

JNIEXPORT jint JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_Netplay_getHostPort(JNIEnv*, jclass)
{
  return static_cast<jint>(Config::Get(Config::NETPLAY_HOST_PORT));
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_Netplay_getUseUpnp(JNIEnv*, jclass)
{
  return static_cast<jboolean>(Config::Get(Config::NETPLAY_USE_UPNP));
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_Netplay_getEnableChunkedUploadLimit(JNIEnv*, jclass)
{
  return static_cast<jboolean>(Config::Get(Config::NETPLAY_ENABLE_CHUNKED_UPLOAD_LIMIT));
}

JNIEXPORT jint JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_Netplay_getChunkedUploadLimit(JNIEnv*, jclass)
{
  return static_cast<jint>(Config::Get(Config::NETPLAY_CHUNKED_UPLOAD_LIMIT));
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_Netplay_getUseIndex(JNIEnv*, jclass)
{
  return static_cast<jboolean>(Config::Get(Config::NETPLAY_USE_INDEX));
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_Netplay_getIndexRegion(JNIEnv* env, jclass)
{
  return ToJString(env, Config::Get(Config::NETPLAY_INDEX_REGION));
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_Netplay_getIndexName(JNIEnv* env, jclass)
{
  return ToJString(env, Config::Get(Config::NETPLAY_INDEX_NAME));
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_Netplay_getIndexPassword(JNIEnv* env, jclass)
{
  return ToJString(env, Config::Get(Config::NETPLAY_INDEX_PASSWORD));
}

JNIEXPORT jint JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_Netplay_getClientBufferSize(JNIEnv*, jclass)
{
  return static_cast<jint>(Config::Get(Config::NETPLAY_CLIENT_BUFFER_SIZE));
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_Netplay_setClientBufferSize(JNIEnv*, jclass,
                                                                            jint buffer)
{
  Config::SetBase(Config::NETPLAY_CLIENT_BUFFER_SIZE, static_cast<u32>(buffer));
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_Netplay_SaveSetup(
    JNIEnv* env, jclass, jstring jnickname, jstring traversalChoice, jstring jaddress,
    jstring jhostCode, jint connectPort, jint hostPort, jboolean useUpnp, jboolean useListenPort,
    jint listenPort, jboolean enableChunkedUploadLimit, jint chunkedUploadLimit, jboolean useIndex,
    jstring jindexRegion, jstring jindexName, jstring jindexPassword)
{
  Config::ConfigChangeCallbackGuard config_guard;

  Config::SetBaseOrCurrent(Config::NETPLAY_NICKNAME, GetJString(env, jnickname));
  Config::SetBaseOrCurrent(Config::NETPLAY_TRAVERSAL_CHOICE, GetJString(env, traversalChoice));
  Config::SetBaseOrCurrent(Config::NETPLAY_ADDRESS, GetJString(env, jaddress));
  Config::SetBaseOrCurrent(Config::NETPLAY_HOST_CODE, GetJString(env, jhostCode));
  Config::SetBaseOrCurrent(Config::NETPLAY_CONNECT_PORT, static_cast<u16>(connectPort));
  Config::SetBaseOrCurrent(Config::NETPLAY_HOST_PORT, static_cast<u16>(hostPort));
  Config::SetBaseOrCurrent(Config::NETPLAY_USE_UPNP, static_cast<bool>(useUpnp));
  Config::SetBaseOrCurrent(Config::NETPLAY_ENABLE_CHUNKED_UPLOAD_LIMIT,
                           static_cast<bool>(enableChunkedUploadLimit));
  Config::SetBaseOrCurrent(Config::NETPLAY_CHUNKED_UPLOAD_LIMIT,
                           static_cast<u32>(chunkedUploadLimit));
  Config::SetBaseOrCurrent(Config::NETPLAY_USE_INDEX, static_cast<bool>(useIndex));
  Config::SetBaseOrCurrent(Config::NETPLAY_INDEX_REGION, GetJString(env, jindexRegion));
  Config::SetBaseOrCurrent(Config::NETPLAY_INDEX_NAME, GetJString(env, jindexName));
  Config::SetBaseOrCurrent(Config::NETPLAY_INDEX_PASSWORD, GetJString(env, jindexPassword));
  Config::SetBaseOrCurrent(Config::NETPLAY_LISTEN_PORT, static_cast<u16>(listenPort));
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_Netplay_isClientConnected(JNIEnv* env, jclass)
{
  return static_cast<jboolean>(GetPointer(env)->IsConnected());
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_Netplay_sendMessage(JNIEnv* env, jclass,
                                                                    jstring jmessage)
{
  if (auto* client = GetPointer(env))
    client->SendChatMessage(GetJString(env, jmessage));
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_Netplay_adjustPadBufferSize(JNIEnv* env, jclass,
                                                                            jint buffer)
{
  if (auto* client = GetPointer(env))
    client->AdjustPadBufferSize(static_cast<u32>(buffer));
}

JNIEXPORT jlong JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_Netplay_Join(JNIEnv* env, jclass)
{
  const std::string traversal_choice = Config::Get(Config::NETPLAY_TRAVERSAL_CHOICE);
  const bool is_traversal = traversal_choice == "traversal";

  std::string host_ip;
  host_ip = is_traversal ? Config::Get(Config::NETPLAY_HOST_CODE) :
            Config::Get(Config::NETPLAY_ADDRESS);

  const u16 host_port = Config::Get(Config::NETPLAY_CONNECT_PORT);
  const std::string traversal_host = Config::Get(Config::NETPLAY_TRAVERSAL_SERVER);
  const u16 traversal_port = Config::Get(Config::NETPLAY_TRAVERSAL_PORT);
  const std::string nickname = Config::Get(Config::NETPLAY_NICKNAME);

  jobject jgame_file_cache = env->GetStaticObjectField(
      IDCache::GetGameFileCacheManagerClass(), IDCache::GetGameFileCacheManagerInstance());
  auto* game_file_cache = reinterpret_cast<UICommon::GameFileCache*>(
      env->GetLongField(jgame_file_cache, IDCache::GetGameFileCachePointer()));

  std::vector<std::shared_ptr<const UICommon::GameFile>> games;
  game_file_cache->ForEach(
      [&games](const std::shared_ptr<const UICommon::GameFile>& game) { games.push_back(game); });

  auto* client = new NetPlay::NetPlayClient(
      host_ip, host_port, new NetPlay::NetPlayUICallbacks(std::move(games)), nickname,
      NetPlay::NetTraversalConfig{is_traversal, traversal_host, traversal_port});

  return reinterpret_cast<jlong>(client);
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_Netplay_ReleaseNetplayClient(JNIEnv* env, jclass)
{
  delete GetPointer(env);
}

}  // extern "C"
