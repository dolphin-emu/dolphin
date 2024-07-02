// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string>

#include <jni.h>

#include "VideoCommon/GraphicsModSystem/Config/GraphicsModGroup.h"
#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"

static GraphicsModSystem::Config::GraphicsModGroup::GraphicsModWithMetadata* GetPointer(JNIEnv* env,
                                                                                        jobject obj)
{
  return reinterpret_cast<GraphicsModSystem::Config::GraphicsModGroup::GraphicsModWithMetadata*>(
      env->GetLongField(obj, IDCache::GetGraphicsModPointer()));
}

extern "C" {

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_cheats_model_GraphicsMod_getName(JNIEnv* env, jobject obj)
{
  return ToJString(env, GetPointer(env, obj)->m_mod.m_title);
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_cheats_model_GraphicsMod_getCreator(JNIEnv* env,
                                                                            jobject obj)
{
  return ToJString(env, GetPointer(env, obj)->m_mod.m_author);
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_cheats_model_GraphicsMod_getNotes(JNIEnv* env, jobject obj)
{
  return ToJString(env, GetPointer(env, obj)->m_mod.m_description);
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_cheats_model_GraphicsMod_getEnabled(JNIEnv* env,
                                                                            jobject obj)
{
  return static_cast<jboolean>(GetPointer(env, obj)->m_enabled);
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_cheats_model_GraphicsMod_setEnabledImpl(JNIEnv* env,
                                                                                jobject obj,
                                                                                jboolean enabled)
{
  GetPointer(env, obj)->m_enabled = static_cast<bool>(enabled);
}
}
