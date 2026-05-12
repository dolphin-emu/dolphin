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

extern "C" {

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetplaySession_nativeSendMessage(JNIEnv* env, jobject obj,
                                                                          jstring jmessage)
{
  if (auto* client = GetClientPointer(env, obj))
    client->SendChatMessage(GetJString(env, jmessage));
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetplaySession_nativeAdjustPadBufferSize(JNIEnv* env,
                                                                                   jobject obj,
                                                                                   jint buffer)
{
  if (auto* client = GetClientPointer(env, obj))
    client->AdjustPadBufferSize(static_cast<u32>(buffer));
}

JNIEXPORT jlong JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetplaySession_nativeCreateUICallbacks(JNIEnv* env,
                                                                                 jobject obj)
{
  jobject jgame_file_cache = env->GetStaticObjectField(
      IDCache::GetGameFileCacheManagerClass(), IDCache::GetGameFileCacheManagerInstance());
  auto* game_file_cache = reinterpret_cast<UICommon::GameFileCache*>(
      env->GetLongField(jgame_file_cache, IDCache::GetGameFileCachePointer()));

  std::vector<std::shared_ptr<const UICommon::GameFile>> games;
  game_file_cache->ForEach(
      [&games](const std::shared_ptr<const UICommon::GameFile>& game) { games.push_back(game); });

  return reinterpret_cast<jlong>(new NetPlay::NetPlayUICallbacks(obj, std::move(games)));
}

JNIEXPORT jlong JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetplaySession_nativeJoin(JNIEnv* env, jobject obj)
{
  auto* ui = GetUICallbacksPointer(env, obj);

  const std::string traversal_host = Config::Get(Config::NETPLAY_TRAVERSAL_SERVER);
  const u16 traversal_port = Config::Get(Config::NETPLAY_TRAVERSAL_PORT);
  const std::string nickname = Config::Get(Config::NETPLAY_NICKNAME);

  const std::string traversal_choice = Config::Get(Config::NETPLAY_TRAVERSAL_CHOICE);
  const bool is_traversal = traversal_choice == "traversal";
  const std::string host_ip = is_traversal ? Config::Get(Config::NETPLAY_HOST_CODE) :
                                             Config::Get(Config::NETPLAY_ADDRESS);
  const u16 host_port = Config::Get(Config::NETPLAY_CONNECT_PORT);

  auto* client = new NetPlay::NetPlayClient(
      host_ip, host_port, ui, nickname,
      NetPlay::NetTraversalConfig{is_traversal, traversal_host, traversal_port});

  if (!client->IsConnected())
  {
    delete client;
    return 0;
  }

  return reinterpret_cast<jlong>(client);
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetplaySession_nativeReleaseUICallbacks(JNIEnv*,
                                                                                  jobject,
                                                                                  jlong pointer)
{
  delete reinterpret_cast<NetPlay::NetPlayUICallbacks*>(pointer);
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetplaySession_nativeReleaseBootSessionData(JNIEnv*,
                                                                                      jobject,
                                                                                      jlong pointer)
{
  delete reinterpret_cast<BootSessionData*>(pointer);
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetplaySession_nativeReleaseClient(JNIEnv*, jobject,
                                                                             jlong pointer)
{
  delete reinterpret_cast<NetPlay::NetPlayClient*>(pointer);
}

}  // extern "C"
