// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <jni.h>

#include <array>

#include "AndroidCommon/AndroidCommon.h"
#include "AndroidCommon/IDCache.h"
#include "Core/IOS/USB/Emulated/Skylander.h"
#include "Core/System.h"

extern "C" {

JNIEXPORT jobject JNICALL
Java_org_dolphinemu_dolphinemu_features_skylanders_SkylanderConfig_getSkylanderMap(JNIEnv* env,
                                                                                   jclass clazz)
{
  jobject hash_map_obj = env->NewObject(IDCache::GetHashMapClass(), IDCache::GetHashMapInit(),
                                        static_cast<u16>(IOS::HLE::USB::list_skylanders.size()));

  jclass skylander_class =
      env->FindClass("org/dolphinemu/dolphinemu/features/skylanders/model/SkylanderPair");

  jmethodID skylander_init = env->GetMethodID(skylander_class, "<init>", "(II)V");

  for (const auto& it : IOS::HLE::USB::list_skylanders)
  {
    const std::string& name = it.second;
    jobject skylander_obj =
        env->NewObject(skylander_class, skylander_init, it.first.first, it.first.second);
    env->CallObjectMethod(hash_map_obj, IDCache::GetHashMapPut(), skylander_obj,
                          ToJString(env, name));
    env->DeleteLocalRef(skylander_obj);
  }

  return hash_map_obj;
}

JNIEXPORT jobject JNICALL
Java_org_dolphinemu_dolphinemu_features_skylanders_SkylanderConfig_getInverseSkylanderMap(
    JNIEnv* env, jclass clazz)
{
  jobject hash_map_obj = env->NewObject(IDCache::GetHashMapClass(), IDCache::GetHashMapInit(),
                                        static_cast<u16>(IOS::HLE::USB::list_skylanders.size()));

  jclass skylander_class =
      env->FindClass("org/dolphinemu/dolphinemu/features/skylanders/model/SkylanderPair");

  jmethodID skylander_init = env->GetMethodID(skylander_class, "<init>", "(II)V");

  for (const auto& it : IOS::HLE::USB::list_skylanders)
  {
    const std::string& name = it.second;
    jobject skylander_obj =
        env->NewObject(skylander_class, skylander_init, it.first.first, it.first.second);
    env->CallObjectMethod(hash_map_obj, IDCache::GetHashMapPut(), ToJString(env, name),
                          skylander_obj);
    env->DeleteLocalRef(skylander_obj);
  }

  return hash_map_obj;
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_skylanders_SkylanderConfig_removeSkylander(JNIEnv* env,
                                                                                   jclass clazz,
                                                                                   jint slot)
{
  auto& system = Core::System::GetInstance();
  return static_cast<jboolean>(system.GetSkylanderPortal().RemoveSkylander(slot));
}

JNIEXPORT jobject JNICALL
Java_org_dolphinemu_dolphinemu_features_skylanders_SkylanderConfig_loadSkylander(JNIEnv* env,
                                                                                 jclass clazz,
                                                                                 jint slot,
                                                                                 jstring file_name)
{
  File::IOFile sky_file(GetJString(env, file_name), "r+b");
  if (!sky_file)
  {
    return nullptr;
  }
  std::array<u8, 0x40 * 0x10> file_data;
  if (!sky_file.ReadBytes(file_data.data(), file_data.size()))
  {
    return nullptr;
  }

  jclass pair_class = env->FindClass("android/util/Pair");
  jmethodID pair_init =
      env->GetMethodID(pair_class, "<init>", "(Ljava/lang/Object;Ljava/lang/Object;)V");
  jclass integer_class = env->FindClass("java/lang/Integer");
  jmethodID int_init = env->GetMethodID(integer_class, "<init>", "(I)V");

  auto& system = Core::System::GetInstance();
  system.GetSkylanderPortal().RemoveSkylander(slot);
  std::string name = "Unknown";

  std::pair<u16, u16> id_var = system.GetSkylanderPortal().CalculateIDs(file_data);

  const auto it = IOS::HLE::USB::list_skylanders.find(std::make_pair(id_var.first, id_var.second));

  if (it != IOS::HLE::USB::list_skylanders.end())
  {
    name = it->second;
  }

  return env->NewObject(pair_class, pair_init,
                        env->NewObject(integer_class, int_init,
                                       system.GetSkylanderPortal().LoadSkylander(
                                           file_data.data(), std::move(sky_file))),
                        ToJString(env, name));
}

JNIEXPORT jobject JNICALL
Java_org_dolphinemu_dolphinemu_features_skylanders_SkylanderConfig_createSkylander(
    JNIEnv* env, jclass clazz, jint id, jint var, jstring fileName, jint slot)
{
  u16 sky_id = static_cast<u16>(id);
  u16 sky_var = static_cast<u16>(var);

  std::string file_name = GetJString(env, fileName);

  auto& system = Core::System::GetInstance();
  system.GetSkylanderPortal().CreateSkylander(file_name, sky_id, sky_var);
  system.GetSkylanderPortal().RemoveSkylander(slot);

  jclass pair_class = env->FindClass("android/util/Pair");
  jmethodID pair_init =
      env->GetMethodID(pair_class, "<init>", "(Ljava/lang/Object;Ljava/lang/Object;)V");

  jclass integer_class = env->FindClass("java/lang/Integer");
  jmethodID integer_init = env->GetMethodID(integer_class, "<init>", "(I)V");

  File::IOFile sky_file(file_name, "r+b");
  if (!sky_file)
  {
    return nullptr;
  }
  std::array<u8, 0x40 * 0x10> file_data;
  if (!sky_file.ReadBytes(file_data.data(), file_data.size()))
  {
    return nullptr;
  }

  std::string name = "Unknown";

  const auto it = IOS::HLE::USB::list_skylanders.find(std::make_pair(sky_id, sky_var));

  if (it != IOS::HLE::USB::list_skylanders.end())
  {
    name = it->second;
  }

  return env->NewObject(pair_class, pair_init,
                        env->NewObject(integer_class, integer_init,
                                       system.GetSkylanderPortal().LoadSkylander(
                                           file_data.data(), std::move(sky_file))),
                        ToJString(env, name));
}
}
