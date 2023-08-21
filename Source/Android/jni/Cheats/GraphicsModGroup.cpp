// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <jni.h>

#include <set>
#include <vector>

#include "VideoCommon/GraphicsModSystem/Config/GraphicsMod.h"
#include "VideoCommon/GraphicsModSystem/Config/GraphicsModGroup.h"
#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"

static GraphicsModGroupConfig* GetPointer(JNIEnv* env, jobject obj)
{
  return reinterpret_cast<GraphicsModGroupConfig*>(
      env->GetLongField(obj, IDCache::GetGraphicsModGroupPointer()));
}

jobject GraphicsModToJava(JNIEnv* env, GraphicsModConfig* mod, jobject jGraphicsModGroup)
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
  GraphicsModGroupConfig* mod_group = GetPointer(env, obj);

  std::set<std::string> groups;

  for (const GraphicsModConfig& mod : mod_group->GetMods())
  {
    for (const GraphicsTargetGroupConfig& group : mod.m_groups)
      groups.insert(group.m_name);
  }

  std::vector<GraphicsModConfig*> mods;

  for (GraphicsModConfig& mod : mod_group->GetMods())
  {
    // If no group matches the mod's features, or if the mod has no features, skip it
    if (std::none_of(mod.m_features.begin(), mod.m_features.end(),
                     [&groups](const GraphicsModFeatureConfig& feature) {
                       return groups.count(feature.m_group) == 1;
                     }))
    {
      continue;
    }

    mods.push_back(&mod);
  }

  return VectorToJObjectArray(
      env, mods, IDCache::GetGraphicsModClass(),
      [obj](JNIEnv* env, GraphicsModConfig* mod) { return GraphicsModToJava(env, mod, obj); });
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
  auto* mod_group = new GraphicsModGroupConfig(GetJString(env, jGameId));

  mod_group->Load();

  return env->NewObject(IDCache::GetGraphicsModGroupClass(),
                        IDCache::GetGraphicsModGroupConstructor(), mod_group);
}
}
