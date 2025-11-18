// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <jni.h>

#include <vector>

#include "VideoCommon/GraphicsModSystem/Config/GraphicsModGroup.h"
#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"

static GraphicsModSystem::Config::GraphicsModGroup* GetPointer(JNIEnv* env, jobject obj)
{
  return reinterpret_cast<GraphicsModSystem::Config::GraphicsModGroup*>(
      env->GetLongField(obj, IDCache::GetGraphicsModGroupPointer()));
}

jobject GraphicsModToJava(JNIEnv* env,
                          GraphicsModSystem::Config::GraphicsModGroup::GraphicsModWithMetadata* mod,
                          jobject jGraphicsModGroup)
{
  return env->NewObject(IDCache::GetGraphicsModClass(), IDCache::GetGraphicsModConstructor(),
                        reinterpret_cast<jlong>(mod), jGraphicsModGroup);
}

extern "C" {

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_cheats_model_GraphicsModGroup_finalize(JNIEnv* env,
                                                                               jobject obj)
{
  delete GetPointer(env, obj);
}

JNIEXPORT jobjectArray JNICALL
Java_org_dolphinemu_dolphinemu_features_cheats_model_GraphicsModGroup_getMods(JNIEnv* env,
                                                                              jobject obj)
{
  GraphicsModSystem::Config::GraphicsModGroup* mod_group = GetPointer(env, obj);
  std::vector<GraphicsModSystem::Config::GraphicsModGroup::GraphicsModWithMetadata*> mods;

  for (auto& mod : mod_group->GetMods())
  {
    if (mod.m_mod.m_actions.empty())
      continue;

    mods.push_back(&mod);
  }

  return VectorToJObjectArray(
      env, mods, IDCache::GetGraphicsModClass(),
      [obj](JNIEnv* env,
            GraphicsModSystem::Config::GraphicsModGroup::GraphicsModWithMetadata* mod) {
        return GraphicsModToJava(env, mod, obj);
      });
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_cheats_model_GraphicsModGroup_save(JNIEnv* env, jobject obj)
{
  GetPointer(env, obj)->Save();
}

JNIEXPORT jobject JNICALL
Java_org_dolphinemu_dolphinemu_features_cheats_model_GraphicsModGroup_load(JNIEnv* env, jclass,
                                                                           jstring jGameId)
{
  auto* mod_group = new GraphicsModSystem::Config::GraphicsModGroup(GetJString(env, jGameId));

  mod_group->Load();

  return env->NewObject(IDCache::GetGraphicsModGroupClass(),
                        IDCache::GetGraphicsModGroupConstructor(), mod_group);
}
}
