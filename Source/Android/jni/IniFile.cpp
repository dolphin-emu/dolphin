// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <jni.h>

#include "Common/IniFile.h"
#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"

using Common::IniFile;

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
static T GetInSection(JNIEnv* env, jobject obj, jstring key, T default_value)
{
  T result;
  GetSectionPointer(env, obj)->Get(GetJString(env, key), &result, default_value);
  return result;
}

template <typename T>
static void SetInSection(JNIEnv* env, jobject obj, jstring key, T new_value)
{
  GetSectionPointer(env, obj)->Set(GetJString(env, key), new_value);
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

extern "C" {

JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_utils_IniFile_00024Section_exists(
    JNIEnv* env, jobject obj, jstring key)
{
  return static_cast<jboolean>(GetSectionPointer(env, obj)->Exists(GetJString(env, key)));
}

JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_utils_IniFile_00024Section_delete(
    JNIEnv* env, jobject obj, jstring key)
{
  return static_cast<jboolean>(GetSectionPointer(env, obj)->Delete(GetJString(env, key)));
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_utils_IniFile_00024Section_getString(
    JNIEnv* env, jobject obj, jstring key, jstring default_value)
{
  return ToJString(env, GetInSection(env, obj, key, GetJString(env, default_value)));
}

JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_utils_IniFile_00024Section_getBoolean(
    JNIEnv* env, jobject obj, jstring key, jboolean default_value)
{
  return static_cast<jboolean>(GetInSection(env, obj, key, static_cast<bool>(default_value)));
}

JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_utils_IniFile_00024Section_getInt(
    JNIEnv* env, jobject obj, jstring key, jint default_value)
{
  return GetInSection(env, obj, key, default_value);
}

JNIEXPORT jfloat JNICALL Java_org_dolphinemu_dolphinemu_utils_IniFile_00024Section_getFloat(
    JNIEnv* env, jobject obj, jstring key, jfloat default_value)
{
  return GetInSection(env, obj, key, default_value);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_utils_IniFile_00024Section_setString(
    JNIEnv* env, jobject obj, jstring key, jstring new_value)
{
  SetInSection(env, obj, key, GetJString(env, new_value));
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_utils_IniFile_00024Section_setBoolean(
    JNIEnv* env, jobject obj, jstring key, jboolean new_value)
{
  SetInSection(env, obj, key, static_cast<bool>(new_value));
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_utils_IniFile_00024Section_setInt(
    JNIEnv* env, jobject obj, jstring key, jint new_value)
{
  SetInSection(env, obj, key, new_value);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_utils_IniFile_00024Section_setFloat(
    JNIEnv* env, jobject obj, jstring key, jfloat new_value)
{
  SetInSection(env, obj, key, new_value);
}

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

JNIEXPORT jobject JNICALL Java_org_dolphinemu_dolphinemu_utils_IniFile_getOrCreateSection(
    JNIEnv* env, jobject obj, jstring section_name)
{
  return SectionToJava(
      env, obj, GetIniFilePointer(env, obj)->GetOrCreateSection(GetJString(env, section_name)));
}

JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_utils_IniFile_exists__Ljava_lang_String_2(
    JNIEnv* env, jobject obj, jstring section_name)
{
  return static_cast<jboolean>(GetIniFilePointer(env, obj)->Exists(GetJString(env, section_name)));
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_utils_IniFile_exists__Ljava_lang_String_2Ljava_lang_String_2(
    JNIEnv* env, jobject obj, jstring section_name, jstring key)
{
  return static_cast<jboolean>(
      GetIniFilePointer(env, obj)->Exists(GetJString(env, section_name), GetJString(env, key)));
}

JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_utils_IniFile_deleteSection(
    JNIEnv* env, jobject obj, jstring section_name)
{
  return static_cast<jboolean>(
      GetIniFilePointer(env, obj)->DeleteSection(GetJString(env, section_name)));
}

JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_utils_IniFile_deleteKey(
    JNIEnv* env, jobject obj, jstring section_name, jstring key)
{
  return static_cast<jboolean>(
      GetIniFilePointer(env, obj)->DeleteKey(GetJString(env, section_name), GetJString(env, key)));
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

JNIEXPORT jfloat JNICALL Java_org_dolphinemu_dolphinemu_utils_IniFile_getFloat(
    JNIEnv* env, jobject obj, jstring section_name, jstring key, jfloat default_value)
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

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_utils_IniFile_setFloat(
    JNIEnv* env, jobject obj, jstring section_name, jstring key, jfloat new_value)
{
  Set(env, obj, section_name, key, new_value);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_utils_IniFile_finalize(JNIEnv* env,
                                                                             jobject obj)
{
  delete GetIniFilePointer(env, obj);
}

JNIEXPORT jlong JNICALL Java_org_dolphinemu_dolphinemu_utils_IniFile_newIniFile(JNIEnv* env,
                                                                                jobject)
{
  return reinterpret_cast<jlong>(new IniFile);
}

JNIEXPORT jlong JNICALL Java_org_dolphinemu_dolphinemu_utils_IniFile_copyIniFile(JNIEnv* env,
                                                                                 jobject,
                                                                                 jobject other)
{
  return reinterpret_cast<jlong>(new IniFile(*GetIniFilePointer(env, other)));
}
}
