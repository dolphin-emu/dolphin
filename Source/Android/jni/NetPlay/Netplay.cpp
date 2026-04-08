// Copyright 2003 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string>

#include <jni.h>

#include "Common/CommonTypes.h"
#include "Core/Config/NetplaySettings.h"

#include "jni/AndroidCommon/AndroidCommon.h"

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

}  // extern "C"
