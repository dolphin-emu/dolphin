// Copyright 2003 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <memory>
#include <string>
#include <vector>

#include <jni.h>

#include "Common/CommonTypes.h"
#include "Core/Boot/Boot.h"
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
Java_org_dolphinemu_dolphinemu_features_netplay_Netplay_ReleaseBootSessionData(JNIEnv* env, jclass)
{
  auto* data = reinterpret_cast<BootSessionData*>(
      env->GetStaticLongField(IDCache::GetNetplayClass(), IDCache::GetNetplayBootSessionDataPointer()));
  delete data;
  env->SetStaticLongField(IDCache::GetNetplayClass(), IDCache::GetNetplayBootSessionDataPointer(), 0);
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_Netplay_ReleaseNetplayClient(JNIEnv* env, jclass)
{
  delete GetPointer(env);
}

}  // extern "C"
