// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string>

#include <jni.h>

#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"

#include "Common/FatFsUtil.h"
#include "Common/ScopeGuard.h"

#include "Core/CommonTitles.h"
#include "Core/HW/WiiSave.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/IOS.h"
#include "Core/WiiUtils.h"

#include "DiscIO/NANDImporter.h"

// The hardcoded values here must match WiiUtils.java
static jint ConvertCopyResult(WiiSave::CopyResult result)
{
  switch (result)
  {
  case WiiSave::CopyResult::Success:
    return 0;
  case WiiSave::CopyResult::Error:
    return 1;
  case WiiSave::CopyResult::Cancelled:
    return 2;
  case WiiSave::CopyResult::CorruptedSource:
    return 3;
  case WiiSave::CopyResult::TitleMissing:
    return 4;
  default:
    ASSERT(false);
    return 1;
  }

  static_assert(static_cast<int>(WiiSave::CopyResult::NumberOfEntries) == 5);
}

static jint ConvertUpdateResult(WiiUtils::UpdateResult result)
{
  switch (result)
  {
  case WiiUtils::UpdateResult::Succeeded:
    return 0;
  case WiiUtils::UpdateResult::AlreadyUpToDate:
    return 1;
  case WiiUtils::UpdateResult::RegionMismatch:
    return 2;
  case WiiUtils::UpdateResult::MissingUpdatePartition:
    return 3;
  case WiiUtils::UpdateResult::DiscReadFailed:
    return 4;
  case WiiUtils::UpdateResult::ServerFailed:
    return 5;
  case WiiUtils::UpdateResult::DownloadFailed:
    return 6;
  case WiiUtils::UpdateResult::ImportFailed:
    return 7;
  case WiiUtils::UpdateResult::Cancelled:
    return 8;
  default:
    ASSERT(false);
    return 7;
  }

  static_assert(static_cast<int>(WiiUtils::UpdateResult::NumberOfEntries) == 9);
}

extern "C" {

JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_utils_WiiUtils_installWAD(JNIEnv* env,
                                                                                    jclass,
                                                                                    jstring jFile)
{
  const std::string path = GetJString(env, jFile);
  return static_cast<jboolean>(WiiUtils::InstallWAD(path));
}

JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_utils_WiiUtils_importWiiSave(
    JNIEnv* env, jclass, jstring jFile, jobject jCanOverwrite)
{
  const std::string path = GetJString(env, jFile);
  const auto can_overwrite = [&] {
    const jmethodID get = IDCache::GetBooleanSupplierGet();
    return static_cast<bool>(env->CallBooleanMethod(jCanOverwrite, get));
  };

  return ConvertCopyResult(WiiSave::Import(path, can_overwrite));
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_utils_WiiUtils_importNANDBin(JNIEnv* env,
                                                                                   jclass,
                                                                                   jstring jFile)
{
  const std::string path = GetJString(env, jFile);

  return DiscIO::NANDImporter().ImportNANDBin(
      path,
      [] {
        // This callback gets called every now and then in case we want to update the GUI. However,
        // we have no way of knowing what the current progress is, so we can't do anything
        // especially useful. DolphinQt chooses to show the elapsed time, for reference.
      },
      [] {
        // This callback gets called if the NAND file does not have decryption keys appended to it.
        // We're supposed to ask the user for a separate file containing keys, but this is probably
        // more work to implement on Android than it's worth, as this case almost never comes up.
        PanicAlertFmtT("The decryption keys need to be appended to the NAND backup file.");
        return "";
      });
}

JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_utils_WiiUtils_doOnlineUpdate(
    JNIEnv* env, jclass, jstring jRegion, jobject jCallback)
{
  const std::string region = GetJString(env, jRegion);

  jobject jCallbackGlobal = env->NewGlobalRef(jCallback);
  Common::ScopeGuard scope_guard([jCallbackGlobal, env] { env->DeleteGlobalRef(jCallbackGlobal); });

  const auto callback = [&jCallbackGlobal](int processed, int total, u64 title_id) {
    JNIEnv* env = IDCache::GetEnvForThread();
    return static_cast<bool>(env->CallBooleanMethod(
        jCallbackGlobal, IDCache::GetWiiUpdateCallbackFunction(), processed, total, title_id));
  };

  WiiUtils::UpdateResult result = WiiUtils::DoOnlineUpdate(callback, region);

  return ConvertUpdateResult(result);
}

JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_utils_WiiUtils_doDiscUpdate(JNIEnv* env,
                                                                                  jclass,
                                                                                  jstring jPath,
                                                                                  jobject jCallback)
{
  const std::string path = GetJString(env, jPath);

  jobject jCallbackGlobal = env->NewGlobalRef(jCallback);
  Common::ScopeGuard scope_guard([jCallbackGlobal, env] { env->DeleteGlobalRef(jCallbackGlobal); });

  const auto callback = [&jCallbackGlobal](int processed, int total, u64 title_id) {
    JNIEnv* env = IDCache::GetEnvForThread();
    return static_cast<bool>(env->CallBooleanMethod(
        jCallbackGlobal, IDCache::GetWiiUpdateCallbackFunction(), processed, total, title_id));
  };

  WiiUtils::UpdateResult result = WiiUtils::DoDiscUpdate(callback, path);

  return ConvertUpdateResult(result);
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_utils_WiiUtils_isSystemMenuInstalled(JNIEnv* env, jclass)
{
  IOS::HLE::Kernel ios;
  const auto tmd = ios.GetESCore().FindInstalledTMD(Titles::SYSTEM_MENU);

  return tmd.IsValid();
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_utils_WiiUtils_isSystemMenuvWii(JNIEnv* env, jclass)
{
  IOS::HLE::Kernel ios;
  const auto tmd = ios.GetESCore().FindInstalledTMD(Titles::SYSTEM_MENU);

  return tmd.IsvWii();
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_utils_WiiUtils_getSystemMenuVersion(JNIEnv* env, jclass)
{
  IOS::HLE::Kernel ios;
  const auto tmd = ios.GetESCore().FindInstalledTMD(Titles::SYSTEM_MENU);

  if (!tmd.IsValid())
  {
    return ToJString(env, "");
  }

  return ToJString(env, DiscIO::GetSysMenuVersionString(tmd.GetTitleVersion(), tmd.IsvWii()));
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_utils_WiiUtils_syncSdFolderToSdImage(JNIEnv* env, jclass)
{
  return static_cast<jboolean>(Common::SyncSDFolderToSDImage([]() { return false; }, false));
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_utils_WiiUtils_syncSdImageToSdFolder(JNIEnv* env, jclass)
{
  return static_cast<jboolean>(Common::SyncSDImageToSDFolder([]() { return false; }));
}
}
