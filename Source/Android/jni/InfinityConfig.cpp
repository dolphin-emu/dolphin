// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <jni.h>

#include <array>

#include "AndroidCommon/AndroidCommon.h"
#include "AndroidCommon/IDCache.h"
#include "Core/IOS/USB/Emulated/Infinity.h"
#include "Core/System.h"

extern "C" {

JNIEXPORT jobject JNICALL
Java_org_dolphinemu_dolphinemu_features_infinitybase_InfinityConfig_getFigureMap(JNIEnv* env,
                                                                                 jobject obj)
{
  auto& system = Core::System::GetInstance();

  jobject hash_map_obj = env->NewObject(IDCache::GetHashMapClass(), IDCache::GetHashMapInit(),
                                        system.GetInfinityBase().GetFigureList().size());

  jclass long_class = env->FindClass("java/lang/Long");
  jmethodID long_init = env->GetMethodID(long_class, "<init>", "(J)V");

  for (const auto& it : system.GetInfinityBase().GetFigureList())
  {
    const std::string& name = it.first;
    jobject figure_number = env->NewObject(long_class, long_init, (jlong)it.second);
    env->CallObjectMethod(hash_map_obj, IDCache::GetHashMapPut(), figure_number,
                          ToJString(env, name));
    env->DeleteLocalRef(figure_number);
  }

  return hash_map_obj;
}

JNIEXPORT jobject JNICALL
Java_org_dolphinemu_dolphinemu_features_infinitybase_InfinityConfig_getInverseFigureMap(JNIEnv* env,
                                                                                        jobject obj)
{
  auto& system = Core::System::GetInstance();

  jobject hash_map_obj = env->NewObject(IDCache::GetHashMapClass(), IDCache::GetHashMapInit(),
                                        system.GetInfinityBase().GetFigureList().size());

  jclass long_class = env->FindClass("java/lang/Long");
  jmethodID long_init = env->GetMethodID(long_class, "<init>", "(J)V");

  for (const auto& it : system.GetInfinityBase().GetFigureList())
  {
    const std::string& name = it.first;
    jobject figure_number = env->NewObject(long_class, long_init, (jlong)it.second);
    env->CallObjectMethod(hash_map_obj, IDCache::GetHashMapPut(), ToJString(env, name),
                          figure_number);
    env->DeleteLocalRef(figure_number);
  }

  return hash_map_obj;
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_infinitybase_InfinityConfig_removeFigure(JNIEnv* env,
                                                                                 jclass clazz,
                                                                                 jint position)
{
  auto& system = Core::System::GetInstance();
  system.GetInfinityBase().RemoveFigure(position);
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_infinitybase_InfinityConfig_loadFigure(JNIEnv* env,
                                                                               jclass clazz,
                                                                               jint position,
                                                                               jstring file_name)
{
  File::IOFile inf_file(GetJString(env, file_name), "r+b");
  if (!inf_file)
  {
    return nullptr;
  }
  std::array<u8, 0x14 * 0x10> file_data{};
  if (!inf_file.ReadBytes(file_data.data(), file_data.size()))
  {
    return nullptr;
  }

  auto& system = Core::System::GetInstance();
  system.GetInfinityBase().RemoveFigure(position);
  return ToJString(env,
                   system.GetInfinityBase().LoadFigure(file_data, std::move(inf_file), position));
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_infinitybase_InfinityConfig_createFigure(
    JNIEnv* env, jclass clazz, jlong figure_number, jstring fileName, jint position)
{
  u32 fig_num = static_cast<u32>(figure_number);

  std::string file_name = GetJString(env, fileName);

  auto& system = Core::System::GetInstance();
  system.GetInfinityBase().CreateFigure(file_name, fig_num);
  system.GetInfinityBase().RemoveFigure(position);

  File::IOFile inf_file(file_name, "r+b");
  if (!inf_file)
  {
    return nullptr;
  }
  std::array<u8, 0x14 * 0x10> file_data{};
  if (!inf_file.ReadBytes(file_data.data(), file_data.size()))
  {
    return nullptr;
  }

  return ToJString(env,
                   system.GetInfinityBase().LoadFigure(file_data, std::move(inf_file), position));
}
}
