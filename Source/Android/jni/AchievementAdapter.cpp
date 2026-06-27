// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <jni.h>

#include <latch>
#include "Common/Event.h"
#include "Common/HookableEvent.h"
#include "Core/AchievementManager.h"
#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"

extern "C" {

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_AchievementModel_init(JNIEnv* env, jclass)
{
  AchievementManager::GetInstance().Init(nullptr);
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_AchievementModel_login(JNIEnv* env, jclass,
                                                                              jstring password)
{
  auto& instance = AchievementManager::GetInstance();
  bool success;
  std::latch login_complete_event{1};
  Common::EventHook login_hook =
      instance.login_event.Register([&login_complete_event, &success](int result) {
        success = (result == RC_OK);
        login_complete_event.count_down();
      });
  instance.Login(GetJString(env, password));
  login_complete_event.wait();
  return success;
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_AchievementModel_logout(JNIEnv* env, jclass)
{
  AchievementManager::GetInstance().Logout();
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_AchievementModel_isHardcoreModeActive(
    JNIEnv* env, jclass)
{
  return AchievementManager::GetInstance().IsHardcoreModeActive();
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_AchievementModel_shutdown(JNIEnv* env,
                                                                                 jclass)
{
  AchievementManager::GetInstance().Shutdown();
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_achievements_model_AchievementProgressViewModel_isGameLoaded(
        JNIEnv* env, jclass)
{
    return AchievementManager::GetInstance().IsGameLoaded();
}

extern "C"
JNIEXPORT jobjectArray JNICALL
Java_org_dolphinemu_dolphinemu_features_achievements_model_AchievementProgressViewModel_fetchProgress(
        JNIEnv *env, jclass clazz)
{
    auto& instance = AchievementManager::GetInstance();
    if (!instance.IsGameLoaded())
        return env->NewObjectArray(0, IDCache::GetAchievementBucketClass(), nullptr);
    auto* client = instance.GetClient();
    auto* achievement_list =
            rc_client_create_achievement_list(client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE_AND_UNOFFICIAL,
                                              RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
    if (!achievement_list)
        return env->NewObjectArray(0, IDCache::GetAchievementBucketClass(), nullptr);

    auto progress = env->NewObjectArray(achievement_list->num_buckets, IDCache::GetAchievementBucketClass(), nullptr);
    for (u32 ix = 0; ix < achievement_list->num_buckets; ix++)
    {
        auto bucket = env->NewObject(IDCache::GetAchievementBucketClass(), IDCache::GetAchievementBucketConstructor(), achievement_list->buckets[ix].num_achievements);
        env->SetObjectArrayElement(progress, ix, bucket);
        env->SetObjectField(bucket, IDCache::GetAchievementBucketLabel(), ToJString(env, achievement_list->buckets[ix].label));
        env->SetIntField(bucket, IDCache::GetAchievementBucketSubsetId(), achievement_list->buckets[ix].subset_id);
        env->SetIntField(bucket, IDCache::GetAchievementBucketBucketType(), achievement_list->buckets[ix].bucket_type);
        auto bucket_achievements = static_cast<jobjectArray>(env->GetObjectField(bucket, IDCache::GetAchievementBucketAchievements()));
        for (u32 jx = 0; jx < achievement_list->buckets[ix].num_achievements; jx++)
        {
            auto achievement = env->GetObjectArrayElement(bucket_achievements, jx);
            env->SetObjectField(achievement, IDCache::GetAchievementTitle(), ToJString(env, achievement_list->buckets[ix].achievements[jx]->title));
            env->SetObjectField(achievement, IDCache::GetAchievementDescription(), ToJString(env, achievement_list->buckets[ix].achievements[jx]->description));
            env->SetObjectField(achievement, IDCache::GetAchievementBadgeName(), ToJString(env, achievement_list->buckets[ix].achievements[jx]->badge_name));
            env->SetObjectField(achievement, IDCache::GetAchievementMeasuredProgress(), ToJString(env, achievement_list->buckets[ix].achievements[jx]->measured_progress));
            env->SetFloatField(achievement, IDCache::GetAchievementMeasuredPercent(), achievement_list->buckets[ix].achievements[jx]->measured_percent);
            env->SetIntField(achievement, IDCache::GetAchievementId(), achievement_list->buckets[ix].achievements[jx]->id);
            env->SetIntField(achievement, IDCache::GetAchievementPoints(), achievement_list->buckets[ix].achievements[jx]->points);
            // TODO: Convert to string? Or something.
            //env->SetObjectField(achievement, IDCache::GetAchievementUnlockTime(), ToJString(env, achievement_list->buckets[ix].achievements[jx]->unlock_time));
            env->SetIntField(achievement, IDCache::GetAchievementState(), achievement_list->buckets[ix].achievements[jx]->state);
            env->SetIntField(achievement, IDCache::GetAchievementCategory(), achievement_list->buckets[ix].achievements[jx]->category);
            env->SetIntField(achievement, IDCache::GetAchievementBucket(), achievement_list->buckets[ix].achievements[jx]->bucket);
            env->SetIntField(achievement, IDCache::GetAchievementUnlocked(), achievement_list->buckets[ix].achievements[jx]->unlocked);
            env->SetFloatField(achievement, IDCache::GetAchievementRarity(), achievement_list->buckets[ix].achievements[jx]->rarity);
            env->SetFloatField(achievement, IDCache::GetAchievementRarityHardcore(), achievement_list->buckets[ix].achievements[jx]->rarity_hardcore);
            env->SetIntField(achievement, IDCache::GetAchievementType(), achievement_list->buckets[ix].achievements[jx]->type);
            env->SetObjectField(achievement, IDCache::GetAchievementBadgeUrl(), ToJString(env, achievement_list->buckets[ix].achievements[jx]->badge_url));
            env->SetObjectField(achievement, IDCache::GetAchievementBadgeLockedUrl(), ToJString(env, achievement_list->buckets[ix].achievements[jx]->badge_locked_url));
            env->DeleteLocalRef(achievement);
        }
        env->DeleteLocalRef(bucket);
    }
    rc_client_destroy_achievement_list(achievement_list);
    return progress;
}

}  // extern "C"
