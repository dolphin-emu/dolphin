// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <jni.h>

#include "Common/IniFile.h"
#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"

static IniFile::Section* GetSectionPointer(JNIEnv* env, jobject obj)
{
  return reinterpret_cast<IniFile::Section*>(
      env->GetLongField(obj, IDCache::GetIniFileSectionPointer()));
}

static IniFile* GetIniFilePointer(JNIEnv* env, jobject obj)
{
  return reinterpret_cast<IniFile*>(env->GetLongField(obj, IDCache::GetIniFilePointer()));
}

static jobject SectionToJava(JNIEnv* env, jobject ini_file, IniFile::Section* section)
{
  if (!section)
    return nullptr;

  return env->NewObject(IDCache::GetIniFileSectionClass(), IDCache::GetIniFileSectionConstructor(),
                        ini_file, reinterpret_cast<jlong>(section));
}

template <typename T>
static T Get(JNIEnv* env, jobject obj, jstring section_name, jstring key, T default_value)
{
  T result;
  GetIniFilePointer(env, obj)
      ->GetOrCreateSection(GetJString(env, section_name))
      ->Get(GetJString(env, key), &result, default_value);
  return result;
}

template <typename T>
static void Set(JNIEnv* env, jobject obj, jstring section_name, jstring key, T new_value)
{
  GetIniFilePointer(env, obj)
      ->GetOrCreateSection(GetJString(env, section_name))
      ->Set(GetJString(env, key), new_value);
}

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_utils_IniFile_load(
    JNIEnv* env, jobject obj, jstring path, jboolean keep_current_data)
{
  return static_cast<jboolean>(
      GetIniFilePointer(env, obj)->Load(GetJString(env, path), keep_current_data));
}

JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_utils_IniFile_save(JNIEnv* env,
                                                                             jobject obj,
                                                                             jstring path)
{
  return static_cast<jboolean>(GetIniFilePointer(env, obj)->Save(GetJString(env, path)));
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_utils_IniFile_getString(
    JNIEnv* env, jobject obj, jstring section_name, jstring key, jstring default_value)
{
  return ToJString(env, Get(env, obj, section_name, key, GetJString(env, default_value)));
}

JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_utils_IniFile_getBoolean(
    JNIEnv* env, jobject obj, jstring section_name, jstring key, jboolean default_value)
{
  return static_cast<jboolean>(Get(env, obj, section_name, key, static_cast<bool>(default_value)));
}

JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_utils_IniFile_getInt(JNIEnv* env, jobject obj,
                                                                           jstring section_name,
                                                                           jstring key,
                                                                           jint default_value)
{
  return Get(env, obj, section_name, key, default_value);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_utils_IniFile_setString(
    JNIEnv* env, jobject obj, jstring section_name, jstring key, jstring new_value)
{
  Set(env, obj, section_name, key, GetJString(env, new_value));
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_utils_IniFile_setBoolean(
    JNIEnv* env, jobject obj, jstring section_name, jstring key, jboolean new_value)
{
  Set(env, obj, section_name, key, static_cast<bool>(new_value));
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_utils_IniFile_setInt(JNIEnv* env, jobject obj,
                                                                           jstring section_name,
                                                                           jstring key,
                                                                           jint new_value)
{
  Set(env, obj, section_name, key, new_value);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_utils_IniFile_finalize(JNIEnv* env,
                                                                             jobject obj)
{
  delete GetIniFilePointer(env, obj);
}

JNIEXPORT jlong JNICALL Java_org_dolphinemu_dolphinemu_utils_IniFile_newIniFile(JNIEnv* env,
                                                                                jobject obj)
{
  return reinterpret_cast<jlong>(new IniFile);
}

#ifdef __cplusplus
}
#endif
