// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "jni/GameList/TitleDatabase.h"

#include <jni.h>

#include "Core/TitleDatabase.h"
#include "jni/AndroidCommon/IDCache.h"

Core::TitleDatabase* TitleDatabaseFromJava(JNIEnv* env, jobject obj)
{
  return reinterpret_cast<Core::TitleDatabase*>(
      env->GetLongField(obj, IDCache::GetTitleDatabasePointer()));
}
extern "C" {

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_model_TitleDatabase_finalize(JNIEnv* env,
                                                                                   jobject obj)
{
  delete TitleDatabaseFromJava(env, obj);
}

JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_model_TitleDatabase_userTitleMapEquals(
    JNIEnv* env, jobject obj, jobject other)
{
  return other != nullptr && TitleDatabaseFromJava(env, obj)->GetUserTitleMap() ==
                                 TitleDatabaseFromJava(env, other)->GetUserTitleMap();
}

JNIEXPORT jobject JNICALL Java_org_dolphinemu_dolphinemu_model_TitleDatabase_load(JNIEnv* env,
                                                                                  jclass)
{
  return env->NewObject(IDCache::GetTitleDatabaseClass(), IDCache::GetTitleDatabaseConstructor(),
                        reinterpret_cast<jlong>(new Core::TitleDatabase));
}
}
